/* SPDX-License-Identifier: GPL-3.0-only */
#define _GNU_SOURCE

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sysexits.h>
#include <getopt.h>
#include <fnmatch.h>

#include <sys/fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#include "deserialize.h"
#include "all-symbols.h"

#include "common.h"

#define CMD_HELP                0x1000
#define CMD_VERSION             0x1001

static char const			CMDLINE_SHORT[] = \
	"T:D:A:v:d:";

static struct option const		CMDLINE_OPTIONS[] = {
	{ "help",         no_argument,       0, CMD_HELP },
	{ "version",      no_argument,       0, CMD_VERSION },
	{ "type",         required_argument, 0, 'T' },
	{ "bus-device",   required_argument, 0, 'D' },
	{ "bus-addr",     required_argument, 0, 'A' },
	{ "value",        required_argument, 0, 'v' },
	{ "definitions",  required_argument, 0, 'd' },
	{ NULL, 0, 0, 0 }
};

static void show_help(void)
{
	printf(""
	       "Usage: decode [--help] [--version]\n"
	       "    --type|-T <type> --definitions|-d <file>\n"
	       "    [--bus-device|-D <device>] [--bus-addr|-A <addr>]\n"
	       "    [--value|-v <value>]\n"
	       "\n"
	       "Required options:\n"
	       "    - I2C: --bus-device (e.g. '/dev/i2c-2'), --bus-addr,\n"
	       "    - MEM: --bus-device (e..g '/dev/mem')\n"
	       "    - EMU: --value\n"
	       "\n");

	exit(0);
}

static void show_version(void)
{
	printf("decode\n");
	exit(0);
}

enum endianess {
	ENDIAN_LITLLE,
	ENDIAN_BIG,
};

enum device_type {
	DEVTYPE_NONE,
	DEVTYPE_EMU,
	DEVTYPE_I2C,
	DEVTYPE_MEM,
};

struct device;
struct device_ops {
	void			(*deinit)(struct device *);
	int			(*read)(struct device *, uintptr_t addr,
					unsigned int width, reg_t *val);
	void			(*select_unit)(struct device *,
					       struct cpu_unit const *);
};

struct device_emu {
	uintmax_t		value;
};

struct device_i2c {
	int			fd;
	unsigned int		i2c_addr;
	enum endianess		endianess;
	unsigned int		addr_width;
};

struct device_mem {
	int			fd;
	void const		*mem;
	uintptr_t		page;
	size_t			page_sz;
	bool			is_mapped;
};

struct device {
	enum device_type		type;

	union {
		struct device_emu	emu;
		struct device_i2c	i2c;
		struct device_mem	mem;
	};

	struct device_ops  const	*ops;
};

struct ctx {
	struct device			dev;

	struct cpu_unit const		*last_unit;

	unsigned int			num_shown;

	char const			*unit_glob;
};

union uinttype {
	uint8_t		u8;
	uint16_t	u16;
	uint32_t	u32;
	uint64_t	u64;
};

struct definitions {
	void const	*mem;
	size_t		len;

	struct cpu_unit	*units;
	size_t		num_units;
};

static void htole(union uinttype *dst, uintmax_t src, unsigned int width)
{
	switch (width) {
	case 8:
		dst->u8 = src;
		break;
	case 16:
		dst->u16 = htole16(src);
		break;
	case 32:
		dst->u32 = htole32(src);
		break;
	case 64:
		dst->u64 = htole64(src);
		break;
	default:
		abort();
	}
}

static void letoh(uintmax_t *dst, union uinttype const *src, unsigned int width)
{
	switch (width) {
	case 8:
		*dst = src->u8;
		break;
	case 16:
		*dst = le16toh(src->u16);
		break;
	case 32:
		*dst = le16toh(src->u32);
		break;
	case 64:
		*dst = le16toh(src->u64);
		break;
	default:
		abort();
	}
}

