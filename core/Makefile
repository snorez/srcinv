include ../pre_defines.makefile

CC = gcc
CC_OPT =$(CFLAGS) -rdynamic -fPIC -D_FILE_OFFSET_BITS=64 $(CC_RELEASE)

OUTDIR = $(SRCINV_BIN)
OUTFILE = si_core
INFILE = si_core.c \
	 si_cmd.c \
	 si_conf.c \
	 si_module.c \
	 si_src.c

all: $(OUTFILE)

$(OUTFILE): $(INFILE)
	$(CC) $(CC_OPT) -I$(SRCINV_INC) -I$(CLIB_INC) $(INFILE) -L$(CLIB_LIB) -ldl -l$(CLIB_SO) -o $(OUTFILE) -Wl,-rpath $(CLIB_LIB)

install:
	@cp $(OUTFILE) $(OUTDIR)/

clean:
	@rm -vf $(OUTFILE)

distclean: clean
	@rm -vf $(OUTDIR)/$(OUTFILE)
