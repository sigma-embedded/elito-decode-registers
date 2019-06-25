/*	--*- c -*--
 * Copyright (C) 2015 Enrico Scholz <enrico.scholz@ensc.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <stddef.h>
#include <inttypes.h>
#include <assert.h>

#include "deserialize.h"

static uint8_t const	STREAM[] = {
	#include "test-deserialize_stream.h"
};

#define STR_FMT		"%.*s"
#define STR_ARG(_s)	(int)((_s)->len), (_s)->data


static size_t g_buf_len = 1024;

struct {
	void		*base;
	size_t		len;
	void		*ptr;
}			heap;

struct dump_data {
	bool		do_introspect;
	bool		is_terse;
	char		*buf;
	size_t		buf_len;

	struct cpu_register const	*reg;
	char				*ptr;
	char const			*delim;
};

static bool		heap_alloc = false;

void *deserialize_alloc(size_t len)
{
	if (heap_alloc) {
		void	*ptr = (void *)(((uintptr_t)(heap.ptr) + 7) / 8 * 8);

		if (heap.len - (ptr - heap.base) < len) {
			abort();
			return NULL;
		}

		heap.ptr = ptr + len;
		return ptr;
	} else {
		return malloc(len);
	}
}

static char *_deserialize_dump_bool(bool v, char *buf, size_t len)
{
	assert(len >= 6);

	return strcpy(buf, v ? "true" : "false");
}

static void _deserialize_dump_bool_terse(struct cpu_regfield_bool const *fld,
					 bool v, struct dump_data *priv)
{
	int			rc;

	priv->reg = fld->reg.reg;

	if (1) {
		rc = sprintf(priv->ptr, "%s%s" STR_FMT,
			     priv->delim,
			     v ? "" : "!",
			     STR_ARG(&fld->reg.name));

		priv->ptr += rc;
		priv->delim = ", ";
	}
}

static char *deserialize_introspect_bool(struct cpu_regfield_bool const *fld,
					 char *buf, size_t len)
{
	int	rc;

	rc = snprintf(buf, len, "\t\t\t\t@boolean %d\n", fld->bit);

	assert((size_t)rc < len);

	return buf;
}

void deserialize_dump_bool(struct cpu_regfield_bool const *fld,
			    bool v, void *priv_)
{
	struct dump_data	*priv = priv_;

	if (priv->do_introspect)
		deserialize_introspect_bool(fld, priv->buf, priv->buf_len);
	else if (!priv->is_terse)
		_deserialize_dump_bool(v, priv->buf, priv->buf_len);
	else
		_deserialize_dump_bool_terse(fld, v, priv);
}

static char *_deserialize_dump_uint(unsigned int v, char *buf, size_t len)
{
	int	rc;

	rc = snprintf(buf, len, "%u", v);
	assert((size_t)rc < len);
	return buf;
}

static void _deserialize_dump_uint_terse(struct cpu_regfield_int const *fld,
					 unsigned int v, struct dump_data *priv)
{
	int			rc;

	priv->reg = fld->reg.reg;

	if (1) {
		rc = sprintf(priv->ptr, "%s" STR_FMT ":%u",
			     priv->delim, STR_ARG(&fld->reg.name), v);

		priv->ptr += rc;
		priv->delim = ", ";
	}
}

static char *deserialize_introspect_int(struct cpu_regfield_int const *fld,
					char *buf, size_t len)
{
	char	sbuf[REGISTER_PRINT_SZ];
	int	rc;

	rc = snprintf(buf, len, "\t\t\t\t@uint %s\n",
		      print_reg_t(sbuf, &fld->bitmask, fld));

	assert((size_t)rc < len);

	return buf;
}

void deserialize_dump_uint(struct cpu_regfield_int const *fld,
			   regmax_t v, void *priv_)
{
	struct dump_data	*priv = priv_;

	if (priv->do_introspect)
		deserialize_introspect_int(fld, priv->buf, priv->buf_len);
	else if (!priv->is_terse)
		_deserialize_dump_uint(v, priv->buf, priv->buf_len);
	else
		_deserialize_dump_uint_terse(fld, v, priv);
}

/* signed int */
static char *_deserialize_dump_sint(signed int v, char *buf, size_t len)
{
	int	rc;

	rc = snprintf(buf, len, "%d", v);
	assert((size_t)rc < len);
	return buf;
}

