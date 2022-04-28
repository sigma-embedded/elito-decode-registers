/*	--*- c -*--
 * Copyright (C) 2022 Enrico Scholz <enrico.scholz@sigma-chemnitz.de>
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

#define _GNU_SOURCE	1

#include "../lib/compat.h"

#include <assert.h>
#include <stdint.h>
#include <stdbool.h>

static void test0(void)
{
	int		r_i;
	unsigned int	r_u;
	intmax_t	r_mi;
	signed char	r_c;

	assert(!_builtin_add_overflow_fallback(1, 2, &r_i) && r_i == 3);
	assert(!_builtin_add_overflow_fallback(1, 2, &r_u) && r_u == 3);

	assert(!_builtin_add_overflow_fallback(1, -2, &r_i) && r_i == -1);
	assert( _builtin_add_overflow_fallback(1, -2, &r_u));

	assert( _builtin_add_overflow_fallback(INTMAX_MIN + 1, -2, &r_mi));
	assert(!_builtin_add_overflow_fallback(INTMAX_MIN + 2, -2, &r_mi));
	assert( _builtin_add_overflow_fallback(INTMAX_MAX - 1, +2, &r_mi));
	assert(!_builtin_add_overflow_fallback(INTMAX_MAX - 2, +2, &r_mi));

	assert(_builtin_add_overflow_fallback(250, 250, &r_c));
	assert(_builtin_add_overflow_fallback(127,   1, &r_c));
}

int main(void)
{
	test0();
}
