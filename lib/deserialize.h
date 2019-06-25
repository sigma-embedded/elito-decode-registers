/*	--*- c -*--
 * Copyright (C) 2015 Enrico Scholz <enrico.scholz@sigma-chemnitz.de>
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

#ifndef H_ELITO_DECODER_DESERIALIZER_H
#define H_ELITO_DECODER_DESERIALIZER_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#ifndef REGISTER_MAX_REGSIZE
#  define REGISTER_MAX_REGSIZE	(256u)
#endif

#ifdef REGISTER_REG_T
typedef REGISTER_DEF_T		regmax_t;
#else
typedef uintmax_t		regmax_t;
#endif

#ifdef REGISTER_CALC_T
typedef REGISTER_CALC_T		regcalc_t;
#else
typedef regmax_t		regcalc_t;
#endif

union _reg {
	uint8_t		u8;
	uint16_t	u16;
	uint32_t	u32;
	regmax_t	max;

#ifndef REGISTER_DISABLE_U64
	uint64_t	u64;
#endif

	uint8_t		raw[REGISTER_MAX_REGSIZE / 8];
	regmax_t	reg_raw[REGISTER_MAX_REGSIZE / 8 / sizeof(regmax_t)];
	regcalc_t	calc_raw[REGISTER_MAX_REGSIZE / 8 / sizeof(regcalc_t)];
};

typedef union _reg		reg_t;

struct cpu_regfield;
typedef void			(*deserialize_decoder_fn)(
	struct cpu_regfield const *, reg_t const *v, void *priv)
	__attribute__((__nonnull__(1,2)));

struct string {
	size_t			len;
	char const		*data;
};

struct cpu_register;
struct cpu_unit {
	uintptr_t			start;
	uintptr_t			end;

	struct string			id;
	struct string			name;

	uint8_t				addr_width;
	uint8_t				endian;

	size_t				num_registers;
	struct cpu_register const	*registers;
};

struct cpu_register {
	uintptr_t			offset;
	unsigned char			width;
	uint8_t				flags;

	struct string			id;
	struct string			name;

	struct cpu_unit const		*unit;

	size_t				num_fields;
	struct cpu_regfield const	**fields;
};

struct cpu_regfield {
	struct string			id;
	struct string			name;
	struct cpu_register const	*reg;
	deserialize_decoder_fn		fn;
	uint8_t				flags;
};

struct cpu_regfield_bool {
	struct cpu_regfield		reg;
	unsigned char			bit;
};

struct cpu_regfield_frac {
	struct cpu_regfield		reg;
	reg_t				int_part;
	reg_t				frac_part;
};

struct cpu_regfield_enum_val {
	struct string			name;
	regmax_t			val;
};

struct cpu_regfield_enum {
	struct cpu_regfield		reg;
	reg_t				bitmask;

	size_t				num_enums;
	struct cpu_regfield_enum_val	enums[];
};

struct cpu_regfield_reserved {
	struct cpu_regfield		reg;
	reg_t				bitmask;
};

struct cpu_regfield_int {
	struct cpu_regfield		reg;
	reg_t				bitmask;
	bool				is_signed;
};

extern void *deserialize_alloc(size_t len);
extern void deserialize_dump_bool(struct cpu_regfield_bool const *fld,
				  bool v, void *priv);
extern void deserialize_dump_enum(struct cpu_regfield_enum const *fld,
				  struct cpu_regfield_enum_val const *val,
				  size_t idx, void *priv);
extern void deserialize_dump_frac(struct cpu_regfield_frac const *fld,
				  regmax_t int_part, regmax_t frac_part,
				  void *priv);
extern void deserialize_dump_uint(struct cpu_regfield_int const *fld,
				  regmax_t val, void *priv);
extern void deserialize_dump_sint(struct cpu_regfield_int const *fld,
				  signed long val, void *priv);
extern void deserialize_dump_reserved(struct cpu_regfield_reserved const *fld,
				      reg_t const *v, void *priv);

unsigned int deserialize_popcount(reg_t const *reg, unsigned int width);

inline static size_t deserialize_get_sz(void const *start, size_t len,
					void const *end)
{
	if (!end)
		return len;
	else
		return (uintptr_t)(end) - (uintptr_t)(start);
}

void deserialize_cpu_unit_release(struct cpu_unit const *unit);

bool deserialize_cpu_units(struct cpu_unit **unit, size_t *cnt,
			   void const **buf, size_t *sz);

void deserialize_decode_reg(struct cpu_register const *reg,
			    reg_t const *val, void *priv);

bool deserialize_decode(struct cpu_unit const units[], size_t unit_cnt,
			uintptr_t addr, reg_t const *val, void *priv);

int deserialize_decode_range(struct cpu_unit const units[], size_t unit_cnt,
			     uintptr_t start_addr, uintptr_t end_addr,
			     int (*decode_fn)(struct cpu_register const *reg,
					      void *priv),
			     void *priv);

#endif	/* H_ELITO_DECODER_DESERIALIZER_H */
