RELDIR=$(subst $(TOPDIR)/, "", $(shell pwd))
OUTDIR = $(SRCINV_BIN)

CXX_SRCS = compiler_gcc.cc

CXX_OBJS += $(CXX_SRCS:%.cc=%.o)

CXX_SO += $(CXX_SRCS:%.cc=%.so)

OUTLIBS = $(addprefix lib,$(CXX_SO))

INSTALLS = $(addprefix $(OUTDIR)/,$(OUTLIBS))

CXXFLAGS += \
	    -fPIC \
	    -shared \
	    -rdynamic \
	   -fno-rtti \
	    -Wno-literal-suffix \
	    -D_FILE_OFFSET_BITS=64

all: $(OUTLIBS) install

lib%: %
	@echo $(GEN_ECHO) $(RELDIR)/$@ $(SRC_ECHO) $(RELDIR)/$<
	$(Q)cp $< $@

$(CXX_SO): %.so: %.o
	@echo $(LD_ECHO) $(RELDIR)/$@ $(SRC_ECHO) $(RELDIR)/$<
	$(Q)$(CXX) $(CXXFLAGS) -L$(CLIB_LIB) -ldl -l$(CLIB_SO) -o $@ $< -Wl,-rpath $(CLIB_LIB)

$(CXX_OBJS): %.o: %.cc
	@echo $(CXX_ECHO) $(RELDIR)/$@ $(SRC_ECHO) $(RELDIR)/$<
	$(Q)$(CXX) $(CXXFLAGS) -I$(GCC_PLUGIN_INC) -I$(SRCINV_INC) -I$(CLIB_INC) -c -o $@ $<

install: $(INSTALLS)

$(OUTDIR)/%: %
	@echo $(INSTALL_ECHO) $(subst $(TOPDIR)/,"",$@) $(SRC_ECHO) $(RELDIR)/$<
	$(Q)$(INSTALL) $< $@

clean:
	@echo $(CLEAN_ECHO) $(RELDIR)/
	$(Q)$(RM) $(CXX_OBJS)
	$(Q)$(RM) $(CXX_SO)
	$(Q)$(RM) $(OUTLIBS)

distclean: clean
	@echo $(CLEAN_ECHO) $(subst $(TOPDIR)/, "", $(INSTALLS))
	$(Q)$(RM) $(INSTALLS)
