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

static int do_mark_entry(union siid *tid, char *string, int flag)
{
	int retval = 0;

	struct sinode *sn;
	struct func_node *fn;
	sn = analysis__sinode_search(siid_type(tid), SEARCH_BY_ID, tid);
	if (!sn) {
		err_dbg(0, "No match");
		return -1;
	}

	fn = (struct func_node *)sn->data;
	if (!fn)
		return 0;

	switch (flag) {
	case 0:
		/* start with string */
		if (strncmp(fn->name, string, strlen(string)))
			break;
		
		test_and_set_bit(0, (long *)&fn->call_depth);
		break;
	case 1:
		/* all match */
		if (strcmp(fn->name, string))
			break;

		test_and_set_bit(0, (long *)&fn->call_depth);
		break;
	default:
		err_dbg(0, "flag: %d not handled", flag);
		retval = -1;
		break;
	}

	return retval;
}

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
		do_mark_entry(tid, "__do_sys_", 0);
	}

	return retval;
}

static int mark_kern_entry(void)
{
	int retval = 0;

	switch (si->type.os_type) {
	case SI_TYPE_OS_LINUX:
		retval = mark_linux_kern_entry();
		break;
	default:
		err_dbg(0, "SRC si_type.os_type not handled: %d",
				si->type.os_type);
		retval = -1;
		break;
	}

	return retval;
}

static int mark_user_entry(void)
{
	/* TODO */
	err_dbg(0, "SRC SI_TYPE_USER not handled yet.");
	return 0;
}

int mark_entry(void)
{
	if (si->type.kernel == SI_TYPE_KERN) {
		return mark_kern_entry();
	} else if (si->type.kernel == SI_TYPE_USER) {
		return mark_user_entry();
	} else {
		err_dbg(0, "SRC si_type.kernel err: %d", si->type.kernel);
		return -1;
	}
}
