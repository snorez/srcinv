include ../pre_defines.makefile

CPP = g++
CPP_OPT = $(CPPFLAGS) -shared -rdynamic -fPIC -Wno-literal-suffix -fno-rtti -D_FILE_OFFSET_BITS=64

OUTDIR = $(SRCINV_BIN)
OUTFILE_GCC = libcompiler_gcc.so
INFILE_GCC = compiler_gcc.cc

all: $(OUTFILE_GCC) install

$(OUTFILE_GCC): $(INFILE_GCC)
	$(CPP) $(CPP_OPT) -I$(GCC_PLUGIN_INC) -I$(SRCINV_INC) -I$(CLIB_INC) $(INFILE_GCC) -L$(CLIB_LIB) -ldl -l$(CLIB_SO) -o $(OUTFILE_GCC) -Wl,-rpath $(CLIB_LIB)

install:
	@cp $(OUTFILE_GCC) $(OUTDIR)/

clean:
	@rm -vf $(OUTFILE_GCC)

distclean: clean
	@rm -vf $(OUTDIR)/$(OUTFILE_GCC)
