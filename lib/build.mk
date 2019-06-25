REGISTERS_GENDESC ?= decode-registers-gendesc
REGISTERS_GENDESC_FLAGS ?=
REGISTERS_DEFDIR  ?= ${abs_srcdir}/regs-$*

run_gendesc = \
	${REGISTERS_GENDESC} -D '$*' \
	${REGISTERS_GENDESC_FLAGS} ${REGISTERS_GENDESC_FLAGS_$*} \
	$1 $@ \
	$(if ${REGISTERS_DEFDIR_$*},${REGISTERS_DEFDIR_$*},${REGISTERS_DEFDIR})  \

define _set_dev_type
clean:	.clean-$1
.clean-$1:.clean-%:
	rm -f registers-$$*.inc.h symbols-$$*.h decode-$$* regstream-$$*.bin

.prepare-$1:
endef

set_dev_type =	$(eval $(call _set_dev_type,$1,$2))

registers-%.inc.h:	.prepare-%
	$(call run_gendesc,--datastream-c)

symbols-%.h:		.prepare-%
	$(call run_gendesc,--c-define)

regstream-%.bin:	.prepare-%
	$(call run_gendesc,--datastream)
