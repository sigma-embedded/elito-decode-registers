#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

#include <deserialize.h>

#include "common.h"

#define COL_ADDR	"\033[31m"	/* red */
#define COL_REGNAME	"\033[1m"	/* bold */
#define COL_RAWVAL	"\033[34m"	/* blue */
#define COL_OFF		"\033[0;39m"

#define COL_TRUE	"\033[94m"
#define COL_FALSE	"\033[91m"

void *deserialize_alloc(size_t len)
{
	return malloc(len);
}

void deserialize_dump_frac(struct cpu_regfield_frac const *fld,
			   reg_t int_part, reg_t frac_part,
			   void *priv_)
{
	double			v;

	v  = frac_part;
	v /= 1 << __builtin_popcount(frac_part);
	v += int_part;

	col_printf("\n  %-28" STR_FMT ":\t%f", STR_ARG(&fld->reg.name), v);
}

void deserialize_dump_bool(struct cpu_regfield_bool const *fld,
			   bool v, void *priv_)
{
	col_printf("\n  %-28" STR_FMT ":\t%s",
	       STR_ARG(&fld->reg.name), col_boolstr(v));
}

void deserialize_dump_enum(struct cpu_regfield_enum const *fld,
			   struct cpu_regfield_enum_val const *val,
			   size_t idx, void *priv_)
{
	if (val)
		col_printf("\n  %-28" STR_FMT ":\t%" STR_FMT,
			   STR_ARG(&fld->reg.name),
			   STR_ARG(&val->name));
	else
		col_printf("\n  %-28" STR_FMT ":\t#%zu",
			   STR_ARG(&fld->reg.name),
			   idx);
}

void deserialize_dump_sint(struct cpu_regfield_int const *fld,
			   signed int v, void *priv_)
{
	col_printf("\n  %-28" STR_FMT ":\t%d", STR_ARG(&fld->reg.name), v);
}

void deserialize_dump_uint(struct cpu_regfield_int const *fld,
			   unsigned int v, void *priv_)
{
	col_printf("\n  %-28" STR_FMT ":\t%u", STR_ARG(&fld->reg.name), v);
}

void deserialize_dump_reserved(struct cpu_regfield_reserved const *fld,
			       reg_t v, void *priv)
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
	if (g_col_output_enabled == -1) {
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