static void _deserialize_dump_sint_terse(struct cpu_regfield_int const *fld,
					 signed int v, struct dump_data *priv)
{
	int			rc;

	priv->reg = fld->reg.reg;

	if (1) {
		rc = sprintf(priv->ptr, "%s" STR_FMT ":%d",
			     priv->delim, STR_ARG(&fld->reg.name), v);

		priv->ptr += rc;
		priv->delim = ", ";
	}
}

void deserialize_dump_sint(struct cpu_regfield_int const *fld,
			   signed long v, void *priv_)
{
	struct dump_data	*priv = priv_;

	if (priv->do_introspect)
		deserialize_introspect_int(fld, priv->buf, priv->buf_len);
	else if (!priv->is_terse)
		_deserialize_dump_sint(v, priv->buf, priv->buf_len);
	else
		_deserialize_dump_sint_terse(fld, v, priv);
}


static char *deserialize_introspect_frac(struct cpu_regfield_frac const *fld,
					 char *buf, size_t len)
{
	char	sbuf0[REGISTER_PRINT_SZ];
	char	sbuf1[REGISTER_PRINT_SZ];
	int	rc;

	/* TODO: fix reg_t printf */
	rc = snprintf(buf, len, "\t\t\t\t@frac %s %s\n",
		      print_reg_t(sbuf0, &fld->int_part, fld),
		      print_reg_t(sbuf1, &fld->frac_part, fld));

	assert((size_t)rc < len);

	return buf;
}

struct frac_info {
	regmax_t	int_part;
	regmax_t	frac_part;
	unsigned int	num_frac_bits;
	double		v;
};

static char *_deserialize_dump_frac(struct frac_info const *v,
				    char *buf, size_t len)
{
	int	rc;

	rc = snprintf(buf, len, "%f", v->v);

	assert((size_t)rc < len);

	return buf;
}

static void _deserialize_dump_frac_terse(struct cpu_regfield_frac const *fld,
					 struct frac_info const *v,
					 struct dump_data *priv)
{
	int			rc;

	priv->reg = fld->reg.reg;

	if (1) {
		rc = sprintf(priv->ptr, "%s" STR_FMT ":%f",
			     priv->delim, STR_ARG(&fld->reg.name), v->v);

		priv->ptr += rc;
		priv->delim = ", ";
	}
}

void deserialize_dump_frac(struct cpu_regfield_frac const *fld,
			   regmax_t int_part, regmax_t frac_part,
			   void *priv_)
{
	struct dump_data	*priv = priv_;
	struct frac_info	info = {
		.int_part	= int_part,
		.frac_part	= frac_part,
		.num_frac_bits	= deserialize_popcount(&fld->frac_part,
						       fld->reg.reg->width),
	};

	info.v  = info.frac_part;
	info.v /= 1 << info.num_frac_bits;
	info.v += info.int_part;

	if (priv->do_introspect)
		deserialize_introspect_frac(fld, priv->buf, priv->buf_len);
	else if (!priv->is_terse)
		_deserialize_dump_frac(&info, priv->buf, priv->buf_len);
	else
		_deserialize_dump_frac_terse(fld, &info, priv);
}

static char *deserialize_introspect_enum(struct cpu_regfield_enum const *fld,
					 char *buf, size_t len)
{
	char	sbuf[REGISTER_PRINT_SZ];
	char	*res = buf;
	int	rc;

	rc = snprintf(buf, len, "\t\t\t\t@bitmask %s\n",
		      print_reg_t(sbuf, &fld->bitmask, fld));

	assert((size_t)(rc) < len);

	buf += rc;
	len -= rc;

	for (size_t i = 0; i < fld->num_enums; ++i) {
		rc = snprintf(buf, len,
			      "\t\t\t\t@enum %u " STR_FMT "\n",
			      (unsigned int)(fld->enums[i].val),
			      STR_ARG(&(fld->enums[i].name)));

		assert((size_t)(rc) < len);

		buf += rc;
		len -= rc;
	}

	return res;
}

