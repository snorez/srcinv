include ../Makefile.opt

RELDIR=$(subst $(TOPDIR)/, "", $(shell pwd))
OUTDIR = $(SRCINV_BIN)

CC_SRCS = si_core.c \
	 si_cmd.c \
	 si_conf.c \
	 si_module.c \
	 si_src.c \
	 si_signal.c

CC_OBJS = $(CC_SRCS:.c=.o)

OUTLIBS = si_core

INSTALLS = $(addprefix $(OUTDIR)/,$(OUTLIBS))

CFLAGS += \
	  -fPIC \
	  -rdynamic \
	  -D_FILE_OFFSET_BITS=64

all: $(OUTLIBS)

$(OUTLIBS): $(CC_OBJS)
	@echo $(LD_ECHO) $(RELDIR)/$(OUTLIBS) $(SRC_ECHO) $(RELDIR)/{$(CC_OBJS)}
	$(Q)$(CC) $(CFLAGS) $(CC_OBJS) -L$(CLIB_LIB) -ldl -l$(CLIB_SO) -o $(OUTLIBS) -Wl,-rpath $(CLIB_LIB)

$(CC_OBJS): %.o: %.c
	@echo $(CC_ECHO) $(RELDIR)/$@ $(SRC_ECHO) $(RELDIR)/$<
	$(Q)$(CC) $(CFLAGS) -I$(SRCINV_INC) -I$(CLIB_INC) -c -o $@ $<

install: $(INSTALLS)

$(OUTDIR)/%: %
	@echo $(INSTALL_ECHO) $(subst $(TOPDIR)/,"",$@) $(SRC_ECHO) $(RELDIR)/$<
	$(Q)$(INSTALL) $< $@

clean:
	@echo $(CLEAN_ECHO) $(RELDIR)/
	$(Q)$(RM) $(CC_OBJS)
	$(Q)$(RM) $(OUTLIBS)

distclean: clean
	@echo $(CLEAN_ECHO) $(subst $(TOPDIR)/, "", $(INSTALLS))
	$(Q)$(RM) $(INSTALLS)
