
COMPILE_ARCH    ?= $(shell uname -m | sed -e s/i.86/x86_32/)
CFLAGS += -g -O0 -DPIC -fPIC -shared
OBJDIR=obj/$(COMPILE_ARCH)
INCLUDES += -I.
INCLUDES += -I../include
OUTPUT=$(OBJDIR)/libvbus.so
PREFIX ?= /usr/local

ifneq ($(KERNEL_SRC), )
INCLUDES += -I$(KERNEL_SRC)/include
endif

ifeq ($(COMPILE_ARCH), x86_64)
LIBDIR = lib64
else
LIBDIR = lib
endif

ifneq ($(PREFIX), /usr/local)
PPREFIX = $(PREFIX)/usr
endif

INSTDIR = $(PPREFIX)/$(LIBDIR)

SRCS += $(shell find *.cc)

OBJS = $(patsubst %.cc,$(OBJDIR)/%.o,$(SRCS))

all: $(OUTPUT) $(OBJS)

$(OBJDIR)/%.o: %.cc	Makefile
	@echo "Compiling (C++) $< to $@"
	@mkdir -p $(OBJDIR)
	$(CXX) $(CFLAGS) $(INCLUDES) -c -o $@ $<

$(OBJDIR)/%.so: $(OBJS)
	@echo "Linking $@"
	@mkdir -p $(OBJDIR)
	$(CXX) $(CFLAGS) $(INCLUDES) -o $@ $(filter %.o %.a,$+) $(LIBRARIES)

clean:
	@rm -f $(OBJDIR)/*

install: $(OUTPUT)
	@mkdir -p $(INSTDIR)
	cp $(OUTPUT) $(INSTDIR)
	@mkdir -p $(PPREFIX)/include
	cp ../include/vbus.hh $(PPREFIX)/include

