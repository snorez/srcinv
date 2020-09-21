/*
 * decode code in sinode.
 *	SI_TYPE_DF_GIMPLE: gimple
 *	SI_TYPE_DF_ASM: binary
 *
 * We need to know what the code does, the input should be a code_path, and
 * its type, the output is a cp_state list.
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

int dec_next(struct sample_set *sset, int idx, struct func_node *start_fn)
{
	int ret = 0;
	struct sibuf *buf;
	struct file_content *fc;
	struct lang_ops *ops;

	if (!start_fn)
		return 0;

	if (!sset) {
		si_log1_err("Param sample_state must not be NULL\n");
		return -1;
	}

	buf = find_target_sibuf(start_fn->node);
	if (!buf) {
		si_log1_todo("find_target_sibuf return NULL\n");
		return -1;
	}

	analysis__resfile_load(buf);
	fc = (struct file_content *)buf->load_addr;
	ops = lang_ops_find(&analysis_lang_ops_head, &fc->type);
	if (!ops) {
		si_log1_todo("lang_ops_find return NULL\n");
		return -1;
	}

	ret = ops->dec(sset, idx, start_fn);
	return ret;
}

/*
 * @dec_special_call: handle some special calls.
 * Return value:
 *	0: this call is properly handled.
 *	-1: this call is not handled, caller handle it itself.
 */
int dec_special_call(struct sample_set *sset, int idx, struct fn_list *fnl,
			struct func_node *call_fn)
{
	si_log1_todo("not implement yet\n");
	return -1;
}
