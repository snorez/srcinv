include Makefile.opt

TARGETS = all install clean distclean
PREDIRS = include lib
dirs = core collect analysis hacking

MAKECMDGOALS ?= all
ifeq (, $(filter $(TARGETS), $(MAKECMDGOALS)))
MAKECMDGOALS=all
endif

.PHONY: $(dirs) $(PREDIRS)

$(TARGETS): prepare $(dirs)

prepare: $(PREDIRS)

include:
	$(Q)$(MAKE) $(MAKE_OPT) -C $@ $(MAKECMDGOALS)

lib: include
	$(Q)$(MAKE) $(MAKE_OPT) -C $@ $(MAKECMDGOALS)

$(dirs): prepare
	$(Q)$(MAKE) $(MAKE_OPT) -C $@ $(MAKECMDGOALS)
