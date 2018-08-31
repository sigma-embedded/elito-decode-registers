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

typedef uint32_t		reg_t;

struct cpu_regfield;
typedef void			(*deserialize_decoder_fn)(
	struct cpu_regfield const *, uintmax_t v, void *priv);

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
	reg_t				val;
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
	reg_t				val;
	bool				is_signed;
};

extern void *deserialize_alloc(size_t len);
extern void deserialize_dump_bool(struct cpu_regfield_bool const *fld,
				  bool v, void *priv);
extern void deserialize_dump_enum(struct cpu_regfield_enum const *fld,
				  struct cpu_regfield_enum_val const *val,
				  size_t idx, void *priv);
extern void deserialize_dump_frac(struct cpu_regfield_frac const *fld,
				  reg_t int_part, reg_t frac_part,
				  void *priv);
extern void deserialize_dump_uint(struct cpu_regfield_int const *fld,
				  unsigned int val, void *priv);
extern void deserialize_dump_sint(struct cpu_regfield_int const *fld,
				  signed int val, void *priv);
extern void deserialize_dump_reserved(struct cpu_regfield_reserved const *fld,
				      reg_t v, void *priv);

inline static size_t deserialize_get_sz(void const *start, size_t len,
					void const *end)
{
	if (!end)
		return len;
	else
		return (uintptr_t)(end) - (uintptr_t)(start);
}

bool deserialize_cpu_units(struct cpu_unit **unit, size_t *cnt,
			   void const **buf, size_t *sz);

void deserialize_decode_reg(struct cpu_register const *reg,
			    uintmax_t val, void *priv);

bool deserialize_decode(struct cpu_unit const units[], size_t unit_cnt,
			uintptr_t addr, uintmax_t val, void *priv);

int deserialize_decode_range(struct cpu_unit const units[], size_t unit_cnt,
			     uintptr_t start_addr, uintptr_t end_addr,
			     void (*print_fn)(struct cpu_register const *reg,
					      uintmax_t val, void *priv),
			     int (*read_fn)(uintptr_t addr, unsigned int num_bits,
					    uintmax_t *val, void *priv),
			     void *priv);

#endif	/* H_ELITO_DECODER_DESERIALIZER_H */
