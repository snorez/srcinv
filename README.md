# SRCINV

[中文版](https://github.com/hardenedlinux/srcinv/blob/master/doc/README.zh)

[English original](https://github.com/hardenedlinux/srcinv/blob/master/doc/README.en)

[English](https://github.com/hardenedlinux/srcinv/blob/master/doc/README_en.md)

# Build this project
To build this project, you need several libraries:
+	libncurses
+	libreadline
+	libcapstone
+	[clib: use the latest version](https://github.com/snorez/clib/)
+	gcc-plugin, test gcc/g++ 5.4.0/6.3.0

Modify CLIB_PATH and gcc plugin include path in ./Makefile ./plugins/Makefile ./collect/Makefile

Run `make` or `make ver=[quick_dbg|release]`

# Usage
### NOTICE
if a project generates several executable files, run `make` separately.

One resfile is ***JUST*** for one executable file. Thus, you may need to modify the Makefiles in the project.

For linux kernel, vmlinux and .ko are executable files.

### Example, for linux kernel(4.14.x)
+ change to the target directory, `make mrproper`
+ prepare .config file, `make oldconfig`
+ `make EXTRA_CFLAGS+='-fplugin=/.../.../srcinv/collect/c.so -fplugin-arg-c-output=/.../.../.../xxx' vmlinux -jx`
+ back into srcinv directory, `./si_core`
+ `getinfo /.../.../.../xxx 1 1 0`
	+ also, we could split steps:
		+ `getinfo /.../.../.../xxx 1 1 1` `quit`
			here, we can make a backup for srcoutput file, like 4.14_src1
		+ `./si_core` `load_srcfile`
		+ `getinfo /.../.../.../xxx 1 1 2` `quit` and backup, 4.14_src2
		+ `getinfo /.../.../.../xxx 1 1 3` `quit`, 4.14_src3
		+ ...
	+ When something goes wrong, you can rebuild this project without `ver=`,
		restore the 4.14_srcx file,
		and use `gdb ./si_core` `load_srcfile`, to find out the BUG and
		fix it.

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
