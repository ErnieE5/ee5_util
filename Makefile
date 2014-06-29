target=\
$(if $(wildcard  $(2)Makefile.root),\
 $(eval include  $(2)Makefile.root) \
 $(eval include  $(BUILD_TOOLS)utilities.mk),\
 $(if $(subst /,,$(realpath ../$(2))),\
  $(call $(0),$(1),../$(2)),\
  $(error Makefile.root not found.)))
$(call target)
$(call start_point)
