# SRCINV implementation v0.6
This doc will show you how to use this framework, how it works,
what it can do, etc.

This document still needs to be perfected.



<span id="0"></span>
### Contents
- [1 - Introduction](#1)
	- [1.1 - The framework](#1.1)
	- [1.2 - The dependencies](#1.2)
	- [1.3 - The target](#1.3)
- [2 - Usage of this framework](#2)
- [3 - Collect information of the target project](#3)
- [4 - Parse the data](#4)
	- [4.1 - ADJUST](#4.1)
	- [4.2 - GET BASE](#4.2)
	- [4.3 - GET DETAIL](#4.3)
	- [4.4 - PHASE4](#4.4)
	- [4.5 - PHASE5](#4.5)
	- [4.6 - PHASE6](#4.6)
- [5 - Use the information to do something](#5)
- [6 - Future work](#6)
- [7 - References](#7)
- [8 - Greetings](#8)



<span id="1"></span>
### 1 - Introduction
SRCINV is short for source code investigator.

Source code(or binary code) is not only for human who read it, but also for the
compiler(or interpreter or CPU).

This framework aims to do code audit automatically for open source projects.

For QA/researchers, they could find different kinds of bugs in their projects.

This framework may also automatically generate samples to test the bug and
patches for the bug.

[Back to Contents](#0)



<span id="1.1"></span>
### 1.1 - The framework
- **Directories**
	- analysis
	> parse resfile(s), and provide some helper functions for hacking to
	> call

	- bin
	> srcinv binaries and modules

	- collect
	> As a compiler plugin, run in target project root directory, collect
	> the compiling data(AST)

	- config
	> runtime configs. Currently in-use file is module.conf

	- core
	> the main process, handle user input, parse configs, load modules, etc.

	- doc
	> Documatations, ChangeLog

	- hacking
	> do anything you want to do. For example, locations that field gid of
	> struct cred get used, list functions in directory or a single souce
	> file, etc.

	- include
	> header files

	- lib
	> provide some compiler functions to use in analysis.

	- testcase
	> some cases that should be handled in analysis.

	- tmp
	> result data: resfile(s), src.saved, log, etc

[Back to Contents](#0)



<span id="1.2"></span>
### 1.2 - The dependencies
The framework could only run on 64bit GNU/Linux systems, and need personality
system call to disable the current process aslr.

The colorful prompt could be incompatible.

Libraries:
	[clib](https://github.com/snorez/clib/)
	ncurses
	readline
	libcapstone

Header files:
	gcc plugin library

[Back to Contents](#0)



<span id="1.3"></span>
### 1.3 - The targets
For now, the framework is only for C projects compiled by GCC.

[Back to Contents](#0)



<span id="2"></span>
### 2 - Usage of this framework
This framework has several levels:
- level 0: SRCINV(the main).
- level 1: collect, analysis, hacking.

Commands for level 0:
```c
SRCINV> help
================== USAGE INFO ==================
help
	Show this help message
quit
	Exit this main process
exit
	Exit this main process
do_sh
	Execute bash command
showlog
	Show current log messages
reload_config
	Reload config
load_srcfile
	(srcid)
flush_srcfile
	Write current src info to srcfile
collect
	Enter collect subshell
analysis
	Enter analysis subshell
hacking
	Enter hacking subshell
================== USAGE END ==================
```

Commands for level 1 - collect:
```c
<1> collect> help
================== USAGE INFO ==================
help
	Show this help message
exit
	Return to the previous shell
quit
	Return to the previous shell
show
	(type) [path]
	type: (SRC/BIN)_(KERN/USER)_(LINUX/...)_(...) check si_core.h for more
	Pick appropriate module and show comment
================== USAGE END ==================
```

Commands for level 1 - analysis:
```c
<1> analysis> help
================== USAGE INFO ==================
help
	Show this help message
exit
	Return to the previous shell
quit
	Return to the previous shell
parse
	(resfile) (kernel) (builtin) (step) (auto_Y)
	Get information of resfile, steps are:
		0 Get all information
		1 Get information adjusted
		2 Get base information
		3 Get detail information
		4 Prepare for step5
		5 Get indirect call information
		6 Check if all GIMPLE_CALL are set
getoffs
	(resfile) (filecnt)
	Count filecnt files and calculate the offset
cmdline
	(resfile) (filepath)
	Show the command line used to compile the file
================== USAGE END ==================
```

Commands for level 1 - hacking:
```c
================== USAGE INFO ==================
help
	Show this help message
exit
	Return to the previous shell
quit
	Return to the previous shell
itersn
	[output_path]
	Traversal all sinodes to stderr/file
havefun
	Run hacking modules
load_sibuf
	(sibuf_addr)
	Loading specific sibuf
list_function
	[dir/file] (format)
	List functions in dir or file
	format:
		0(normal output)
		1(markdown output)
================== USAGE END ==================
```

This framework take a few steps to parse the project: collect the information,
parse the collected data, use the parsed data.

[Back to Contents](#0)



<span id="3"></span>
### 3 - Collect information of the target project
**NOTE: before collecting information of a project, make sure the resfile not
exist yet.**

When compiling a source file, GCC will handle several data formats: AST, GIMPLE,
etc. This framework collects the lower GIMPLEs right at `ALL_IPA_PASSES_END`,
researchers could use these information to parse the project.

Generally, a GCC plugin could only see the information of the current compiling
file. When we put the information of all compiled files together, we could see
the big picture of the project, make it more convenience to parse the project.

For each project, this framework use a src structure to track all information,
which could be:
- sibuf: a list of compiled files
- resfile: a list of all the resfiles for the target project.
> For linux kernel, there could be one vmlinux-resfile which is builtin and
> a bunch of module-resfiles which are non-builtin.
- sinodes: nodes for non-local variables, functions, data types.
> These nodes could be searched by name or by location.
>
> For static file-scope variables, static file-scope functions,
> data types have a location, we use location to search and insert.
>
> For global variables, global functions, and data types with a name but
> without a location, we use name to search and insert.
>
> For data types without location or name, we put them into sibuf.

About GCC plugin, this framework gets the lower GIMPLEs.
```c
source file ---> AST ---> High-level GIMPLE ---> Low-level GIMPLE
```

We collect the GIMPLEs at ALL\_IPA\_PASSES\_END.
```c
all_lowering_passes
|--->	useless		[GIMPLE_PASS]
|--->	mudflap1	[GIMPLE_PASS]
|--->	omplower	[GIMPLE_PASS]
|--->	lower		[GIMPLE_PASS]
|--->	ehopt		[GIMPLE_PASS]
|--->	eh		[GIMPLE_PASS]
|--->	cfg		[GIMPLE_PASS]
...
all_ipa_passes
|--->	visibility	[SIMPLE_IPA_PASS]
...
```

Each source file will have only one chance to call the plugin callback function
at ALL\_IPA\_PASSES\_END. It means that the data of a function we collected
before would not be modified when we collecting some other function's data.

For more about GCC plugin, check [refs[1]](#ref1) or [refs[2]](#ref2).

For C projects, `collect/c.cc` is used when compile the target project. It is
not used in `si_core` process. It is a GCC plugin. When this plugin gets
involved, it detects the current compling file's name and full path, register
a callback function for PLUGIN\_ALL\_IPA\_PASSES\_START event. The resfile is
PAGE\_SIZE aligned.

We can't register PLUGIN\_PRE\_GENERICIZE to handle each tree\_function\_decl,
cause the function may be incompleted.
```c
static void test_func0(void);
static void test_func1(void)
{
	test_func0();
}

static void test_func0(void)
{
	/* test_func0 body */
}
```
In this case, when PLUGIN\_PRE\_GENERICIZE event happens, the function
test\_func1 is handled. However, the tree\_function\_decl for test\_func0 is
not initialized yet.

When the registered callback function gets called, the function body would be
moved to `tree_function_decl->f->cfg` from `tree_function_decl->saved_tree`:
```c
tree_function_decl->saved_tree --->
tree_function_decl->f->gimple_body --->
tree_function_decl->f->cfg
```

When collect data of a var, we focus on non-local variables. GCC provides a
function to test if this is a global var.
```c
static inline bool is_global_var (const_tree t)
{
	return (TREE_STATIC(t) || DECL_EXTERNAL(t));
}
```
for VAR\_DECL, TREE\_STATIC check whether this var has a static storage or not.
DECL\_EXTERNAL check if this var is defined elsewhere.
We add an extra check:
```c
if (is_global_var(node) &&
	((!DECL_CONTEXT(node)) ||
	 (TREE_CODE(DECL_CONTEXT(node)) == TRANSLATION_UNIT_DECL))) {
		objs[start].is_global_var = 1;
	}
```
DECL\_CONTEXT is NULL or the TREE\_CODE of it is TRANSLATION\_UNIT\_DECL, we
take this var as a non-local variable.

The test is for ubuntu 18.04 linux-5.3.y, gcc 8.3.0. The size of the data we
collect is about 30G.

[Back to Contents](#0)



<span id="4"></span>
### 4 - Parse the data
The main feature of the framework is parsing the data. However, it is
the most complicated part.

This is implemented in analysis/analysis.c and analysis/gcc/c.cc(for GCC c
project).

For a better experience, the framework use ncurses to show the parse status
(check pre\_defines.makefile).

To parse the data, we need to load the resfile into memory.
However, the resfile for linux-5.3.y is about 30G. We can not load it all.
Here comes the solution:
- Disable aslr and restart the process.
- Use mmap to load resfile at RESFILE\_BUF\_START, if the resfile current
loaded in memory is larger than RESFILE_BUF_SIZE, unmap the last, and do the
next mmap just at the end of the last memory area.
- For each compiled file, we use a sibuf to track the address where to load
the data, the size, and the offset of the resfile.
- Use `resfile__resfile_load()` to load a compiled file's information.

```c
memory layout:
0x0000000000000000	0x0000000000400000	NULL pages
0x0000000000400000	0x0000000000403000	si_core .text
0x0000000000602000	0x0000000000603000	si_core .rodata ...
0x0000000000603000	0x0000000000605000	si_core .data ...
0x0000000000605000	0x0000000000647000	heap
SRC_BUF_START     	RESFILE_BUF_START 	srcfile
RESFILE_BUF_START 	0x????????????????	resfiles
0x0000700000000000	0x00007fffffffffff	threads libs plugins stack...
```

If SRC\_BUF\_START is 0x100000000, RESFILE\_BUF\_START is 0x1000000000,
the size of src memory area is up to 64G, the size of resfile could be 1024G
or larger.

The size of the src.saved after PHASE6 is 4.7G. It takes about 6 hours to do
all the six phases.

[Back to Contents](#0)



<span id="4.1"></span>
### 4.1 - ADJUST
The data we collect, a lot of pointers in it. We must adjust these pointers
before we read it, locations as well.

Note that, location\_t in GCC is 4 bytes, so we set it an offset value.
use `*(expanded_location *)(sibuf->payload + loc_value)` to get the location.

[Back to Contents](#0)



<span id="4.2"></span>
### 4.2 - GET BASE
Base info include:
- TYPE\_FUNC\_GLOBAL,
- TYPE\_FUNC\_STATIC,
- TYPE\_VAR\_GLOBAL,
- TYPE\_VAR\_STATIC,
- TYPE\_TYPE,
- TYPE\_FILE

Check if the location or name exists in sinodes. If not, alloc a new sinode.
If exists, and searched by name, should check if the name conflict(weak
symbols?).

[Back to Contents](#0)



<span id="4.3"></span>
### 4.3 - GET DETAIL
For TYPE\_TYPE, need to know the type it points to, or the size, or the fields.

For TYPE\_VAR\_\*, just get the type of it.

For function, get the type of return value, the arguments list, and the
function body.

[Back to Contents](#0)



<span id="4.4"></span>
### 4.4 - PHASE4
PHASE4, set possible\_list for each non-local variables, get variables and
functions(not direct call) marked(use\_at\_list).

[Back to Contents](#0)



<span id="4.5"></span>
### 4.5 - PHASE5
PHASE5, handle marked functions, then try best to trace calls.

A direct call is the second op of GIMPLE_CALL statement is addr of a function.
If the second op of GIMPLE\_CALL is VAR\_DECL or PARM\_DECL, it is an indirect
call.

NOTE, we are dealing with tree\_ssa\_name now.

[Back to Contents](#0)



<span id="4.6"></span>
### 4.6 - PHASE6
Nothing to do here, do phase5 again.

[Back to Contents](#0)



<span id="5"></span>
### 5 - Use the information to do something
We need to write some plugins to use parsed data. As we collect the lower
GIMPLEs, these are what we handle in hacking plugins.

For example(obsolete):
```c
static void test_func(int flag)
{
	int need_free;
	char *buf;
	if (flag) {
		buf = (char *)malloc(0x10);
		need_free = 1;
	}

	/* do something here */

	if (need_free)
		free(buf);
}
```
plugins/uninit.cc shows how to detect this kind of bugs.

This plugin detects all functions one by one:
- generate all possible code_path of this function
- traversal all local variables(not static)
	- traversal code_paths, find first position this variable used
	- check if the first used statement is to read this variable. [The demo](https://www.youtube.com/watch?v=anNoHjrYqVc)

[Back to Contents](#0)



<span id="6"></span>
### 6 - Future work
Check [TODO.md](https://github.com/hardenedlinux/srcinv/blob/master/doc/TODO.md)

[Back to Contents](#0)



<span id="7"></span>
### 7 - References
[0] [srcinv project homepage](https://github.com/hardenedlinux/srcinv/)

<span id="ref1"></span>
[1] [GNU Compiler Collection Internals](https://gcc.gnu.org/onlinedocs/gcc-8.3.0/gccint.pdf)

<span id="ref2"></span>
[2] [GCC source code](http://mirrors.concertpass.com/gcc/releases/gcc-8.3.0/gcc-8.3.0.tar.gz)

[3] [深入分析GCC](https://www.amazon.cn/dp/B06XCPZFKD)

[4] [gcc plugins for linux kernel](https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git/tree/scripts/gcc-plugins?h=v4.14.85)

[Back to Contents](#0)


<span id="8"></span>
### 8 - Greetings
Thanks to the author of <<深入分析GCC>> for helping me to understand the
inside of GCC.

Thanks to PYQ and other workmates, with your understanding and support, I can
focus on this framework.

Thanks to CG for his support during the development of the framework.

Certainly... Thanks to all the people who managed to read the whole text.

There is still a lot of work to be done. Ideas or contributions are always
welcome. Feel free to send push requests.

[Back to Contents](#0)
