include ../pre_defines.makefile

TARGETS = all install clean distclean
dirs = gcc

.PHONY: $(dirs)

OUTDIR = $(SRCINV_BIN)
THIS_SO = analysis.so
SMF_TEST=sm.c
SMF=
THIS_CFLAGS=$(CFLAGS)
ifeq ($(SMF_TEST), $(wildcard $(SMF_TEST)))
	SMF=$(SMF_TEST)
	THIS_CFLAGS+=-DHAS_SM
endif
OBJS = analysis.c \
       sinode.c \
       sibuf.c \
       resfile.c \
       utils.c $(SMF)

INSTALLS := $(addprefix $(OUTDIR)/,$(THIS_SO))

CC=gcc
CC_OPT=$(THIS_CFLAGS) -rdynamic -shared -fPIC -D_FILE_OFFSET_BITS=64

all: $(THIS_SO) $(dirs)

$(THIS_SO): $(OBJS)
	$(CC) $(CC_OPT) $(OBJS) -I$(CLIB_INC) -I$(SRCINV_INC) -L$(CLIB_LIB) -lpthread -l$(CLIB_SO) -o $@ -Wl,-rpath $(CLIB_LIB)

install: $(dirs) $(INSTALLS)

$(OUTDIR)/%: %
	install $< $@

clean: $(dirs)
	@rm -vf $(THIS_SO)

distclean: clean
	@rm -vf $(OUTDIR)/$(THIS_SO)

$(dirs):
	@make -C $@ $(MAKECMDGOALS)
