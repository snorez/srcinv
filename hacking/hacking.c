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
#include "./hacking.h"

LIST_HEAD(hacking_module_head);
CLIB_MODULE_NAME(hacking);
CLIB_MODULE_NEEDED0();

static char itersn_cmdname[] = "itersn";
C_SYM void itersn_usage(void);
C_SYM long itersn_cb(int argc, char *argv[]);

static char hacking_cmdname[] = "hacking";
static void hacking_usage(void)
{
	fprintf(stdout, "\t\tRun hacking modules\n");
}

static long hacking_cb(int argc, char *argv[])
{
	struct hacking_module *tmp;
	list_for_each_entry(tmp, &hacking_module_head, sibling) {
		fprintf(stdout, "Run %s ...\n", tmp->name);
		if (tmp->callback)
			tmp->callback();
	}
	return 0;
}

CLIB_MODULE_INIT()
{
	INIT_LIST_HEAD(&hacking_module_head);

	int err;
	err = clib_cmd_add(itersn_cmdname, itersn_cb, itersn_usage);
	if (err) {
		err_msg("clib_cmd_add err");
		return -1;
	}

	err = clib_cmd_add(hacking_cmdname, hacking_cb, hacking_usage);
	if (err) {
		err_msg("clib_cmd_add err");
		goto err0;
	}

	/* load all hacking modules */
	struct list_head *head = si_module_get_head(SI_PLUGIN_CATEGORY_HACKING);
	err = si_module_load_all(head);
	if (err) {
		err_dbg(0, "si_module_load_all err");
		goto err1;
	}

	return 0;

err1:
	clib_cmd_del(hacking_cmdname);
err0:
	clib_cmd_del(itersn_cmdname);
	return -1;
}

CLIB_MODULE_EXIT()
{
	struct list_head *head = si_module_get_head(SI_PLUGIN_CATEGORY_HACKING);
	si_module_unload_all(head);

	clib_cmd_del(itersn_cmdname);
	clib_cmd_del(hacking_cmdname);
	return;
}
