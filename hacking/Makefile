include ../pre_defines.makefile

TARGETS = all install clean distclean

# ==============================================================================
OUTDIR0 = $(SRCINV_BIN)
OUTCORE = hacking.so
INFILE = hacking.c \
	 itersn.c

OUTDIR1 = $(SRCINV_BIN)/hacking
# ==============================================================================
CC0 = gcc
CC0_OPT = -Wall -std=gnu11 -rdynamic -shared -fPIC -D_FILE_OFFSET_BITS=64 $(CC_RELEASE)

CC1 = g++
CC1_OPT=-Wall -std=gnu++11 -rdynamic -shared -fPIC -Wno-literal-suffix -fno-rtti -D_FILE_OFFSET_BITS=64 $(CC_RELEASE)
# ==============================================================================
CC0_SO =

CC1_SO = uninit.so \
	 gensample.so \
	 debuild.so \
	 fuzz.so
# ==============================================================================

all: $(OUTCORE) $(CC0_SO) $(CC1_SO)

$(OUTCORE): $(INFILE)
	$(CC0) $(CC0_OPT) $(INFILE) -I$(CLIB_INC) -I$(SRCINV_INC) -L$(CLIB_LIB) -l$(CLIB_SO) -o $(OUTCORE) -Wl,-rpath $(CLIB_LIB)

$(CC0_SO): %.so : %.c
	$(CC0) $(CC0_OPT) $< -I$(SRCINV_INC) -I$(CLIB_INC) -L$(CLIB_LIB) -l$(CLIB_SO) -lpthread -o $@ -Wl,-rpath $(CLIB_LIB)

$(CC1_SO): %.so : %.cc
	$(CC1) $(CC1_OPT) $< -I$(GCC_PLUGIN_INC) -I$(SRCINV_INC) -I$(CLIB_INC) -L$(CLIB_LIB) -l$(CLIB_SO) -lpthread -o $@ -Wl,-rpath $(CLIB_LIB)

install:
	@cp -v $(OUTCORE) $(OUTDIR0)/
	@cp -v $(CC0_SO) $(CC1_SO) $(OUTDIR1)/

clean:
	@rm -vf $(OUTCORE)
	@rm -vf $(CC0_SO) $(CC1_SO)

distclean: clean
	@rm -vf $(OUTDIR0)/$(OUTCORE)
	@rm -vf $(OUTDIR1)/*.so
