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
#include "./analysis.h"

int dec_next(struct sample_state *sample, struct code_path *cp)
{
	int ret = 0;
	struct sibuf *buf;
	struct file_content *fc;
	struct lang_ops *ops;

	buf = find_target_sibuf(cp->func->node);
	if (!buf) {
		si_log1_todo("find_target_sibuf return NULL\n");
		return ret;
	}

	analysis__resfile_load(buf);
	fc = (struct file_content *)buf->load_addr;
	ops = lang_ops_find(&analysis_lang_ops_head, &fc->type);
	if (!ops) {
		si_log1_todo("lang_ops_find return NULL\n");
		return ret;
	}

	if (cp->state->status == CSS_EMPTY)
		return 0;

	ret = ops->dec(sample, cp);
	return ret;
}
