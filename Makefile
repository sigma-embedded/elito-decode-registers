PACKAGE =	decode-registers

bin_PROGRAMS = \
	decode-device

bin_SCRIPTS = \
	decode-registers-gendesc

py_DATA = \
	src/bga.py \
	src/block.py \
	src/generator.py \
	src/generator_cbga.py \
	src/generator_ccommon.py \
	src/generator_cdef.py \
	src/generator_cfill.py \
	src/generator_stream.py \
	src/line.py \
	src/pin.py \
	src/register.py \
	src/unit.py \

ch_DATA = \
	lib/common.c \
	lib/common.h \
	lib/deserialize.c \
	lib/deserialize.h \
	lib/decode-devmem.c \
	lib/decode-i2c.c \

mk_DATA = \
	lib/cpudef.m4 \
	lib/build.mk \

decode-device_SOURCES = \
	lib/decode-device.c \
	lib/common.c \
	lib/common.h \
	lib/deserialize.c \
	lib/deserialize.h \
	lib/all-symbols.h \

prefix ?= /usr/local
bindir ?= ${prefix}/bin
datadir ?= ${prefix}/data
pkgdatadir ?= ${datadir}/${PACKAGE}
pydir ?= ${pkgdatadir}/py
chdir ?= ${pkgdatadir}/c
mkdir ?= ${pkgdatadir}/mk

CC ?=		gcc
AM_CFLAGS =	-std=gnu11 -Wall -W -Wno-unused-parameter
CFLAGS ?=	-O2 -g3 -Werror -D_FORTIFY_SOURCE=2 -fstack-protector

PYTHON3 ?=	/usr/bin/python3
SED = 		sed

SED_CMD = \
	-e 's!@PYDIR@!$(pydir)!g' \
	-e '1s,^\(\#! *\)/usr/bin/python3,\1${PYTHON3},' \

INSTALL = install
INSTALL_BIN = ${INSTALL} -p -m 0755
INSTALL_DATA = ${INSTALL} -p -m 0644
INSTALL_D = ${INSTALL} -d -m 0755

SUBDIRS = testsuite

all:	$(bin_SCRIPTS) $(bin_PROGRAMS) $(py_DATA) $(ch_DATA)

clean:	.subdir-clean
	rm -rf __pycache__
	rm -f *.pyc
	rm -f decode-registers-gendesc
	rm -f ${bin_PROGRAMS}

decode-registers-gendesc:	src/gendesc Makefile
	@rm -f $@
	$(SED) ${SED_CMD} $< >$@
	chmod a-r,a+rx $@

decode-device:	${decode-device_SOURCES}
	${CC} ${AM_CFLAGS} ${CFLAGS} $(filter %.c,$^) -o $@ -I. -DDESERIALIZE_SYMBOLS='<$(filter %/all-symbols.h,$^)>'

install:	.install-py .install-bin .install-ch .install-mk

.install-py:	${py_DATA}
	${INSTALL_D} ${DESTDIR}${pydir}
	${INSTALL} $^ ${DESTDIR}${pydir}/

.install-ch:	${ch_DATA}
	${INSTALL_D} ${DESTDIR}${chdir}
	${INSTALL} $^ ${DESTDIR}${chdir}/

.install-mk:	${mk_DATA}
	${INSTALL_D} ${DESTDIR}${mkdir}
	${INSTALL} $^ ${DESTDIR}${mkdir}/

.install-bin:	${bin_SCRIPTS} ${bin_PROGRAMS}
	${INSTALL_D} ${DESTDIR}${bindir}
	${INSTALL} $^ ${DESTDIR}${bindir}/

.subdir-%:
	${MAKE} SUBDIR_TARGET='$*' .run-subdir

.run-subdir:	${addprefix .run-subdir-,${SUBDIRS}}

.run-subdir-%:
	${MAKE} -C '$*' ${SUBDIR_TARGET}
