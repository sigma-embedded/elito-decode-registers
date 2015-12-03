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

#define STORE_BE	0
#define ASSIGN_IN_PLACE	1

#include "deserialize.h"

#include <stdlib.h>
#include <stdint.h>
#include <endian.h>
#include <string.h>
#include <stddef.h>

#include DESERIALIZE_SYMBOLS

#define container_of(_ptr, _type, _attr) __extension__		\
	({								\
		__typeof__( ((_type *)0)->_attr) *_tmp_mptr = (_ptr);	\
		(_type *)((uintptr_t)_tmp_mptr - offsetof(_type, _attr)); \
	})

#ifndef BUG
#  include <stdio.h>
#  define BUG()		do {						\
		fprintf(stderr, "%s:%u aborted\n", __func__, __LINE__); \
		abort();						\
	} while (1)
#endif

inline static void *deserialize_calloc(size_t cnt, size_t len)
{
	if (cnt != 0 && SIZE_MAX/cnt <= len) {
		BUG();
		return NULL;
	} else {
		return deserialize_alloc(cnt * len);
	}
}

static bool pop_mem(void *dst, size_t len, void const **src, size_t *sz)
{
	if (*sz < len) {
		*sz = (size_t)(-1);
		BUG();
		return false;
	}

	memcpy(dst, *src, len);
	*src += len;
	*sz  -= len;

	return true;
}

static void const *assign_mem(size_t len, void const **src, size_t *sz)
{
	void const	*res;

	if (*sz < len) {
		printf("%zu < %zu\n", *sz, len);
		
		*sz = (size_t)(-1);
		BUG();
		return NULL;
	}

	res   = *src;
	*src += len;
	*sz  -= len;

	return res;
}

static bool pop_u32(uint32_t *v, void const **buf, size_t *sz)
{
	uint32_t	tmp;

	if (!pop_mem(&tmp, sizeof tmp, buf, sz))
		return false;


	if (STORE_BE)
		*v = be32toh(tmp);
	else
		*v = le32toh(tmp);

	return true;
}

static bool pop_u16(uint16_t *v, void const **buf, size_t *sz)
{
	uint16_t	tmp;

	if (!pop_mem(&tmp, sizeof tmp, buf, sz))
		return false;


	if (STORE_BE)
		*v = be16toh(tmp);
	else
		*v = le16toh(tmp);

	return true;
}

static bool pop_u8(uint8_t *v, void const **buf, size_t *sz)
{
	return pop_mem(v, sizeof *v, buf, sz);
}

static bool pop_uint_var(uintmax_t *s, unsigned int order,
			 void const **buf, size_t *sz)
{
	if (order <= 8) {
		uint8_t	tmp;
		if (!pop_u8(&tmp, buf, sz))
			return false;
		*s = tmp;
	} else if (order <= 16) {
		uint16_t	tmp;
		if (!pop_u16(&tmp, buf, sz))
			return false;
		*s = tmp;
	} else if (order <= 32) {
		uint32_t	tmp;
		if (!pop_u32(&tmp, buf, sz))
			return false;
		*s = tmp;
	} else {
		BUG();
		return false;
	}

	if (0)
		printf("%u, %u\n", (unsigned int)(*s), order);

	return true;
}

static bool pop_size_t_var(size_t *s, unsigned int order,
			 void const **buf, size_t *sz)
{
	uintmax_t	tmp;

	if (!pop_uint_var(&tmp, order, buf, sz))
		return false;

	*s = tmp;
	return true;
}

static bool pop_reg_t_var(reg_t *s, unsigned int order,
			  void const **buf, size_t *sz)
{
	uintmax_t	tmp;

	if (!pop_uint_var(&tmp, order, buf, sz))
		return false;

	*s = tmp;
	return true;
}

static bool pop_string(struct string *str, void const **buf, size_t *sz)
{
	uint32_t	dlen;

	if (!pop_u32(&dlen, buf, sz))
		return false;

	if (!ASSIGN_IN_PLACE) {
		char	*data;

		data = deserialize_alloc(dlen);
		if (!data)
			return false;

		if (!pop_mem(data, dlen, buf, sz))
			return false;

		str->data = data;
	} else {
		str->data = assign_mem(dlen, buf, sz);

		if (!str->data)
			return false;
	}

	str->len  = dlen;

	return true;
}

static bool pop_uintptr_t(uintptr_t *ptr, void const **buf, size_t *sz)
{
	uint32_t	tmp;

	if (!pop_u32(&tmp, buf, sz))
		return false;

	*ptr = tmp;
	return true;
}

static bool pop_size_t(size_t *ptr, void const **buf, size_t *sz)
{
	uint32_t	tmp;

	if (!pop_u32(&tmp, buf, sz))
		return false;

	*ptr = tmp;
	return true;
}

