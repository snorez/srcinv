/*
 * main process
 *
 * Copyright (C) 2018  zerons
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

/*
 * main interface, controller
 * show what this program could do.
 *	cmd			info
 *	help			show help message
 *	load_plugin		load some plugins
 *	...
 * put everything does real things in plugin folder
 */
#define	_GNU_SOURCE
#include "./si_core.h"

/*
 * ************************************************************************
 * bash command prompt
 * ************************************************************************
 */
/*
 * https://unix.stackexchange.com/questions/105958/terminal-
 * prompt-not-wrapping-correctly
 */
#define CHAR_B		'\033'
#define	CHAR_LEFT	'\001'
#define	CHAR_RIGHT	'\002'
static char term_prompt[] = {
	CHAR_LEFT,CHAR_B,'[','0','m',CHAR_B,'[','0','4',';','4','2','m',CHAR_RIGHT,
	'S','R','C','I','N', 'V', '>',
	CHAR_LEFT,CHAR_B,'[','0','m',CHAR_RIGHT,
	' ',
};

/*
 * ************************************************************************
 * srcinv default cmds
 * ************************************************************************
 */
void si_help_usage(void)
{
	fprintf(stdout, "\t\t\tShow this help message\n");
}
long si_help(int argc, char *argv[])
{
	clib_cmd_usages();
	return 0;
}

/*
 * si_exit: exit the main process
 */
void si_exit_usage(void)
{
	fprintf(stdout, "\t\t\tExit this main process\n");
}
long si_exit(int argc, char *argv[])
{
	exit(0);
	return 0;
}

/*
 * si_load_plugin: load a specific plugin, and execute the target
 * `clib_plugin_init(struct clib_plugin *, argv)` function,
 * pass argv[3:] to `clib_plugin_init`
 *
 * argv layout:
 *	[0]: load_plugin
 *	[1]: plugin path
 *	[2]: plugin args
 */
void si_load_plugin_usage(void)
{
	fprintf(stdout, "\t(plugin_path) [plugin_args...]\n");
}
long si_load_plugin(int argc, char *argv[])
{
	if (argc < 2) {
		err_dbg(0, err_fmt("argc invalid"));
		si_load_plugin_usage();
		return -1;
	}

	int err = clib_plugin_load(argc-1, &argv[1]);
	if (err) {
		err_dbg(0, err_fmt("clib_plugin_load err"));
		return -1;
	}

	return 0;
}

/*
 * si_unload_plugin: unload specific plugin, could be id or path
 */
void si_unload_plugin_usage(void)
{
	fprintf(stdout, "\t(id|path)\tUnload specific plugin\n");
}
long si_unload_plugin(int argc, char *argv[])
{
	int err;
	if (argc != 2) {
		err_dbg(0, err_fmt("argc invalid"));
		si_unload_plugin_usage();
		return -1;
	}

	err = clib_plugin_unload(argc-1, &argv[1]);
	if (err) {
		err_dbg(0, err_fmt("clib_plugin_unload err"));
		return -1;
	}

	return 0;
}

/*
 * si_reload_plugin: reload a plugin, just unload it and load it again
 */
void si_reload_plugin_usage(void)
{
	fprintf(stdout, "\t(id|path) [plugin_args]\tReload target plugin\n");
}
long si_reload_plugin(int argc, char *argv[])
{
	int err;
	if (argc != 2) {
		err_dbg(0, err_fmt("argc invalid"));
		si_reload_plugin_usage();
		return -1;
	}

	err = clib_plugin_reload(argc-1, &argv[1]);
	if (err) {
		err_dbg(0, err_fmt("clib_plugin_reload err"));
		return -1;
	}

	return 0;
}

/*
 * si_list_plugin: list current loaded plugins, and id them
 */
void si_list_plugin_usage(void)
{
	fprintf(stdout, "\t\t\tShow current loaded plugins\n");
}
long si_list_plugin(int argc, char *argv[])
{
	clib_plugin_print();
	return 0;
}

