ARCH=$(shell getconf LONG_BIT)
CLIB_PATH=/home/zerons/workspace/clib
CLIB_INC=$(CLIB_PATH)/include
CLIB_LIB=$(CLIB_PATH)/lib
CLIB_SO=clib$(ARCH)

CC_DBG=-g
CC_RELEASE =
ifdef ver
ifeq ($(ver), release)
CC_DBG=
CC_RELEASE = -O3
else
ifeq ($(ver), quick_dbg)
CC_DBG=-g
CC_RELEASE = -O3
endif
endif
endif
CC=gcc
#CC_FLAGS=-Wno-literal-suffix -fno-rtti
CC_FLAGS=-Wall $(CC_DBG) $(CC_RELEASE)
CC_OPT=-std=gnu11 -rdynamic $(CC_FLAGS)
GCC_PLUGIN_INCLUDE=/usr/lib/gcc/x86_64-linux-gnu/6/plugin/include/
CC_INCLUDE=./

# needed header
PREC_HEADER=treecodes.h

# main source files
CORE_INNAME= si_core.c
CORE_OUTNAME=si_core

# subfolders
COLLECT_DIR=collect/
PLUGIN_DIR=plugins/

all: $(PREC_HEADER) $(CORE_OUTNAME)
	@echo ""
	@make -C $(COLLECT_DIR) ver=$(ver)
	@echo ""
	@make -C $(PLUGIN_DIR) ver=$(ver)

$(CORE_OUTNAME): $(CORE_INNAME)
	@rm -f $@
	$(CC) $(CC_OPT) -I$(CC_INCLUDE) -I$(CLIB_INC) $(CORE_INNAME) -L$(CLIB_LIB) -ldl -l$(CLIB_SO) -o $@ -Wl,-rpath $(CLIB_LIB)

$(PREC_HEADER): tree-codes.h
	gcc -I$(GCC_PLUGIN_INCLUDE) $< -E -P > $@

clean:
	@rm -vf $(PREC_HEADER)
	@rm -vf $(CORE_OUTNAME)
	@echo ""
	@make -C $(COLLECT_DIR) clean
	@echo ""
	@make -C $(PLUGIN_DIR) clean