static bool pop_uchar(unsigned char *c, void const **buf, size_t *sz)
{
	uint8_t	tmp;

	if (!pop_u8(&tmp, buf, sz))
		return false;

	*c = tmp;
	return true;
}

static bool pop_reg_t(reg_t *r, void const **buf, size_t *sz)
{
	uint32_t	tmp;

	if (!pop_u32(&tmp, buf, sz))
		return false;

	*r = tmp;
	return true;
}

static void _deserialize_dump_bool(struct cpu_regfield const *fld_,
				   uintmax_t v, void *priv)
{
	struct cpu_regfield_bool const	*fld =
		container_of(fld_, struct cpu_regfield_bool const, reg);

	deserialize_dump_bool(fld, !!(v & (1 << fld->bit)), priv);
}

static bool pop_cpu_regfield_bool(struct cpu_regfield_bool **fld,
					  void const **buf, size_t *sz)
{
	*fld = deserialize_alloc(sizeof **fld);
	if (!(*fld))
		return false;

	if (!pop_uchar(&(*fld)->bit, buf, sz))
		return false;

	(*fld)->reg.fn = _deserialize_dump_bool;

	return true;
}

static uintmax_t get_masked_value(uintmax_t v, reg_t mask)
{
	uintmax_t	res = 0;
	unsigned int	pos = 0;

	while (mask) {
		int	p = __builtin_ffs(mask) - 1;

		res  |= ((v >> p) & 1) << pos;
		++pos;

		mask &= ~(1 << p);
	}

	return res;
}

static void _deserialize_dump_frac(struct cpu_regfield const *fld_,
				   uintmax_t v, void *priv)
{
	struct cpu_regfield_frac const	*fld =
		container_of(fld_, struct cpu_regfield_frac const, reg);

	uintmax_t		parts[2];

	parts[0] = get_masked_value(v, fld->int_part);
	parts[1] = get_masked_value(v, fld->frac_part);

	deserialize_dump_frac(fld, parts[0], parts[1], priv);
}

static bool pop_cpu_regfield_frac(struct cpu_regfield_frac **fld,
				  void const **buf, size_t *sz)
{
	*fld = deserialize_alloc(sizeof **fld);
	if (!(*fld))
		return false;

	if (!pop_u32(&(*fld)->int_part, buf, sz) ||
	    !pop_u32(&(*fld)->frac_part, buf, sz))
		return false;

	(*fld)->reg.fn = _deserialize_dump_frac;

	return true;
}

static void _deserialize_dump_reserved(struct cpu_regfield const *fld_,
				       uintmax_t v, void *priv)
{
	struct cpu_regfield_reserved const	*fld =
		container_of(fld_, struct cpu_regfield_reserved const, reg);

	deserialize_dump_reserved(fld, v, priv);
}

static bool pop_cpu_regfield_reserved(struct cpu_regfield_reserved **fld,
				      void const **buf, size_t *sz)
{
	*fld = deserialize_alloc(sizeof **fld);
	if (!(*fld))
		return false;

	if (!pop_reg_t(&(*fld)->bitmask, buf, sz))
		return false;

	(*fld)->reg.fn = _deserialize_dump_reserved;

	return true;
}

static void _deserialize_dump_enum(struct cpu_regfield const *fld_,
				   uintmax_t v, void *priv)
{
	struct cpu_regfield_enum const	*fld =
		container_of(fld_, struct cpu_regfield_enum const, reg);

	unsigned int				idx = 0;
	struct cpu_regfield_enum_val const	*val = NULL;

	idx = get_masked_value(v, fld->bitmask);

	for (size_t i = 0; i < fld->num_enums; ++i) {
		if (idx == fld->enums[i].val) {
			val = &fld->enums[i];
			break;
		}
	}

	deserialize_dump_enum(fld, val, idx, priv);
}

static bool pop_cpu_regfield_enum_val(struct cpu_regfield_enum_val *eval,
				      unsigned int order,
				      void const **buf, size_t *sz)
{
	return (pop_reg_t_var(&eval->val, order, buf, sz) &&
		pop_string(&eval->name, buf, sz));
}

static bool pop_cpu_regfield_enum(struct cpu_regfield_enum **fld,
					  void const **buf, size_t *sz)
{
	reg_t		bitmask;
	size_t		num_enums;
	unsigned int	order;

	if (!pop_reg_t(&bitmask, buf, sz))
		return false;

	order = __builtin_popcount(bitmask);

	if (!pop_size_t_var(&num_enums, order, buf, sz))
		return false;

	*fld = deserialize_alloc(sizeof **fld +
				 num_enums * sizeof (*fld)->enums[0]);

	if (!(*fld))
		return false;

	for (size_t i = 0; i < num_enums; ++i) {
		if (!pop_cpu_regfield_enum_val(&(*fld)->enums[i],
					       order, buf, sz))
			return false;
	}

	(*fld)->bitmask = bitmask;
	(*fld)->num_enums = num_enums;
	(*fld)->reg.fn = _deserialize_dump_enum;

	return true;
}

