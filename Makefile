# See http://make.mad-scientist.net/papers/advanced-auto-dependency-generation/
# for the template used to start this file
SDIR := src
ODIR := .objs
BDIR := bin
# -- TODO: customize the list below for your project ---
# List of source .c files used with the project
SRCS := $(wildcard $(SDIR)/*.c)

# The aplication generated 
APPNAME := $(BDIR)/server
# -- End of customization section ---

# Replace .c extension on SRCS to get objfiles using gnu make pattern rules and substitution references.
# See https://www.gnu.org/software/make/manual/html_node/Pattern-Intro.html#Pattern-Intro for pattern rules and 
# https://www.gnu.org/software/make/manual/html_node/Substitution-Refs.html#Substitution-Refs for substitution references overview
OBJFILES := $(SRCS:$(SDIR)/%.c=$(ODIR)/%.o)

# Build the app you've specified in APPNAME for the "all" or "default" target
all : dirs $(APPNAME)
default : dirs $(APPNAME)

dirs:
	@mkdir -p $(ODIR)
	@mkdir -p $(BDIR)	 
# Remove all build intermediates and output file
clean : ; @rm -rf $(APPNAME) $(DEPDIR)/*.d $(ODIR)/*.o

# Build the application by running the link step with all objfile inputs
$(APPNAME) : $(OBJFILES)
	$(CXX) $(LDFLAGS) $^ -o $(APPNAME)

# Add all warnings/errors to cflags default.  This is not required but is a best practice
CFLAGS += -std=c++17 -g -Wall -Werror

# The below content is from  http://make.mad-scientist.net/papers/advanced-auto-dependency-generation/
# with the following changes:
#   1) Added comments
#   2) Removed TARGET_ARCH from COMPILE.c since it's no longer listed in the [default rules](https://www.gnu.org/software/make/manual/html_node/Catalogue-of-Rules.html#Catalogue-of-Rules) and [isn't documented](https://lists.gnu.org/archive/html/help-make/2010-06/msg00005.html)
# Original content below is:
# Copyright © 1997-2019 Paul D. Smith Verbatim copying and distribution is permitted in any medium, provided this notice is preserved.

# The directory (hidden) where dependency files will be stored
DEPDIR := .deps
# Flags passed to gcc to automatically build dependencies when compiling
# See https://www.gnu.org/software/make/manual/html_node/Automatic-Variables.html for detail about variable names
# $@ references the target file of the rule and will be "main.o" when compiling "main.c"
# $* references the stem of the rule, and will be "main" when target is "main.o"
DEPFLAGS = -MT $@ -MMD -MP -MF $(DEPDIR)/$*.d

# Rules for compiling a C file, including DEPFLAGS along with Implicit GCC variables.
# See https://www.gnu.org/software/make/manual/html_node/Implicit-Variables.html
# and see https://www.gnu.org/software/make/manual/html_node/Catalogue-of-Rules.html#Catalogue-of-Rules
# for the default c rule
COMPILE.c = $(CXX) $(DEPFLAGS) $(CFLAGS) $(CPPFLAGS) -c

# Delete the built-in rules for building object files from .c files
%.o : %.c
# Define a rule to build object files based on .c or dependency files by making the associated dependency file
# a prerequisite of the target.  Make the DEPDIR an order only prerequisite of the target, so it will be created when needed, meaning
# the targets won't get rebuilt when the timestamp on DEPDIR changes
# See https://www.gnu.org/software/make/manual/html_node/Prerequisite-Types.html for order only prerequesites overview.
$(ODIR)/%.o : $(SDIR)/%.c $(DEPDIR)/%.d | $(DEPDIR)
	$(COMPILE.c) $(OUTPUT_OPTION) $<

$(BDIR): ; @mkdir -p $@
# Create the DEPDIR when it doesn't exist
$(DEPDIR): ; @mkdir -p $@

# Use pattern rules to build a list of DEPFILES
DEPFILES := $(SRCS:$(SDIR)/%.c=$(DEPDIR)/%.d)
# Mention each of the dependency files as a target, so make won't fail if the file doesn't exist
$(DEPFILES):

# Include all dependency files which exist, to include the relevant targets.
# See https://www.gnu.org/software/make/manual/html_node/Wildcard-Function.html for wildcard function documentation
include $(wildcard $(DEPFILES))
