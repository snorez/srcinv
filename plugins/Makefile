# each source file should generate one executable file

ARCH=$(shell getconf LONG_BIT)
CLIB_PATH=/home/zerons/workspace/clib
CLIB_INC=$(CLIB_PATH)/include
CLIB_LIB=$(CLIB_PATH)/lib
CLIB_SO=clib$(ARCH)
TMP=/tmp/srcinv_plugin
vpath %.c ./
vpath %.o $(TMP)/
vpath %.so ./

NCURSES_FLAG=
CC_DBG=-g
CC_RELEASE=
ifdef ver
ifeq ($(ver), release)
NCURSES_FLAG=-DUSE_NCURSES
CC_DBG=
CC_RELEASE=-O3
else
ifeq ($(ver), quick_dbg)
CC_DBG=-g
CC_RELEASE = -O3
endif
endif
endif
CC0=gcc $(NCURSES_FLAG)
CC1=g++
CC0_FLAGS=-Wall -D_FILE_OFFSET_BITS=64 $(CC_DBG) $(CC_RELEASE)
CC1_FLAGS=-Wno-literal-suffix -fno-rtti -D_FILE_OFFSET_BITS=64 $(CC_DBG) $(CC_RELEASE)
CC0_OPT=-std=gnu11 -rdynamic -shared -fPIC $(CC0_FLAGS)
CC1_OPT=-std=gnu++11 -rdynamic -shared -fPIC $(CC1_FLAGS)
CC0_INCLUDE=../
CC1_INCLUDE=/usr/lib/gcc/x86_64-linux-gnu/6/plugin/include

CC0_OBJ=src.o \
	sinode.o \
	sibuf.o \
	resfile.o \
	getinfo.o \
	itersn.o \
	sn_load.o
CC1_OBJ=utils.o \
	c.o \
	staticchk.o \
	uninit.o \
	gensample.o \
	debuild.o \
	fuzz.o \
	test.o

CC0_SO=src.so \
       sinode.so \
       sibuf.so \
       resfile.so \
       getinfo.so \
       itersn.so \
       sn_load.so
CC1_SO=utils.so \
       c.so \
       staticchk.so \
       uninit.so \
       gensample.so \
       debuild.so \
       fuzz.so \
       test.so

all: prepare $(CC0_SO) $(CC1_SO)

prepare:
	@mkdir -p $(TMP)

$(CC0_SO): %.so : %.o
	$(CC0) $(CC0_OPT) $(TMP)/$< -L$(CLIB_LIB) -l$(CLIB_SO) -o $@ -Wl,-rpath $(CLIB_LIB)

$(CC1_SO): %.so : %.o
	$(CC1) $(CC1_OPT) $(TMP)/$< -L$(CLIB_LIB) -l$(CLIB_SO) -o $@ -Wl,-rpath $(CLIB_LIB)

$(CC0_OBJ): %.o : %.c
	$(CC0) $(CC0_OPT) -c -I$(CC0_INCLUDE) -I$(CLIB_INC) -lpthread $< -o $(TMP)/$@

$(CC1_OBJ): %.o : %.cc
	$(CC1) $(CC1_OPT) -c -I$(CC1_INCLUDE) -I$(CLIB_INC) $< -o $(TMP)/$@

clean:
	@rm -vf $(TMP)/*.o
	@rm -vf ./*.so
