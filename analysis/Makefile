include ../pre_defines.makefile

TARGETS = all install clean distclean
dirs = gcc

.PHONY: $(dirs)

OUTDIR = $(SRCINV_BIN)
THIS_SO = analysis.so
OBJS = analysis.c \
       sinode.c \
       sibuf.c \
       resfile.c \
       utils.c

CC=gcc
CC_OPT=-Wall -std=gnu11 -rdynamic -shared -fPIC -D_FILE_OFFSET_BITS=64 $(CC_RELEASE)

all: $(THIS_SO) $(dirs)

$(THIS_SO): $(OBJS)
	$(CC) $(CC_OPT) $(OBJS) -I$(CLIB_INC) -I$(SRCINV_INC) -L$(CLIB_LIB) -lpthread -l$(CLIB_SO) -o $@ -Wl,-rpath $(CLIB_LIB)

install: $(dirs)
	@cp -v $(THIS_SO) $(OUTDIR)/

clean: $(dirs)
	@rm -vf $(THIS_SO)

distclean: clean
	@rm -vf $(OUTDIR)/$(THIS_SO)

$(dirs):
	@make -C $@ $(MAKECMDGOALS)
