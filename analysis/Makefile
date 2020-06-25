RELDIR=$(subst $(TOPDIR)/, "", $(shell pwd))
dirs = gcc
OUTDIR = $(SRCINV_BIN)

CC_SRCS = analysis.c \
       sinode.c \
       sibuf.c \
       resfile.c \
       parse.c \
       mark-entry.c \
       utils.c \
       dec.c \
       dec-asm.c \
       trace.c

CXX_SRCS = dec-gcc.cc

CC_OBJS = $(CC_SRCS:%.c=%.o)
CXX_OBJS = $(CXX_SRCS:%.cc=%.o)

OUTLIBS = analysis.so

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

.PHONY: $(dirs)

all: $(OUTLIBS) $(dirs)

$(OUTLIBS): $(CC_OBJS) $(CXX_OBJS)
	@echo $(LD_ECHO) $(RELDIR)/$(OUTLIBS) $(SRC_ECHO) $(RELDIR)/{$(CC_OBJS)} $(RELDIR)/{$(CXX_OBJS)}
	$(Q)$(CC) $(CFLAGS) $(CC_OBJS) $(CXX_OBJS) -L$(CLIB_LIB) -lpthread -l$(CLIB_SO) -o $(OUTLIBS) -Wl,-rpath $(CLIB_LIB)

$(CC_OBJS): %.o: %.c
	@echo $(CC_ECHO) $(RELDIR)/$@ $(SRC_ECHO) $(RELDIR)/$<
	$(Q)$(CC) $(CFLAGS) -I$(CLIB_INC) -I$(SRCINV_INC) -c -o $@ $<

$(CXX_OBJS): %.o: %.cc
	@echo $(CXX_ECHO) $(RELDIR)/$@ $(SRC_ECHO) $(RELDIR)/$<
	$(Q)$(CXX) $(CXXFLAGS) -I$(GCC_PLUGIN_INC) -I$(CLIB_INC) -I$(SRCINV_INC) -c -o $@ $<

install: $(dirs) $(INSTALLS)

$(OUTDIR)/%: %
	@echo $(INSTALL_ECHO) $(subst $(TOPDIR)/,"",$@) $(SRC_ECHO) $(RELDIR)/$<
	$(Q)$(INSTALL) $< $@

clean: $(dirs)
	@echo $(CLEAN_ECHO) $(RELDIR)/
	$(Q)$(RM) $(CC_OBJS)
	$(Q)$(RM) $(OUTLIBS)

distclean: clean
	@echo $(CLEAN_ECHO) $(subst $(TOPDIR)/, "", $(INSTALLS))
	$(Q)$(RM) $(INSTALLS)

$(dirs):
	$(Q)$(MAKE) -C $@ $(MAKECMDGOALS)
