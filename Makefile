
DIRS:=src tests

default:
	@$(foreach d,$(DIRS),cd $(d) && $(MAKE) -S -s && cd ..;)

target_sup=\
$(if $(wildcard $(2)Makefile.root),\
$(eval include  $(2)Makefile.root) \
$(eval include  $(BUILD_TOOLS)$(strip $(1))),\
$(if $(subst /,,$(realpath ../$(2))),\
$(call $(0),$(1),../$(2)),\
$(error Makefile.root not found.)))



