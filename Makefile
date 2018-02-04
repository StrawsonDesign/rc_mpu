# makefile for rc_mpu, builds example programs

# directories
SRCDIR		:= src
BUILDDIR	:= build
INCLUDEDIR	:= include
BINDIR		:= bin
EXAMPLEDIR	:= examples

EXAMPLES	:= $(shell find $(EXAMPLEDIR) -type f -name *.c)
TARGETS		:= $(EXAMPLES:$(EXAMPLEDIR)/%.c=$(BINDIR)/%)
INCLUDES	:= $(shell find include -name '*.h')
SOURCES		:= $(shell find $(SRCDIR) -type f -name *.c)
OBJECTS		:= $(SOURCES:$(SRCDIR)/%.c=$(BUILDDIR)/%.o)
.PRECIOUS	:  $(OBJECTS) # stop make from deleting objects

CC		:= gcc
LINKER		:= gcc

WFLAGS		:= -Wall -Wextra
CFLAGS		:= -pthread -I $(INCLUDEDIR)
LFLAGS		:= -lm -lrt -pthread -lpthread

# enable O3 optimization and vectorized math only for math libs
MATH_OPT_FLAGS	:= -O3 -ffast-math -ftree-vectorize

# commands
RM		:= rm -rf
INSTALL		:= install -m 4755
INSTALLDIR	:= install -d -m 755

# prefix variable in case this is used to make a deb package
prefix		:= /usr/local



all : $(TARGETS)

debug :
	$(MAKE) $(MAKEFILE) DEBUGFLAG="-g -D DEBUG"
	@echo " "
	@echo "Make Debug Complete"
	@echo " "

clean :
	@$(RM) $(BINDIR)
	@$(RM) $(BUILDDIR)
	@echo "Clean Complete"

install:
	$(MAKE)
	@$(INSTALL) $(TARGETS) $(prefix)/bin/
	@echo "install complete"

uninstall:
	@for f in $(TARGETS); do $(RM) $(prefix)/$$f; done
	@echo "uinstall complete"



# rule for compiling math lib objects
$(BUILDDIR)/math/%.o : $(SRCDIR)/math/%.c $(INCLUDES)
	@mkdir -p $(dir $(@))
	@$(CC) -c -o $@ $< $(CFLAGS) $(WFLAGS) $(ARCFLAGS) $(MATH_OPT_FLAGS) $(FLOAT_FLAG) $(DEBUGFLAG)
	@echo "made: $(@)"

# rule for compiling all other objects
$(BUILDDIR)/%.o : $(SRCDIR)/%.c $(INCLUDES)
	@mkdir -p $(dir $(@))
	@$(CC) -c -o $@ $< $(CFLAGS) $(WFLAGS) $(ARCFLAGS) $(OPT_FLAGS) $(FLOAT_FLAG) $(DEBUGFLAG)
	@echo "made: $(@)"

# rules for compiling and linking examples simultaneously
$(BINDIR)/% : $(EXAMPLEDIR)/%.c $(OBJECTS) $(INCLUDES)
	@mkdir -p $(BINDIR)
	@$(CC) -o $@ $< $(CFLAGS) $(WFLAGS) $(ARCFLAGS) $(OPT_FLAGS) $(FLOAT_FLAG) $(DEBUGFLAG) $(OBJECTS) $(LFLAGS)
	@echo "made: $@"
