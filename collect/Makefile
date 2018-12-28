# for gcc plugins
# collect all the information we need
#
COLOR_0=\033[03;30m
COLOR_1=\033[0m

ARCH=$(shell getconf LONG_BIT)
CLIB_PATH=/home/zerons/workspace/clib
CLIB_INC=$(CLIB_PATH)/include
CLIB_LIB=$(CLIB_PATH)/lib
CLIB_SO=clib$(ARCH)

CC_DBG=-g
CC_RELEASE=
ifdef ver
ifeq ($(ver), release)
CC_DBG=
CC_RELEASE=-O3
else
ifeq ($(ver), quick_dbg)
CC_DBG=-g
CC_RELEASE = -O3
endif
endif
endif
CC=g++
CC_FLAGS=-Wno-literal-suffix -D_FILE_OFFSET_BITS=64 -fno-rtti $(CC_DBG) $(CC_RELEASE)
CC_COLLECT_OPT=-std=gnu++11 -shared -fPIC -rdynamic $(CC_FLAGS)
CC_INCLUDE=/usr/lib/gcc/x86_64-linux-gnu/6/plugin/include/

C_OUTNAME=c.so
C_INNAME=c.cc
USAGE_INFO=use 'make EXTRA_CFLAGS+=\"-fplugin=.../c.so -fplugin-arg-c-output=...\" -jx vmlinux' to generate vmlinux

TEST_OUTNAME=test.so
TEST_INNAME=test.cc

all: $(C_OUTNAME) $(TEST_OUTNAME) $(COLLECT_OUTNAME1)


$(C_OUTNAME): $(C_INNAME)
	@rm -f $@
	$(CC) $(CC_COLLECT_OPT) $(CC_FLAGS) -I$(CC_INCLUDE) -I$(CLIB_INC) $(C_INNAME) -L$(CLIB_LIB) -l$(CLIB_SO) -o $@ -Wl,-rpath $(CLIB_LIB)
	@echo "$(COLOR_0)$(USAGE_INFO)$(COLOR_1)"


$(TEST_OUTNAME): $(TEST_INNAME)
	@rm -f $@
	$(CC) $(CC_COLLECT_OPT) $(CC_FLAGS) -I$(CC_INCLUDE) -I$(CLIB_INC) $(TEST_INNAME) -o $@


clean:
	@rm -vf ./*.so
