#!/bin/bash

# This is a simple configure script.
# Later, may switch to use config tools.

DEF_GCC_PLUGIN_BASE=/usr/lib/gcc/x86_64-linux-gnu

usage()
{
	echo "Usage: $0 (CLIB_PATH) (SRCINV_ROOT) [GCC_PLUGIN_BASE]"
	echo "    GCC_PLUGIN_BASE: Default: $DEF_GCC_PLUGIN_BASE"
}

gen_file()
{
	OPTFILE=$4

	CONTENT=$''

	CONTENT+=$'SELF_CFLAGS_N=2\n'
	CONTENT+=$'ANALYSIS_THREAD_N=0x10\n'
	CONTENT+=$'\n'

	CONTENT+=$'export MAKE_OPT := --no-print-directory\n'
	CONTENT+=$'export Q := @\n'
	CONTENT+=$'export CC = gcc\n'
	CONTENT+=$'export CXX = g++\n'
	CONTENT+=$'export MAKE = make\n'
	CONTENT+=$'export RM = rm -f\n'
	CONTENT+=$'export INSTALL = install\n'
	CONTENT+=$'export CC_ECHO = 	"  CC   "\n'
	CONTENT+=$'export CXX_ECHO = 	"  CXX  "\n'
	CONTENT+=$'export LD_ECHO = 	"  LD   "\n'
	CONTENT+=$'export GEN_ECHO = 	"  GEN  "\n'
	CONTENT+=$'export CLEAN_ECHO = 	" CLEAN "\n'
	CONTENT+=$'export INSTALL_ECHO = 	"INSTALL"\n'
	CONTENT+=$'export SRC_ECHO = 	" <== "\n'
	CONTENT+=$'\n'

	CONTENT+=$'ifeq ($(Q), @)\n'
	CONTENT+=$'export MAKE_OPT += -s\n'
	CONTENT+=$'endif\n'
	CONTENT+=$'\n'

	CONTENT+=$'export CLIB_PATH='$1$'\n'
	CONTENT+=$'export SRCINV_ROOT='$2$'\n'
	CONTENT+=$'\n'

	CONTENT+=$'GCC_VER_MAJ:=$(shell expr `gcc -dumpversion | cut -f1 -d.`)\n'
	CONTENT+=$'GCC_PLUGIN_BASE='$3$'\n'
	CONTENT+=$'export GCC_PLUGIN_INC=$(GCC_PLUGIN_BASE)/$(GCC_VER_MAJ)/plugin/include\n'
	CONTENT+=$'\n'

	CONTENT+=$'SELF_CFLAGS=\n'
	CONTENT+=$'ifeq ($(SELF_CFLAGS_N), 0)\n'
	CONTENT+=$'SELF_CFLAGS=-g\n'
	CONTENT+=$'endif\n'
	CONTENT+=$'SELF_CFLAGS+=-Wall -O$(SELF_CFLAGS_N)\n'
	CONTENT+=$'SELF_CFLAGS+=-DCONFIG_ANALYSIS_THREAD=$(ANALYSIS_THREAD_N)\n'
	CONTENT+=$'SELF_CFLAGS+=-DCONFIG_DEBUG_MODE=1\n'
	CONTENT+=$'SELF_CFLAGS+=-DHAVE_CLIB_DBG_FUNC\n'
	CONTENT+=$'#SELF_CFLAGS+=-DUSE_NCURSES\n'
	CONTENT+=$'\n'

	CONTENT+=$'#SELF_CFLAGS+=-Wno-packed-not-aligned\n'
	CONTENT+=$'#SELF_CFLAGS+=-fno-omit-frame-pointer\n'
	CONTENT+=$'ifeq "$(GCC_VER_MAJ)" "9"\n'
	CONTENT+=$'\tSELF_CFLAGS+=-Wno-address-of-packed-member\n'
	CONTENT+=$'endif\n'
	CONTENT+=$'\n'

	CONTENT+=$'#SELF_CFLAGS+=-DCONFIG_THREAD_STACKSZ=0x20*1024*1024\n'
	CONTENT+=$'\n'

	CONTENT+=$'#SELF_CFLAGS+=-DCONFIG_ID_VALUE_BITS=28\n'
	CONTENT+=$'#SELF_CFLAGS+=-DCONFIG_ID_TYPE_BITS=4\n'
	CONTENT+=$'\n'

	CONTENT+=$'#SELF_CFLAGS+=-DCONFIG_SRC_BUF_START=0x100000000\n'
	CONTENT+=$'#SELF_CFLAGS+=-DCONFIG_SRC_BUF_BLKSZ=0x10000000\n'
	CONTENT+=$'#SELF_CFLAGS+=-DCONFIG_SRC_BUF_END=0x300000000\n'
	CONTENT+=$'#SELF_CFLAGS+=-DCONFIG_RESFILE_BUF_START=0x1000000000\n'
	CONTENT+=$'#SELF_CFLAGS+=-DCONFIG_RESFILE_BUF_SIZE=0x80000000\n'
	CONTENT+=$'#SELF_CFLAGS+=-DCONFIG_SIBUF_LOADED_MAX=0x100000000\n'
	CONTENT+=$'\n'

	CONTENT+=$'#SELF_CFLAGS+=-DCONFIG_SI_PATH_MAX=128\n'
	CONTENT+=$'#SELF_CFLAGS+=-DCONFIG_SRC_ID_LEN=4\n'
	CONTENT+=$'\n'

	CONTENT+=$'#SELF_CFLAGS+=-DCONFIG_MAX_OBJS_PER_FILE=0x800000\n'
	CONTENT+=$'#SELF_CFLAGS+=-DCONFIG_MAX_SIZE_PER_FILE=0x8000000\n'
	CONTENT+=$'\n'

	CONTENT+=$'#SELF_CFLAGS+=-DCONFIG_SAVED_SRC="src.saved"\n'
	CONTENT+=$'\n'

	CONTENT+=$'#SELF_CFLAGS+=-DGCC_CONTAIN_FREE_SSANAMES\n'
	CONTENT+=$'\n'

	CONTENT+=$'export CFLAGS=-std=gnu17 $(SELF_CFLAGS) $(EXTRA_CFLAGS)\n'
	CONTENT+=$'export CXXFLAGS=-std=gnu++17 $(SELF_CFLAGS) $(EXTRA_CFLAGS)\n'
	CONTENT+=$'\n'

	CONTENT+=$'export ARCH=$(shell getconf LONG_BIT)\n'
	CONTENT+=$'export CLIB_INC=$(CLIB_PATH)/include\n'
	CONTENT+=$'export CLIB_LIB=$(CLIB_PATH)/lib\n'
	CONTENT+=$'export CLIB_SO=clib$(ARCH)\n'
	CONTENT+=$'export SRCINV_BIN=$(SRCINV_ROOT)/bin\n'
	CONTENT+=$'export SRCINV_INC=$(SRCINV_ROOT)/include\n'
	CONTENT+=$'export TOPDIR=$(SRCINV_ROOT)\n'
	CONTENT+=$'\n'

	CONTENT+=$'GCC_VER_MATCH := $(shell expr `gcc -dumpversion | cut -f1 -d.` \== `g++ -dumpversion | cut -f1 -d.`)\n'
	CONTENT+=$'ifeq "$(GCC_VER_MATCH)" "0"\n'
	CONTENT+=$'$(error gcc version not match g++ version)\n'
	CONTENT+=$'endif'

	echo "$CONTENT" > $OPTFILE
}

main()
{
	if [[ $# != 2 && $# != 3 ]]; then
		usage
		exit 1
	fi

	CLIB_PATH=$(realpath $1)
	SRCINV_ROOT=$(realpath $2)
	GCC_PLUGIN_BASE=$DEF_GCC_PLUGIN_BASE
	OPTFILE=Makefile.opt

	if [[ $# == 3 ]]; then
		GCC_PLUGIN_BASE=$3
	fi

	echo "Using"
	echo "    CLIB_PATH=$CLIB_PATH"
	echo "    SRCINV_ROOT=$SRCINV_ROOT"
	echo "    GCC_PLUGIN_BASE=$GCC_PLUGIN_BASE"
	echo "to generate $OPTFILE..."
	gen_file $CLIB_PATH $SRCINV_ROOT $GCC_PLUGIN_BASE $OPTFILE
	echo "$OPTFILE ready."
	echo ""
	echo "Now, you can run 'make distclean && make && make install'"
}

main $@
