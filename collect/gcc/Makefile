# for gcc plugins
# collect all the information we need
#
include ../../pre_defines.makefile

CC=g++
CC_OPT=-Wall -std=gnu++11 -shared -fPIC -rdynamic -Wno-literal-suffix -D_FILE_OFFSET_BITS=64 -fno-rtti $(CC_RELEASE)

OUTDIR=$(SRCINV_BIN)/collect
C_OUTNAME=gcc_c.so
C_INNAME=c.cc

all: $(C_OUTNAME)

$(C_OUTNAME): $(C_INNAME)
	$(CC) $(CC_OPT) -I$(GCC_PLUGIN_INC) -I$(CLIB_INC) -I$(SRCINV_INC) $(C_INNAME) -L$(CLIB_LIB) -l$(CLIB_SO) -o $(C_OUTNAME) -Wl,-rpath $(CLIB_LIB)

install:
	@cp -v $(C_OUTNAME) $(OUTDIR)/

clean:
	@rm -vf $(C_OUTNAME)

distclean: clean
	@rm -vf $(OUTDIR)/$(C_OUTNAME)
