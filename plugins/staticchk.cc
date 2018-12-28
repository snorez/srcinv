/*
 * TODO
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
#include "../si_core.h"

CLIB_PLUGIN_NAME(staticchk);
CLIB_PLUGIN_NEEDED2(getinfo, utils);

LIST_HEAD(staticchk_method_head);

static char cmd0[] = "staticchk";
static void cmd0_usage(void);
static long cmd0_cb(int argc, char *argv[]);
static struct clib_cmd _cmd0 = {
	_cmd0.cmd = cmd0,
	_cmd0.cb = cmd0_cb,
	_cmd0.usage = cmd0_usage,
};

CLIB_PLUGIN_INIT()
{
	int err;

	INIT_LIST_HEAD(&staticchk_method_head);

	err = clib_cmd_add(&_cmd0);
	if (err) {
		err_msg("clib_cmd_add err");
		return -1;
	}

	return 0;
}

CLIB_PLUGIN_EXIT()
{
	clib_cmd_del(cmd0);
	return;
}

static void cmd0_usage(void)
{
	fprintf(stdout, "\tRun registered static check methods\n");
}

static long cmd0_cb(int argc, char *argv[])
{
	int err;
	if (argc != 1) {
		err_dbg(0, err_fmt("invalid arguments"));
		return -1;
	}

	struct staticchk_method *tmp;
	list_for_each_entry(tmp, &staticchk_method_head, sibling) {
		tmp->callback();
	}

	return 0;
}
