#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>

#include "deserialize.h"

#include "common.h"

#include DESERIALIZE_SYMBOLS

#define COL_ADDR	"\033[31m"	/* red */
#define COL_REGNAME	"\033[1m"	/* bold */
#define COL_RAWVAL	"\033[34m"	/* blue */
#define COL_ACCESS	"\033[1m"	/* bold */
#define COL_OFF		"\033[0;39m"

#define COL_TRUE	"\033[94m"
#define COL_FALSE	"\033[91m"

void *deserialize_alloc(size_t len)
{
	return malloc(len);
}

static void dump_field_start(struct cpu_regfield const *fld)
{
	col_printf("\n  %-36" STR_FMT ":\t", STR_ARG(&fld->name));
}

static void dump_field_end(struct cpu_regfield const *fld)
{
	switch (fld->flags & FIELD_FLAG_ACCESS_msk) {
	case FIELD_FLAG_ACCESS_READ:
		col_printf(" (&Aro&#)");
		break;
	case FIELD_FLAG_ACCESS_WRITE:
		col_printf(" (&Awo&#)");
		break;
	}
}

void deserialize_dump_frac(struct cpu_regfield_frac const *fld,
			   regmax_t int_part, regmax_t frac_part,
			   void *priv_)
{
	double			v;

	v  = frac_part;
	v /= 1 << __builtin_popcount(frac_part);
	v += int_part;

	dump_field_start(&fld->reg);
	col_printf("%f", v);
	dump_field_end(&fld->reg);
}

void deserialize_dump_bool(struct cpu_regfield_bool const *fld,
			   bool v, void *priv_)
{
	dump_field_start(&fld->reg);
	col_printf("%s", col_boolstr(v));
	dump_field_end(&fld->reg);
}

void deserialize_dump_enum(struct cpu_regfield_enum const *fld,
			   struct cpu_regfield_enum_val const *val,
			   size_t idx, void *priv_)
{
	dump_field_start(&fld->reg);
	if (val)
		col_printf("%" STR_FMT, STR_ARG(&val->name));
	else
		col_printf("#%zu", idx);
	dump_field_end(&fld->reg);
}

void deserialize_dump_sint(struct cpu_regfield_int const *fld,
			   signed long v, void *priv_)
{
	int			w;

	w = deserialize_popcount(&fld->bitmask, fld->reg.reg->width);
	w = (w + 3) / 4;

	dump_field_start(&fld->reg);
	switch (fld->reg.flags & FIELD_FLAG_DISPLAY_msk) {
	case FIELD_FLAG_DISPLAY_HEX:
		col_printf("0x%.*lx", w, v);
		break;
	case FIELD_FLAG_DISPLAY_DEC:
	default:
		col_printf("%ld", v);
		break;
	}
	dump_field_end(&fld->reg);
}

void deserialize_dump_uint(struct cpu_regfield_int const *fld,
			   regmax_t v_, void *priv_)
{
	unsigned long long	v = v_;
	int			w;

	w = deserialize_popcount(&fld->bitmask, fld->reg.reg->width);
	w = (w + 3) / 4;

	dump_field_start(&fld->reg);
	switch (fld->reg.flags & FIELD_FLAG_DISPLAY_msk) {
	case FIELD_FLAG_DISPLAY_HEX:
		col_printf("0x%.*llx", w, v);
		break;
	case FIELD_FLAG_DISPLAY_DEC:
	default:
		col_printf("%llu", v);
		break;
	}
	dump_field_end(&fld->reg);
}

void deserialize_dump_reserved(struct cpu_regfield_reserved const *fld,
			       reg_t const *v, void *priv)
{
	(void)fld;
	(void)v;
	(void)priv;
}

struct strbuf {
	size_t		len;
	size_t		alloc;
	char		*d;
};

static void strbuf_extend(struct strbuf *strbuf, void const *d, size_t l)
{
	if (strbuf->len + l > strbuf->alloc) {
		void	*tmp;
		size_t	new_alloc = strbuf->len + l + 64;

		tmp = realloc(strbuf->d, new_alloc);
		if (!tmp)
			abort();

		strbuf->d = tmp;
		strbuf->alloc = new_alloc;
	}

	memcpy(&strbuf->d[strbuf->len], d, l);

	strbuf->len += l;
}

static void strbuf_addstr(struct strbuf *strbuf, char const *s)
{
	strbuf_extend(strbuf, s, strlen(s));

}

static void strbuf_append(struct strbuf *strbuf, char c)
{
	strbuf_extend(strbuf, &c, 1);
}

static int		g_col_output_enabled = -1;

void col_init(int v)
{
	if (g_col_output_enabled == -1 || v != -1) {
		if (v == -1)
			v = isatty(1) ? 1 : 0;

		g_col_output_enabled = v;
	}
}


char const *col_boolstr(bool v)
{
	col_init(-1);

	if (g_col_output_enabled)
		return v ? (COL_TRUE "true" COL_OFF) : (COL_FALSE "false" COL_OFF);
	else
		return v ? "true" : "false";
}

