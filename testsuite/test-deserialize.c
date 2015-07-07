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
#include <stddef.h>
#include <inttypes.h>
#include <assert.h>

#include "deserialize.h"

static uint8_t const	STREAM[] = {
	#include "test-deserialize_stream.h"
};

#define STR_FMT		"%.*s"
#define STR_ARG(_s)	(int)((_s)->len), (_s)->data



struct {
	void		*base;
	size_t		len;
	void		*ptr;
}			heap;

struct dump_data {
	bool		do_introspect;
	char		*buf;
	size_t		buf_len;
};

void *deserialize_alloc(size_t len)
{
	if (1) {
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
	else
		_deserialize_dump_bool(v, priv->buf, priv->buf_len);
}

static char *deserialize_introspect_enum(struct cpu_regfield_enum const *fld,
					 char *buf, size_t len)
{
	char	*res = buf;
	int	rc;

	rc = snprintf(buf, len, "\t\t\t\t@bitmask 0x%08x\n",
		      (unsigned int)fld->bitmask);

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

void deserialize_dump_enum(struct cpu_regfield_enum const *fld,
			   struct cpu_regfield_enum_val const *val,
			   size_t idx, void *priv_)
{
	struct dump_data	*priv = priv_;

	if (priv->do_introspect)
		deserialize_introspect_enum(fld, priv->buf, priv->buf_len);
	else
		_deserialize_dump_enum(val, idx, priv->buf, priv->buf_len);
}

void deserialize_dump_reserved(struct cpu_regfield_reserved const *fld,
			       reg_t v, void *priv)
{
	(void)fld;
	(void)v;
	(void)priv;
}

static void dump_field(struct cpu_regfield const *fld)
{
	char			buf[1024];
	struct dump_data	d = {
		.do_introspect	= true,
		.buf		= buf,
		.buf_len	= sizeof buf,
	};

	printf("\t\t\t@field " STR_FMT "\n"
	       "\t\t\t\t@name " STR_FMT "\n",
	       STR_ARG(&fld->id), STR_ARG(&fld->name));

	fld->fn(fld, 0, &d);
	printf("%s", buf);
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

int main(void)
{
	void const	*stream = STREAM;
	size_t		stream_len = sizeof STREAM;

	size_t		num_units;
	struct cpu_unit	*units;

	heap.len  = stream_len * 8;
	heap.base = malloc(heap.len );
	heap.ptr  = heap.base;

	if (!deserialize_cpu_units(&units, &num_units, &stream, &stream_len))
		abort();


	for (size_t i = 0; i < num_units; ++i) {
		dump_unit(&units[i]);
	}
}
