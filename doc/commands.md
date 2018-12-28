# SRCINV commands manual
If a parameter is in parentheses, it must be specified.
If a parameter is in bracket, it should be specified when needed.

+ help
> Show current usable commands

+ exit/quit
> Exit this main process

+ load_plugin (id | path) [plugin_args]
> Load a plugin
> the (id | path) arguments could get from `list_plugin` command.

+ unload_plugin (id | path)
> Unload a plugin
> the (id | path) arguments could get from `list_plugin` command.

+ reload_plugin (id | path) [plugin_args]
> Reload a plugin
> the (id | path) argument could get from `list_plugin` command.

+ list_plugin
> List all plugins
> Output format:
>> `plugin_id` `refcount` `loaded?` `plugin_name` `plugin_path`

+ do_make (c | cpp | ...) (so_path) (project_path) (out_resfile) [extra_info]
> Make target project
> (c | cpp | ...): which language the project use
> (so_path): the plugin used to collect info, in collect/, with name c.so cpp.so ...
> (project_path): the project we want to collect information
> (out_resfile): where to store the resfile
> [extra_info]: some other arguments the target Makefile takes

+ do_sh
> Execute a bash command
> Example: do_sh ls

+ set_plugin_dir
> Setup the plugin directory
> (plugin_dir): set a new plugin dir. The plugins in the original are still in count

+ showlog
> Show the log messages

+ load_srcfile [srcfile]
> Load srcfile
> Once you finish parsing the resfile(`getinfo`), you can exit the process and
> restart it, then `load_srcfile`, you could use the information directly, like
> execute `staticchk`

+ set_srcfile (srcfile_path)
> Set the path of srcfile
> (srcfile_path): the path of the srcfile
> When use `quit` or `exit` to exit the process, the process would write current
> src information to srcfile, which you can use it the next time this process start.

+ getinfo (resfile) (is_builtin) (linux_kernel?) (step)
> Parse and get info from resfile
> (resfile): the resfile need to be parsed
> (is_builtin): for linux kernel, is the resfile for vmlinux?
> (linux_kernel?): is the project linux kernel or userspace program?
> (step):
>> 0 Get all information
>> 1 Get base information
>> 2 Get detail information
>> 3 Get xrefs information
>> 4 Get indirect call information
>> 5 Check if all GIMPLE_CALL are set


+ staticchk
> Static check
> this will run all registered plugins' callback functions to test the project

+ itersn [output_path]
> Traversal all sinodes to stderr/file