void col_printf(char const *fmt, ...)
{
	struct strbuf	fmt_buf = { .len = 0 };
	va_list		ap;
	bool		is_colcode = false;

	col_init(-1);

	while (*fmt) {
		char	c = *fmt++;

		if (is_colcode) {
			switch (c) {
			case '&':
				strbuf_append(&fmt_buf, c);
				break;

			case '#':
				if (g_col_output_enabled)
					strbuf_addstr(&fmt_buf, COL_OFF);
				break;

			case '@':
				if (g_col_output_enabled)
					strbuf_addstr(&fmt_buf, COL_ADDR);
				break;

			case 'N':
				if (g_col_output_enabled)
					strbuf_addstr(&fmt_buf, COL_REGNAME);
				break;

			case 'A':
				if (g_col_output_enabled)
					strbuf_addstr(&fmt_buf, COL_ACCESS);
				break;

			case '~':
				if (g_col_output_enabled)
					strbuf_addstr(&fmt_buf, COL_RAWVAL);
				break;

			default:
				fprintf(stderr, "INTERNAL ERROR: bad col code '%c'\n", c);
				break;
			}

			is_colcode = false;
		} else {
			switch (c) {
			case '&':
				is_colcode = true;
				break;

			default:
				strbuf_append(&fmt_buf, c);
				break;
			}
		}
	}

	if (is_colcode)
		fprintf(stderr, "INTERNAL ERROR: lone '&'\n");

	strbuf_append(&fmt_buf, '\0');

	va_start(ap, fmt);
	vprintf(fmt_buf.d, ap);
	va_end(ap);

	free(fmt_buf.d);
}

char *string_to_c(struct string const *str)
{
	char		*res = malloc(str->len + 1);

	if (!res)
		return res;

	memcpy(res, str->data, str->len);
	res[str->len] = '\0';

	return res;
}

static size_t print_reg_t_col(void *dst, size_t len, reg_t const *reg,
				   struct cpu_register const *creg)
{
	unsigned int	w = creg->width;
	size_t		l;

	if (w <= 8) {
		l = snprintf(dst, len, "0x%02" PRIx8, reg->u8);
	} else if (w <= 16) {
		l = snprintf(dst, len, "0x%04" PRIx16, reg->u16);
	} else if (w <= 32) {
		l = snprintf(dst, len, "0x%04x'%04x",
			     reg->u32 >> 16, reg->u32 & 0xffffu);
	} else if (w <= 64) {
		l = snprintf(dst, len, "0x%08" PRIx64 "'%08" PRIx64,
			     reg->u64 >> 32, reg->u64 & 0xfffffffflu);
	} else {
		l = snprintf(dst, len, "0x");

		BUG_ON((w % 8) != 0);

		for (unsigned i = 0; i < w; i += 8) {
			size_t	idx = i / 8;

			if (__BYTE_ORDER != __BIG_ENDIAN)
				idx = w / 8 - idx - 1;

			if (l + 1 >= len) {
				l = len;
				break;
			}

			l += snprintf(dst + l, len - l, "%02x", reg->raw[idx]);
			if (i + 8 < w && ((i + 8) % 32) == 0) {
				strcpy(dst + l, "'");
				l += 1;
			}
		}
	}

	return l;
}

static size_t print_reg_t_simple(void *dst, size_t len, reg_t const *reg,
				 struct cpu_register const *creg)
{
	unsigned int	w = creg->width;
	size_t		l;

	if (w <= 8) {
		l = snprintf(dst, len, "0x%02" PRIx8, reg->u8);
	} else if (w <= 16) {
		l = snprintf(dst, len, "0x%04" PRIx16, reg->u16);
	} else if (w <= 32) {
		l = snprintf(dst, len, "0x%08" PRIx32, reg->u32);
	} else if (w <= 64) {
		l = snprintf(dst, len, "0x%16" PRIx64, reg->u64);
	} else {
		l = snprintf(dst, len, "0x");

		BUG_ON((w % 8) != 0);

		for (unsigned i = 0; i < w; i += 8) {
			size_t	idx = i / 8;

			if (__BYTE_ORDER != __BIG_ENDIAN)
				idx = w / 8 - idx - 1;

			if (l + 1 >= len) {
				l = len;
				break;
			}

			l += snprintf(dst + l, len - l, "%02x", reg->raw[idx]);
		}
	}

	return l;
}

char const *deserialze_print_reg_t(void *dst, size_t len, reg_t const *reg,
				   struct cpu_register const *creg)
{
	size_t		l;

	col_init(-1);

	if (g_col_output_enabled)
		l = print_reg_t_col(dst, len, reg, creg);
	else
		l = print_reg_t_simple(dst, len, reg, creg);

	BUG_ON(l >= len);

	return dst;
}

bool reg_is_zero(reg_t const *reg, size_t width)
{
	for (size_t i = 0; i < width; i += 8) {
		/* TODO: handle 'width % 8 != 0' case! */
		if (reg->raw[i / 8] != 0)
			return false;
	}

	return true;
}
