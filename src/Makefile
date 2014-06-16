

TARGET:=ee5_util
COMPILER=clang++

LIBRARY_MAKE=dynamic static
LIBRARY_VER=0.0.1
#LIBRARY_MAKE=static

SOURCES:=\
    console_logger.cpp\
    error.cpp\
    system.cpp\
    threadpool.cpp\
    workthread.cpp

SYMBOLS=1

FARGUMENTS:= \
	error-limit=5\
	no-exceptions


LIBS+=\
	pthread \


default:
	$(call SUB_MAKE,a)

target_sup=\
$(if $(wildcard $(2)Makefile.root),\
$(eval include  $(2)Makefile.root) \
$(eval include  $(BUILD_TOOLS)$(strip $(1))),\
$(if $(subst /,,$(realpath ../$(2))),\
$(call $(0),$(1),../$(2)),\
$(error Makefile.root not found.)))
$(call target_sup, $(COMPILER).mk )
