# Copyright (C) 2015 Enrico Scholz <enrico.scholz@ensc.de>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 3 of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

PYTHON3 =	python3
XXD =		xxd

AM_CFLAGS =	-std=gnu11 -Wall -W -pedantic -Wno-pointer-arith -I ../lib
CFLAGS =	-Werror -O0 -g3 -D_XFORTIFY_SOURCE=2

test_PROGRAMS = \
  test-deserialize

test-deserialize_SOURCES = \
  test-deserialize.c \
  ../lib/deserialize.c \
  ../lib/deserialize.h

TEST_test-deserialize_DEFS = data-0

define build_sym
$(CC) $(AM_CFLAGS) $(CFLAGS) -DDESERIALIZE_SYMBOLS=\"$(abspath $(filter %_symbols.h,$^))\" $(filter %.c,$^) -o $@ ${LIBS}
endef

all:	${test_PROGRAMS}

clean:
	rm -f ${test_PROGRAMS}
	rm -f *.bin *.tmp
	rm -f ${addsuffix _fill.c,${test_PROGRAMS}}
	rm -f ${addsuffix _stream.h,${test_PROGRAMS}}
	rm -f ${addsuffix _symbols.h,${test_PROGRAMS}}

test-deserialize:	${test-deserialize_SOURCES} test-deserialize_symbols.h test-deserialize_stream.h
	$(call build_sym)

%_symbols.h %_fill.c %_stream.bin:	.gendesc-%.stamp
	@:

.gendesc-%.stamp:	../gendesc Makefile
	rm -f $*_*.tmp $*_symbols.h $*_fill.c $*_stream.bin
	$(PYTHON3) $< --c-defines $*_symbols.h.tmp --c-fill $*_fill.c.tmp --datastream $*_stream.bin.tmp ${TEST_$*_DEFS}
	mv $*_symbols.h.tmp $*_symbols.h
	mv $*_fill.c.tmp $*_fill.c
	mv $*_stream.bin.tmp $*_stream.bin
	@touch $@

%_stream.h:	%_stream.bin
	$(XXD) -i < $< > $@
