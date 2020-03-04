include ../../pre_defines.makefile

CC = g++
CC_OPT = $(CPPFLAGS) -shared -fPIC -rdynamic -Wno-literal-suffix -fno-rtti -D_FILE_OFFSET_BITS=64

OUTDIR = $(SRCINV_BIN)/analysis
OUTFILE = gcc_c.so
INFILE = c.cc
LIBDIR = $(SRCINV_BIN)
COMPILER_GCC_SO = compiler_gcc

all: $(OUTFILE)

$(OUTFILE): $(INFILE)
	$(CC) $(CC_OPT) -I$(GCC_PLUGIN_INC) -I$(CLIB_INC) -I$(SRCINV_INC) $(INFILE) -L$(CLIB_LIB) -L$(LIBDIR) -l$(CLIB_SO) -l$(COMPILER_GCC_SO) -o $(OUTFILE) -Wl,-rpath $(CLIB_LIB) -Wl,-rpath $(LIBDIR)

install:
	@cp -v $(OUTFILE) $(OUTDIR)/

clean:
	@rm -vf $(OUTFILE)

distclean: clean
	@rm -vf $(OUTDIR)/$(OUTFILE)