static char *_deserialize_dump_enum(struct cpu_regfield_enum_val const *val,
				    size_t idx, char *buf, size_t len)
{
	int			rc;
	assert(len >= 6);

	if (val)
		rc = snprintf(buf, len, "%*s (%zu)",
			      (int)(val->name.len), val->name.data, idx);
	else
		rc = snprintf(buf, len, "??? (%zu)", idx);

	assert((size_t)(rc) < len);

	return buf;
}

static void _deserialize_dump_enum_terse(struct cpu_regfield_enum const *fld,
					struct cpu_regfield_enum_val const *val,
					size_t idx, struct dump_data *priv)
{
	int			rc;

	priv->reg = fld->reg.reg;

	if (1) {
		if (val) {
			rc = sprintf(priv->ptr, "%s" STR_FMT ":" STR_FMT,
				     priv->delim,
				     STR_ARG(&fld->reg.name),
				     STR_ARG(&val->name));
		} else {
			rc = sprintf(priv->ptr, "%s" STR_FMT ":#%zu",
				     priv->delim,
				     STR_ARG(&fld->reg.name),
				     idx);
		}

		priv->ptr += rc;
		priv->delim = ", ";
	}
}


void deserialize_dump_enum(struct cpu_regfield_enum const *fld,
			   struct cpu_regfield_enum_val const *val,
			   size_t idx, void *priv_)
{
	struct dump_data	*priv = priv_;

	if (priv->do_introspect)
		deserialize_introspect_enum(fld, priv->buf, priv->buf_len);
	else if (!priv->is_terse)
		_deserialize_dump_enum(val, idx, priv->buf, priv->buf_len);
	else
		_deserialize_dump_enum_terse(fld, val, idx, priv);
}

static char *deserialize_introspect_reserved(struct cpu_regfield_reserved const *fld,
					     char *buf, size_t len)
{
	char	sbuf[REGISTER_PRINT_SZ];
	int	rc;

	rc = snprintf(buf, len, "\t\t\t\t@reserved %s\n",
		      print_reg_t(sbuf, &fld->bitmask, fld));

	assert((size_t)rc < len);

	return buf;
}

static char *_deserialize_dump_reserved(struct cpu_regfield_reserved const *fld,
					char *buf, size_t len)
{
	reg_t const	*bitmask = &fld->bitmask;
	char		sbuf[REGISTER_PRINT_SZ];
	int		rc;

	rc = snprintf(buf, len, "%s", print_reg_t(sbuf, bitmask, fld));
	assert((size_t)rc < len);
	return buf;
}

static void _deserialize_dump_reserved_terse(struct cpu_regfield_reserved const *fld,
					     reg_t const *bitmask,
					     struct dump_data *priv)
{
	char	sbuf[REGISTER_PRINT_SZ];
	int	rc;

	priv->reg = fld->reg.reg;

	if (1) {
		rc = sprintf(priv->ptr, "%s", print_reg_t(sbuf, bitmask, fld));

		priv->ptr += rc;
		priv->delim = ", ";
	}
}

void deserialize_dump_reserved(struct cpu_regfield_reserved const *fld,
			       reg_t const *v, void *priv_)
{
	struct dump_data	*priv = priv_;

	if (priv->do_introspect)
		deserialize_introspect_reserved(fld, priv->buf, priv->buf_len);
	else if (!priv->is_terse)
		_deserialize_dump_reserved(fld, priv->buf, priv->buf_len);
	else
		_deserialize_dump_reserved_terse(fld, &fld->bitmask, priv);
}

