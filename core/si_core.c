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
 *	load_module		load some modules
 *	...
 * put everything does real things in module folder
 */
#define	_GNU_SOURCE
#include "si_core.h"

/* term prompt */
#define CHAR_B		'\033'
#define	CHAR_LEFT	'\001'
#define	CHAR_RIGHT	'\002'
static char term_prompt[] = {
	CHAR_LEFT,CHAR_B,'[','0','m',CHAR_B,'[','0','4',';','4','2','m',CHAR_RIGHT,
	'S','R','C','I','N', 'V', '>',
	CHAR_LEFT,CHAR_B,'[','0','m',CHAR_RIGHT,
	' ',
};

C_SYM int si_config(void);
C_SYM int si_cmd_setup(void);
C_SYM int si_module_setup(void);
C_SYM int si_src_setup(void);
static int si_init(void)
{
	int err;

	/*
	 * > parse config files
	 * > setup commands and modules
	 * > init src
	 * > set command line mode
	 */
	err = si_config();
	if (err) {
		err_msg("si_config err");
		return -1;
	}

	err = si_cmd_setup();
	if (err) {
		err_msg("si_cmd_setup err");
		return -1;
	}

	err = si_module_setup();
	if (err) {
		err_msg("si_module_setup err");
		return -1;
	}

	err = si_src_setup();
	if (err) {
		err_msg("si_src_setup err");
		return -1;
	}

	clib_set_autocomplete();
	return 0;
}

int main(int argc, char *argv[])
{
	int err;

	/* 1st, close aslr */
	err = no_aslr(argc, argv);
	if (err == -1) {
		err_msg("no_aslr err");
		return -1;
	}

	if (argc != 1) {
		err_msg("usage: %s", argv[0]);
		return -1;
	}

	set_dbg_mode(1);

	int err0 = tmp_close_std(STDOUT_FILENO);

	err = si_init();
	if (!err0)
		err0 = restore_std(STDOUT_FILENO);
	if (err0) {
		err_msg("tmp_close_std restore_std err");
		return -1;
	}

	if (err) {
		err_msg("initializing... ERR");
		return -1;
	} else {
		fprintf(stdout, "initializing... OK\n");
		fprintf(stdout, "PID is %d\n", getpid());
		fprintf(stdout, "use `help` for more information\n");
		fflush(stdout);
	}

	/* 3rd, read commands and parse them */
	char *ibuf = NULL;
	while (1) {
		ibuf = clib_readline_add_history(term_prompt);
		if (!ibuf) {
			err_msg("readline get EOF and empty line, redo");
			continue;
		}

		int cmd_argc;
		int cmd_arg_max = 16;
		char *cmd_argv[cmd_arg_max];
		memset(cmd_argv, 0, cmd_arg_max*sizeof(char *));
		err = clib_cmd_getarg(ibuf, strlen(ibuf)+1,
					&cmd_argc, cmd_argv, cmd_arg_max);
		if (err) {
			err_msg("clib_cmd_getarg err, redo");
			free(ibuf);
			continue;
		}

		err = clib_cmd_exec(cmd_argv[0], cmd_argc, cmd_argv);
		if (err) {
			err_msg("clib_cmd_exec err");
			free(ibuf);
			continue;
		}
		free(ibuf);
	}

	return 0;
}
