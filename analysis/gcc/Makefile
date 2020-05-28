include ../../pre_defines.makefile

CC = g++
CC_OPT = $(CPPFLAGS) -shared -fPIC -rdynamic -Wno-literal-suffix -fno-rtti -D_FILE_OFFSET_BITS=64

LIBDIR = $(SRCINV_BIN)
COMPILER_GCC_SO = compiler_gcc

OUTDIR = $(SRCINV_BIN)/analysis
TMPOUT=c.so \
       asm.so

OUTFILES=$(addprefix gcc_,$(TMPOUT))
INSTALLS := $(addprefix $(OUTDIR)/,$(OUTFILES))

all: $(OUTFILES)

$(OUTFILES): $(TMPOUT)

$(TMPOUT): %.so: %.cc
	$(CC) $(CC_OPT) -I$(GCC_PLUGIN_INC) -I$(CLIB_INC) -I$(SRCINV_INC) $< -L$(CLIB_LIB) -L$(LIBDIR) -l$(CLIB_SO) -l$(COMPILER_GCC_SO) -o $@ -Wl,-rpath $(CLIB_LIB) -Wl,-rpath $(LIBDIR)
	@cp $@ gcc_$@

install: $(INSTALLS)

$(OUTDIR)/%: %
	install $< $@

clean:
	@rm -vf $(TMPOUT)
	@rm -vf $(OUTFILES)

distclean: clean
	@rm -vf $(INSTALLS)
