include ../../pre_defines.makefile

CC = g++
CC_OPT = $(CPPFLAGS) -shared -fPIC -rdynamic -Wno-literal-suffix -fno-rtti -D_FILE_OFFSET_BITS=64 $(CC_RELEASE)

OUTDIR = $(SRCINV_BIN)/analysis
OUTFILE = gcc_c.so
INFILE = c.cc

all: $(OUTFILE)

$(OUTFILE): $(INFILE)
	$(CC) $(CC_OPT) -I$(GCC_PLUGIN_INC) -I$(CLIB_INC) -I$(SRCINV_INC) $(INFILE) -L$(CLIB_LIB) -l$(CLIB_SO) -o $(OUTFILE) -Wl,-rpath $(CLIB_LIB)

install:
	@cp -v $(OUTFILE) $(OUTDIR)/

clean:
	@rm -vf $(OUTFILE)

distclean: clean
	@rm -vf $(OUTDIR)/$(OUTFILE)