static void htobe(union uinttype *dst, uintmax_t src, unsigned int width)
{
	switch (width) {
	case 8:
		dst->u8 = src;
		break;
	case 16:
		dst->u16 = htobe16(src);
		break;
	case 32:
		dst->u32 = htobe32(src);
		break;
	case 64:
		dst->u64 = htobe64(src);
		break;
	default:
		abort();
	}
}


static void betoh(uintmax_t *dst, union uinttype const *src, unsigned int width)
{
	switch (width) {
	case 8:
		*dst = src->u8;
		break;
	case 16:
		*dst = be16toh(src->u16);
		break;
	case 32:
		*dst = be16toh(src->u32);
		break;
	case 64:
		*dst = be16toh(src->u64);
		break;
	default:
		abort();
	}
}

static void device_i2c_deinit(struct device *dev)
{
	close(dev->i2c.fd);
}

static int device_i2c_read(struct device *dev, uintptr_t addr,
			   unsigned int width, reg_t *val)
{
	struct device_i2c	*i2c = &dev->i2c;
	union uinttype		e_addr;
	union uinttype		tmp;
	int			rc;

	struct i2c_msg			msg[] = {
		[0] = {
			.addr	= i2c->i2c_addr,
			.flags	= 0,
			.len	= i2c->addr_width / 8,
			.buf	= (void *)&e_addr,
		},
		[1] = {
			.addr	= i2c->i2c_addr,
			.flags	= I2C_M_RD,
			.len	= width / 8,
			.buf	= (void *)&tmp,
		},
	};

	struct i2c_rdwr_ioctl_data	i2c_data = {
		.msgs	= msg,
		.nmsgs	= 2,
	};

	switch (i2c->endianess) {
	case ENDIAN_LITLLE:
		htole(&e_addr, addr, i2c->addr_width);
		break;
	case ENDIAN_BIG:
		htobe(&e_addr, addr, i2c->addr_width);
		break;
	default:
		abort();
	}

	rc = ioctl(i2c->fd, I2C_RDWR, &i2c_data);
	if (rc < 0) {
		fprintf(stderr, "ioctl(<I2C_RDWR>): %m\n");
		return rc;
	}

	switch (i2c->endianess) {
	case ENDIAN_LITLLE:
		letoh(val, &tmp, width);
		break;
	case ENDIAN_BIG:
		betoh(val, &tmp, width);
		break;
	default:
		abort();
	}

	return 0;
}

static void device_i2c_select_unit(struct device *dev,
				   struct cpu_unit const *unit)
{
	unsigned int	addr_width;
	enum endianess	endian;

	switch (unit->addr_width) {
	case 0:
		/* default value */
		addr_width = 8;
		break;
	case 8:
	case 16:
	case 32:
		addr_width = unit->addr_width;
		break;
	default:
		fprintf(stderr,
			"invalid address width %d in unit %" STR_FMT "\n",
			unit->addr_width, STR_ARG(&unit->name));
		abort();
	}

	switch (unit->endian) {
	case UNIT_ENDIAN_BIG:
		endian = ENDIAN_BIG;
		break;
	case UNIT_ENDIAN_LITTLE:
		endian = ENDIAN_LITLLE;
		break;
	case UNIT_ENDIAN_NATIVE:
		/* endianess on i2c is usually big... */
		endian = ENDIAN_BIG;
		break;
	default:
		fprintf(stderr,
			"unsupported endinaess %d in unit %" STR_FMT "\n",
			unit->endian, STR_ARG(&unit->name));
		abort();
	}

	if (dev) {
		dev->i2c.addr_width = addr_width;
		dev->i2c.endianess  = endian;
	}
}

static struct device_ops const	device_ops_i2c = {
	.deinit		= device_i2c_deinit,
	.read		= device_i2c_read,
	.select_unit	= device_i2c_select_unit,
};

