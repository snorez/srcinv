TARGETS = all install clean distclean
dirs = include core lib collect analysis hacking

MAKECMDGOALS ?= all
ifeq (, $(filter $(TARGETS), $(MAKECMDGOALS)))
MAKECMDGOALS=all
endif

GCC_VER_MATCH := $(shell expr `gcc -dumpversion | cut -f1 -d.` \== `g++ -dumpversion | cut -f1 -d.`)

ifeq "$(GCC_VER_MATCH)" "0"
$(error gcc version not match g++ version)
endif

.PHONY: $(dirs)

$(TARGETS): $(dirs)

prepare: include

$(dirs):
	@echo "==============================================================="
	@make -C $@ $(MAKECMDGOALS)
	@echo "==============================================================="
	@echo ""
