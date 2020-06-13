/*
 * decode code in sinode.
 *	SINODE_FMT_GCC: gimple
 *	SINODE_FMT_ASM: binary
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

C_SYM int dec_asm_insn(struct code_path *cp, struct sample_state *head);
C_SYM int dec_gimple_insn(struct code_path *cp, struct sample_state *head);

int dec_insn(int type, struct code_path *cp, struct sample_state *head)
{
	int err = 0;

	if (!list_empty(&head->cp_state_list)) {
		si_log1_emer("sample_state is not empty\n");
		return -1;
	}

	switch (type) {
	case SINODE_FMT_ASM:
		err = dec_asm_insn(cp, head);
		break;
	case SINODE_FMT_GCC:
		err = dec_gimple_insn(cp, head);
		break;
	default:
	{
		si_log1_todo("type(%d) not implemented\n", type);
		err = -1;
		break;
	}
	}

	if (!err)
		head->data_fmt = type;
	return err;
}