static int device_i2c_init(struct device *dev, char const *bus_device,
			   unsigned int addr)
{
	int			fd;

	if (!bus_device) {
		fprintf(stderr, "missing --bus-device\n");
		return EX_USAGE;
	}

	fd = open(bus_device, O_RDWR | O_CLOEXEC);
	if (fd < 0) {
		fprintf(stderr, "open(%s): %m\n", bus_device);
		return EX_OSERR;
	}

	*dev = (struct device) {
		.type		= DEVTYPE_I2C,
		.i2c		= {
			.fd		= fd,
			.i2c_addr	= addr,
			.addr_width	= 8,
			.endianess	= ENDIAN_BIG,
		},
		.ops		= &device_ops_i2c,
	};

	return EX_OK;
}

/* EMU */
static void device_emu_deinit(struct device *dev)
{
}

static int device_emu_read(struct device *dev, uintptr_t addr,
			   unsigned int width, reg_t *val)
{
	val->max = dev->emu.value;

	return 0;
}

static struct device_ops const	device_ops_emu = {
	.deinit		= device_emu_deinit,
	.read		= device_emu_read,
};

static int device_emu_init(struct device *dev, uintmax_t value)
{
	*dev = (struct device) {
		.type		= DEVTYPE_EMU,
		.emu		= {
			.value		= value,
		},
		.ops		= &device_ops_emu,
	};

	return EX_OK;
}

/* MEM */
static void device_mem_deinit(struct device *dev)
{
	if (dev->mem.is_mapped)
		munmap((void *)dev->mem.mem, dev->mem.page_sz);

	close(dev->mem.fd);
}

static int device_mem_read(struct device *dev, uintptr_t addr,
			   unsigned int width, reg_t *val)
{
	struct device_mem	*mdev = &dev->mem;
	uintptr_t		page = addr & ~(mdev->page_sz - 1);
	void const		*mem;

	if (!mdev->is_mapped || mdev->page != page) {
		if (mdev->is_mapped) {
			munmap((void *)mdev->mem, mdev->page_sz);
			mdev->mem = NULL;
			mdev->is_mapped = false;
		}

		mem = mmap(NULL, mdev->page_sz, PROT_READ, MAP_SHARED,
			   mdev->fd, page);
		if (mem == MAP_FAILED) {
			perror("mmap()");
			return -1;
		}

		mdev->mem = mem;
		mdev->page = page;
		mdev->is_mapped = true;
	}

	mem = mdev->mem + (addr & (mdev->page_sz - 1));

	switch (width) {
	case 8:
		memcpy(&val->u8, mem, sizeof val->u8);
		break;
	case 16:
		memcpy(&val->u16, mem, sizeof val->u16);
		break;
	case 32:
		memcpy(&val->u32, mem, sizeof val->u32);
		break;
	case 64:
		memcpy(&val->u64, mem, sizeof val->u64);
		break;
	default:
		/* ensure that we stay on the same page */
		BUG_ON(((addr + (width + 7) / 8) | (mdev->page_sz - 1)) !=
		       (addr  | (mdev->page_sz - 1)));
		memcpy(&val->u64, mem, sizeof val->u64);
		break;
	}

	return 0;
}

static struct device_ops const	device_ops_mem = {
	.deinit		= device_mem_deinit,
	.read		= device_mem_read,
};

static int device_mem_init(struct device *dev, char const *bus_device)
{
	int		fd;

	if (bus_device == NULL)
		bus_device = "/dev/mem";

	fd = open(bus_device, O_RDWR | O_CLOEXEC);
	if (fd < 0) {
		fprintf(stderr, "failed to open '%s': %m\n", bus_device);
		return EX_OSERR;
	}

	*dev = (struct device) {
		.mem		= {
			.fd		= fd,
			.page_sz	= sysconf(_SC_PAGESIZE),
		},
		.ops		= &device_ops_mem,
	};

	return 0;
}

static bool parse_devtype(enum device_type *type, char const *str)
{
	if (strcmp(str, "emu") == 0) {
		*type = DEVTYPE_EMU;
	} else if (strcmp(str, "i2c") == 0) {
		*type = DEVTYPE_I2C;
	} else if (strcmp(str, "mem") == 0) {
		*type = DEVTYPE_MEM;
	} else {
		fprintf(stderr, "unsupported device type '%s'\n", str);
		return false;
	}

	return true;
}

