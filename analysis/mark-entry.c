/*
 * mark syscall entry for kernel or main entry for userspace
 *
 * Copyright (C) 2020 zerons
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

static void mark_subcall_entry(struct func_node *fn, int call_level)
{
	if ((fn->call_depth) && (fn->call_depth <= call_level))
		return;

	int call_level_max = (1 << CALL_DEPTH_BITS) - 1;
	if (call_level > call_level_max) {
		si_log1_warn("call_level exceed %d\n", call_level_max);
		return;
	}

	fn->call_depth = call_level;

	struct id_list *tmp;
	slist_for_each_entry(tmp, &fn->callees, sibling) {
		struct sinode *sub_fsn;
		if (tmp->value_flag)
			continue;
		union siid *this_id = (union siid *)&tmp->value;
		sub_fsn = analysis__sinode_search(siid_type(this_id),
						  SEARCH_BY_ID, this_id);
		if (!sub_fsn)
			continue;
		if (!sub_fsn->data)
			continue;
		mark_subcall_entry((struct func_node *)sub_fsn->data,
				   call_level+1);
	}

	return;
}

static int do_mark_entry(union siid *tid, char *string, int flag)
{
	int retval = 0;

	struct sinode *sn;
	struct func_node *fn;
	sn = analysis__sinode_search(siid_type(tid), SEARCH_BY_ID, tid);
	if (!sn) {
		/* this sinode may be replaced */
		return 0;
	}

	fn = (struct func_node *)sn->data;
	if (!fn)
		return 0;

	switch (flag) {
	case 0:
		/* start with string */
		if (strncmp(fn->name, string, strlen(string)))
			break;
		
		mark_subcall_entry(fn, 1);
		break;
	case 1:
		/* all match */
		if (strcmp(fn->name, string))
			break;

		mark_subcall_entry(fn, 1);
		break;
	default:
		err_dbg(0, "flag: %d not handled", flag);
		retval = -1;
		break;
	}

	return retval;
}

/* TODO: this is only for linux kernel, amd64 */
static int mark_linux_kern_entry(void)
{
	int retval = 0;

	/*
	 * mark all functions start with sys_
	 */
	unsigned long func_id = 0;
	union siid *tid = (union siid *)&func_id;

	tid->id0.id_type = TYPE_FUNC_GLOBAL;
	for (; func_id < si->id_idx[TYPE_FUNC_GLOBAL].id1; func_id++) {
		/* For x86_64 */
		do_mark_entry(tid, "__x64_sys_", 0);
	}

	return retval;
}

/* For projects we don't know its si_type */
static int mark_unknown_entry(void)
{
	/* find all global functions with zero callers */
	unsigned long _id = 0;
	union siid *id = (union siid *)&_id;

	id->id0.id_type = TYPE_FUNC_GLOBAL;
	for (; _id < si->id_idx[TYPE_FUNC_GLOBAL].id1; _id++) {
		struct sinode *fsn;
		struct func_node *fn;

		fsn = analysis__sinode_search(siid_type(id), SEARCH_BY_ID, id);
		if ((!fsn) || (!fsn->data))
			continue;

		fn = (struct func_node *)fsn->data;
		if (!slist_empty(&fn->callers))
			continue;

		mark_subcall_entry(fn, 1);
	}

	return 0;
}

int mark_entry(void)
{
	if (src_is_linux_kernel()) {
		return mark_linux_kern_entry();
	} else {
		return mark_unknown_entry();
	}
}
