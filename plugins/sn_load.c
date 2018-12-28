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

CLIB_PLUGIN_NAME(sn_load);
CLIB_PLUGIN_NEEDED1(getinfo);
static char _cmd0[] = "sn_load";

static void cmd0_usage(void)
{
	fprintf(stdout, "\t[id]\t\tmost for test cases\n");
}
static long cmd0_cb(int argc, char *argv[])
{
	if (argc != 2) {
		err_dbg(0, err_fmt("argc invalid"));
		return -1;
	}

	unsigned int id1 = atol(argv[1]);
	if (!id1) {
		err_dbg(0, err_fmt("argv[1] invalid"));
		return -1;
	}

	union siid id = *(union siid *)&id1;

	struct sinode *sn = sinode__sinode_search(siid_get_type(&id), SEARCH_BY_ID,
							&id);
	if (!sn) {
		err_dbg(0, err_fmt("id not found"));
		return -1;
	}

	resfile__resfile_load(sn->buf);

	return 0;
}

static struct clib_cmd cmd0 = {
	.cmd = _cmd0,
	.cb = cmd0_cb,
	.usage = cmd0_usage,
};

CLIB_PLUGIN_INIT()
{
	int err;

	err = clib_cmd_add(&cmd0);
	if (err) {
		err_msg(err_fmt("clib_cmd_add"));
		return -1;
	}

	return 0;
}

CLIB_PLUGIN_EXIT()
{
	clib_cmd_del(cmd0.cmd);
	return;
}
