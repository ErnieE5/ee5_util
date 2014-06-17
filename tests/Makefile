

COMPILER=clang++

TESTS:=\
    command_line\
    atomic_stack\
    delegate\
    spin_locks\
    memory_pools\
    threading\
    stopwatch\

SOURCES:=\
    main.cpp\
    $(TESTS:%=tst_%.cpp)

TARGET:=ee5util_ft

SYMBOLS=1

LIBS+=\
	ee5_util

LIB_PATHS+=\
	$(L_BIN)

default:
	$(call SUB_MAKE,all)

target_sup=\
$(if $(wildcard $(2)Makefile.root),\
 $(eval include  $(2)Makefile.root) \
 $(eval include  $(BUILD_TOOLS)$(strip $(1))),\
 $(if $(subst /,,$(realpath ../$(2))),\
  $(call $(0),$(1),../$(2)),\
  $(error Makefile.root not found.)))
$(call target_sup, $(COMPILER).mk )
