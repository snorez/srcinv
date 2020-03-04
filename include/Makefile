include ../pre_defines.makefile

all: pre_header

GCC_ORIG_HFILE=gcc_treecodes_orig.h
GCC_OUT_HFILE=gcc_treecodes.h
GPP_OUT_HFILE=g++_treecodes.h
DEF_DEFINEFILE=defdefine.h
pre_header: $(GCC_OUT_HFILE) $(DEF_DEFINEFILE)

$(GCC_OUT_HFILE): $(GCC_ORIG_HFILE)
	@echo generate $(GCC_OUT_HFILE)
	@gcc -I$(GCC_PLUGIN_INC) $(GCC_ORIG_HFILE) -E -P > $(GCC_OUT_HFILE)
	@gcc -I$(GCC_PLUGIN_INC) -DFOR_CPP $(GCC_ORIG_HFILE) -E -P > $(GPP_OUT_HFILE)

$(DEF_DEFINEFILE):
	@echo generate $(DEF_DEFINEFILE)
	@echo '#define DEF_WORKDIR "$(SRCINV_ROOT)"' > $(DEF_DEFINEFILE)
	@echo '#define DEF_PLUGINDIR "$(SRCINV_ROOT)/bin"' >> $(DEF_DEFINEFILE)
	@echo '#define DEF_CONFDIR "$(SRCINV_ROOT)/config"' >> $(DEF_DEFINEFILE)
	@echo '#define DEF_TMPDIR "$(SRCINV_ROOT)/tmp"' >> $(DEF_DEFINEFILE)

install:

clean:

distclean: clean
	@rm -vf $(GCC_OUT_HFILE)
	@rm -vf $(GPP_OUT_HFILE)
	@rm -vf $(DEF_DEFINEFILE)
