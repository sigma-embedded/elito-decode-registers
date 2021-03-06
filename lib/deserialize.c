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

#define _unused_ __attribute__((__unused__))

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
		__builtin_unreachable();				\
	} while (1)
#endif

#ifndef BUG_ON
#  define BUG_ON(_cond)	do {			\
		if ((_cond))			\
			BUG();			\
	} while (0)
#endif

/* workaround for ancient gcc versions
 * REMOVE ME after 2021-01-01 */
#if __GNUC__ == 4 && __GNUC_MINOR__ < 9

#  define _ffs(_v)	__builtin_ffsll(_v)
#  define _popcount(_v)	__builtin_popcountll(_v)

#else

#define _ffs(_v)						\
	_Generic(_v,						\
		 unsigned char: __builtin_ffs(_v),		\
		 unsigned short: __builtin_ffs(_v),		\
		 unsigned int: __builtin_ffs(_v),		\
		 unsigned long: __builtin_ffsl(_v),		\
		 unsigned long long: __builtin_ffsll(_v))

#define _popcount(_v)						\
	_Generic(_v,						\
		 unsigned char: __builtin_popcount(_v),		\
		 unsigned short: __builtin_popcount(_v),	\
		 unsigned int: __builtin_popcount(_v),		\
		 unsigned long: __builtin_popcountl(_v),	\
		 unsigned long long: __builtin_popcountll(_v))

#endif	/* __GNUC__ */

#define _bit_type(_t, _p) (((__typeof__(_t))1u) << (_p))

static regmax_t get_masked_value(reg_t const *v_ext,
				 reg_t const *mask_ext,
				 struct cpu_register const *reg)

{
	regmax_t	res = 0;
	unsigned int	pos = 0;
	unsigned int	width = reg->width;
	void const	*v_in = v_ext->reg_raw;
	void const	*m_in = mask_ext->reg_raw;

	BUG_ON(!v_ext);
	BUG_ON(width > 8 * sizeof *mask_ext);
	BUG_ON(width % 8 != 0);

	width = (width + 7) / 8;

	while (width >= sizeof(regcalc_t)) {
		regcalc_t	v;
		regcalc_t	mask;

		memcpy(&v,    v_in, sizeof v);
		memcpy(&mask, m_in, sizeof mask);

		while (mask) {
			int		p = _ffs(mask) - 1;
			regmax_t	tmp = ((v >> p) & 1);

			tmp <<= pos;
			res  |= tmp;
			++pos;

			mask &= ~_bit_type(mask, p);
		}

		v_in += sizeof v;
		m_in += sizeof v;
		width -= sizeof v;
	}

	while (width > 0) {
		uint8_t	v;
		uint8_t	mask;

		memcpy(&v,    v_in, sizeof v);
		memcpy(&mask, m_in, sizeof mask);

		while (mask) {
			int		p = _ffs(mask) - 1;
			regmax_t	tmp = ((v >> p) & 1);

			tmp <<= pos;
			res  |= tmp;
			++pos;

			mask &= ~_bit_type(mask, p);
		}

		v_in += sizeof v;
		m_in += sizeof v;
		width -= sizeof v;
	}

	BUG_ON(pos > 8 * sizeof res);

	return res;
}

unsigned int deserialize_popcount(reg_t const *reg, unsigned int width)
{
	unsigned int	res = 0;

	BUG_ON(width > 8 * sizeof *reg);
	BUG_ON(width % 8 != 0);

	width = (width + 7) / 8;

	for (size_t i = 0; i < width; ++i)
		res += _popcount(reg->raw[i]);

	return res;
}

static bool test_bit(reg_t const *reg, unsigned int bit)
{
	uint8_t		v;
	uint8_t		msk = 1u;

	BUG_ON(bit > sizeof reg->raw * 8);

	v = reg->raw[bit / (8 * sizeof v)];
	msk <<= bit % (8 * sizeof v);

	return (v & msk) != 0;
}

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

static void byte_revert(void *buf_, size_t len)
{
	unsigned char	*buf = buf_;
	size_t		a = 0;
	size_t		b = len;

	while (a + 1u < b) {
		unsigned char	tmp;

		--b;

		tmp    = buf[a];
		buf[a] = buf[b];
		buf[b] = tmp;

		++a;
	}
}

static bool pop_reg(reg_t *reg, unsigned int width,
		    void const **buf, size_t *sz)
{
	switch (width) {
	case 8:
		return pop_u8(&reg->u8, buf, sz);
	case 16:
		return pop_u16(&reg->u16, buf, sz);
	case 32:
		return pop_u32(&reg->u32, buf, sz);
	}

	BUG_ON(width % 8 != 0);

	if (!pop_mem(reg->raw, (width + 7) / 8, buf, sz))
		return false;

	if (!!STORE_BE != !!(__BYTE_ORDER == __BIG_ENDIAN))
		byte_revert(reg->raw, width / 8);

	return true;
}

static bool pop_uint_var(regmax_t *s, unsigned int order,
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
	regmax_t	tmp;

	if (!pop_uint_var(&tmp, order, buf, sz))
		return false;

	*s = tmp;
	return true;
}

