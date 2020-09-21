include ../Makefile.opt

RELDIR=$(subst $(TOPDIR)/, "", $(shell pwd))
GCC_ORIG_HFILE=gcc_treecodes_orig.h
GCC_OUT_HFILE=gcc_treecodes.h
GPP_OUT_HFILE=g++_treecodes.h
DEF_DEFINEFILE=defdefine.h

all: pre_header

pre_header: $(DEF_DEFINEFILE)

$(GCC_OUT_HFILE): $(GCC_ORIG_HFILE)
	@echo $(GEN_ECHO) $(RELDIR)/$(GCC_OUT_HFILE)
	$(Q)$(CC) -I$(GCC_PLUGIN_INC) $(GCC_ORIG_HFILE) -E -P > $(GCC_OUT_HFILE)
	@echo $(GEN_ECHO) $(RELDIR)/$(GPP_OUT_HFILE)
	$(Q)$(CC) -I$(GCC_PLUGIN_INC) -DFOR_CXX $(GCC_ORIG_HFILE) -E -P > $(GPP_OUT_HFILE)

$(DEF_DEFINEFILE):
	@echo $(GEN_ECHO) $(RELDIR)/$(DEF_DEFINEFILE)
	@echo '#define DEF_WORKDIR "$(SRCINV_ROOT)"' > $(DEF_DEFINEFILE)
	@echo '#define DEF_PLUGINDIR "$(SRCINV_ROOT)/bin"' >> $(DEF_DEFINEFILE)
	@echo '#define DEF_CONFDIR "$(SRCINV_ROOT)/config"' >> $(DEF_DEFINEFILE)
	@echo '#define DEF_TMPDIR "$(SRCINV_ROOT)/tmp"' >> $(DEF_DEFINEFILE)

install:

clean:

distclean: clean
	@echo $(CLEAN_ECHO) $(RELDIR)/
	$(Q)$(RM) $(GCC_OUT_HFILE)
	$(Q)$(RM) $(GPP_OUT_HFILE)
	$(Q)$(RM) $(DEF_DEFINEFILE)
