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

static char itersn_cmdname[] = "itersn";
C_SYM void itersn_usage(void);
C_SYM long itersn_cb(int argc, char *argv[]);

static char load_sibuf_cmdname[] = "load_sibuf";
static void load_sibuf_usage(void)
{
	fprintf(stdout, "\t(sibuf_addr)\n"
			"\tLoading specific sibuf\n");
}

static long load_sibuf_cb(int argc, char *argv[])
{
	if (argc != 2) {
		err_dbg(0, "arg check err");
		return -1;
	}

	long addr = atol(argv[1]);
	int found = 0;
	struct sibuf *b;
	list_for_each_entry(b, &si->sibuf_head, sibling) {
		if ((long)b != addr)
			continue;
		found = 1;
		analysis__resfile_load(b);
		break;
	}
	if (!found)
		err_dbg(0, "given addr not match to any sibuf");
	return 0;
}

static char havefun_cmdname[] = "havefun";
static void havefun_usage(void)
{
	fprintf(stdout, "\tRun hacking modules\n");
}

static long havefun_cb(int argc, char *argv[])
{
	struct hacking_module *tmp;
	list_for_each_entry(tmp, &hacking_module_head, sibling) {
		fprintf(stdout, "Run %s ...\n", tmp->name);
		if (tmp->callback)
			tmp->callback();
	}
	return 0;
}

SI_MOD_SUBENV_INIT()
{
	int err;
	INIT_LIST_HEAD(&hacking_module_head);

	err = clib_cmd_ac_add(itersn_cmdname, itersn_cb, itersn_usage);
	if (err) {
		err_msg("clib_cmd_ac_add err");
		return -1;
	}

	err = clib_cmd_ac_add(havefun_cmdname, havefun_cb, havefun_usage);
	if (err) {
		err_msg("clib_cmd_ac_add err");
		goto err0;
	}

	err = clib_cmd_ac_add(load_sibuf_cmdname, load_sibuf_cb,
				load_sibuf_usage);
	if (err) {
		err_msg("clib_cmd_ac_add err");
		goto err1;
	}

	/* load all hacking modules */
	struct list_head *head;
	head = si_module_get_head(SI_PLUGIN_CATEGORY_HACKING);
	err = si_module_load_all(head);
	if (err) {
		err_dbg(0, "si_module_load_all err");
		goto err2;
	}

	return 0;

err2:
	clib_cmd_ac_del(load_sibuf_cmdname);
err1:
	clib_cmd_ac_del(havefun_cmdname);
err0:
	clib_cmd_ac_del(itersn_cmdname);
	return -1;
}

SI_MOD_SUBENV_DEINIT()
{
	struct list_head *head;
	head = si_module_get_head(SI_PLUGIN_CATEGORY_HACKING);
	si_module_unload_all(head);
}

SI_MOD_SUBENV_SETUP(hacking);
