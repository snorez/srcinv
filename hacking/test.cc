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
#include "si_gcc.h"
#include "./hacking.h"

CLIB_MODULE_NAME(test);
CLIB_MODULE_NEEDED0();
static void test_cb(void);
static struct hacking_module test;

CLIB_MODULE_INIT()
{
	test.flag = HACKING_FLAG_STATIC;
	test.callback = test_cb;
	test.name = this_module_name;
	register_hacking_module(&test);
	return 0;
}

CLIB_MODULE_EXIT()
{
	unregister_hacking_module(&test);
	return;
}

static void test_fsn(struct sinode *fsn)
{
	if (strncmp(fsn->name, "main", fsn->namelen))
		return;

	analysis__resfile_load(fsn->buf);

	struct func_node *fn;
	fn = (struct func_node *)fsn->data;

	struct code_path *next_cp;
	analysis__get_func_code_paths_start(fn->cps[0]);
	while ((next_cp = analysis__get_func_next_code_path())) {
		basic_block bb;
		gimple_seq gs;

		bb = (basic_block)next_cp->cp;
		gs = bb->il.gimple.seq;

		si_log2("For BB: %p\n", bb);
		for (; gs; gs = gs->next) {
			show_gimple(gs);
		}
	}
	return;
}

static void test_cb(void)
{
	si_log2("run test\n");

	unsigned long func_id = 0;
	union siid *tid = (union siid *)&func_id;

	tid->id0.id_type = TYPE_FUNC_GLOBAL;
	for (; func_id < si->id_idx[TYPE_FUNC_GLOBAL].id1; func_id++) {
		struct sinode *fsn;
		fsn = analysis__sinode_search(siid_type(tid),
						SEARCH_BY_ID, tid);
		if (!fsn)
			continue;
		test_fsn(fsn);
	}

	func_id = 0;
	tid->id0.id_type = TYPE_FUNC_STATIC;
	for (; func_id < si->id_idx[TYPE_FUNC_STATIC].id1; func_id++) {
		struct sinode *fsn;
		fsn = analysis__sinode_search(siid_type(tid),
						SEARCH_BY_ID, tid);
		if (!fsn)
			continue;
		test_fsn(fsn);
	}


	si_log2("run test done\n");
	return;
}
