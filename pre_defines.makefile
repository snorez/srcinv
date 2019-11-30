CLIB_PATH=/home/zerons/workspace/clib
SRCINV_ROOT=/home/zerons/workspace/srcinv

GCC_VER_MAJ:=$(shell expr `gcc -dumpversion | cut -f1 -d.`)
GCC_PLUGIN_INC=/usr/lib/gcc/x86_64-linux-gnu/$(GCC_VER_MAJ)/plugin/include
# possible release options: -g -ON -DUSE_NCURSES
CC_RELEASE=-g -O3 -DUSE_NCURSES

#CFLAGS=-Wall -std=gnu11 -Wno-packed-not-aligned
#CPPFLAGS=-Wall -std=gnu++11 -Wno-packed-not-aligned
CFLAGS=-Wall -std=gnu11
CPPFLAGS=-Wall -std=gnu++11

ARCH=$(shell getconf LONG_BIT)
CLIB_INC=$(CLIB_PATH)/include
CLIB_LIB=$(CLIB_PATH)/lib
CLIB_SO=clib$(ARCH)
SRCINV_BIN=$(SRCINV_ROOT)/bin
SRCINV_INC=$(SRCINV_ROOT)/include
