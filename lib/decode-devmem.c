#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sysexits.h>

#include <sys/fcntl.h>
#include <sys/mman.h>

#include <deserialize.h>

#include "common.h"

static uint8_t const	DATA[] = {
#include REGISTERS_DEFS_H
};

struct decode_info {
	int			fd;
	void const		*mem;
	uintptr_t		page;
	bool			is_mapped;
	size_t			page_sz;
	uintptr_t		last_addr;
	unsigned int		num_shown;
	struct cpu_unit const	*last_unit;

	uintmax_t		value;
};

static int _mem_read(uintptr_t addr, unsigned int width, uintmax_t *val,
		     struct decode_info	*priv)
{
	uintptr_t		page = addr & ~(priv->page_sz - 1);
	void const		*mem;
	union {
		uint8_t		u8;
		uint16_t	u16;
		uint32_t	u32;
		uint64_t	u64;
	}			tmp;

	if (!priv->is_mapped || priv->page != page) {
		if (priv->is_mapped) {
			munmap((void *)priv->mem, priv->page_sz);
			priv->mem = NULL;
			priv->is_mapped = false;
		}

		mem = mmap(NULL, priv->page_sz, PROT_READ, MAP_SHARED,
			   priv->fd, page);
		if (mem == MAP_FAILED) {
			perror("mmap()");
			return -1;
		}

		priv->mem = mem;
		priv->page = page;
		priv->is_mapped = true;
	}

	mem = priv->mem + (addr & (priv->page_sz - 1));

	switch (width) {
	case 8:
		memcpy(&tmp.u8, mem, sizeof tmp.u8);
		*val = tmp.u8;
		break;
	case 16:
		memcpy(&tmp.u16, mem, sizeof tmp.u16);
		*val = tmp.u16;
		break;
	case 32:
		memcpy(&tmp.u32, mem, sizeof tmp.u32);
		*val = tmp.u32;
		break;
	case 64:
		memcpy(&tmp.u64, mem, sizeof tmp.u64);
		*val = tmp.u64;
		break;
	default:
		abort();
		break;
	}

	return 0;
}

static int _decode_reg(struct cpu_register const *reg, void *priv_)
{
	struct decode_info	*priv = priv_;
	unsigned long		addr = reg->offset + reg->unit->start;
	uintmax_t		val;
	int			rc;

	if (priv->fd < 0) {
		val = priv->value;
	} else {
		rc = _mem_read(addr, reg->width, &val, priv);
		if (rc < 0)
			return rc;
	}

	if (reg->unit !=  priv->last_unit) {
		col_printf("%s======================== %" STR_FMT " ==============================",
			   priv->num_shown > 0 ? "\n" : "",
			   STR_ARG(&reg->unit->name));
	}

	col_printf("\n&@0x%08lx&# &N%-28" STR_FMT "&#\t&~0x%0*llx&#",
		   addr, STR_ARG(&reg->name),
		   (unsigned int)(reg->width / 4),
		   (unsigned long long)val);

	deserialize_decode_reg(reg, val, priv);

	priv->last_addr = addr;
	priv->last_unit = reg->unit;

	col_printf("\n");

	++priv->num_shown;

	return 0;
}

int main(int argc, char *argv[])
{
	void const	*stream = DATA;
	size_t		stream_len = sizeof DATA;
	struct cpu_unit	*units = NULL;
	size_t		num_units = 0;
	uintptr_t	addr_start = 0;
	uintptr_t	addr_end = ~((uintptr_t)0);

	bool		do_decode_single = false;

	int		fd;
	int		rc;

	struct decode_info	info = {
		.is_mapped	= false,
		.page_sz	= sysconf(_SC_PAGESIZE),
	};

	if (argc >= 2) {
		char const	*addr = argv[1];
		if (addr[0] == '?') {
			do_decode_single = true;
			++addr;
		}
		addr_start = strtoul(addr, NULL, 0);
	}

	if (argc >= 3)
		addr_end = strtoul(argv[2], NULL, 0);

	if (!deserialize_cpu_units(&units, &num_units, &stream, &stream_len)) {
		fprintf(stderr, "failed to decode stream\n");
		return EX_SOFTWARE;
	}

	if (do_decode_single) {
		fd = -1;
		info.value = addr_end;
		addr_end = addr_start;
	} else {
		fd = open("/dev/mem", O_RDONLY);
		if (fd < 0) {
			perror("open(/dev/mem");
			return EX_OSERR;
		}
	}

	info.fd = fd;
	info.last_addr = addr_start;

	rc = deserialize_decode_range(units, num_units, addr_start, addr_end,
				      _decode_reg, &info);

	if (fd >= 0)
		close(fd);

	if (rc < 0)
		return EX_OSERR;

	return info.num_shown > 0 ? EX_OK : EX_NOINPUT;
}
