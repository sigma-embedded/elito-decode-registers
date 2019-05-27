PACKAGE =	decode-registers

bin_SCRIPTS = \
	decode-registers-gendesc

py_DATA = \
	bga.py \
	block.py \
	generator.py \
	generator_cbga.py \
	generator_ccommon.py \
	generator_cdef.py \
	generator_cfill.py \
	generator_stream.py \
	line.py \
	pin.py \
	register.py \
	unit.py \

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

prefix ?= /usr/local
bindir ?= ${prefix}/bin
datadir ?= ${prefix}/data
pkgdatadir ?= ${datadir}/${PACKAGE}
pydir ?= ${pkgdatadir}/py
chdir ?= ${pkgdatadir}/c
mkdir ?= ${pkgdatadir}/mk

SED = sed

INSTALL = install
INSTALL_BIN = ${INSTALL} -p -m 0755
INSTALL_DATA = ${INSTALL} -p -m 0644
INSTALL_D = ${INSTALL} -d -m 0755

SUBDIRS = testsuite

all:	$(bin_SCRIPTS) $(py_DATA) $(ch_DATA)

clean:	.subdir-clean
	rm -rf __pycache__
	rm -f *.pyc
	rm -f decode-registers-gendesc

decode-registers-gendesc:	gendesc Makefile
	@rm -f $@
	$(SED) -e 's!@PYDIR@!$(pydir)!g' $< >$@
	chmod a-r,a+rx $@

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

.install-bin:	${bin_SCRIPTS}
	${INSTALL_D} ${DESTDIR}${bindir}
	${INSTALL} $^ ${DESTDIR}${bindir}/

.subdir-%:
	${MAKE} SUBDIR_TARGET='$*' .run-subdir

.run-subdir:	${addprefix .run-subdir-,${SUBDIRS}}

.run-subdir-%:
	${MAKE} -C '$*' ${SUBDIR_TARGET}