static bool pop_string(struct string *str, void const **buf, size_t *sz)
{
	uint16_t	dlen;

	if (!pop_u16(&dlen, buf, sz))
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
	uint16_t	tmp;

	if (!pop_u16(&tmp, buf, sz))
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

static void _deserialize_dump_bool(struct cpu_regfield const *fld_,
				   reg_t const *v, void *priv)
{
	struct cpu_regfield_bool const	*fld =
		container_of(fld_, struct cpu_regfield_bool const, reg);

	BUG_ON(fld->bit >= fld->reg.reg->width);

	deserialize_dump_bool(fld, test_bit(v, fld->bit), priv);
}

static bool _unused_ pop_cpu_regfield_bool(struct cpu_regfield_bool **fld,
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

static void _deserialize_dump_frac(struct cpu_regfield const *fld_,
				   reg_t const *v, void *priv)
{
	struct cpu_regfield_frac const	*fld =
		container_of(fld_, struct cpu_regfield_frac const, reg);

	regmax_t		parts[2];

	parts[0] = get_masked_value(v, &fld->int_part, fld_->reg);
	parts[1] = get_masked_value(v, &fld->frac_part, fld_->reg);

	deserialize_dump_frac(fld, parts[0], parts[1], priv);
}

static bool _unused_ pop_cpu_regfield_frac(struct cpu_regfield_frac **fld,
					   struct cpu_register const *reg,
					   void const **buf, size_t *sz)
{
	*fld = deserialize_alloc(sizeof **fld);
	if (!(*fld))
		return false;

	if (!pop_reg(&(*fld)->int_part,  reg->width, buf, sz) ||
	    !pop_reg(&(*fld)->frac_part, reg->width, buf, sz))
		return false;

	(*fld)->reg.fn = _deserialize_dump_frac;

	return true;
}

static void _deserialize_dump_reserved(struct cpu_regfield const *fld_,
				       reg_t const *v, void *priv)
{
	struct cpu_regfield_reserved const	*fld =
		container_of(fld_, struct cpu_regfield_reserved const, reg);

	deserialize_dump_reserved(fld, v, priv);
}

static bool _unused_ pop_cpu_regfield_reserved(struct cpu_regfield_reserved **fld,
					       struct cpu_register const *reg,
					       void const **buf, size_t *sz)
{
	*fld = deserialize_alloc(sizeof **fld);
	if (!(*fld))
		return false;

	if (!pop_reg(&(*fld)->bitmask, reg->width, buf, sz))
		return false;

	(*fld)->reg.fn = _deserialize_dump_reserved;

	return true;
}

static void _deserialize_dump_enum(struct cpu_regfield const *fld_,
				   reg_t const *v, void *priv)
{
	struct cpu_regfield_enum const	*fld =
		container_of(fld_, struct cpu_regfield_enum const, reg);

	unsigned int				idx = 0;
	struct cpu_regfield_enum_val const	*val = NULL;

	BUG_ON(!v);

	idx = get_masked_value(v, &fld->bitmask, fld->reg.reg);

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
	return (pop_uint_var(&eval->val, order, buf, sz) &&
		pop_string(&eval->name, buf, sz));
}

static bool _unused_ pop_cpu_regfield_enum(struct cpu_regfield_enum **fld,
					   struct cpu_register const *reg,
					   void const **buf, size_t *sz)
{
	reg_t		bitmask;
	size_t		num_enums;
	unsigned int	order;

	if (!pop_reg(&bitmask, reg->width, buf, sz))
		return false;

	order = deserialize_popcount(&bitmask, reg->width);

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

static void _deserialize_dump_int(struct cpu_regfield const *fld_,
				  reg_t const *v, void *priv)
{
	struct cpu_regfield_int const	*fld =
		container_of(fld_, struct cpu_regfield_int const, reg);
	regmax_t			tmp;

	tmp = get_masked_value(v, &fld->bitmask, fld->reg.reg);

	if (fld->is_signed)
		/* TODO: fix regmax_t -> signed int conversion */
		deserialize_dump_sint(fld, (signed long)(tmp), priv);
	else
		deserialize_dump_uint(fld, (regmax_t)(tmp), priv);
}

static bool _unused_ pop_cpu_regfield_int(struct cpu_regfield_int **fld,
					  struct cpu_register const *reg,
					  void const **buf, size_t *sz,
					  bool is_signed)
{
	*fld = deserialize_alloc(sizeof **fld);
	if (!(*fld))
		return false;

	if (!pop_reg(&(*fld)->bitmask, reg->width, buf, sz))
		return false;

	(*fld)->is_signed = is_signed;
	(*fld)->reg.fn = _deserialize_dump_int;

	return true;
}

static bool _unused_ is_signed_type(uint32_t type)
{
	switch (type) {
#ifdef TYPE_SINT
	case TYPE_SINT:
		return true;
#endif

#ifdef TYPE_UINT
	case TYPE_UINT:
		return false;
#endif

	default:
		BUG();
	}
}

static bool pop_cpu_regfield(struct cpu_regfield **field,
			     struct cpu_register const *reg,
			     void const **buf, size_t *sz)
{
	struct string		id;
	struct string		name;
	uint8_t			type;
	regmax_t		reg_flags;

	if (!pop_uint_var(&reg_flags, 2, buf, sz) ||
	    !pop_string(&id, buf, sz) ||
	    !pop_string(&name, buf, sz) ||
	    !pop_u8(&type, buf, sz))
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

		if (!pop_cpu_regfield_enum(&fld, reg, buf, sz))
			return false;

		*field = &fld->reg;
		break;
	}
#endif

#ifdef TYPE_FRAC
	case TYPE_FRAC: {
		struct cpu_regfield_frac	*fld;

		if (!pop_cpu_regfield_frac(&fld, reg, buf, sz))
			return false;

		*field = &fld->reg;
		break;
	}
#endif

#if defined(TYPE_SINT) || defined(TYPE_UINT)
#  ifdef TYPE_SINT
	case TYPE_SINT:
#  endif
#  ifdef TYPE_UINT
	case TYPE_UINT:
#  endif
	{
		struct cpu_regfield_int		*fld;

		if (!pop_cpu_regfield_int(&fld, reg, buf, sz,
					  is_signed_type(type)))
			return false;

		*field = &fld->reg;
		break;
	}
#endif

#ifdef TYPE_RESERVED
	case TYPE_RESERVED: {
		struct cpu_regfield_reserved	*fld;

		if (!pop_cpu_regfield_reserved(&fld, reg, buf, sz))
			return false;

		*field = &fld->reg;
		break;
	}
#endif

	default:
		BUG();
		return false;
	}

	(*field)->reg = reg;
	(*field)->flags = reg_flags;
	(*field)->id = id;
	(*field)->name = name;

	return true;
}

static bool pop_cpu_register(struct cpu_register *reg,
			     void const **buf, size_t *sz)
{
	size_t				num_fields;
	struct cpu_regfield const	**fields;
	regmax_t			reg_flags;

	if (!pop_uintptr_t(&reg->offset, buf, sz) ||
	    !pop_u8(&reg->width, buf, sz) ||
	    !pop_uint_var(&reg_flags, 2, buf, sz) ||
	    !pop_string(&reg->id, buf, sz) ||
	    !pop_string(&reg->name, buf,sz) ||
	    !pop_size_t(&num_fields, buf, sz))
		return false;

	BUG_ON((unsigned int)reg->width > sizeof(reg_t) * 8);

	reg->flags = reg_flags;

	fields = deserialize_calloc(num_fields, sizeof fields[0]);
	if (!fields && num_fields > 0)
		return false;

	for (size_t i = 0; i < num_fields; ++i) {
		struct cpu_regfield	*field;

		if (!pop_cpu_regfield(&field, reg, buf, sz))
			return false;

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
	    !pop_u8(&unit->addr_width, buf, sz) ||
	    !pop_u8(&unit->endian, buf, sz) ||
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
			    reg_t const *val, void *priv)
{
	for (size_t i = 0; i < reg->num_fields; ++i) {
		struct cpu_regfield const	*fld = reg->fields[i];

		fld->fn(fld, val, priv);
	}
}

bool deserialize_decode(struct cpu_unit const units[], size_t unit_cnt,
			uintptr_t addr, reg_t const *val, void *priv)
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

int deserialize_decode_range(struct cpu_unit const units[], size_t unit_cnt,
			     uintptr_t start_addr, uintptr_t end_addr,
			     int (*decode_fn)(struct cpu_register const *reg,
					      void *priv),
			     void *priv)
{
	int	rc;

	if (start_addr > end_addr)
		return 0;

	rc = 0;

	for (size_t i = 0; i < unit_cnt; ++i) {
		struct cpu_unit const	*unit = &units[i];
		uintptr_t		base = unit->start;

		if (start_addr > unit->end || end_addr < unit->start)
			continue;

		for (size_t j = 0; j < unit->num_registers; ++j) {
			struct cpu_register const	*reg = &unit->registers[j];
			uintptr_t			abs_addr;

			abs_addr = base + reg->offset;

			if (abs_addr > end_addr || abs_addr < start_addr)
				continue;

			rc = decode_fn(reg, priv);
			if (rc < 0)
				break;
		}

		if (rc < 0)
			break;
	}

	return rc;
}

static void xfree(void const *p)
{
	free((void *)p);
}

static void deserialize_cpu_regfield_free(struct cpu_regfield const *fld)
{
	/* TODO: this is hacky; we should add a _release() callback to the
	 * cpu_regfield_XXX objects. For now, assume/require that 'reg' is the
	 * first attribute in the specialized object */

	xfree(fld);
}

static void deserialize_cpu_register_release(struct cpu_register const *reg)
{
	for (size_t i = 0; i < reg->num_fields; ++i)
		deserialize_cpu_regfield_free(reg->fields[i]);

	xfree(reg->fields);
}

void deserialize_cpu_unit_release(struct cpu_unit const *unit)
{
	for (size_t i = 0; i < unit->num_registers; ++i)
		deserialize_cpu_register_release(&unit->registers[i]);

	xfree(unit->registers);
}
