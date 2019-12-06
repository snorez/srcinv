/*
 * TODO
 * Copyright (C) 2019  zerons
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
#include "si_core.h"

/*
 * ************************************************************************
 * srcinv default cmds
 * ************************************************************************
 */
static void si_help_usage(void)
{
	fprintf(stdout, "\t\tShow this help message\n");
}
static long si_help(int argc, char *argv[])
{
	clib_cmd_usages();
	return 0;
}

/*
 * si_exit: exit the main process
 */
static void si_exit_usage(void)
{
	fprintf(stdout, "\t\tExit this main process\n");
}
static long si_exit(int argc, char *argv[])
{
	exit(0);
	return 0;
}

#if 0
/*
 * si_load_module: load a specific module, and execute the target
 * `clib_module_init(struct clib_module *, argv)` function,
 * pass argv[3:] to `clib_module_init`
 *
 * argv layout:
 *	[0]: load_module
 *	[1]: module path
 *	[2]: module args
 */
static void si_load_module_usage(void)
{
	fprintf(stdout, "\t(module_path) [module_args...]\n");
}
static long si_load_module(int argc, char *argv[])
{
	if (argc < 2) {
		err_dbg(0, "argc invalid");
		si_load_module_usage();
		return -1;
	}

	int err = clib_module_load(argc-1, &argv[1]);
	if (err) {
		err_dbg(0, "clib_module_load err");
		return -1;
	}

	return 0;
}

/*
 * si_unload_module: unload specific module, could be id or path
 */
static void si_unload_module_usage(void)
{
	fprintf(stdout, "\t(id|path)\tUnload specific module\n");
}
static long si_unload_module(int argc, char *argv[])
{
	int err;
	if (argc != 2) {
		err_dbg(0, "argc invalid");
		si_unload_module_usage();
		return -1;
	}

	err = clib_module_unload(argc-1, &argv[1]);
	if (err) {
		err_dbg(0, "clib_module_unload err");
		return -1;
	}

	return 0;
}

/*
 * si_reload_module: reload a module, just unload it and load it again
 */
static void si_reload_module_usage(void)
{
	fprintf(stdout, "\t(id|path) [module_args]\tReload target module\n");
}
static long si_reload_module(int argc, char *argv[])
{
	int err;
	if (argc != 2) {
		err_dbg(0, "argc invalid");
		si_reload_module_usage();
		return -1;
	}

	err = clib_module_reload(argc-1, &argv[1]);
	if (err) {
		err_dbg(0, "clib_module_reload err");
		return -1;
	}

	return 0;
}

/*
 * si_list_module: list current loaded modules, and id them
 */
static void si_list_module_usage(void)
{
	fprintf(stdout, "\t\t\tShow current loaded modules\n");
}
static long si_list_module(int argc, char *argv[])
{
	clib_module_print();
	return 0;
}

#define	DO_CMD_LEN	2048
static void si_do_make_usage(void)
{
	fprintf(stdout, "\t(c|cpp|...) (sodir) (projectdir) (outfile) [extras]\n"
			"\t\t\tBuild target project, make sure Makefile has "
			"EXTRA_CFLAGS\n");
}
static long si_do_make(int argc, char *argv[])
{
	int err = 0;
	if ((argc != 5) && (argc != 6)) {
		err_dbg(0, "argc invalid");
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
		err_dbg(0, "run command \"%s\" err", cmd);
		return -1;
	}
	return 0;
}
#endif

static void si_do_sh_usage(void)
{
	fprintf(stdout, "\t\tExecute bash command\n");
}

static long si_do_sh(int argc, char *argv[])
{
	int err = 0;
	if (argc <= 1) {
		err_dbg(0, "argc invalid");
		si_do_sh_usage();
		return -1;
	}

#if 1
#define USE_SYSTEM_FUNC
#endif
#ifndef USE_SYSTEM_FUNC
	int pid;
	if ((pid = fork()) < 0) {
		err_dbg(1, "fork");
		return -1;
	} else if (pid == 0) {
		extern char **environ;
		err = execvpe(argv[1], &argv[1], environ);
		if (err == -1)
			err_exit(1, "execvpe err");
	} else {
		waitpid(pid, NULL, 0);
	}
	return 0;
#else
#define DO_CMD_LEN	512
	char cmd[DO_CMD_LEN];
	memset(cmd, 0, DO_CMD_LEN);
	for (int i = 1; i < argc; i++) {
		snprintf(cmd+strlen(cmd), DO_CMD_LEN-1-strlen(cmd), "%s ", argv[i]);
	}

	err = system(cmd);
	if (err) {
		err_dbg(0, "run bash command \"%s\" err", cmd);
		return -1;
	}
	return 0;
#endif
}

