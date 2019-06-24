/*	--*- c -*--
 * Copyright (C) 2018 Enrico Scholz <enrico.scholz@sigma-chemnitz.de>
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

#ifndef H_DECODE_MX8_AR052X_DESERIALIZE_H
#define H_DECODE_MX8_AR052X_DESERIALIZE_H

#include <stdbool.h>

#define STR_FMT		".*s"
#define STR_ARG(_s)	(int)((_s)->len), (_s)->data

void col_printf(char const *fmt, ...) __attribute__((__format__(gnu_printf, 1,2)));
char const	*col_boolstr(bool v) __attribute__((__pure__));
void		col_init(int);

struct string;
extern char *string_to_c(struct string const *);

#endif	/* H_DECODE_MX8_AR052X_DESERIALIZE_H */
