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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#define _GNU_SOURCE

#include <stdlib.h>
#include <setjmp.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

#define STORE_BE	0

static void		*buf;
static void		*out_ptr;
static void const	*end_ptr;
static jmp_buf		alloc_jmp;


static void check_space(size_t sz)
{
	if ((size_t)(end_ptr - out_ptr) < sz)
		longjmp(alloc_jmp, 0);
}

static void push_u8(uint8_t v)
{
	check_space(sizeof v);
	out_ptr = mempcpy(out_ptr, &v, sizeof v);
}

static void push_u16(uint16_t v)
{
	if (STORE_BE)
		v = htobe16(v);
	else
		v = htole16(v);

	check_space(sizeof v);
	out_ptr = mempcpy(out_ptr, &v, sizeof v);
}

static void push_u32(uint32_t v)
{
	if (STORE_BE)
		v = htobe32(v);
	else
		v = htole32(v);

	check_space(sizeof v);
	out_ptr = mempcpy(out_ptr, &v, sizeof v);
}

static void push_data(size_t sz, void const *data)
{
	push_u32(sz);
	check_space(sz);
	out_ptr = mempcpy(out_ptr, data, sz);
}

static void do_fill(void)
{
	(void)push_data;
	(void)push_u32;
	(void)push_u16;
	(void)push_u8;

#include "registers.h"	
}

int main(void)
{
	size_t		l = 1; // 64*1024;

	for (;;) {
		void *	tmp;
		
		tmp = realloc(buf, l);
		if (!tmp)
			abort();

		buf = tmp;
		out_ptr = buf;
		end_ptr = buf + l;
	
		if (setjmp(alloc_jmp) > 0) {
			l *= 2;
			continue;
		}

		do_fill();
		break;
	}

	l = out_ptr - buf;
	write(1, buf, l);
}
