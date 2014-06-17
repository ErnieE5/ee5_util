
DIRS:=src tests

GOALS=r

default:
	@$(call RM,$(BL)) ${NL}
	@$(foreach d,$(DIRS),cd $(d) && $(MAKE) -S -s $(GOALS) && cd ..;)

target_sup=\
$(if $(wildcard $(2)Makefile.root),\
$(eval include  $(2)Makefile.root) \
$(eval include  $(BUILD_TOOLS)$(strip $(1))),\
$(if $(subst /,,$(realpath ../$(2))),\
$(call $(0),$(1),../$(2)),\
$(error Makefile.root not found.)))



