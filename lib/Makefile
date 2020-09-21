include ../Makefile.opt

RELDIR=$(subst $(TOPDIR)/, "", $(shell pwd))
dirs = gcc
OUTDIR = $(SRCINV_BIN)

.PHONY: $(dirs)

all: $(dirs)

install: $(dirs)

clean: $(dirs)

distclean: clean
	@echo $(CLEAN_ECHO) $(RELDIR)/

$(dirs):
	$(Q)$(MAKE) $(MAKE_OPT) -C $@ $(MAKECMDGOALS)
