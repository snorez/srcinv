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

#if 0
	switch (gimple_code(gs)) {
	case GIMPLE_ASM:
	case GIMPLE_ASSIGN:
	case GIMPLE_CALL:
	case GIMPLE_COND:
	case GIMPLE_LABEL:
	case GIMPLE_GOTO:
	case GIMPLE_NOP:
	case GIMPLE_RETURN:
	case GIMPLE_SWITCH:
	case GIMPLE_PHI:
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
		break;
	default:
	{
		si_log1_todo("miss %s in %s\n",
				gimple_code_name[gimple_code(gs)],
				cp->func->name);
		break;
	}
	}
#endif
