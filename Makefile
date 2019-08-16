TARGETS = all install clean distclean
dirs = include core collect analysis hacking

MAKECMDGOALS ?= all
ifeq (, $(filter $(TARGETS), $(MAKECMDGOALS)))
MAKECMDGOALS=all
endif

.PHONY: $(dirs)

$(TARGETS): $(dirs)

prepare: include

$(dirs):
	@echo "====================================================================="
	@make -C $@ $(MAKECMDGOALS)
	@echo "====================================================================="
	@echo ""
