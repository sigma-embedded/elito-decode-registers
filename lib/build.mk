REGISTERS_GENDESC ?= decode-registers-gendesc
REGISTERS_GENDESC_FLAGS ?=
REGISTERS_ADDR_TYPE ?= uint16_t
REGISTERS_DEFDIR  ?= ${abs_srcdir}/regs-$*

_decode_mkdir := $(dir $(abspath ${lastword ${MAKEFILE_LIST}}))
decode_mkdir  ?= ${_decode_mkdir}
decode_srcdir ?= $(abspath ${_decode_mkdir}/../c)

decode_SOURCES = \
	${decode_srcdir}/deserialize.c \
	${decode_srcdir}/deserialize.h \
	${decode_srcdir}/common.c \
	${decode_srcdir}/common.h \

decode_CFLAGS = \
	-I . \
	-I ${decode_srcdir} \
	-D DESERIALIZE_SYMBOLS='"$(filter %symbols-$*.h,$^)"' \
	-D REGISTERS_DEFS_H='"$(filter %registers-$*.inc.h,$^)"' \
	-D REGISTER_ADDR_TYPE='${REGISTERS_ADDR_TYPE}' \

run_gendesc = \
	${REGISTERS_GENDESC} -D '$*' \
	${REGISTERS_GENDESC_FLAGS} ${REGISTERS_GENDESC_FLAGS_$*} \
	$1 $@ \
	$(if ${REGISTERS_DEFDIR_$*},${REGISTERS_DEFDIR_$*},${REGISTERS_DEFDIR})  \

run_build_decode = ${CC} -o $@ \
	${decode_CFLAGS} ${CPPFLAGS_$@} ${AM_CFLAGS} ${CFLAGS} $(filter %.c,$^) ${LDFLAGS} ${LIBS}

define _set_dev_type
decode-$1:decode-%:	$${decode_srcdir}/decode-$2.c $${decode_SOURCES} registers-%.inc.h symbols-%.h regstream-%.bin
	$$(call run_build_decode)

clean:	.clean-$1
.clean-$1:.clean-%:
	rm -f registers-$$*.inc.h symbols-$$*.h decode-$$* regstream-$$*.bin

endef

set_dev_type =	$(eval $(call _set_dev_type,$1,$2))

registers-%.inc.h:
	$(call run_gendesc,--datastream-c)

symbols-%.h:
	$(call run_gendesc,--c-define)

regstream-%.bin:
	$(call run_gendesc,--datastream)
