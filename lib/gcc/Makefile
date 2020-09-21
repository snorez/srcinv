include ../../Makefile.opt

RELDIR=$(subst $(TOPDIR)/, "", $(shell pwd))
OUTDIR = $(SRCINV_BIN)

CC_SRCS = 
CXX_SRCS = compiler_gcc.cc \
	   tree.cc \
	   gimple.cc \
	   wide-int.cc \
	   wide-int-print.cc \
	   fold-const.cc \
	   tree-dfa.cc \
	   real.cc \
	   dfp.cc \
	   calls.cc \
	   tree-cfg.cc \
	   cfganal.cc \
	   tree-pretty-print.cc

CC_OBJS += $(CC_SRCS:%.c=%.o)
CXX_OBJS += $(CXX_SRCS:%.cc=%.o)

OUTSO = compiler_gcc.so

OUTLIBS = $(addprefix lib,$(OUTSO))

INSTALLS = $(addprefix $(OUTDIR)/,$(OUTLIBS))

CFLAGS += \
	  -fPIC \
	  -shared \
	  -rdynamic \
	  -D_FILE_OFFSET_BITS=64

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

$(OUTSO): $(CC_OBJS) $(CXX_OBJS)
	@echo $(LD_ECHO) $(RELDIR)/$@ $(SRC_ECHO) $(RELDIR)/{$(CC_OBJS) $(CXX_OBJS)}
	$(Q)$(CXX) $(CXXFLAGS) $(CC_OBJS) $(CXX_OBJS) -L$(CLIB_LIB) -ldl -l$(CLIB_SO) -o $@ -Wl,-rpath $(CLIB_LIB)

$(CC_OBJS): %.o: %.c
	@echo $(CC_ECHO) $(RELDIR)/$@ $(SRC_ECHO) $(RELDIR)/$<
	$(Q)$(CC) $(CFLAGS) -I$(GCC_PLUGIN_INC) -I$(SRCINV_INC) -I$(CLIB_INC) -c -o $@ $<

$(CXX_OBJS): %.o: %.cc
	@echo $(CXX_ECHO) $(RELDIR)/$@ $(SRC_ECHO) $(RELDIR)/$<
	$(Q)$(CXX) $(CXXFLAGS) -I$(GCC_PLUGIN_INC) -I$(SRCINV_INC) -I$(CLIB_INC) -c -o $@ $<

install: $(INSTALLS)

$(OUTDIR)/%: %
	@echo $(INSTALL_ECHO) $(subst $(TOPDIR)/,"",$@) $(SRC_ECHO) $(RELDIR)/$<
	$(Q)$(INSTALL) $< $@

clean:
	@echo $(CLEAN_ECHO) $(RELDIR)/
	$(Q)$(RM) $(CC_OBJS)
	$(Q)$(RM) $(CXX_OBJS)
	$(Q)$(RM) $(OUTSO)
	$(Q)$(RM) $(OUTLIBS)

distclean: clean
	@echo $(CLEAN_ECHO) $(subst $(TOPDIR)/, "", $(INSTALLS))
	$(Q)$(RM) $(INSTALLS)
