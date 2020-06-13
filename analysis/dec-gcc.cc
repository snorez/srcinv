/*
 * TODO
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
#include "./analysis.h"

C_SYM int dec_gimple_insn(struct code_path *cp, struct sample_state *head);

static void dec_single_gimple(gimple_seq gs, struct code_path *cp,
				struct sample_state *head)
{
	return;
}

int dec_gimple_insn(struct code_path *cp, struct sample_state *head)
{
	basic_block this_bb;
	gimple_seq gs;

	this_bb = (basic_block)cp->cp;
	gs = this_bb->il.gimple.seq;

	for (; gs; gs = gs->next) {
		dec_single_gimple(gs, cp, head);
	}

	head->cp_entry = cp;
	return 0;
}
