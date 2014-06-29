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
ifneq (,$(DIRS))

ifneq (,$(SOURCES))
$(error DIRS and SOURCES should not be in the same directory.)
endif

.PHONY: Debug debug dbg d
Debug debug dbg d: fresh_log
	$(call MAKE_DIRS,$(DIRS),debug)

.PHONY: Release release rel r
Release release rel r: fresh_log
	$(call MAKE_DIRS,$(DIRS),release)

.PHONY: Clean clean cln c
Clean clean cln c: fresh_log
	$(call MAKE_DIRS,$(DIRS),clean)

endif