static void dump_field(struct cpu_regfield const *fld)
{
	char			buf[1024];
	struct dump_data	d = {
		.do_introspect	= true,
		.buf		= buf,
		.buf_len	= sizeof buf,
	};

	/* allocate it dynamically to detect more issues with valid */
	size_t			v_l = (fld->reg->width + 7) / 8;
	reg_t			*v = malloc(v_l);

	assert(v != NULL);

	memset(v, 0x00, v_l);

	printf("\t\t\t@field " STR_FMT "\n"
	       "\t\t\t\t@name " STR_FMT "\n",
	       STR_ARG(&fld->id), STR_ARG(&fld->name));

	fld->fn(fld, v, &d);
	printf("%s", buf);

	free(v);
}

static void dump_register(struct cpu_register const *reg)
{
	printf("\t@register " STR_FMT "\n"
	       "\t\t#id \"" STR_FMT "\"\n"
	       "\t\t@addr 0x%08" PRIxPTR " %d\n",

	       STR_ARG(&reg->name), STR_ARG(&reg->id),
	       reg->offset, reg->width);

	for (size_t i = 0; i < reg->num_fields; ++i)
		dump_field(reg->fields[i]);
}

static void dump_unit(struct cpu_unit const *unit)
{
	printf("@unit " STR_FMT "\n"
	       "\t@name \"" STR_FMT "\"\n"
	       "\t@reg 0x%08" PRIxPTR " 0x%" PRIxPTR "\t# end 0x%08" PRIxPTR "\n\n",
	       STR_ARG(&unit->id), STR_ARG(&unit->name),
	       unit->start, unit->end - unit->start + 1, unit->end);

	for (size_t i = 0; i < unit->num_registers; ++i)
		dump_register(&unit->registers[i]);
}

static void dump_regval(struct cpu_unit const units[], size_t num_units,
			uintptr_t addr, uint32_t val)
{
	struct dump_data	priv_ = {
		.do_introspect	= false,
		.buf		= malloc(g_buf_len),
		.buf_len	= g_buf_len,
		.delim		= "",
		.is_terse	= true,
	};
	struct dump_data	*priv = &priv_;
	bool			rc;
	reg_t			r = { .u32 = val };

	priv->ptr = priv->buf;

	rc = deserialize_decode(units, num_units, addr, &r, priv);
	*priv->ptr = '\0';

	if (!rc) {
		printf("[0x%08" PRIxPTR "]\t\t\t%08x", addr, val);
		/* unknown register */
	} else if (!priv->reg) {
		fprintf(stderr,
			"register 0x%08" PRIxPTR " without known fields\n",
		       (unsigned long)addr);
		/* todo: known register but no known content */
	} else {
		size_t cnt;

		cnt  = printf(STR_FMT, STR_ARG(&priv->reg->name)) + 0;
		cnt /= 8;
		while (cnt < 1) {
			++cnt;
			printf("\t");
		}

		printf("\t[%s]", priv->buf);
	}

	if (addr == 0x021b001c && (val & 0xfff0) == 0x8030) {
		printf("\n\t\t\t\t# ");
		dump_regval(units, num_units,
			    0xffff0000 + (val & 0x7) * 4, val >> 16);
	}
}

int main(int argc, char *argv[])
{
	void const	*stream = STREAM;
	size_t		stream_len = sizeof STREAM;

	size_t		num_units;
	struct cpu_unit	*units;

	heap_alloc = getenv("TEST_HEAP_ALLOC");

	heap.len  = stream_len * 8;
	heap.base = malloc(heap.len );
	heap.ptr  = heap.base;

	if (!deserialize_cpu_units(&units, &num_units, &stream, &stream_len))
		abort();

	if (getenv("TEST_BUFLEN"))
		g_buf_len = atoi(getenv("TEST_BUFLEN"));

	if (argc == 1) {
		for (size_t i = 0; i < num_units; ++i) {
			dump_unit(&units[i]);
		}
	} else {
		dump_regval(units, num_units,
			    strtoul(argv[1], NULL, 0),
			    strtoul(argv[2], NULL, 0));
		printf("\n");
	}

	if (!heap_alloc) {
		for (size_t i = num_units; i > 0; --i)
			deserialize_cpu_unit_release(&units[i - 1]);

		free(units);
	}

	free(heap.base);
}
