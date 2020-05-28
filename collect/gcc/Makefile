# for gcc plugins
# collect all the information we need
#
include ../../pre_defines.makefile

CC=g++
CC_OPT=$(CPPFLAGS) -shared -fPIC -rdynamic -Wno-literal-suffix -D_FILE_OFFSET_BITS=64 -fno-rtti

OUTDIR=$(SRCINV_BIN)/collect
TMPOUT=c.so \
	  asm.so \
	  test.so

OUTFILES=$(addprefix gcc_,$(TMPOUT))

INSTALLS := $(addprefix $(OUTDIR)/,$(OUTFILES))

all: $(OUTFILES)

$(OUTFILES): $(TMPOUT)

$(TMPOUT): %.so: %.cc
	$(CC) $(CC_OPT) -I$(GCC_PLUGIN_INC) -I$(CLIB_INC) -I$(SRCINV_INC) $< -L$(CLIB_LIB) -l$(CLIB_SO) -o $@ -Wl,-rpath $(CLIB_LIB)
	@cp $@ gcc_$@

install: $(INSTALLS)

$(OUTDIR)/%: %
	install $< $@

clean:
	@rm -vf $(TMPOUT)
	@rm -vf $(OUTFILES)

distclean: clean
	@rm -vf $(INSTALLS)
