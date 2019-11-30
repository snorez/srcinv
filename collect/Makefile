include ../pre_defines.makefile

TARGETS = all install clean distclean
dirs = gcc

.PHONY: $(dirs)

OUTDIR = $(SRCINV_BIN)
OUTNAME = collect.so
OBJS = collect.c

CC = gcc
CC_OPT = $(CFLAGS) -rdynamic -shared -fPIC -D_FILE_OFFSET_BITS=64 $(CC_RELEASE)

all: $(OUTNAME) $(dirs)

$(OUTNAME): $(OBJS)
	$(CC) $(CC_OPT) $(OBJS) -I$(CLIB_INC) -I$(SRCINV_INC) -L$(CLIB_LIB) -l$(CLIB_SO) -o $(OUTNAME) -Wl,-rpath $(CLIB_LIB)

install: $(dirs)
	@cp -v $(OUTNAME) $(OUTDIR)/

clean: $(dirs)
	@rm -vf $(OUTNAME)

distclean: clean $(dirs)
	@rm -vf $(OUTDIR)/$(OUTNAME)

$(dirs):
	@make -C $@ $(MAKECMDGOALS)
