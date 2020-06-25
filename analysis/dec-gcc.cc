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

C_SYM int dec_gimple(struct code_path *cp, void *insn);

static int dec_gimple_asm(struct sample_state *sample,
			  struct code_path *cp,
			  gimple_seq gs)
{
	/* parse_ssa_operands() get_asm_stmt_operands() */
	struct gasm *insn;
	insn = (struct gasm *)gs;
	return 0;
}

static int dec_gimple_assign(struct sample_state *sample,
			     struct code_path *cp,
			     gimple_seq gs)
{
	struct gassign *insn;
	insn = (struct gassign *)gs;
	return 0;
}

static int dec_gimple_call(struct sample_state *sample,
			   struct code_path *cp,
			   gimple_seq gs)
{
	struct gcall *insn;
	insn = (struct gcall *)gs;
	return 0;
}

static int dec_gimple_cond(struct sample_state *sample,
			   struct code_path *cp,
			   gimple_seq gs)
{
	struct gcond *insn;
	insn = (struct gcond *)gs;
	return 0;
}

static int dec_gimple_return(struct sample_state *sample,
			     struct code_path *cp,
			     gimple_seq gs)
{
	struct greturn *insn;
	insn = (struct greturn *)gs;
	return 0;
}

static int dec_gimple_phi(struct sample_state *sample,
			  struct code_path *cp,
			  gimple_seq gs)
{
	struct gphi *insn;
	insn = (struct gphi *)gs;
	return 0;
}

int dec_gimple(struct sample_state *sample, struct code_path *cp)
{
	int ret = 0;
	gimple_seq gs;
	enum gimple_code gc;
	gs = (gimple_seq)cp->state->cur_point;

	if (!gs) {
		basic_block bb;
		bb = (basic_block)cp->cp;
		gs = bb->il.gimple.seq;
	} else {
		gs = gs->next;
	}

	gc = gimple_code(gs);

	switch (gc) {
	case GIMPLE_ASM:
		ret = dec_gimple_asm(sample, cp, gs);
		break;
	case GIMPLE_ASSIGN:
		ret = dec_gimple_assign(sample, cp, gs);
		break;
	case GIMPLE_CALL:
		ret = dec_gimple_call(sample, cp, gs);
		break;
	case GIMPLE_COND:
		ret = dec_gimple_cond(sample, cp, gs);
		break;
	case GIMPLE_RETURN:
		ret = dec_gimple_return(sample, cp, gs);
		break;
	case GIMPLE_PHI:
		ret = dec_gimple_phi(sample, cp, gs);
		break;
	case GIMPLE_SWITCH:
	case GIMPLE_OMP_PARALLEL:
	case GIMPLE_OMP_TASK:
	case GIMPLE_OMP_ATOMIC_LOAD:
	case GIMPLE_OMP_ATOMIC_STORE:
	case GIMPLE_OMP_FOR:
	case GIMPLE_OMP_CONTINUE:
	case GIMPLE_OMP_SINGLE:
	case GIMPLE_OMP_TARGET:
	case GIMPLE_OMP_TEAMS:
	case GIMPLE_OMP_RETURN:
	case GIMPLE_OMP_SECTIONS:
	case GIMPLE_OMP_SECTIONS_SWITCH:
	case GIMPLE_OMP_MASTER:
	case GIMPLE_OMP_TASKGROUP:
	case GIMPLE_OMP_SECTION:
	case GIMPLE_OMP_GRID_BODY:
	case GIMPLE_OMP_ORDERED:
	case GIMPLE_OMP_CRITICAL:
	case GIMPLE_BIND:
	case GIMPLE_TRY:
	case GIMPLE_CATCH:
	case GIMPLE_EH_FILTER:
	case GIMPLE_EH_MUST_NOT_THROW:
	case GIMPLE_EH_ELSE:
	case GIMPLE_RESX:
	case GIMPLE_EH_DISPATCH:
	case GIMPLE_DEBUG:
	case GIMPLE_PREDICT:
	case GIMPLE_TRANSACTION:
	case GIMPLE_LABEL:
	case GIMPLE_NOP:
	case GIMPLE_GOTO:
		si_log1_todo("miss %s in %s at %p\n", gimple_code_name[gc],
				cp->func->name, gs);
		break;
	default:
	{
		si_log1_todo("miss %s in %s at %p\n", gimple_code_name[gc],
				cp->func->name, gs);
		ret = -1;
		break;
	}
	}

	if (!ret)
		cp->state->cur_point = (void *)gs;

	return ret;
}
