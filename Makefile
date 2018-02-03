

SRCDIR		:= src
BUILDDIR	:= build
INCLUDEDIR	:= include
BINDIR		:= bin
EXAMPLEDIR	:= examples


# This is a general use makefile for robotics cape projects written in C.
# Just change the target name to match your main source code filename.
EXAMPLES	:= $(shell find $(EXAMPLEDIR) -type f -name *.c)
EXAMPLEOBJECTS	:= $(EXAMPLES:$(EXAMPLEDIR)/%.c=$(BUILDDIR)/%.o)
TARGETS		:= $(EXAMPLES:$(EXAMPLEDIR)/%.c=$(BINDIR)/%)


CC		:= gcc
LINKER		:= gcc

#CFLAGS		:= -Wall -Wextra -I $(INCLUDEDIR)
CFLAGS		:= -pthread -I $(INCLUDEDIR)
LFLAGS		:= -lm -lrt -pthread


# enable O3 optimization and vectorized math only for math libs
MATH_OPT_FLAGS	:= -O3 -ffast-math -ftree-vectorize

SOURCES		:= $(shell find $(SRCDIR) -type f -name *.c)
OBJECTS		:= $(SOURCES:$(SRCDIR)/%.c=$(BUILDDIR)/%.o)
INCLUDES	:= $(shell find include -name '*.h')

prefix		:= /usr/local
RM		:= rm -rf
INSTALL		:= install -m 4755
INSTALLDIR	:= install -d -m 755




# link objects to targets
$(EXAMPLEDIR)/% : $(BUILDDIR)/%.o
	@echo "linking"
	@mkdir -p $(BINDIR)
	@$(LINKER) -o $@ $(OBJECTS) $< $(LFLAGS)
	@echo "Done linking $(@)"

# rule for example objects from lib objects
# rule for all other objects
$(BUILDDIR)/%.o : $(EXAMPLEDIR)/%.c $(INCLUDES)
	@mkdir -p $(dir $(@))
	@$(CC) -c $(CFLAGS) $(ARCFLAGS) $(OPT_FLAGS) $(FLOAT_FLAG) $(DEBUGFLAG) $< -o $(@)
	@echo "made: $(@)"

# rule for math libs
$(BUILDDIR)/math/%.o : $(SRCDIR)/math/%.c $(INCLUDES)
	@mkdir -p $(dir $(@))
	@$(CC) -c $(CFLAGS) $(ARCFLAGS) $(MATH_OPT_FLAGS) $(FLOAT_FLAG)  $(DEBUGFLAG) $< -o $(@)
	@echo "made: $(@)"

# rule for all other objects
$(BUILDDIR)/%.o : $(SRCDIR)/%.c $(INCLUDES)
	@mkdir -p $(dir $(@))
	@$(CC) -c $(CFLAGS) $(ARCFLAGS) $(OPT_FLAGS) $(FLOAT_FLAG) $(DEBUGFLAG) $< -o $(@)
	@echo "made: $(@)"


all:
	$(TARGETS)

debug:
	$(MAKE) $(MAKEFILE) DEBUGFLAG="-g -D DEBUG"
	@echo " "
	@echo "$(TARGET) Make Debug Complete"
	@echo " "



clean:
	@$(RM) $(BINDIR)
	@$(RM) $(BUILDDIR)
	@echo "Clean Complete"

