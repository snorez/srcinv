+ For Linux Kernel:
	+ get symbols in .s/.S files, to build the full call chains
	+ add mitigations detection
	+ ...
+ Trace variables, where it comes from.
+ Identify the actions, read or write, or get the address of it.
+ Generate code_pathes for two different functions.
+ The dependencies of code_pathes. For example, sys_read need the result of sys_open.
+ Generate samples for a specified code_path.
+ Generate patches for some kind of bugs.
+ More program language support.
+ ...
