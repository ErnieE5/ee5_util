#--------------------------------------------------------------------------------------------------
# Copyright (C) 2014 Ernest R. Ewert
# 
# Feel free to use this as you see fit. 
# I ask that you keep my name with the code.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
# IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
# OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
# ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
# OTHER DEALINGS IN THE SOFTWARE.
#
#
#	This is a "master" control make file. The following variables can be found
#	in the "root" makefile file and some are required.
#
#	SOURCES
#		A space separated list of source files.  At this time .cpp is expected.
#
#	TARGET
#		The target of the output.  ~Currently~ this target is only an "executable"
#		and not a shared or static library.
#
#	DEFINES
#		A space separated list of -D values provided to the compiler.
#
#	INCLUDE_PATHS
#		A space separated list of include directories that are added as -I values
#		for inclusion.
#
#	LIBRARY_PATHS
#		A list of paths that are added as -L options.
#
#	LIBRARIES
#		A list of libs that are added as -l options.
#
#	WARNINGS
#		An optional list of -W options.
#
#	FARGUMENTS
#		A space separated list of any -f options to send to the compiler.
#
#	CPP_11_OFF
#		Set to a non-empty value to turn C++ 11 support OFF.
#
# Notes:
#
#   Variable names in this file try to follow these conventions.
#
# 		C_ 		Compile macros
# 		L_ 		Link Macros
# 		O_ 		Output (Intermediary and Target) macros
#
# 		cXX  	Color variables (from utilities.mk)
#
.PHONY: usage
usage:
	@printf "\n"
	@printf "$(cER)A build target is required when building with clang.mk...\n\n"
	@printf "$(cIM)Try:\n"
	@printf "$(cTS)\tmake Debug   | debug   | dbg | d\n"
	@printf "$(cTS)\tmake Release | release | rel | r\n"
	@printf "$(cTS)\tmake Clean   | clean   | cln | c\n"
	@printf "\n"

include utilities.mk


#=======================================================================================================================
#
# Default settings based on optional override variables.
#
ARCHITECTURES   ?= x86_64
O_TYPE          ?= Unknown
O_ARCH          ?= x86_64
OPTIMIZE        ?= 0
WARNINGS        ?= all
BUILDS          ?= Debug Release
AR              ?= llvm-ar
RANLIB          ?= llvm-ranlib

O_INTER:=$(O_ARCH)/$(O_TYPE)

#=======================================================================================================================
# Diagnostics or setup for missing required values.
#
# TODO: Need to fix multi ARCH
#
ifeq ($(strip $(ARCHITECTURES)),)
	$(error You must specify a single ARCH ~for now~.)
endif


ifneq ($(strip $(TARGET)),)
	O_TARG:=$(O_INTER)/$(TARGET)
else
	$(error You must specify a TARGET value.)
endif

ifeq ($(strip $(SOURCES)),)
	$(error You must specify a set of SOURCES.)
endif

#=======================================================================================================================
#
# "Standard" include locations...
#
INCLUDE_PATHS+=\
    /usr/include \
    /usr/local/include

#=======================================================================================================================
#
# TODO: Make the build version real
#
DEFINES+=\
	BUILD_VERSION=$(shell date -u +"%F_%T")

ifeq ($(O_TYPE),Release)
DEFINES+= \
	NDEBUG=1
endif



#=======================================================================================================================
#
# Expansion for any -f arguments.
#
FARGUMENTS+= message-length=0

# We need to "force" the "pretty" colors if selected because of the
# redirections of stdout to logs.
#
ifneq ($(strip $(color)),)
FARGUMENTS+= color-diagnostics
endif

#=======================================================================================================================
#
# Compiler arguments
#
C_I := $(INCLUDE_PATHS:%=-I%)
C_W := $(WARNINGS:%=-W%)
C_D := $(DEFINES:%=-D%)
C_F := $(FARGUMENTS:%=-f%)

BC_FILES:=$(SOURCES:%.cpp=${O_INTER}/%.bc)

#
# This file is setup to default to emitting byte code.
#
C_FLAGS:=-emit-llvm -c

#
# Default to C++ 11 unless explicitly disabled.
#
ifeq ($(strip $(CPP_11_OFF)),)
C_STD:= -std=c++11
endif

