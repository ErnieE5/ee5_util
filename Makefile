
DIRS:=src tests

GOALS=r

default:
	@$(call RM,$(BL)) ${NL}
	@$(foreach d,$(DIRS),cd $(d) && $(MAKE) -S -s $(GOALS) && cd ..;)


.PHONY: Debug debug dbg d
Debug debug dbg d:
	@$(call RM,$(BL)) ${NL}
	@$(foreach d,$(DIRS),cd $(d) && $(MAKE) -S -s debug && cd ..;)

.PHONY: Release release rel r
Release release rel r:
	@$(call RM,$(BL)) ${NL}
	@$(foreach d,$(DIRS),cd $(d) && $(MAKE) -S -s release && cd ..;)

.PHONY: Clean clean cln c
Clean clean cln c:
	@$(call RM,$(BL)) ${NL}
	@$(foreach d,$(DIRS),cd $(d) && $(MAKE) -S -s clean && cd ..;)


target_sup=\
$(if $(wildcard $(2)Makefile.root),\
$(eval include  $(2)Makefile.root) \
$(eval include  $(BUILD_TOOLS)$(strip $(1))),\
$(if $(subst /,,$(realpath ../$(2))),\
$(call $(0),$(1),../$(2)),\
$(error Makefile.root not found.)))



