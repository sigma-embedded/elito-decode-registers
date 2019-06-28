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

PYTHON3 ?=	/usr/bin/python3
CHECKER ?=
VALGRIND ?=	valgrind --quiet --tool=memcheck --leak-check=full --show-leak-kinds=all --error-exitcode=23

AM_CFLAGS =	-std=gnu11 -Wall -W -pedantic -Wno-pointer-arith -Wno-unused-parameter -I ../lib
AM_LDFLAGS =	-Wl,-as-needed
CFLAGS ?=	-O1 -g3 -Werror -D_FORTIFY_SOURCE=2 -fstack-protector
LDFLAGS ?=

test_PROGRAMS = \
  test-deserialize

test-deserialize_SOURCES = \
  test-deserialize.c \
  ../lib/deserialize.c \
  ../lib/deserialize.h

TEST_test-deserialize_DEFS = data-0

TEST_OUTPUT = $(if ${TEST_VERBOSE},,> /dev/null)

define build_sym
$(CC) $(AM_CFLAGS) $(CFLAGS) ${AM_LDFLAGS} ${LDFLAGS} -DDESERIALIZE_SYMBOLS=\"$(abspath $(filter %_symbols.h,$^))\" $(filter %.c,$^) -o $@ ${LIBS}
endef

all:	${test_PROGRAMS}

tests:	all .run-tests

clean:
	rm -f ${test_PROGRAMS}
	rm -f *.bin *.tmp *.gcda *.gcno
	rm -f ${addsuffix _fill.c,${test_PROGRAMS}}
	rm -f ${addsuffix _stream.h,${test_PROGRAMS}}
	rm -f ${addsuffix _symbols.h,${test_PROGRAMS}}

test-deserialize:	${test-deserialize_SOURCES} test-deserialize_symbols.h test-deserialize_stream.h
	$(call build_sym)

%_symbols.h %_fill.c %_stream.bin %_stream.h:	.gendesc-%.stamp
	@:

.gendesc-%.stamp:	../src/gendesc Makefile
	rm -f $*_*.tmp $*_symbols.h $*_fill.c $*_stream.bin $*_stream.h
	$(PYTHON3) $< --c-defines $*_symbols.h.tmp --c-fill $*_fill.c.tmp --datastream $*_stream.bin.tmp --datastream-c $*_stream.h.tmp ${TEST_$*_DEFS}
	mv $*_symbols.h.tmp $*_symbols.h
	mv $*_fill.c.tmp $*_fill.c
	mv $*_stream.bin.tmp $*_stream.bin
	mv $*_stream.h.tmp $*_stream.h
	@touch $@

.run-tests:	..run-test-deserialize ..run-test-decode

..run-test-decode:	../decode-device test-deserialize_stream.bin FORCE
	${CHECKER} $(abspath $<) --help $(TEST_OUTPUT)
	${CHECKER} $(abspath $<) --version $(TEST_OUTPUT)
	${CHECKER} $(abspath $<) --type emu --definitions $(filter %.bin,$^)  ${TEST_OUTPUT}
	${CHECKER} $(abspath $<) --type emu --definitions $(filter %.bin,$^) -C ${TEST_OUTPUT}
	${CHECKER} $(abspath $<) --type emu --definitions $(filter %.bin,$^) @Test-0       ${TEST_OUTPUT}
	! ${CHECKER} $(abspath $<) --type emu --definitions $(filter %.bin,$^) @no-match   2>/dev/null
	${CHECKER} $(abspath $<) --type emu --definitions $(filter %.bin,$^) @Test-0 IRQ\* ${TEST_OUTPUT}
	! ${CHECKER} $(abspath $<) --type emu --definitions $(filter %.bin,$^) @Test-0 no-match 2>/dev/null

..run-test-deserialize:	test-deserialize FORCE
	env -u TEST_HEAP_ALLOC ${CHECKER} $(abspath $<) ${TEST_OUTPUT}
	env TEST_HEAP_ALLOC=1  ${CHECKER} $(abspath $<) ${TEST_OUTPUT}

FORCE:
.PHONY:		FORCE
