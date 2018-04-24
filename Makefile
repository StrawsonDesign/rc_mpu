# makefile for rc_mpu, builds example programs

# directories
SRCDIR		:= src
BUILDDIR	:= build
INCLUDEDIR	:= include
BINDIR		:= bin
EXAMPLEDIR	:= examples
SONAME		:= librcmpu.so
LIBDIR		:= lib
TARGET		:= $(LIBDIR)/$(SONAME)

EXAMPLES	:= $(shell find $(EXAMPLEDIR) -type f -name *.c)
TARGETS		:= $(EXAMPLES:$(EXAMPLEDIR)/%.c=$(BINDIR)/%)
INCLUDES	:= $(shell find include -name '*.h')
SOURCES		:= $(shell find $(SRCDIR) -type f -name *.c)
OBJECTS		:= $(SOURCES:$(SRCDIR)/%.c=$(BUILDDIR)/%.o)
.PRECIOUS	:  $(OBJECTS) # stop make from deleting objects

CC		:= gcc
LINKER		:= gcc

WFLAGS		:= -Wall -Wextra
CFLAGS		:= -pthread -fPIC -I $(INCLUDEDIR)
LFLAGS		:= -lm -lrt -pthread -lpthread -lrcmpu
LDFLAGS		:= -lm -lrt -pthread -shared -Wl,-soname,$(SONAME)

# enable O3 optimization and vectorized math only for math libs
MATH_OPT_FLAGS	:= -O3 -ffast-math -ftree-vectorize

# commands
RM		:= rm -rf
INSTALL		:= install -m 4755
INSTALLDIR	:= install -d -m 755

# prefix variable in case this is used to make a deb package
prefix		:= /usr/local



all : $(TARGETS) $(TARGET)

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
	@$(INSTALLDIR) $(DESTDIR)$(prefix)/include
	@cp -r include/* $(DESTDIR)$(prefix)/include
	@$(INSTALLDIR) $(DESTDIR)$(prefix)/lib
	@$(INSTALL) $(TARGET) $(DESTDIR)$(prefix)/lib
	@echo "install complete"

uninstall:
	@for f in $(TARGETS); do $(RM) $(prefix)/$$f; done
	$(RM) $(DESTDIR)$(prefix)/$(TARGET)
	$(RM) $(DESTDIR)$(prefix)/include/rc
	@echo "uinstall complete"

# rule for building library
$(TARGET): $(OBJECTS)
	@mkdir -p $(LIBDIR)
	@$(LINKER) -o $(TARGET) $(OBJECTS) $(LDFLAGS)
	@echo "Done making $(TARGET)"

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
$(BINDIR)/% : $(EXAMPLEDIR)/%.c $(TARGET) $(INCLUDES)
	@mkdir -p $(BINDIR)
	$(CC) -o $@ $< $(CFLAGS) $(WFLAGS) $(ARCFLAGS) $(OPT_FLAGS) $(FLOAT_FLAG) $(DEBUGFLAG) $(LFLAGS)
	@echo "made: $@"
