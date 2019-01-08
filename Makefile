USAGE="make ver=xxx CLIB_PATH=... GCC_PLUGIN_INCLUDE=..."
ARCH=$(shell getconf LONG_BIT)
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
CC_INCLUDE=./

# main source files
CORE_INNAME= si_core.c
CORE_OUTNAME=si_core

# subfolders
COLLECT_DIR=collect/
PLUGIN_DIR=plugins/

all: PRE_CHECK PREC_HEADER $(CORE_OUTNAME)
	@echo ""
	@make -C $(COLLECT_DIR) ver=$(ver) CLIB_PATH=$(CLIB_PATH) GCC_PLUGIN_INCLUDE=$(GCC_PLUGIN_INCLUDE)
	@echo ""
	@make -C $(PLUGIN_DIR) ver=$(ver) CLIB_PATH=$(CLIB_PATH) GCC_PLUGIN_INCLUDE=$(GCC_PLUGIN_INCLUDE)

PRE_CHECK:
ifndef CLIB_PATH
	$(error usage: $(USAGE))
else
ifndef GCC_PLUGIN_INCLUDE
	$(error usage: $(USAGE))
endif
endif

PREC_HEADER:
	gcc -I$(GCC_PLUGIN_INCLUDE) tree-codes.h -E -P > treecodes.h
	@echo -n "" > defdefines.h
	@echo "#define DEFAULT_PLUGIN_DIR \"$(shell pwd)/plugins\"" >> defdefines.h
	@echo "#define DEFAULT_MIDOUT_DIR \"$(shell pwd)/output\"" >> defdefines.h
	@echo "#define DEFAULT_LOG_FILE \"$(shell pwd)/output/log.txt\"" >> defdefines.h

$(CORE_OUTNAME): $(CORE_INNAME)
	@rm -f $@
	$(CC) $(CC_OPT) -I$(CC_INCLUDE) -I$(CLIB_INC) $(CORE_INNAME) -L$(CLIB_LIB) -ldl -l$(CLIB_SO) -o $@ -Wl,-rpath $(CLIB_LIB)

clean:
	@rm -vf $(CORE_OUTNAME)
	@echo ""
	@make -C $(COLLECT_DIR) clean
	@echo ""
	@make -C $(PLUGIN_DIR) clean
