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

CLIB_PLUGIN_NAME(test);
CLIB_PLUGIN_NEEDED1(src);
static char cmd0[] = "test";
static void cmd0_usage(void);
static long cmd0_cb(int argc, char *argv[]);

static struct clib_cmd _cmd0 = {
	.cmd = cmd0,
	.cb = cmd0_cb,
	.usage = cmd0_usage,
};

static void cmd0_usage(void)
{
	return;
}

static long cmd0_cb(int argc, char *argv[])
{
	if (argc != 1) {
		err_dbg(0, err_fmt("invalid arguments"));
		return -1;
	}

	unsigned long from_id = 0x030000000000008c;
	unsigned long to_id = 0x0300000000000014;
	union siid *from_tid = (union siid *)&from_id;
	union siid *to_tid = (union siid *)&to_id;
	struct sinode *from, *to;
	from = sinode__sinode_search(siid_get_type(from_tid), SEARCH_BY_ID,
					from_tid);
	to = sinode__sinode_search(siid_get_type(to_tid), SEARCH_BY_ID, to_tid);
	BUG_ON(!from);
	BUG_ON(!to);

	struct list_head head;
	utils__gen_func_pathes(from, to, &head, 0);
	struct path_list_head *tmp0;
	list_for_each_entry(tmp0, &head, sibling) {
		struct func_path_list *tmp1;
		list_for_each_entry(tmp1, &tmp0->path_head, sibling) {
			fprintf(stderr, "%s ", tmp1->fsn->name);
		}
		fprintf(stderr, "\n");
	}
#if 0
	struct list_head head;
	utils__gen_code_pathes(from, 0, from, -1, &head);
	fprintf(stderr, "codepath for %s\n", from->name);
	struct path_list_head *tmp0;
	list_for_each_entry(tmp0, &head, sibling) {
		struct code_path_list *tmp1;
		list_for_each_entry(tmp1, &tmp0->path_head, sibling) {
			fprintf(stderr, "%p ", tmp1->cp);
		}
		fprintf(stderr, "\n");
	}
#endif
	return 0;
}

CLIB_PLUGIN_INIT()
{
	int err;

	err = clib_cmd_add(&_cmd0);
	if (err) {
		err_dbg(0, err_fmt("clib_cmd_add err"));
		return -1;
	}

	return 0;
}

CLIB_PLUGIN_EXIT()
{
	clib_cmd_del(cmd0);
	return;
}