C_FLAGS+= -O$(OPTIMIZE) $(SYMBOLS)

CXX:=clang++ $(C_STD)

#=======================================================================================================================
#
# Linker arguments
#
L_SEARCH:=$(LIBRARY_PATHS:%=-L%)
L_LIBRARIES:=$(LIBRARIES:%=-l%)

#=======================================================================================================================
#
# Create list of "sub make" includes for tracking dependency checking of headers.
#
D_FILES:=$(SOURCES:%.cpp=${O_INTER}/%.d)



#=======================================================================================================================
# Target helpers
#
#	This section contains helpers that control the build.
#

#
# Generate the included and "magically" created target and dependency
# rules.
#
define GEN_PREREQUISITES
	$(call LOG_COMMAND,$(CXX) -MM $(C_I) $< | sed 's~\(.*\)\.o[ :]*~${O_INTER}/\1.bc $@: ~g' > $@)
endef




#=======================================================================================================================
#
# Targets
#

#
# Build all build types and architectures
# ---------------------------------------
#
.PHONY: All all a
All all a:
	$(foreach b,$(BUILDS),$(call SUB_MAKE,$(b))${NL})
	@echo # Intentional White Space
#
# Re-entry rule for build targets
# -------------------------------
#
.PHONY: doit
doit: $(O_TARG)

#
# Build debug target using a sub-build
# ------------------------------------
#
.PHONY: Debug debug dbg d
Debug debug dbg d:
	@echo # Intentional White Space
	$(call PRINT_MESSAGE,$(cIM),Debug build begin.)
	$(call SUB_MAKE     ,doit O_ARCH=$(O_ARCH) O_TYPE=Debug SYMBOLS=-g)
	$(call PRINT_MESSAGE,$(cNO),Debug build finished.)

#
# Build release target using a sub-build
# --------------------------------------
#
.PHONY: Release release rel r
Release release rel r:
	@echo # Intentional White Space
	$(call PRINT_MESSAGE,$(cIM),Release build begin.)
	$(call SUB_MAKE     ,doit O_ARCH=$(O_ARCH) O_TYPE=Release OPTIMIZE=3)
	$(call PRINT_MESSAGE,$(cNO),Release build finished.)

#
# Clean targets for both release and debug.
# -----------------------------------------
#
.PHONY: Clean clean cln c
Clean clean cln c:
	$(foreach a, $(ARCHITECTURES),                        \
		$(if $(call dir_exists,$(a)),                     \
			$(foreach b,$(BUILDS),                        \
				$(if $(call dir_exists,$(a)/$(b)),        \
					$(call CLEAN_BUILD,$(a)/$(b))) ${NL}  \
	         )                                            \
			$(call RM,$(a)) ${NL}                         \
		)                                                 \
	)
	$(call PRINT_MESSAGE,$(cNO),Clean Finished.)

#
# Compile a cpp source file into LLVM byte-code file
# --------------------------------------------------
#
$(O_INTER)/%.bc: %.cpp
	$(call RUN_COMMAND,$(CXX) $(C_FLAGS) $(C_D) $(C_W) $(C_I) $(C_F) -o $@ $<)

#
# Link the target from all of the byte-code files
# -----------------------------------------------
#
$(O_TARG): $(BC_FILES)
	$(call RUN_COMMAND,$(CXX) $(SYMBOLS) $(BC_FILES) -o $(O_TARG) $(L_SEARCH) $(L_LIBRARIES))

#=======================================================================================================================
#
# Auto generated .d make rules
#
#

#
# Rule for creating sub-make .d rule files for include header change tracking
# ---------------------------------------------------------------------------
#
$(D_FILES): $(SOURCES)
	@$(if $(findstring Unknown,$(O_INTER)),echo -n,$(call verify_directory,$(O_INTER)))
	@$(if $(findstring Unknown,$(O_INTER)),echo -n,$(GEN_PREREQUISITES))


# Expanding D_FILES has ~something~ to do with the
# include of them and the MAKECMDGOALS.
#
__UNUSED__=$(D_FILES)
#
ifneq (,$(findstring clean,$(MAKECMDGOALS)))
# Don't include if we are cleaning
else ifneq (,$(findstring wipe,$(MAKECMDGOALS)))
# Don't include if we are cleaning
else
	-include $(D_FILES)
endif