static bool parse_uint(uintmax_t *v, char const *str)
{
	char			*err;
	unsigned long long	tmp;

	errno = 0;
	tmp = strtoull(str, &err, 0);
	if (err == str || *err || errno != 0) {
		fprintf(stderr, "invalid numeric value '%s'\n", str);
		return false;
	}

	*v = tmp;
	return true;
}

static void definitions_release(struct definitions *def)
{
	free((void *)def->mem);
	/* TODO: free def->units */
}

static int definitions_read(struct definitions *def, char const *fname)
{
	static size_t const	BLK_SZ = 128 * 1024;
	FILE			*f;
	int			rc;
	void			*mem = NULL;
	size_t			len = 0;
	size_t			alloc = 0;
	size_t			num_units;
	struct cpu_unit		*units;

	f = fopen(fname, "re");
	if (!f) {
		fprintf(stderr, "can not read definitions from file '%s': %m",
			fname);
		return EX_NOINPUT;
	}

	rc = EX_OK;
	while (!feof(f)) {
		size_t		l;

		if (len + BLK_SZ > alloc) {
			mem = realloc(mem, len + BLK_SZ);
			if (!mem)
				abort();

			alloc = len + BLK_SZ;
		}

		l = fread(mem + len, 1, BLK_SZ, f);

		if (ferror(f)) {
			fprintf(stderr, "failed to read file '%s': %m", fname);
			rc = EX_OSERR;
			break;
		}

		len += l;
	}

	if (rc != EX_OK)
		goto out;

	mem = realloc(mem, len);
	if (!mem)
		abort();

	{
		void const	*stream = mem;
		size_t		stream_len = len;

		if (!deserialize_cpu_units(&units, &num_units,
					   &stream, &stream_len)) {
			fprintf(stderr, "failed to decode stream\n");
			rc = EX_DATAERR;
			goto out;
		}

		if (stream_len > 0) {
			fprintf(stderr, "excess data (%zu) in '%s'\n",
				stream_len, fname);
		}
	}

	if (rc == EX_OK) {
		*def = (struct definitions) {
			.mem		= mem,
			.len		= len,
			.units		= units,
			.num_units	= num_units,
		};

		mem = NULL;
	}

out:
	free(mem);
	fclose(f);

	return rc;
}

static int _decode_reg(struct cpu_register const *reg, void *ctx_)
{
	struct ctx		*ctx = ctx_;
	unsigned long		addr = reg->offset + reg->unit->start;
	reg_t			val;
	int			rc;

	if (reg->unit != ctx->last_unit) {
		col_printf("%s======================== %" STR_FMT " ==============================",
			   ctx->num_shown > 0 ? "\n" : "",
			   STR_ARG(&reg->unit->name));

		if (ctx->dev.ops->select_unit)
			ctx->dev.ops->select_unit(&ctx->dev, reg->unit);

		ctx->last_unit = reg->unit;
	}

	rc = ctx->dev.ops->read(&ctx->dev, addr, reg->width, &val);
	if (rc < 0)
		return rc;

	/* TODO: fix  printing of large bitfields */
	col_printf("\n&@0x%08lx&# &N%-28" STR_FMT "&#\t&~0x%0*llx&#",
		   addr, STR_ARG(&reg->name),
		   (unsigned int)(reg->width / 4),
		   (unsigned long long)val.max);

	deserialize_decode_reg(reg, &val, ctx);

	col_printf("\n");

	++ctx->num_shown;

	return 0;
}

static bool unit_match(struct cpu_unit const *unit, struct ctx *ctx)
{
	char const		*name = string_to_c(&unit->name);
	bool			rc;

	if (!name)
		abort();

	rc = fnmatch(ctx->unit_glob, name, FNM_CASEFOLD) == 0;
	free((void *)name);

	return rc;
}

