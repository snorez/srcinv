RELDIR=$(subst $(TOPDIR)/, "", $(shell pwd))
OUTDIR0 = $(SRCINV_BIN)
OUTDIR1 = $(SRCINV_BIN)/hacking

CC_SRCS = hacking.c \
	  itersn.c
CC_EXTRA_SRCS = 
CC_SRCS += $(wildcard $(CC_EXTRA_SRCS))

CC_OBJS = $(CC_SRCS:%.c=%.o)

CC_SO = hacking.so

CXX_SRCS = uninit.cc \
	   kern_copyuser.cc \
	   gensample.cc \
	   list_function.cc \
	   show_detail.cc \
	   debuild.cc \
	   fuzz.cc \
	   test.cc
CXX_EXTRA_SRCS = uaf.cc \
		 uaf1.cc
CXX_SRCS += $(wildcard $(CXX_EXTRA_SRCS))

CXX_OBJS = $(CXX_SRCS:%.cc=%.o)

CXX_SO = $(CXX_SRCS:%.cc=%.so)

INSTALLS := $(addprefix $(OUTDIR0)/,$(CC_SO)) \
	$(addprefix $(OUTDIR1)/,$(CXX_SO))

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

all: $(CC_SO) $(CXX_SO)

$(CC_SO): $(CC_OBJS)
	@echo $(LD_ECHO) $(RELDIR)/$(CC_SO) $(SRC_ECHO) $(RELDIR)/{$(CC_OBJS)}
	$(Q)$(CC) $(CFLAGS) $(CC_OBJS) -L$(CLIB_LIB) -l$(CLIB_SO) -o $(CC_SO) -Wl,-rpath $(CLIB_LIB)

$(CC_OBJS): %.o: %.c
	@echo $(CC_ECHO) $(RELDIR)/$@ $(SRC_ECHO) $(RELDIR)/$<
	$(Q)$(CC) $(CFLAGS) -I$(CLIB_INC) -I$(SRCINV_INC) -c -o $@ $<

$(CXX_SO): %.so : %.o
	@echo $(LD_ECHO) $(RELDIR)/$@ $(SRC_ECHO) $(RELDIR)/$<
	$(Q)$(CXX) $(CXXFLAGS) $< -L$(CLIB_LIB) -l$(CLIB_SO) -lpthread -o $@ -Wl,-rpath $(CLIB_LIB)

$(CXX_OBJS): %.o: %.cc
	@echo $(CXX_ECHO) $(RELDIR)/$@ $(SRC_ECHO) $(RELDIR)/$<
	$(Q)$(CXX) $(CXXFLAGS) -I$(GCC_PLUGIN_INC) -I$(SRCINV_INC) -I$(CLIB_INC) -c -o $@ $<

install: $(INSTALLS)

$(OUTDIR0)/% $(OUTDIR1)/%: %
	@echo $(INSTALL_ECHO) $(subst $(TOPDIR)/,"",$@) $(SRC_ECHO) $(RELDIR)/$<
	$(Q)$(INSTALL) $< $@

clean:
	@echo $(CLEAN_ECHO) $(RELDIR)/
	$(Q)$(RM) $(CC_OBJS)
	$(Q)$(RM) $(CC_SO)
	$(Q)$(RM) $(CXX_OBJS)
	$(Q)$(RM) $(CXX_SO)

distclean: clean
	@echo $(CLEAN_ECHO) $(subst $(TOPDIR)/, "", $(INSTALLS))
	$(Q)$(RM) $(INSTALLS)
