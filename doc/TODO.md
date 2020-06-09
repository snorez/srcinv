### srcinv
+ performance optimization
+ why crash when `si_global_trees` defined in `compiler_gcc.cc`? It seems that
the `si_global_trees` memory area overlaps with the thread stack.
+ why can't I skip over STEP1?

### collect
+ gcc/c.cc
	+ why functions still contain non-null `saved_tree`?
	+ For Linux Kernel:
		+ ~~ get symbols in .s/.S files to complete call chains ~~
		+ add mitigations detection
		> e.g. CONFIG_GCC_PLUGIN_RANDSTRUCT randomizes the layout
		> of (some) kernel-internal structs
		+ ...
	+ ...
+ ...



### analysis
+ why are there some functions like isra.xx part.yy?
	- isra.xx: in gcc/tree-sra.c
	```c
	/*
	 * modify_function(): perform all the modification required in IPA-SRA
	 * it calls cgraph_node->create_version_clone_with_body
	 * in gcc/cgraphclones.c, create_version_clone_with_body()
	 * it calls clone_function_name() to generate a new assemble name
	 *
	 * Later, back in modify_function(), it calls convert_callers() to
	 * modify the call_fndecl to the new function_decl, right?
	 */
	```
	- part.yy: in gcc/ipa-split.c
	```c
	/*
	 * The purpose of this pass is to split function bodies to improve
	 * inlining.  I.e. for function of the form:
	 *
	 * func (...)
	 * {
         *	if (cheap_test)
	 *		something_small
         *	else
	 *		something_big
	 * }
	 *
	 * Produce:
	 *
	 * func.part (...)
	 * {
	 *	something_big
	 * }
	 *
	 * func (...)
	 * {
         *	if (cheap_test)
	 *		something_small
         *	else
	 *		func.part (...);
	 * }
	 *
	 * When func becomes inlinable and when cheap_test is often true,
	 * inlining func, but not fund.part leads to performance improvement
	 * similar as inlining original func while the code size growth is
	 * smaller.
	 *
	 * The pass is organized in three stages:
	 * 1) Collect local info about basic block into BB_INFO structure and
	 * compute function body estimated size and time.
	 * 2) Via DFS walk find all possible basic blocks where we can split
	 * and chose best one.
	 * 3) If split point is found, split at the specified BB by creating
	 * a clone and updating function to call it.
	 *
	 * The decisions what functions to split are in
	 * execute_split_functions and consider_split.
	 *
	 * There are several possible future improvements for this pass
	 * including:
	 *
	 * 1) Splitting to break up large functions
	 * 2) Splitting to reduce stack frame usage
	 * 3) Allow split part of function to use values computed in the
	 * header part. The values needs to be passed to split function,
	 * perhaps via same interface as for nested functions or as argument.
	 * 4) Support for simple rematerialization.  I.e. when split part use
	 * value computed in header from function parameter in very cheap way,
	 * we can just recompute it.
	 * 5) Support splitting of nested functions.
	 * 6) Support non-SSA arguments.
	 * 7) There is nothing preventing us from producing multiple parts of
	 * single function when needed or splitting also the parts.
	 */
	```
	- constprop
	```c
	/*
	 * in gcc/ipa-cp.c, create_specialized_node()
	 * Create a specialized version of NODE with known constants in
	 * KNOWN_CSTS, known contexts in KNOWN_CONTEXTS and known aggregate
	 * value in AGGVALS and redirect all edges in CALLERS to it.
	 *
	 * in gcc/cgraphclones.c, create_virtual_clone()
	 * Create callgraph node clone with new declaration. The actual body
	 * will be copied later at compilation stage.
	 */
	```

+ We gen a name for a special type, what if the type from a expanded macro? e.g. DEFINE\_KFIFO.
+ ~~ rewrite parse\_resfile() ~~
+ gcc\_asm: TODOs
	- indirect calls example, compute the real addr:
	In arch/x86/entry_64.S
	```c
	ENTRY(interrupt_entry)
	UNWIND_HINT_FUNC
	ASM_CLAC
	cld

	testb	$3, CS-ORIG_RAX+8(%rsp)
	jz	1f
	SWAPGS
	FENCE_SWAPGS_USER_ENTRY
	```
	disassemble
	```c
	00000000000008c0 <interrupt_entry>:
	     8c0:	90                   	nop
	     8c1:	90                   	nop
	     8c2:	90                   	nop
	     8c3:	fc                   	cld    
	     8c4:	f6 44 24 18 03       	testb  $0x3,0x18(%rsp)
	     8c9:	74 65                	je     930 <interrupt_entry+0x70>
	     8cb:	ff 15 00 00 00 00    	callq  *0x0(%rip)        # 8d1 <interrupt_entry+0x11>
	     8d1:	90                   	nop
	     8d2:	90                   	nop
	     8d3:	90                   	nop
	```
	the rela entry:
	```c
	0000000008cd  004c00000002 R_X86_64_PC32     0000000000000000 pv_cpu_ops + f4
	```
	The swapgs field in pv_cpu_ops is offset 0xf8(x86_64), however, the
	s_addend is f4 in Elf64_Rela.
	For R_X86_64_PC32, the relocation addr is compute S + A - P, check
	https://refspecs.linuxfoundation.org/elf/x86_64-abi-0.99.pdf.
	S is the address of pv_cpu_ops.
	A is 0xF4.
	P is (8cd(mem need to be modified) - 8d1(next_ip)) = -4;
+ Update resfile and src.saved information once the target project get patched.
+ Update all function xrefs after parsing a new module
+ Backtrace variables, where it comes from.
+ Trace variables, where it get used.
+ Mark actions: read or write, lock or unlock, ...
+ Given two path point, generate the code paths.
+ The dependencies of code paths. For example, `sys_read` need the result of `sys_open`.
+ simulator and state machine
+ More program language support.
+ ...



### hacking
+ Given a path point, generate samples to trigger it.
+ Generate patches for some kind of bugs.
+ ...
