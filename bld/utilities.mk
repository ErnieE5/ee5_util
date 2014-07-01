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

#--------------------------------------------------------------------------------------------------
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
cRO ?= $(call c,255)# Final Output

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

BL?=$(BUILD_LOGS)build.log

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
	$(patsubst %,@rm -fv % >> $(BL)${NL},$(ALL_CREATED_FILES ) )
endef


#
#
#
define verify_directory
	@[ ! -d $(1) ] && \
	{ \
		echo "[$(TIMESTAMP)] mkdir -vp $(1)" >> $(BL); \
		mkdir -vp $(1) 2>&1 >> $(BL); \
	}; exit 0
endef

#
#
#
define file_exists
$(shell [ -s $(1) ] && { echo y; })
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

define FIX
$(addsuffix $(suffix $(<)),$(basename $(@F)) )
endef

#
# The ~most~ important part of the the build is any errors.  However, the
# massive number of arguments that are needed makes the output hard to
# filter.
#
define PRINT_TRANSITION
	@printf "[$(TIMESTAMP)] $(1) --> $(2)\n" >> $(BL)
	@printf "$(cST)%s $(cAR)--> $(cDT)%s$(call x,0)\n" "$(1)" "$(2)"
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
	@printf "[$(TIMESTAMP)] $(2)\n" >> $(BL)
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
	@printf "[$(TIMESTAMP)] $(1)\n" >> $(BL)
	@$(1) 2>&1 | tee -a $(BL)$(2)$(3)
endef

#
# Run a command with "fancy" abbreviated output to the console.
# The "intent" of the 'recipe' is displayed in color with a time
# stamp, and then the command is passed into the logging actions.
#
define RUN_COMMAND
	$(PRINT_TIMESTAMP)
	$(call PRINT_TRANSITION,$(3),$(2))
	$(call LOG_COMMAND,$(1),,)
endef


#
# Run a command with "fancy" abbreviated output to the console.
# The "intent" of the 'recipe' is displayed in color with a time
# stamp, and then the command is passed into the logging actions.
#
define MAKE_TARGET
	$(PRINT_TIMESTAMP)
	@printf "[$(TIMESTAMP)] $< --> $@\n" >> $(BL)
	@printf "$(cAR)--> $(cRO)%s$(call x,0)\n" "$@"
	$(call LOG_COMMAND,$(1),,)
endef


#
# Call make recursively, logged.
#
define SUB_MAKE
	@printf "[$(TIMESTAMP)] $(MAKE) -s $(1)\n" >> $(BL);
	@$(MAKE) --stop --silent $(1);
endef

#
# Remove a directory with logging
#
define RM
	$(call LOG_COMMAND  ,rm --verbose --recursive $(1),>,/dev/null)
endef

define REMOVE_TARGET_DIR
	$(if $(call dir_exists,$(call strip,$(1))), $(call PRINT_MESSAGE,$(cDT),removing $(1) ) )
	$(if $(call dir_exists,$(call strip,$(1))), $(call RM ,$(1) ) )
endef

#
# Cleanup the intermediary directory and remove it when done.
#
define CLEAN_BUILD
	$(call PRINT_MESSAGE,$(cDT),removing $(O_OBJ)/$(1)_$(2))
	$(call SUB_MAKE     ,wipe O_TYPE=$(2))
	$(call RM           ,$(O_OBJ)/$(1)_$(2))
	$(call REMOVE_TARGET_DIR,$(BUILD_ROOT)bin/$(1)_$(2))
	$(call REMOVE_TARGET_DIR,$(BUILD_ROOT)lib/$(1)_$(2))

endef


#
# Make a sub directory
#
define MAKE_DIR
	@$(foreach t,$(2),$(MAKE) --directory=$(1) --stop --silent $(t)) ${NL}
endef

#
# Make all sub directories in list
#
define MAKE_DIRS
	$(foreach d,$(1),$(call MAKE_DIR,$(d),$(2)))
	@echo # Intentional White Space
endef


define start_point
$(if $(call file_exists,dirs),\
  $(eval include dirs)\
)\
$(if $(DIRS),\
  $(eval include $(BUILD_TOOLS)dirs.mk),\
  $(if $(call file_exists,sources),\
    $(eval include sources)\
  )\
)\
$(if $(COMPILER),\
  $(eval include $(BUILD_TOOLS)$(COMPILER).mk)\
)
endef

.PHONY: util_default
util_default:
	@printf "\n"
	@printf "$(cIM)Try:\n"
	@printf "$(cTS)\tmake Debug   | debug   | dbg | d\n"
	@printf "$(cTS)\tmake Release | release | rel | r\n"
	@printf "$(cTS)\tmake Clean   | clean   | cln | c\n"
	@printf "\n"

# This is the "worker" for the clean stage. See comment on
# the CLEANUP "expansion."
.PHONY: wipe
wipe:
	@$(CLEANUP)

.PHONY: delete_log
delete_log:
	@$(shell rm -f $(BL))

.PHONY: fresh_log
fresh_log: delete_log
	@printf "[$(TIMESTAMP)] Begin build log.\n" >> $(BL)