static bool pop_cpu_regfield(struct cpu_regfield **field,
			     void const **buf, size_t *sz)
{
	struct string		id;
	struct string		name;
	uint32_t		type;

	if (!pop_string(&id, buf, sz) ||
	    !pop_string(&name, buf, sz) ||
	    !pop_u32(&type, buf, sz))
		return false;

	switch (type) {
#ifdef TYPE_BOOL
	case TYPE_BOOL: {
		struct cpu_regfield_bool	*fld;

		if (!pop_cpu_regfield_bool(&fld, buf, sz))
			return false;

		*field = &fld->reg;
		break;
	}
#endif

#ifdef TYPE_ENUM
	case TYPE_ENUM: {
		struct cpu_regfield_enum	*fld;

		if (!pop_cpu_regfield_enum(&fld, buf, sz))
			return false;

		*field = &fld->reg;
		break;
	}
#endif

#ifdef TYPE_FRAC
	case TYPE_FRAC: {
		struct cpu_regfield_frac	*fld;

		if (!pop_cpu_regfield_frac(&fld, buf, sz))
			return false;

		*field = &fld->reg;
		break;
	}
#endif

#ifdef TYPE_RESERVED
	case TYPE_RESERVED: {
		struct cpu_regfield_reserved	*fld;

		if (!pop_cpu_regfield_reserved(&fld, buf, sz))
			return false;

		*field = &fld->reg;
		break;
	}
#endif

	default:
		BUG();
		return false;
	}

	(*field)->id = id;
	(*field)->name = name;

	return true;
}

static bool pop_cpu_register(struct cpu_register *reg,
			      void const **buf, size_t *sz)
{
	size_t				num_fields;
	struct cpu_regfield const	**fields;

	if (!pop_uintptr_t(&reg->offset, buf, sz) ||
	    !pop_u8(&reg->width, buf, sz) ||
	    !pop_string(&reg->id, buf, sz) ||
	    !pop_string(&reg->name, buf,sz) ||
	    !pop_size_t(&num_fields, buf, sz))
		return false;

	fields = deserialize_calloc(num_fields, sizeof fields[0]);
	if (!fields && num_fields > 0)
		return false;

	for (size_t i = 0; i < num_fields; ++i) {
		struct cpu_regfield	*field;

		if (!pop_cpu_regfield(&field, buf, sz))
			return false;

		field->reg = reg;
		fields[i]  = field;
	}

	reg->fields = fields;
	reg->num_fields = num_fields;

	return true;
}

static bool pop_cpu_unit(struct cpu_unit *unit, void const **buf, size_t *sz)
{
	size_t			num_regs;
	struct cpu_register	*regs;

	if (!pop_uintptr_t(&unit->start, buf, sz) ||
	    !pop_uintptr_t(&unit->end, buf, sz) ||
	    !pop_string(&unit->id, buf, sz) ||
	    !pop_string(&unit->name, buf, sz) ||
	    !pop_size_t(&num_regs, buf, sz))
		return false;

	regs = deserialize_calloc(num_regs, sizeof regs[0]);
	if (!regs && num_regs > 0)
		return false;

	for (size_t i = 0; i < num_regs; ++i) {
		regs[i].unit = unit;
		if (!pop_cpu_register(&regs[i], buf, sz))
			return false;
	}

	unit->num_registers = num_regs;
	unit->registers     = regs;

	return true;
}

bool deserialize_cpu_units(struct cpu_unit **units, size_t *num,
			  void const **buf, size_t *sz)
{
	if (!pop_size_t(num, buf, sz))
		return false;

	*units = deserialize_calloc(*num, sizeof (*units)[0]);
	if (!*units && *num > 0)
		return false;

	for (size_t i = 0; i < *num; ++i) {
		if (!pop_cpu_unit(&(*units)[i], buf, sz))
			return false;
	}

	return true;
}

void deserialize_decode_reg(struct cpu_register const *reg,
			    uintmax_t val, void *priv)
{
	for (size_t i = 0; i < reg->num_fields; ++i) {
		struct cpu_regfield const	*fld = reg->fields[i];

		fld->fn(fld, val, priv);
	}
}

bool deserialize_decode(struct cpu_unit const units[], size_t unit_cnt,
			uintptr_t addr, uintmax_t val, void *priv)
{
	bool	found = false;

	for (size_t i = 0; i < unit_cnt && !found; ++i) {
		struct cpu_unit const	*unit = &units[i];
		uintptr_t		base = unit->start;

		if (addr < base || addr > unit->end)
			continue;

		for (size_t j = 0; j < unit->num_registers; ++j) {
			struct cpu_register const	*reg = &unit->registers[j];

			if (addr != base + reg->offset)
				continue;

			deserialize_decode_reg(reg, val, priv);
			found = true;
			break;
		}
	}

	return found;
}
