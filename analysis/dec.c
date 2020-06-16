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

C_SYM int init_asm_cp_state(struct code_path *cp, struct cp_state *state);
C_SYM int init_gimple_cp_state(struct code_path *cp, struct cp_state *state);

void init_cp_state(int type, struct code_path *cp)
{
	int err = 0;
	struct cp_state *new_cp_state;

	new_cp_state = cp_state_new();

	switch (type) {
	case SINODE_FMT_ASM:
		err = init_asm_cp_state(cp, new_cp_state);
		break;
	case SINODE_FMT_GCC:
		err = init_gimple_cp_state(cp, new_cp_state);
		break;
	default:
	{
		si_log1_todo("type(%d) not implemented\n", type);
		err = -1;
		break;
	}
	}

	if (!err) {
		cp->state = new_cp_state;
		new_cp_state->data_fmt = type;
	} else {
		cp_state_cleanup(new_cp_state);
		cp_state_free(new_cp_state);
	}
}
