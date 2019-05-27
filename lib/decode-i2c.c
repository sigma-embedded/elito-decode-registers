#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sysexits.h>

#include <sys/fcntl.h>
#include <sys/ioctl.h>

#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#include <deserialize.h>

#include "common.h"

#ifndef REGISTER_ADDR_TYPE
#  define REGISTER_ADDR_TYPE	uint16_t
#endif

static uint8_t const	DATA[] = {
#include REGISTERS_DEFS_H
};

typedef REGISTER_ADDR_TYPE	addr_t;

#define htobe(_type, _val)			\
	_Generic(_type,				\
		 uint8_t: (_val),		\
		 uint16_t: htobe16(_val),	\
		 uint32_t: htobe32(_val))

struct decode_info {
	int		fd;
	unsigned int	i2c_addr;
	unsigned long	last_addr;
	unsigned int	num_shown;
};


static int read_i2c_buf(struct decode_info const *info,
			int addr, void *dst, size_t cnt)
{
	int		fd = info->fd;
	unsigned int	i2c_addr = info->i2c_addr;

	while (cnt > 0) {
		size_t		l = cnt > 0x800 ? 0x800 : cnt;
		int		rc;
		addr_t		addr_be = htobe(addr_be, addr);

		struct i2c_msg	msg[] = {
			[0] = {
				.addr	= i2c_addr,
				.flags	= 0,
				.len	= sizeof addr_be,
				.buf	= (void *)&addr_be,
			},
			[1] = {
				.addr	= i2c_addr,
				.flags	= I2C_M_RD,
				.len	= l,
				.buf	= dst
			},
		};

		struct i2c_rdwr_ioctl_data	i2c_data = {
			.msgs	= msg,
			.nmsgs	= 2,
		};

		rc = ioctl(fd, I2C_RDWR, &i2c_data);
		if (rc < 0) {
			perror("ioctl(<I2C_RDWR>)");
			return rc;
		}

		cnt  -= l;
		addr += l;
		dst  += l;
	}

	return 0;
}

static int _i2c_read(uintptr_t addr, unsigned int width, uintmax_t *val,
		     struct decode_info	*priv)
{
	union {
		uint8_t		u8;
		uint16_t	u16;
		uint32_t	u32;
		uint64_t	u64;
	}			tmp;
	int			rc;;

	switch (width) {
	case 8:
		rc = read_i2c_buf(priv, addr, &tmp.u8, 1);
		*val = tmp.u8;
		break;
	case 16:
		rc = read_i2c_buf(priv, addr, &tmp.u16, 2);
		*val = be16toh(tmp.u16);
		break;
	case 32:
		rc = read_i2c_buf(priv, addr, &tmp.u32, 4);
		*val = be32toh(tmp.u32);
		break;
	case 64:
		rc = read_i2c_buf(priv, addr, &tmp.u64, 8);
		*val = be64toh(tmp.u64);
		break;
	default:
		abort();
	}

	return rc;
}

static int _decode_reg(struct cpu_register const *reg, void *priv_)
{
	struct decode_info	*priv = priv_;
	unsigned long		addr = reg->offset + reg->unit->start;
	uintmax_t		val;
	int			rc;

	++priv->num_shown;

	rc = _i2c_read(addr, reg->width, &val, priv);
	if (rc < 0)
		return rc;

	if (reg->offset > priv->last_addr + 10)
		col_printf("\n");

	col_printf("&@0x%04lx&# &N%-28" STR_FMT "&#\t&~0x%0*llx&#",
		   (unsigned long)reg->offset, STR_ARG(&reg->name),
		   (unsigned int)(reg->width / 4),
		   (unsigned long long)val);

	deserialize_decode_reg(reg, val, priv);

	col_printf("\n");

	return 0;
}

int main(int argc, char *argv[])
{
	void const	*stream = DATA;
	size_t		stream_len = sizeof DATA;
	struct cpu_unit	*units = NULL;
	size_t		num_units = 0;

	char const	*i2c_dev = argv[1];
	int		fd;
	int		rc;
	bool		do_decode_single = false;

	uintptr_t	addr_start = 0;
	uintptr_t	addr_end = 0x3fff;

	struct decode_info	info = {
		.i2c_addr	= strtoul(argv[2], NULL, 0),
		.num_shown	= 0,
	};

	if (argc >= 4) {
		char const	*addr = argv[3];
		if (addr[0] == '?') {
			do_decode_single = true;
			++addr;
		}
		addr_start = strtoul(addr, NULL, 0);
	}

	if (argc >= 5)
		addr_end = strtoul(argv[4], NULL, 0);

	if (!deserialize_cpu_units(&units, &num_units, &stream, &stream_len)) {
		fprintf(stderr, "failed to decode stream\n");
		return EX_SOFTWARE;
	}

	fd = open(i2c_dev, O_RDWR | O_CLOEXEC);
	if (fd < 0) {
		perror("open(<i2c>)");
		return EX_NOINPUT;
	}

	info.fd = fd;

	if (do_decode_single)
		rc =deserialize_decode(units, num_units, addr_start,
				       addr_end, &info) ? 0 : -1;
	else
		rc = deserialize_decode_range(units, num_units,
					      addr_start, addr_end + 1,
					      _decode_reg, &info);

	close(fd);

	if (rc < 0)
		return EX_OSERR;

	return info.num_shown > 0 ? EX_OK : EX_NOINPUT;
}
