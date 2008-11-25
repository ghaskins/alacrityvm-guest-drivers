ARCH=$(shell uname -m)
OBJDIR ?= obj/$(ARCH)

NAME=libvbus
VERSION=0.1
RELEASE=1
RPMBIN=$(OBJDIR)/$(NAME)-$(VERSION).tar.gz

SUBDIRS = lib

SUBDIRS:=$(strip $(SUBDIRS))

.PHONY: $(SUBDIRS)

all : $(SUBDIRS)

$(SUBDIRS):
	cd $@ && $(MAKE)

.PHONY: $(SUBDIRS:=-install)

install : $(SUBDIRS:=-install)

$(SUBDIRS:=-install):
	cd $(patsubst %-install,%,$@) && $(MAKE) install

.PHONY: clean
.PHONY: $(SUBDIRS:=-clean)

clean : $(SUBDIRS:=-clean)

$(SUBDIRS:=-clean):
	cd $(patsubst %-clean,%,$@) && $(MAKE) clean

$(OBJDIR)/$(NAME)-$(VERSION):
	@mkdir -p $(OBJDIR)/$(NAME)-$(VERSION)

$(OBJDIR)/$(NAME)-$(VERSION)/$(NAME).spec: $(OBJDIR)/$(NAME)-$(VERSION) $(NAME).spec Makefile
	@echo "Installing RPM specfile $@"
	@cat $(NAME).spec | sed -e 's/_RPM_VERSION/$(VERSION)/;s/_RPM_RELEASE/$(RELEASE)/' > $@

.PHONY: $(RPMBIN)

$(RPMBIN): $(OBJDIR)/$(NAME)-$(VERSION)/$(NAME).spec 
	tar -c --exclude=.git --exclude=*~ --exclude=$(NAME).spec --exclude=test --exclude=obj * ../include/* | (cd $(OBJDIR)/$(NAME)-$(VERSION); tar xf -)
	(cd $(OBJDIR); tar cvz $(NAME)-$(VERSION)) > $(RPMBIN)

srcrpm: $(RPMBIN)
	rpmbuild -ts $(RPMBIN)