int main(int argc, char *argv[])
{
	enum device_type	dev_type = DEVTYPE_NONE;;
	uintmax_t		bus_addr = 0;;
	char const		*bus_device = NULL;
	uintmax_t		value = 0;
	char const		*definitions_file = NULL;
	struct definitions	definitions = { .mem = NULL };
	int			rc;
	struct ctx		ctx = {};
	uintptr_t		addr_start = 0;
	uintptr_t		addr_end   = ~(uintptr_t)0;

	while (1) {
		int         c = getopt_long(argc, argv,
					    CMDLINE_SHORT,
					    CMDLINE_OPTIONS, NULL);

		if (c==-1)
			break;

		switch (c) {
		case CMD_HELP:
			show_help();
			break;

		case CMD_VERSION:
			show_version();
			break;

		case 'T':		/* --type */
			if (!parse_devtype(&dev_type, optarg))
				return EX_USAGE;

			break;

		case 'A':		/* --bus-addr */
			if (!parse_uint(&bus_addr, optarg)) {
				fprintf(stderr, "invalid --bus-addr\n");
				return EX_USAGE;
			}
			break;

		case 'D':
			bus_device = optarg;
			break;

		case 'v':
			if (!parse_uint(&value, optarg)) {
				fprintf(stderr, "invalid --value\n");
				return EX_USAGE;
			}
			break;

		case 'd':
			definitions_file = optarg;
			break;

		default:
			fprintf(stderr, "Try --help for more information\n");
			return EX_USAGE;
		}
	}

	if (dev_type == DEVTYPE_NONE) {
		fprintf(stderr, "missing --type\n");
		return EX_USAGE;
	}

	if (!definitions_file) {
		fprintf(stderr, "missing --definitions\n");
		return EX_USAGE;
	}

	if (optind < argc) {
		char const	*addr = argv[optind];
		uintmax_t	tmp;

		if (addr[0] == '@') {
			ctx.unit_glob = addr + 1;
		} else if (!parse_uint(&tmp, addr)) {
			fprintf(stderr, "invalid start address '%s'\n", addr);
			return EX_USAGE;
		} else {
			addr_start = tmp;
			addr_end   = tmp;
		}
	}

	if (optind + 1 < argc) {
		char const	*addr = argv[optind + 1];
		uintmax_t	tmp;

		if (ctx.unit_glob) {
			fprintf(stderr, "end address with unit-glob is unsupported\n");
			return EX_USAGE;
		}

		if (!parse_uint(&tmp, addr)) {
			fprintf(stderr, "invalid end address '%s'\n", addr);
			return EX_USAGE;
		}

		addr_end = tmp;
	}

	rc = definitions_read(&definitions, definitions_file);
	if (rc != EX_OK)
		goto out;

	switch (dev_type) {
	case DEVTYPE_I2C:
		rc = device_i2c_init(&ctx.dev, bus_device, bus_addr);
		break;

	case DEVTYPE_EMU:
		rc = device_emu_init(&ctx.dev, value);
		break;

	case DEVTYPE_MEM:
		rc = device_mem_init(&ctx.dev, bus_device);
		break;

	default:
		abort();
	}

	if (rc != EX_OK)
		goto out;

	if (!ctx.unit_glob) {
		rc = deserialize_decode_range(definitions.units,
					      definitions.num_units,
					      addr_start, addr_end,
					      _decode_reg, &ctx);
	} else {
		for (size_t i = 0; i < definitions.num_units; ++i) {
			struct cpu_unit const	*unit = &definitions.units[i];

			if (!unit_match(unit, &ctx))
				continue;

			rc = deserialize_decode_range(unit, 1,
						      addr_start, addr_end,
						      _decode_reg, &ctx);

			if (rc < 0)
				break;
		}
	}

	if (rc < 0) {
		rc = EX_OSERR;
		goto out;
	}

out:
	for (size_t i = definitions.num_units; i > 0; --i)
		deserialize_cpu_unit_release(&definitions.units[i - 1]);

	free(definitions.units);

	if (ctx.dev.ops)
		ctx.dev.ops->deinit(&ctx.dev);

	definitions_release(&definitions);
	return rc;
}
