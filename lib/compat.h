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

#ifndef H_ELITO_DECODER_COMPAT_H
#define H_ELITO_DECODER_COMPAT_H

#ifdef __has_builtin
#  if __has_builtin(__builtin_add_overflow)
#    define HAVE__builtin_add_overflow
#  endif
#endif

#define _safe_assign(_dst, _src) __extension__		\
	({						\
		__typeof__(_src)	src = (_src);	\
		__typeof__(*(_dst))	dst = src;	\
							\
		*(_dst) = dst;				\
		(__typeof__(src))(dst) == src;		\
	})

#define _builtin_add_overflow_fallback(_a, _b, _c)	\
	__extension__					\
	({						\
		intmax_t	a = (_a);		\
		intmax_t	b = (_b);		\
		bool		res;			\
							\
		if (b < 0 && INTMAX_MIN - b > a)	\
			res = true;			\
		else if (b >= 0 && a > INTMAX_MAX - b)	\
			res = true;			\
		else					\
			res = !_safe_assign(_c, a + b);	\
							\
		res;					\
	})

#ifndef HAVE__builtin_add_overflow
#  warning Providing legacy __builtin_add_overflow
#  include <stdint.h>
#  include <stdbool.h>
#  define __builtin_add_overflow(_a, _b, _c) \
	_builtin_add_overflow_fallback(_a, _b, _c)
#endif

#endif	/* H_ELITO_DECODER_COMPAT_H */