#if 0
static void si_set_module_dir_usage(void)
{
	fprintf(stdout, "\t(module_dir)\tSet the module directory, "
			"the original still count\n");
}
static long si_set_module_dir(int argc, char *argv[])
{
	int err;
	if (argc != 2) {
		err_dbg(0, "argc invalid");
		return -1;
	}

	err = clib_module_init_root(argv[1]);
	if (err) {
		err_dbg(0, "clib_module_init_root err");
		return -1;
	}
	return 0;
}
#endif

static void si_show_log_usage(void)
{
	fprintf(stdout, "\t\tShow current log messages\n");
}
static long si_show_log(int argc, char *argv[])
{
	int err;
	if (argc != 1) {
		err_dbg(0, "argc invalid");
		return -1;
	}

	if (!si) {
		err_dbg(0, "si not set yet");
		return -1;
	}

	char *logfile = si_get_logfile();
	int fd = open(logfile, O_RDONLY);
	if (fd == -1) {
		err_dbg(1, "open err");
		return -1;
	}

	struct stat st;
	err = fstat(fd, &st);
	if (err == -1) {
		err_dbg(1, "fstat err");
		close(fd);
		return -1;
	}

	char *buf = (char *)xmalloc(st.st_size+1);
	if (!buf) {
		err_dbg(0, "xmalloc err");
		close(fd);
		return -1;
	}

	err = read(fd, buf, st.st_size);
	if (err == -1) {
		err_dbg(1, "read err");
		close(fd);
		free(buf);
		return -1;
	}

	fprintf(stdout, "%s\n", buf);
	free(buf);
	close(fd);
	return 0;
}

C_SYM int si_config(void);
C_SYM int si_module_setup(void);
static void si_reload_config_usage(void)
{
	fprintf(stdout, "\t\tReload config\n");
}
static long si_reload_config(int argc, char *argv[])
{
	int err;
	err = si_config();
	if (err) {
		err_dbg(0, "si_config err");
		return -1;
	}

	err = si_module_setup();
	if (err) {
		err_dbg(0, "si_module_setup err");
		return -1;
	}

	return 0;
}

static struct {
	char		*name;
	clib_cmd_cb	cb;
	clib_cmd_usage	usage;
} si_def_cmds[] = {
	{(char *)"help", si_help, si_help_usage},
	{(char *)"quit", si_exit, si_exit_usage},
#if 0
	{(char *)"load_module", si_load_module, si_load_module_usage},
	{(char *)"unload_module", si_unload_module, si_unload_module_usage},
	{(char *)"reload_module", si_reload_module, si_reload_module_usage},
	{(char *)"list_module", si_list_module, si_list_module_usage},
	{(char *)"do_make", si_do_make, si_do_make_usage},
	{(char *)"set_module_dir", si_set_module_dir, si_set_module_dir_usage},
#endif
	{(char *)"do_sh", si_do_sh, si_do_sh_usage},
	{(char *)"showlog", si_show_log, si_show_log_usage},
	{(char *)"reload_config", si_reload_config, si_reload_config_usage},
	{NULL, NULL, NULL},
};

/* setup default commands */
int si_cmd_setup(void)
{
	int i = 0, err = 0;
	while (si_def_cmds[i].name) {
		err = clib_cmd_ac_add(si_def_cmds[i].name,
					si_def_cmds[i].cb,
					si_def_cmds[i].usage);
		if (err) {
			err_msg("clib_cmd_ac_add %s err", si_def_cmds[i].name);
			clib_cmd_ac_cleanup();
			return -1;
		}

		i++;
	}
	return 0;
}
