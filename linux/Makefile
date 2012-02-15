
OBJDIR ?= obj

NAME=alacrityvm-drivers
VERSION=0.2
RELEASE=1
RPMBIN=$(OBJDIR)/$(NAME)-$(VERSION).tar.gz

all: srcrpm

$(OBJDIR)/$(NAME)-$(VERSION):
	@mkdir -p $(OBJDIR)/$(NAME)-$(VERSION)

.PHONY: $(RPMBIN)

$(RPMBIN): $(OBJDIR)/$(NAME)-$(VERSION)
	@./tar_up.sh $(KSRC) | (cd $(OBJDIR)/$(NAME)-$(VERSION); tar xf -)
	@cat $(NAME).spec | sed -e 's/_RPM_VERSION/$(VERSION)/;s/_RPM_RELEASE/$(RELEASE)/' > $(OBJDIR)/$(NAME)-$(VERSION)/$(NAME).spec
	cp vbus-enet.patch $(OBJDIR)
	(cd $(OBJDIR); tar cvz $(NAME)-$(VERSION)) > $(RPMBIN)

srcrpm: $(RPMBIN)
	rpmbuild -ts $(RPMBIN)

clean:
	rm -rf $(OBJDIR)
