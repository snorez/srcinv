RELDIR=$(subst $(TOPDIR)/, "", $(shell pwd))
dirs = gcc
OUTDIR = $(SRCINV_BIN)

SMF_TEST=sm.c
SMF=
ifeq ($(SMF_TEST), $(wildcard $(SMF_TEST)))
	SMF=$(SMF_TEST)
	CFLAGS += -DHAS_SM
endif
CC_SRCS = analysis.c \
       sinode.c \
       sibuf.c \
       resfile.c \
       parse.c \
       mark-entry.c \
       utils.c $(SMF)

CC_OBJS = $(CC_SRCS:%.c=%.o)

OUTLIBS = analysis.so

INSTALLS = $(addprefix $(OUTDIR)/,$(OUTLIBS))

CFLAGS += \
	  -fPIC \
	  -shared \
	  -rdynamic \
	  -D_FILE_OFFSET_BITS=64

.PHONY: $(dirs)

all: $(OUTLIBS) $(dirs)

$(OUTLIBS): $(CC_OBJS)
	@echo $(LD_ECHO) $(RELDIR)/$(OUTLIBS) $(SRC_ECHO) $(RELDIR)/{$(CC_OBJS)}
	$(Q)$(CC) $(CFLAGS) -L$(CLIB_LIB) -lpthread -l$(CLIB_SO) -o $(OUTLIBS) $(CC_OBJS) -Wl,-rpath $(CLIB_LIB)

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
	$(Q)$(RM) $(OUTLIBS)

distclean: clean
	@echo $(CLEAN_ECHO) $(subst $(TOPDIR)/, "", $(INSTALLS))
	$(Q)$(RM) $(INSTALLS)

$(dirs):
	$(Q)$(MAKE) -C $@ $(MAKECMDGOALS)
