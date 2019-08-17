CLIB_PATH=/home/zerons/workspace/clib
GCC_PLUGIN_INC=/usr/lib/gcc/x86_64-linux-gnu/6/plugin/include
SRCINV_ROOT=/home/zerons/workspace/srcinv
# possible release options: -g -ON -DUSE_NCURSES
CC_RELEASE=-g -O3 -DUSE_NCURSES

ARCH=$(shell getconf LONG_BIT)
CLIB_INC=$(CLIB_PATH)/include
CLIB_LIB=$(CLIB_PATH)/lib
CLIB_SO=clib$(ARCH)
SRCINV_BIN=$(SRCINV_ROOT)/bin
SRCINV_INC=$(SRCINV_ROOT)/include
