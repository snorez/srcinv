/*
 * detect UAF bugs statically, NOTICE: check the project kernel or userspace
 *
 * TODO:
 *	Increase the refcnt while the address is set to some other variables.
 *		Like, ADDR_EXPR?
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
#include "si_gcc.h"

CLIB_MODULE_NAME(uaf);
CLIB_MODULE_NEEDED0();
static struct hacking_module uaf;

static void uaf_doit(struct sample_set *sset, int idx)
{
	return;
}

static void uaf_done(struct sample_set *sset, int idx)
{
	struct data_state_rw *tmp;
	slist_for_each_entry(tmp, &sset->allocated_data_states, base.sibling) {
		if (!tmp->val.flag.freed)
			continue;
		if (!atomic_read(&tmp->val.flag.refcount))
			continue;
		sample_set_set_flag(sset, SAMPLE_SF_UAF);
	}
	return;
}

CLIB_MODULE_INIT()
{
	uaf.flag = HACKING_FLAG_STATIC;
	uaf.doit = uaf_doit;
	uaf.done = uaf_done;
	uaf.name = this_module_name;
	register_hacking_module(&uaf);
	return 0;
}

CLIB_MODULE_EXIT()
{
	unregister_hacking_module(&uaf);
}
