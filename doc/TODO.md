### collect
+ gcc/c.cc
	+ why functions still contain non-null `saved_tree`?
	+ For Linux Kernel:
		+ get symbols in .s/.S files to complete call chains
		+ add mitigations detection
		> e.g. CONFIG_GCC_PLUGIN_RANDSTRUCT randomizes the layout
		> of (some) kernel-internal structs
		+ ...
	+ ...
+ ...



### analysis
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
