# SRCINV v0.6
SRCINV, a source code audit tool.
Tested linux-5.3.y with gcc 8.3.0, both vmlinux and single module.

Two branches: [master](https://github.com/hardenedlinux/srcinv/tree/master) and [dev](https://github.com/hardenedlinux/srcinv/tree/dev)

[Implementation English doc](https://github.com/hardenedlinux/srcinv/blob/master/doc/README_en.md)

### Build srcinv
Dependencies to build this project:
+	libncurses
+	libreadline
+	libcapstone
+	[clib: use the latest version](https://github.com/snorez/clib/)
+	gcc-plugin, test gcc/g++ 8.3.0

About SELF\_CFLAGS in the main Makefile:
- `CLIB_PATH`: path to clib
- `SRCINV_ROOT`: path to srcinv
- `GCC_PLUGIN_INC`: path to gcc plugin headers folder
- `CONFIG_ANALYSIS_THREAD`: how many threads to parse resfile
- `CONFIG_DEBUG_MOODE`: output more messages
- `HAVE_CLIB_DBG_FUNC`: multi-thread backtrace support
- `USE_NCURSES`: use ncurses to show detail of each phase
- `Wno-packed-not-aligned`: not used
- `fno-omit-frame-pointer`: not used
- `CONFIG_THREAD_STACKSZ`: the size of thread to parse
- `CONFIG_ID_VALUE_BITS`: bits to represent the value of siid
- `CONFIG_ID_TYPE_BITS`: bits to represent the type of siid
- `CONFIG_SRC_BUF_START`: start of the src memory area, the global si pointer
- `CONFIG_SRC_BUF_BLKSZ`: the size of each time we expand the src memory area
- `CONFIG_SRC_BUF_END`: end of the src memory area
- `CONFIG_RESFILE_BUF_START`: start of resfile area, where we load the resfile
- `CONFIG_RESFILE_BUF_SIZE`: size of each time we expand resfile area
- `CONFIG_SIBUF_LOADED_MAX`: how many source files(sibuf) mmap into memory
- `CONFIG_SI_PATH_MAX`: length of src path
- `CONFIG_SRC_ID_LEN`: length of src id
- `CONFIG_MAX_OBJS_PER_FILE`: max objects we collect for each source file
- `CONFIG_MAX_SIZE_PER_FILE`: max size for each source file
- `CONFIG_SAVED_SRC`: the filename to save the src content
- `GCC_CONTAIN_FREE_SSANAMES`: set if you want to collect the freed ssanames

Run `make` and `make install`

### Usage
**NOTE**: you should modify config/module.json before you want to use a
specific module. e.g. you code a new module for hacking, you should add it
into config/module.json file before running it.

I **HIGHLY RECOMMEND** you to do all the six steps, till you get the
`src.saved` file. I still can not figure out why it fails when I try
to skip over STEP1. So I just put it on TODO lists.

- **collect**: Do this in the target project root directory, not srcinv root.
	- Each `make` should generate only ONE executable file.
	- For a project that may generate more than one executable file, you need to modify the Makefile(s), and generate them one by one.
	- Example, for linux kernel
		- `make mrproper`
		- `make localmodconfig` to prepare the `.config` file
		- `make EXTRA_CFLAGS+='-fplugin=/path/to/srcinv/collect/c.so -fplugin-arg-c-output=/path/to/srcinv/tmp/xxx/resfile' vmlinux -jx` to generate builtin resfile
		- `make EXTRA_CFLAGS+='-fplugin=/path/to/srcinv/collect/c.so -fplugin-arg-c-output=/path/to/srcinv/tmp/xxx/tty.resfile' -C . M=drivers/tty/ modules` to get the tty module resfile

- **analysis**: in srcinv root directory, `./si_core`
	- `load_srcfile xxx`, xxx is the folder in srcinv/tmp where you just put the resfile(s) into
	- `analysis` into analysis mode
	- `help` list supported commands
	- `parse resfile 1 1 0` the first `1` is set for kernel project, the second `1` is for the core(for linux kernel, it is vmlinux; `0` for `tty.resfile`). You can also parse the resfile by:
		- `parse resfile 1 1 1`
		- `parse resfile 1 1 2`
		- `parse resfile 1 1 3`
		- `parse resfile 1 1 4`
		- `parse resfile 1 1 5`
		- `parse resfile 1 1 6`

- **hacking**: do anything you want to do
	- in `SRCINV>` mode, run `hacking`
	- `help` list supported commands



### screenshots parsing linux kernel
![step_1_0](https://github.com/hardenedlinux/srcinv/blob/master/doc/phase1_0.png)
![step_1_1](https://github.com/hardenedlinux/srcinv/blob/master/doc/phase1_1.png)
![step_1_2](https://github.com/hardenedlinux/srcinv/blob/master/doc/phase1_2.png)
![step_1_3](https://github.com/hardenedlinux/srcinv/blob/master/doc/phase1_3.png)
![step_1_4](https://github.com/hardenedlinux/srcinv/blob/master/doc/phase1_4.png)
![step_2_0](https://github.com/hardenedlinux/srcinv/blob/master/doc/phase2_0.png)
![step_2_1](https://github.com/hardenedlinux/srcinv/blob/master/doc/phase2_1.png)
![step_3_0](https://github.com/hardenedlinux/srcinv/blob/master/doc/phase3_0.png)
![step_3_1](https://github.com/hardenedlinux/srcinv/blob/master/doc/phase3_1.png)
![step_3_2](https://github.com/hardenedlinux/srcinv/blob/master/doc/phase3_2.png)
![step_3_3](https://github.com/hardenedlinux/srcinv/blob/master/doc/phase3_3.png)
![step_4_0](https://github.com/hardenedlinux/srcinv/blob/master/doc/phase4_0.png)
![step_4_1](https://github.com/hardenedlinux/srcinv/blob/master/doc/phase4_1.png)
![step_5_0](https://github.com/hardenedlinux/srcinv/blob/master/doc/phase5_0.png)
![step_5_1](https://github.com/hardenedlinux/srcinv/blob/master/doc/phase5_1.png)
![step_5_2](https://github.com/hardenedlinux/srcinv/blob/master/doc/phase5_2.png)
![step_6_0](https://github.com/hardenedlinux/srcinv/blob/master/doc/phase6_0.png)
![indcfg_0](https://github.com/hardenedlinux/srcinv/blob/master/doc/indcfg_0.png)
![indcfg_1](https://github.com/hardenedlinux/srcinv/blob/master/doc/indcfg_1.png)

# TODO
[TODO list](https://github.com/hardenedlinux/srcinv/blob/master/doc/TODO.md)

# LICENSE
This project is under GPL v3 license. See the LICENSE for more details.