#define	DO_CMD_LEN	2048
void si_do_make_usage(void)
{
	fprintf(stdout, "\t(c|cpp|...) (sodir) (projectdir) (outfile) [extras]\n"
			"\t\t\tBuild target project, make sure Makefile has "
			"EXTRA_CFLAGS\n");
}
long si_do_make(int argc, char *argv[])
{
	int err = 0;
	if ((argc != 5) && (argc != 6)) {
		err_dbg(0, err_fmt("argc invalid"));
		si_do_make_usage();
		return -1;
	}

	char cmd[DO_CMD_LEN];
	memset(cmd, 0, DO_CMD_LEN);
	if (argc == 6)
		snprintf(cmd, DO_CMD_LEN-1, "cd %s;make clean;make EXTRA_CFLAGS+="
				"'-fplugin=%s/%s.so -fplugin-arg-%s-output=%s' %s",
				argv[3], argv[2], argv[1], argv[1], argv[4], argv[5]);
	else
		snprintf(cmd, DO_CMD_LEN-1, "cd %s;make clean;make EXTRA_CFLAGS+="
				"'-fplugin=%s/%s.so -fplugin-arg-%s-output=%s'",
				argv[3], argv[2], argv[1], argv[1], argv[4]);

	err = system(cmd);
	if (err) {
		err_dbg(0, err_fmt("run command \"%s\" err"), cmd);
		return -1;
	}
	return 0;
}

void si_do_sh_usage(void)
{
	fprintf(stdout, "\t\t\tExecute bash command\n");
}

long si_do_sh(int argc, char *argv[])
{
	int err = 0;
	if (argc <= 1) {
		err_dbg(0, err_fmt("argc invalid"));
		si_do_sh_usage();
		return -1;
	}

#ifndef USE_SYSTEM_FUNC
	int pid;
	if ((pid = fork()) < 0) {
		err_dbg(1, err_fmt("fork"));
		return -1;
	} else if (pid == 0) {
		extern char **environ;
		err = execvpe(argv[1], &argv[1], environ);
		if (err == -1)
			err_exit(1, err_fmt("execvpe err"));
	} else {
		waitpid(pid, NULL, 0);
	}
	return 0;
#else
	char cmd[DO_CMD_LEN];
	memset(cmd, 0, DO_CMD_LEN);
	for (int i = 1; i < argc; i++) {
		snprintf(cmd+strlen(cmd), DO_CMD_LEN-1-strlen(cmd), "%s ", argv[i]);
	}

	err = system(cmd);
	if (err) {
		err_dbg(0, err_fmt("run bash command \"%s\" err"), cmd);
		return -1;
	}
	return 0;
#endif
}

void si_set_plugin_dir_usage(void)
{
	fprintf(stdout, "\t(plugin_dir)\tSet the plugin directory, "
			"the original still count\n");
}
long si_set_plugin_dir(int argc, char *argv[])
{
	int err;
	if (argc != 2) {
		err_dbg(0, err_fmt("argc invalid"));
		return -1;
	}

	err = clib_plugin_init_root(argv[1]);
	if (err) {
		err_dbg(0, err_fmt("clib_plugin_init_root err"));
		return -1;
	}
	return 0;
}

void si_show_log_usage(void)
{
	fprintf(stdout, "\tShow current log messages\n");
}
long si_show_log(int argc, char *argv[])
{
	int err;
	if (argc != 1) {
		err_dbg(0, err_fmt("argc invalid"));
		return -1;
	}

	int fd = open(DEFAULT_LOG_FILE, O_RDONLY);
	if (fd == -1) {
		err_dbg(1, err_fmt("open err"));
		return -1;
	}

	struct stat st;
	err = fstat(fd, &st);
	if (err == -1) {
		err_dbg(1, err_fmt("fstat err"));
		close(fd);
		return -1;
	}

	char *buf = (char *)xmalloc(st.st_size+1);
	if (!buf) {
		err_dbg(0, err_fmt("xmalloc err"));
		close(fd);
		return -1;
	}

	err = read(fd, buf, st.st_size);
	if (err == -1) {
		err_dbg(1, err_fmt("read err"));
		close(fd);
		free(buf);
		return -1;
	}

	fprintf(stdout, "%s\n", buf);
	free(buf);
	close(fd);
	return 0;
}

