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

#-----------------------------------------------------------------------------------------------------------------------
#
# Default color settings
#
cTS ?= $(call c,7)#   Time stamp color
cST ?= $(call c,33)#  Source Transform color
cAR ?= $(call c,39)#  Arrow color
cDT ?= $(call c,45)#  Destination Transform color
cIM ?= $(call c,202)# Message color
cNO ?= $(call c,2)#   No Error Message color
cER ?= $(call c,196)# Error Message color

#
# Create the ANSI escape sequence passed as a value.
#
define x
$(if $(color),\033[$(1)m)
endef

#
# Create the escaped text sequence for ANSI colors from the
# 0-255 value passed as an "argument."
#
define c
$(if $(color),\033[38;5;$(1)m)
endef

#
# This weird declaration is used to create a newline in a macro expansion.
#
define NL


endef

# This macro expands to multiple calls to rm with the verbosity and
# force flags set. It will remove all of the intermediary files one
# by one so that a "nice log" of the action is made and we can't
# exceed the maximum command line for removal of files
#
# Each executed line should evaluate to something like this :
#
#	rm -fv file.xx | tee -a build.log
#
define CLEANUP
	$(patsubst %,@rm -fv % | tee -a build.log${NL},$(O_TARG) $(C_FILES) $(D_FILES))
endef

#
#
#
define verify_directory
	[ ! -d $(1) ] && \
	{ \
		echo "[$(TIMESTAMP)] mkdir -vp $(1)" >> build.log; \
		mkdir -vp $(1) 2>&1 >> build.log; \
	}; exit 0
endef

#
#
#
define dir_exists
$(shell [ -d $(1) ] && { echo y; })
endef

#
# Name says it all...
#
define TIMESTAMP
$(shell date +"%F %T")
endef

#
# The ~most~ important part of the the build is any errors.  However, the
# massive number of arguments that are needed makes the output hard to
# filter.
#
define PRINT_TRANSITION
	@printf "[$(TIMESTAMP)] $< --> $@\n" >> build.log
	@printf "$(cST)%s $(cAR)--> $(cDT)%s$(call x,0)\n" "$<" "$@"
endef

#
# Print a time stamp to the screen.
#
define PRINT_TIMESTAMP
	@printf "$(cTS)[$(TIMESTAMP)] "
endef

#
# Print a message with a time stamp to the screen and log the
# output to the log file as well.
#
define PRINT_MESSAGE
	@printf "[$(TIMESTAMP)] $(2)\n" >> build.log
	@$(PRINT_TIMESTAMP)
	@printf "$(1)%s$(call x,0)\n" "$(2)"
endef

#
# Create a log entry with a time stamp of the command and
# execute that command redirecting any output into the
# log file as well as allowing the command to do its normal
# output to the console.
#
define LOG_COMMAND
	@printf "[$(TIMESTAMP)] $(1)\n" >> build.log
	@$(1) 2>&1 | tee -a build.log$(2)$(3)
endef

#
# Run a command with "fancy" abbreviated output to the console.
# The "intent" of the 'recipe' is displayed in color with a time
# stamp, and then the command is passed into the logging actions.
#
define RUN_COMMAND
	$(PRINT_TIMESTAMP)
	$(PRINT_TRANSITION)
	$(call LOG_COMMAND,$(1))
endef

#
# Call make recursively, logged.
#
define SUB_MAKE
	@printf "[$(TIMESTAMP)] $(MAKE) -s $(1)\n" >> build.log;
	@$(MAKE) -s $(1);
endef

#
# Remove a directory with logging
#
define RM
	$(call LOG_COMMAND  ,rm --verbose --recursive $(1),>,/dev/null)
endef

#
# Cleanup the intermediary directory and remove it when done.
#
define CLEAN_BUILD
	$(call PRINT_MESSAGE,$(cIM),Clean $(abspath $(1)))
	$(call SUB_MAKE     ,wipe O_TYPE=$(1))
	$(call RM           ,$(1))
endef

#-----------------------------------------------------------------------------------------------------------------------
#
# "Standard" Targets
#

# This is the "worker" for the clean stage. See comment on
# the CLEANUP "expansion."
.PHONY: wipe
wipe:
	@$(CLEANUP)
