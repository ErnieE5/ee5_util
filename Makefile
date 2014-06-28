
DIRS:=src tests


default: Debug


.PHONY: Debug debug dbg d
Debug debug dbg d: fresh_log
	$(call MAKE_DIRS, $(DIRS), debug)

.PHONY: Release release rel r
Release release rel r: fresh_log
	$(call MAKE_DIRS, $(DIRS), release)

.PHONY: Clean clean cln c
Clean clean cln c: fresh_log
	$(call MAKE_DIRS, $(DIRS), clean)


target_sup=\
$(if $(wildcard $(2)Makefile.root),\
$(eval include  $(2)Makefile.root) \
$(eval include  $(BUILD_TOOLS)$(strip $(1))),\
$(if $(subst /,,$(realpath ../$(2))),\
$(call $(0),$(1),../$(2)),\
$(error Makefile.root not found.)))
$(call target_sup, utilities.mk)