struct clib_cmd cmd_cbs[CLIB_CMD_MAX] = {
	{(char *)"help", si_help, si_help_usage},
	{(char *)"exit", si_exit, si_exit_usage},
	{(char *)"quit", si_exit, si_exit_usage},
	{(char *)"load_plugin", si_load_plugin, si_load_plugin_usage},
	{(char *)"unload_plugin", si_unload_plugin, si_unload_plugin_usage},
	{(char *)"reload_plugin", si_reload_plugin, si_reload_plugin_usage},
	{(char *)"list_plugin", si_list_plugin, si_list_plugin_usage},
	{(char *)"do_make", si_do_make, si_do_make_usage},
	{(char *)"do_sh", si_do_sh, si_do_sh_usage},
	{(char *)"set_plugin_dir", si_set_plugin_dir, si_set_plugin_dir_usage},
	{(char *)"showlog", si_show_log, si_show_log_usage},
};

/*
 * ************************************************************************
 * srcinv main routines
 * ************************************************************************
 */
static struct clib_plugin_load_arg def_plugins[] = {
	CLIB_PLUGIN_LOAD_ARG1(src),
	CLIB_PLUGIN_LOAD_ARG1(sinode),
	CLIB_PLUGIN_LOAD_ARG1(sibuf),
	CLIB_PLUGIN_LOAD_ARG1(resfile),
	CLIB_PLUGIN_LOAD_ARG1(getinfo),
	CLIB_PLUGIN_LOAD_ARG1(utils),
	CLIB_PLUGIN_LOAD_ARG1(c),
	CLIB_PLUGIN_LOAD_ARG1(debuild),
	CLIB_PLUGIN_LOAD_ARG1(staticchk),
	CLIB_PLUGIN_LOAD_ARG1(uninit),
	CLIB_PLUGIN_LOAD_ARG1(gensample),
	CLIB_PLUGIN_LOAD_ARG1(fuzz),
	CLIB_PLUGIN_LOAD_ARG1(itersn),
	CLIB_PLUGIN_LOAD_ARG1(sn_load),
};
int si_setup(void)
{
	set_dbg_mode(1);
	int err;
	set_eh(NULL);
	err = clib_cmd_plugin_setup(cmd_cbs, CLIB_CMD_MAX, DEFAULT_PLUGIN_DIR,
					def_plugins,
					sizeof(def_plugins) / sizeof(def_plugins[0]));
	if (err) {
		err_msg(err_fmt("clib_cmd_plugin_setup err"));
		return -1;
	}

	return 0;
}

int main(int argc, char *argv[])
{
	int err;

	err = no_aslr(argc, argv);
	if (err == -1) {
		err_msg(err_fmt("no_aslr err\n"));
		return -1;
	}

	int err0 = tmp_close_std(STDOUT_FILENO);

	err = si_setup();
	if (!err0)
		err0 = restore_std(STDOUT_FILENO);
	if (err0) {
		err_msg(err_fmt("tmp_close_std restore_std err"));
		return -1;
	}

	if (err) {
		err_msg("[x] initializing...\n");
		return -1;
	} else {
		fprintf(stdout, "[v] initializing... done\n"
				"\tuse `help` for more infomations\n"
				"\tprocess id is %d\n", getpid());
	}

	char *ibuf = NULL;
	while (1) {
		ibuf = clib_readline_add_history(term_prompt);
		if (!ibuf) {
			err_msg(err_fmt("readline get EOF and empty line, redo\n"));
			continue;
		}

		int cmd_argc;
		char *cmd_argv[CMD_ARGS_MAX];
		memset(cmd_argv, 0, CMD_ARGS_MAX*sizeof(char *));
		err = clib_cmd_getarg(ibuf, strlen(ibuf)+1, &cmd_argc, cmd_argv);
		if (err) {
			err_msg(err_fmt("clib_cmd_getarg err, redo\n"));
			free(ibuf);
			continue;
		}

		err = clib_cmd_exec(cmd_argv[0], cmd_argc, cmd_argv);
		if (err) {
			err_msg(err_fmt("clib_cmd_exec err\n"));
			free(ibuf);
			continue;
		}
		free(ibuf);
	}

	return 0;
}
