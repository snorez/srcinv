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

int op_code_prio(enum tree_code code)
{
	switch (code) {
	case TREE_LIST:
	case COMPOUND_EXPR:
	case BIND_EXPR:
		return 1;

	case MODIFY_EXPR:
	case INIT_EXPR:
		return 2;

	case COND_EXPR:
		return 3;

	case TRUTH_OR_EXPR:
	case TRUTH_ORIF_EXPR:
		return 4;

	case TRUTH_AND_EXPR:
	case TRUTH_ANDIF_EXPR:
		return 5;

	case BIT_IOR_EXPR:
		return 6;

	case BIT_XOR_EXPR:
	case TRUTH_XOR_EXPR:
		return 7;

	case BIT_AND_EXPR:
		return 8;

	case EQ_EXPR:
	case NE_EXPR:
		return 9;

	case UNLT_EXPR:
	case UNLE_EXPR:
	case UNGT_EXPR:
	case UNGE_EXPR:
	case UNEQ_EXPR:
	case LTGT_EXPR:
	case ORDERED_EXPR:
	case UNORDERED_EXPR:
	case LT_EXPR:
	case LE_EXPR:
	case GT_EXPR:
	case GE_EXPR:
		return 10;

	case LSHIFT_EXPR:
	case RSHIFT_EXPR:
	case LROTATE_EXPR:
	case RROTATE_EXPR:
	case VEC_WIDEN_LSHIFT_HI_EXPR:
	case VEC_WIDEN_LSHIFT_LO_EXPR:
	case WIDEN_LSHIFT_EXPR:
		return 11;

	case WIDEN_SUM_EXPR:
	case PLUS_EXPR:
	case POINTER_PLUS_EXPR:
	case POINTER_DIFF_EXPR:
	case MINUS_EXPR:
		return 12;

	case VEC_WIDEN_MULT_HI_EXPR:
	case VEC_WIDEN_MULT_LO_EXPR:
	case WIDEN_MULT_EXPR:
	case DOT_PROD_EXPR:
	case WIDEN_MULT_PLUS_EXPR:
	case WIDEN_MULT_MINUS_EXPR:
	case MULT_EXPR:
	case MULT_HIGHPART_EXPR:
	case TRUNC_DIV_EXPR:
	case CEIL_DIV_EXPR:
	case FLOOR_DIV_EXPR:
	case ROUND_DIV_EXPR:
	case RDIV_EXPR:
	case EXACT_DIV_EXPR:
	case TRUNC_MOD_EXPR:
	case CEIL_MOD_EXPR:
	case FLOOR_MOD_EXPR:
	case ROUND_MOD_EXPR:
		return 13;

	case TRUTH_NOT_EXPR:
	case BIT_NOT_EXPR:
	case POSTINCREMENT_EXPR:
	case POSTDECREMENT_EXPR:
	case PREINCREMENT_EXPR:
	case PREDECREMENT_EXPR:
	case NEGATE_EXPR:
	case INDIRECT_REF:
	case ADDR_EXPR:
	case FLOAT_EXPR:
	CASE_CONVERT:
	case FIX_TRUNC_EXPR:
	case TARGET_EXPR:
		return 14;

	case CALL_EXPR:
	case ARRAY_REF:
	case ARRAY_RANGE_REF:
	case COMPONENT_REF:
		return 15;

	/* Special expressions.  */
	case MIN_EXPR:
	case MAX_EXPR:
	case ABS_EXPR:
	case REALPART_EXPR:
	case IMAGPART_EXPR:
	case VEC_UNPACK_HI_EXPR:
	case VEC_UNPACK_LO_EXPR:
	case VEC_UNPACK_FLOAT_HI_EXPR:
	case VEC_UNPACK_FLOAT_LO_EXPR:
#if __GNUC__ >= 9
	case VEC_UNPACK_FIX_TRUNC_HI_EXPR:
	case VEC_UNPACK_FIX_TRUNC_LO_EXPR:
#endif
	case VEC_PACK_TRUNC_EXPR:
	case VEC_PACK_SAT_EXPR:
		return 16;

	default:
		return 9999;
	}
}

int op_prio(const_tree op)
{
	enum tree_code code;

	if (op == NULL)
		return 9999;

	code = TREE_CODE(op);
	if ((code == SAVE_EXPR) || (code == NON_LVALUE_EXPR))
		return op_prio(TREE_OPERAND(op, 0));

	return op_code_prio(code);
}
