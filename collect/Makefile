RELDIR=$(subst $(TOPDIR)/, "", $(shell pwd))
dirs = gcc
OUTDIR = $(SRCINV_BIN)

CC_SRCS = collect.c

CC_OBJS = $(CC_SRCS:%.c=%.o)

CC_SO = $(CC_SRCS:%.c=%.so)

INSTALLS = $(addprefix $(OUTDIR)/,$(CC_SO))

CFLAGS += \
	  -fPIC \
	  -shared \
	  -rdynamic \
	  -D_FILE_OFFSET_BITS=64

.PHONY: $(dirs)

all: $(CC_SO) $(dirs)

$(CC_SO): %.so: %.o
	@echo $(LD_ECHO) $(RELDIR)/$@ $(SRC_ECHO) $(RELDIR)/$<
	$(Q)$(CC) $(CFLAGS) $< -L$(CLIB_LIB) -l$(CLIB_SO) -o $@ -Wl,-rpath $(CLIB_LIB)

$(CC_OBJS): %.o: %.c
	@echo $(CC_ECHO) $(RELDIR)/$@ $(SRC_ECHO) $(RELDIR)/$<
	$(Q)$(CC) $(CFLAGS) -I$(CLIB_INC) -I$(SRCINV_INC) -c -o $@ $<

install: $(dirs) $(INSTALLS)

$(OUTDIR)/%: %
	@echo $(INSTALL_ECHO) $(subst $(TOPDIR)/,"",$@) $(SRC_ECHO) $(RELDIR)/$<
	$(Q)$(INSTALL) $< $@

clean: $(dirs)
	@echo $(CLEAN_ECHO) $(RELDIR)/
	$(Q)$(RM) $(CC_OBJS)
	$(Q)$(RM) $(CC_SO)

distclean: clean $(dirs)
	@echo $(CLEAN_ECHO) $(subst $(TOPDIR)/, "", $(INSTALLS))
	$(Q)$(RM) $(INSTALLS)

$(dirs):
	$(Q)$(MAKE) -C $@ $(MAKECMDGOALS)
