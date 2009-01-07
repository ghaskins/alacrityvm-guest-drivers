CFLAGS += -g -O0
ARCH=$(shell uname -m)
OBJDIR ?= obj/$(ARCH)
LIBRARIES += -lvbus
LIBRARIES += -lboost_thread-mt
LIBRARIES += -lboost_filesystem
NAME=venet-loopback

ifneq ($(KERNEL_SRC), )
INCLUDES += -I$(KERNEL_SRC)/include
endif

SRCS += $(shell find *.cc)

OBJS = $(patsubst %.cc,$(OBJDIR)/%.o,$(SRCS))

OUTPUT = $(OBJDIR)/$(NAME)

all: $(OUTPUT)

$(OBJS): Makefile

$(OUTPUT): $(OBJS)

$(OBJDIR)/%.o: %.cc	Makefile
	@echo "Compiling (C++) $< to $@"
	@mkdir -p $(OBJDIR)
	$(CXX) $(CFLAGS) $(INCLUDES) -c -o $@ $<

$(OBJDIR)/%:
	@echo "Linking $@"
	@mkdir -p $(OBJDIR)
	$(CXX) $(CFLAGS) $(INCLUDES) -o $@ $(filter %.o %.a,$+) $(LIBDIR) $(LIBRARIES)

clean: 
	@rm -f $(OBJDIR)/*


