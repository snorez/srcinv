/*
 * copy from gcc/gcc/tree.c with a little modification.
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

#define DEFTREECODE(SYM, NAME, TYPE, LENGTH) TYPE,
#define END_OF_BASE_TREE_CODES tcc_exceptional,
const enum tree_code_class tree_code_type[] = {
#include "all-tree.def"
};
#undef DEFTREECODE
#undef END_OF_BASE_TREE_CODES

#define DEFTREECODE(SYM, NAME, TYPE, LENGTH) LENGTH,
#define END_OF_BASE_TREE_CODES 0,
const unsigned char tree_code_length[] = {
#include "all-tree.def"
};
#undef DEFTREECODE
#undef END_OF_BASE_TREE_CODES

#define DEFTREECODE(SYM, NAME, TYPE, LEN) NAME,
#define END_OF_BASE_TREE_CODES "@dummy",
const char *const tree_code_name[] = {
#include "all-tree.def"
};
#undef DEFTREECODE
#undef END_OF_BASE_TREE_CODES

const char *get_tree_code_name(enum tree_code code)
{
	const char *invalid = "<invalid tree code>";
	if (code >= MAX_TREE_CODES)
		return invalid;

	return tree_code_name[code];
}

int check_tree_code(tree n)
{
	if (!n)
		return 0;

	size_t len = sizeof(tree_code_name) / sizeof(tree_code_name[0]);
	enum tree_code tc = TREE_CODE(n);
	if (tc >= len)
		return 1;
	else
		return 0;
}

tree get_base_address(tree t)
{
	while (handled_component_p(t))
		t = TREE_OPERAND(t, 0);

	if (((TREE_CODE(t) == MEM_REF) ||
		(TREE_CODE(t) == TARGET_MEM_REF)) &&
	      (TREE_CODE(TREE_OPERAND(t, 0)) == ADDR_EXPR))
		t = TREE_OPERAND(TREE_OPERAND(t, 0), 0);

	if (TREE_CODE(t) == WITH_SIZE_EXPR)
		return NULL_TREE;

	return t;
}

bool integer_zerop(tree expr)
{
	switch (TREE_CODE(expr)) {
	case INTEGER_CST:
	{
		return wi::to_wide(expr) == 0;
	}
	case COMPLEX_CST:
	{
		return (integer_zerop(TREE_REALPART(expr)) &&
			integer_zerop(TREE_IMAGPART(expr)));
	}
	case VECTOR_CST:
	{
		return ((VECTOR_CST_NPATTERNS(expr) == 1) &&
			(VECTOR_CST_DUPLICATE_P(expr)) &&
			(integer_zerop(VECTOR_CST_ENCODED_ELT(expr, 0))));
	}
	default:
	{
		return false;
	}
	}
}

static void process_call_operands(tree t)
{
	bool side_effects = TREE_SIDE_EFFECTS(t);
	bool read_only = false;
	int i = call_expr_flags(t);

	if ((i & ECF_LOOPING_CONST_OR_PURE) || !(i & (ECF_CONST | ECF_PURE)))
		side_effects = true;
	if (i & ECF_CONST)
		read_only = true;

	if (!side_effects || read_only) {
		for (i = 1; i < TREE_OPERAND_LENGTH(t); i++) {
			tree op = TREE_OPERAND(t, i);
			if (op && TREE_SIDE_EFFECTS(op))
				side_effects = true;
			if (op && !TREE_READONLY(op) && !CONSTANT_CLASS_P(op))
				read_only = false;
		}
	}

	TREE_SIDE_EFFECTS(t) = side_effects;
	TREE_READONLY(t) = read_only;
}

tree substitute_placeholder_in_expr(tree exp, tree obj)
{
	enum tree_code code = TREE_CODE(exp);
	tree op0, op1, op2, op3;
	tree new_tree;

	if (code == PLACEHOLDER_EXPR) {
		tree need_type = TYPE_MAIN_VARIANT(TREE_TYPE (exp));
		tree elt;

		for (elt = obj; elt != 0;
			elt = ((TREE_CODE(elt) == COMPOUND_EXPR ||
				TREE_CODE(elt) == COND_EXPR) ?
				TREE_OPERAND(elt, 1) :
				(REFERENCE_CLASS_P(elt) ||
				 UNARY_CLASS_P(elt) ||
				 BINARY_CLASS_P(elt) ||
				 VL_EXP_CLASS_P(elt) ||
				 EXPRESSION_CLASS_P(elt)) ?
				TREE_OPERAND(elt, 0) : 0))
			if (TYPE_MAIN_VARIANT(TREE_TYPE(elt)) == need_type)
				return elt;

		for (elt = obj; elt != 0;
			elt = ((TREE_CODE(elt) == COMPOUND_EXPR ||
				TREE_CODE(elt) == COND_EXPR) ?
				TREE_OPERAND(elt, 1) :
				(REFERENCE_CLASS_P(elt) ||
				 UNARY_CLASS_P(elt) ||
				 BINARY_CLASS_P(elt) ||
				 VL_EXP_CLASS_P(elt) ||
				 EXPRESSION_CLASS_P(elt)) ?
				TREE_OPERAND(elt, 0) : 0))
			if (POINTER_TYPE_P(TREE_TYPE(elt)) &&
				(TYPE_MAIN_VARIANT(TREE_TYPE(TREE_TYPE(elt))) ==
				 need_type))
				return fold_build1(INDIRECT_REF, need_type, elt);

		return exp;
	} else if (code == TREE_LIST) {
		op0 = SUBSTITUTE_PLACEHOLDER_IN_EXPR(TREE_CHAIN(exp), obj);
		op1 = SUBSTITUTE_PLACEHOLDER_IN_EXPR(TREE_VALUE(exp), obj);
		if (op0 == TREE_CHAIN(exp) && op1 == TREE_VALUE(exp))
			return exp;

		return tree_cons(TREE_PURPOSE(exp), op1, op0);
	} else {
		switch (TREE_CODE_CLASS(code)) {
		case tcc_constant:
		case tcc_declaration:
			return exp;

		case tcc_exceptional:
		case tcc_unary:
		case tcc_binary:
		case tcc_comparison:
		case tcc_expression:
		case tcc_reference:
		case tcc_statement:
			switch (TREE_CODE_LENGTH(code)) {
			case 0:
				return exp;

			case 1:
				op0 = SUBSTITUTE_PLACEHOLDER_IN_EXPR(
						TREE_OPERAND(exp, 0), obj);
				if (op0 == TREE_OPERAND(exp, 0))
					return exp;

				new_tree = fold_build1(code, TREE_TYPE(exp),
							op0);
				break;

			case 2:
				op0 = SUBSTITUTE_PLACEHOLDER_IN_EXPR(
						TREE_OPERAND(exp, 0), obj);
				op1 = SUBSTITUTE_PLACEHOLDER_IN_EXPR(
						TREE_OPERAND(exp, 1), obj);

				if (op0 == TREE_OPERAND(exp, 0) &&
					op1 == TREE_OPERAND(exp, 1))
					return exp;

				new_tree = fold_build2(code, TREE_TYPE(exp),
							op0, op1);
				break;

			case 3:
				op0 = SUBSTITUTE_PLACEHOLDER_IN_EXPR(
						TREE_OPERAND(exp, 0), obj);
				op1 = SUBSTITUTE_PLACEHOLDER_IN_EXPR(
						TREE_OPERAND(exp, 1), obj);
				op2 = SUBSTITUTE_PLACEHOLDER_IN_EXPR(
						TREE_OPERAND(exp, 2), obj);

				if (op0 == TREE_OPERAND(exp, 0) &&
					op1 == TREE_OPERAND(exp, 1) &&
					op2 == TREE_OPERAND(exp, 2))
					return exp;

				new_tree = fold_build3(code, TREE_TYPE(exp),
							op0, op1, op2);
				break;

			case 4:
				op0 = SUBSTITUTE_PLACEHOLDER_IN_EXPR(
						TREE_OPERAND(exp, 0), obj);
				op1 = SUBSTITUTE_PLACEHOLDER_IN_EXPR(
						TREE_OPERAND(exp, 1), obj);
				op2 = SUBSTITUTE_PLACEHOLDER_IN_EXPR(
						TREE_OPERAND(exp, 2), obj);
				op3 = SUBSTITUTE_PLACEHOLDER_IN_EXPR(
						TREE_OPERAND(exp, 3), obj);

				if (op0 == TREE_OPERAND(exp, 0) &&
					op1 == TREE_OPERAND(exp, 1) &&
					op2 == TREE_OPERAND(exp, 2) &&
					op3 == TREE_OPERAND(exp, 3))
					return exp;

				new_tree = fold(build4(code, TREE_TYPE(exp),
							op0, op1, op2, op3));
				break;

			default:
				BUG();
			}
			break;

		case tcc_vl_exp:
		{
			int i;

			new_tree = NULL_TREE;

			for (i = 1; i < TREE_OPERAND_LENGTH(exp); i++) {
				tree op = TREE_OPERAND(exp, i);
				tree new_op = SUBSTITUTE_PLACEHOLDER_IN_EXPR(
								op, obj);
				if (new_op != op) {
					if (!new_tree)
						new_tree = copy_node(exp);
					TREE_OPERAND(new_tree, i) = new_op;
				}
			}

			if (new_tree) {
				new_tree = fold(new_tree);
				if (TREE_CODE(new_tree) == CALL_EXPR)
					process_call_operands(new_tree);
			} else
				return exp;
		}
		break;

		default:
			BUG();
		}
	}

	TREE_READONLY(new_tree) |= TREE_READONLY(exp);

	if (code == INDIRECT_REF || code == ARRAY_REF ||
		code == ARRAY_RANGE_REF)
		TREE_THIS_NOTRAP(new_tree) |= TREE_THIS_NOTRAP(exp);

	return new_tree;
}

tree array_ref_element_size(tree exp)
{
	tree aligned_size = TREE_OPERAND(exp, 3);
	tree elmt_type = TREE_TYPE(TREE_TYPE(TREE_OPERAND(exp, 0)));
	location_t loc = EXPR_LOCATION(exp);

	if (aligned_size) {
		if (TREE_TYPE(aligned_size) != si_sizetype(exp))
			aligned_size = fold_convert_loc(loc, si_sizetype(exp),
							aligned_size);
		return size_binop_loc(loc, MULT_EXPR, aligned_size,
					size_int(TYPE_ALIGN_UNIT(elmt_type)));
	} else {
		return SUBSTITUTE_PLACEHOLDER_IN_EXPR(TYPE_SIZE_UNIT(elmt_type),
							exp);
	}
}

tree array_ref_low_bound(tree exp)
{
	tree domain_type = TYPE_DOMAIN(TREE_TYPE(TREE_OPERAND(exp, 0)));

	if (TREE_OPERAND(exp, 2))
		return TREE_OPERAND(exp, 2);

	if (domain_type && TYPE_MIN_VALUE(domain_type))
		return SUBSTITUTE_PLACEHOLDER_IN_EXPR(
				TYPE_MIN_VALUE(domain_type), exp);

	return build_int_cst(TREE_TYPE(TREE_OPERAND(exp, 1)), 0);
}

tree array_ref_up_bound(tree exp)
{
	tree domain_type = TYPE_DOMAIN(TREE_TYPE(TREE_OPERAND(exp, 0)));

	if (domain_type && TYPE_MAX_VALUE(domain_type))
		return SUBSTITUTE_PLACEHOLDER_IN_EXPR(
				TYPE_MAX_VALUE(domain_type), exp);

	return NULL_TREE;
}

bool array_at_struct_end_p(tree ref)
{
	tree atype;

	if ((TREE_CODE(ref) == ARRAY_REF) ||
		(TREE_CODE(ref) == ARRAY_RANGE_REF)) {
		atype = TREE_TYPE(TREE_OPERAND(ref, 0));
		ref = TREE_OPERAND(ref, 0);
	} else if ((TREE_CODE(ref) == COMPONENT_REF) &&
		   (TREE_CODE(TREE_TYPE(TREE_OPERAND(ref, 1))) == ARRAY_TYPE))
		atype = TREE_TYPE(TREE_OPERAND(ref, 1));
	else
		return false;

	if (TREE_CODE(ref) == STRING_CST)
		return false;

	tree ref_to_array = ref;
	while (handled_component_p(ref)) {
		if (TREE_CODE(ref) == COMPONENT_REF) {
			if (TREE_CODE(TREE_TYPE(TREE_OPERAND(ref, 0))) ==
					RECORD_TYPE) {
				tree nextf = DECL_CHAIN(TREE_OPERAND(ref, 1));
				while (nextf &&
					(TREE_CODE(nextf) != FIELD_DECL))
					nextf = DECL_CHAIN(nextf);
				if (nextf)
					return false;
			}
		} else if (TREE_CODE(ref) == ARRAY_REF)
			return false;
		else if (TREE_CODE(ref) == ARRAY_RANGE_REF)
			;
		else if (TREE_CODE(ref) == VIEW_CONVERT_EXPR)
			break;
		else
			BUG();

		ref = TREE_OPERAND(ref, 0);
	}

	if ((!TYPE_SIZE(atype)) ||
		(!TYPE_DOMAIN(atype)) ||
		(!TYPE_MAX_VALUE(TYPE_DOMAIN(atype))))
		return true;

	if ((TREE_CODE(ref) == MEM_REF) &&
		(TREE_CODE(TREE_OPERAND(ref, 0)) == ADDR_EXPR))
		ref = TREE_OPERAND(TREE_OPERAND(ref, 0), 0);

	if (DECL_P(ref) &&
	    (!(flag_unconstrained_commons) && VAR_P(ref) && DECL_COMMON(ref)) &&
	    DECL_SIZE_UNIT(ref) &&
	    (TREE_CODE(DECL_SIZE_UNIT(ref)) == INTEGER_CST)) {
		poly_int64 offset;
		if ((TREE_CODE(TYPE_SIZE_UNIT(TREE_TYPE(atype))) !=
					INTEGER_CST) ||
		    (TREE_CODE(TYPE_MAX_VALUE(TYPE_DOMAIN(atype))) !=
					INTEGER_CST) ||
		    (TREE_CODE(TYPE_MIN_VALUE(TYPE_DOMAIN(atype))) !=
					INTEGER_CST))
			return true;
		if (!get_addr_base_and_unit_offset(ref_to_array, &offset))
			return true;

		if (known_le((wi::to_offset(TYPE_MAX_VALUE(TYPE_DOMAIN(atype)))
		     - wi::to_offset(TYPE_MIN_VALUE(TYPE_DOMAIN(atype))) + 2)
		    * wi::to_offset(TYPE_SIZE_UNIT(TREE_TYPE(atype))),
		    wi::to_offset(DECL_SIZE_UNIT(ref)) - offset))
			return true;

		return false;
	}

	return true;
}

tree component_ref_field_offset(tree exp)
{
	tree aligned_offset = TREE_OPERAND(exp, 2);
	tree field = TREE_OPERAND(exp, 1);
	location_t loc = EXPR_LOCATION(exp);

	if (aligned_offset) {
		if (TREE_TYPE(aligned_offset) != si_sizetype(exp))
			aligned_offset = fold_convert_loc(loc, si_sizetype(exp),
							  aligned_offset);
		return size_binop_loc(loc, MULT_EXPR, aligned_offset,
					size_int(DECL_OFFSET_ALIGN(field) /
						BITS_PER_UNIT));
	} else
		return SUBSTITUTE_PLACEHOLDER_IN_EXPR(DECL_FIELD_OFFSET(field),
							exp);
}

poly_offset_int mem_ref_offset(const_tree t)
{
	return poly_offset_int::from(wi::to_poly_wide(TREE_OPERAND(t, 1)),
					SIGNED);
}

int si_tree_int_cst_equal(const_tree t1, const_tree t2)
{
	if (t1 == t2)
		return 1;

	if ((t1 == 0) || (t2 == 0))
		return 0;

	if ((TREE_CODE(t1) == INTEGER_CST) &&
		(TREE_CODE(t2) == INTEGER_CST) &&
		(wi::to_widest(t1) == wi::to_widest(t2)))
		return 1;

	return 0;
}
