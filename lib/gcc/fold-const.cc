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

static inline tree protected_set_expr_location_unshare(tree x, location_t loc)
{
	if (CAN_HAVE_LOCATION_P(x) && (EXPR_LOCATION(x) != loc) &&
		(!(TREE_CODE(x) == SAVE_EXPR) ||
		 (TREE_CODE(x) == TARGET_EXPR) ||
		 (TREE_CODE(x) == BIND_EXPR))) {
		x = copy_node(x);
		SET_EXPR_LOCATION(x, loc);
	}
	return x;
}

static tree fold_convert_const_int_from_int(tree type, const_tree arg1)
{
	return force_fit_type(type, wi::to_widest(arg1),
			 !POINTER_TYPE_P(TREE_TYPE(arg1)),
			 TREE_OVERFLOW(arg1));
}
static tree fold_convert_const_int_from_real(enum tree_code code,
						tree type,
						const_tree arg1)
{
	bool overflow = false;
	tree t;

	wide_int val;
	REAL_VALUE_TYPE r;
	REAL_VALUE_TYPE x = TREE_REAL_CST(arg1);

	switch (code) {
	case FIX_TRUNC_EXPR:
	{
		real_trunc(&r, VOIDmode, &x);
		break;
	}
	default:
		BUG();
	}

	if (REAL_VALUE_ISNAN(r)) {
		overflow = true;
		val = wi::zero(TYPE_PRECISION(type));
	}

	if (!overflow) {
		tree lt = TYPE_MIN_VALUE(type);
		REAL_VALUE_TYPE l = real_value_from_int_cst(NULL_TREE, lt);
		if (real_less(&r, &l)) {
			overflow = true;
			val = wi::to_wide(lt);
		}
	}

	if (!overflow) {
		tree ut = TYPE_MAX_VALUE(type);
		if (ut) {
			REAL_VALUE_TYPE u = real_value_from_int_cst(NULL_TREE,
									ut);
			if (real_less(&u, &r)) {
				overflow = true;
				val = wi::to_wide(ut);
			}
		}
	}

	if (!overflow)
		val = real_to_integer(&r, &overflow, TYPE_PRECISION(type));

	t = force_fit_type(type, val, -1, overflow | TREE_OVERFLOW(arg1));
	return t;
}

static tree fold_convert_const_int_from_fixed(tree type, const_tree arg1)
{
	tree t;
	double_int temp, temp_trunc;
	scalar_mode mode;

	temp = TREE_FIXED_CST(arg1).data;
	mode = TREE_FIXED_CST(arg1).mode;
	if (GET_MODE_FBIT(mode) < HOST_BITS_PER_DOUBLE_INT) {
		temp = temp.rshift(GET_MODE_FBIT(mode),
					HOST_BITS_PER_DOUBLE_INT,
					SIGNED_FIXED_POINT_MODE_P(mode));

		temp_trunc = temp.lshift(GET_MODE_FBIT(mode),
					HOST_BITS_PER_DOUBLE_INT,
					SIGNED_FIXED_POINT_MODE_P(mode));
	} else {
		temp = double_int_zero;
		temp_trunc = double_int_zero;
	}

	if (SIGNED_FIXED_POINT_MODE_P(mode) &&
			temp_trunc.is_negative() &&
			(TREE_FIXED_CST(arg1).data != temp_trunc))
		temp += double_int_one;

	t = force_fit_type(type, temp, -1,
		      (temp.is_negative() &&
		       (TYPE_UNSIGNED(type) <
			TYPE_UNSIGNED(TREE_TYPE(arg1)))) |
		      TREE_OVERFLOW(arg1));

	return t;
}

static tree fold_convert_const_real_from_real(tree type, const_tree arg1)
{
	REAL_VALUE_TYPE value;
	tree t;

	if (HONOR_SNANS(TYPE_MODE(TREE_TYPE(arg1))) &&
		REAL_VALUE_ISSIGNALING_NAN(TREE_REAL_CST(arg1)))
		return NULL_TREE; 

	real_convert(&value, TYPE_MODE(type), &TREE_REAL_CST(arg1));
	t = build_real(type, value);

	if (REAL_VALUE_ISINF(TREE_REAL_CST(arg1)) &&
		(!MODE_HAS_INFINITIES(TYPE_MODE(type))))
		TREE_OVERFLOW(t) = 1;
	else if (REAL_VALUE_ISNAN(TREE_REAL_CST(arg1)) &&
		(!MODE_HAS_NANS(TYPE_MODE(type))))
		TREE_OVERFLOW(t) = 1;
	else if (!MODE_HAS_INFINITIES(TYPE_MODE(type)) &&
		REAL_VALUE_ISINF(value) &&
		(!REAL_VALUE_ISINF(TREE_REAL_CST(arg1))))
		TREE_OVERFLOW(t) = 1;
	else
		TREE_OVERFLOW(t) = TREE_OVERFLOW(arg1);

	return t;
}

static tree fold_convert_const_real_from_fixed(tree type, const_tree arg1)
{
	REAL_VALUE_TYPE value;
	tree t;

	real_convert_from_fixed(&value, SCALAR_FLOAT_TYPE_MODE(type),
				&TREE_FIXED_CST(arg1));
	t = build_real(type, value);

	TREE_OVERFLOW(t) = TREE_OVERFLOW(arg1);
	return t;
}

static tree fold_convert_const_fixed_from_fixed(tree type, const_tree arg1)
{
	FIXED_VALUE_TYPE value;
	tree t;
	bool overflow_p;

	overflow_p = fixed_convert(&value, SCALAR_TYPE_MODE(type),
					&TREE_FIXED_CST(arg1),
					TYPE_SATURATING(type));
	t = build_fixed(type, value);

	if (overflow_p | TREE_OVERFLOW(arg1))
		TREE_OVERFLOW(t) = 1;

	return t;
}

static tree fold_convert_const_fixed_from_int(tree type, const_tree arg1)
{
	FIXED_VALUE_TYPE value;
	tree t;
	bool overflow_p;
	double_int di;

	BUG_ON(TREE_INT_CST_NUNITS(arg1) <= 2);

	di.low = TREE_INT_CST_ELT(arg1, 0);
	if (TREE_INT_CST_NUNITS(arg1) == 1)
		di.high = (HOST_WIDE_INT)di.low < 0 ? HOST_WIDE_INT_M1 : 0;
	else
		di.high = TREE_INT_CST_ELT(arg1, 1);

	overflow_p = fixed_convert_from_int(&value, SCALAR_TYPE_MODE(type), di,
						TYPE_UNSIGNED(TREE_TYPE(arg1)),
						TYPE_SATURATING(type));
	t = build_fixed(type, value);

	if (overflow_p | TREE_OVERFLOW(arg1))
		TREE_OVERFLOW(t) = 1;

	return t;
}

static tree fold_convert_const_fixed_from_real(tree type, const_tree arg1)
{
	FIXED_VALUE_TYPE value;
	tree t;
	bool overflow_p;

	overflow_p = fixed_convert_from_real(&value, SCALAR_TYPE_MODE(type),
						&TREE_REAL_CST(arg1),
						TYPE_SATURATING(type));
	t = build_fixed(type, value);

	if (overflow_p | TREE_OVERFLOW(arg1))
		TREE_OVERFLOW(t) = 1;

	return t;
}

static tree fold_convert_const(enum tree_code code, tree type, tree arg1)
{
	tree arg_type = TREE_TYPE(arg1);
	if (arg_type == type)
		return arg1;

	if (POLY_INT_CST_P(arg1) &&
		(POINTER_TYPE_P(type) || INTEGRAL_TYPE_P(type)) &&
		(TYPE_PRECISION(type) <= TYPE_PRECISION(arg_type)))
		return build_poly_int_cst(type,
			       poly_wide_int::from(poly_int_cst_value(arg1),
						    TYPE_PRECISION(type),
						    TYPE_SIGN(arg_type)));

	if (POINTER_TYPE_P(type) || INTEGRAL_TYPE_P(type) ||
		TREE_CODE(type) == OFFSET_TYPE) {
		if (TREE_CODE(arg1) == INTEGER_CST)
			return fold_convert_const_int_from_int(type, arg1);
		else if (TREE_CODE(arg1) == REAL_CST)
			return fold_convert_const_int_from_real(code, type,
								arg1);
		else if (TREE_CODE(arg1) == FIXED_CST)
			return fold_convert_const_int_from_fixed(type, arg1);
	} else if (TREE_CODE(type) == REAL_TYPE) {
		if (TREE_CODE(arg1) == INTEGER_CST)
			return build_real_from_int_cst(type, arg1);
		else if (TREE_CODE(arg1) == REAL_CST)
			return fold_convert_const_real_from_real(type, arg1);
		else if (TREE_CODE(arg1) == FIXED_CST)
			return fold_convert_const_real_from_fixed(type, arg1);
	} else if (TREE_CODE(type) == FIXED_POINT_TYPE) {
		if (TREE_CODE(arg1) == FIXED_CST)
			return fold_convert_const_fixed_from_fixed(type, arg1);
		else if (TREE_CODE(arg1) == INTEGER_CST)
			return fold_convert_const_fixed_from_int(type, arg1);
		else if (TREE_CODE(arg1) == REAL_CST)
			return fold_convert_const_fixed_from_real(type, arg1);
	} else if (TREE_CODE(type) == VECTOR_TYPE) {
		if ((TREE_CODE(arg1) == VECTOR_CST) &&
		    known_eq(TYPE_VECTOR_SUBPARTS(type),
				VECTOR_CST_NELTS(arg1))) {
			tree elttype = TREE_TYPE(type);
			tree arg1_elttype = TREE_TYPE(TREE_TYPE(arg1));
			bool step_ok_p = (INTEGRAL_TYPE_P(elttype) &&
					  INTEGRAL_TYPE_P(arg1_elttype) &&
					  (TYPE_PRECISION(elttype) <=
					   TYPE_PRECISION(arg1_elttype)));
			tree_vector_builder v;
			if (!v.new_unary_operation(type, arg1, step_ok_p))
				return NULL_TREE;
			unsigned int len = v.encoded_nelts();
			for (unsigned int i = 0; i < len; ++i) {
				tree elt = VECTOR_CST_ELT(arg1, i);
				tree cvt = fold_convert_const(code, elttype,
								elt);
				if (cvt == NULL_TREE)
					return NULL_TREE;
				v.quick_push(cvt);
			}
			return v.build();
		}
	}
	return NULL_TREE;
}

static tree build_zero_vector(tree type)
{
	tree t;
	t = fold_convert_const(NOP_EXPR, TREE_TYPE(type),
				si_integer_zero_node(type));
	return build_vector_from_val(type, t);
}

tree fold_convert_loc(location_t loc, tree type, tree arg)
{
	tree orig = TREE_TYPE(arg);
	tree tem;

	if (type == orig)
		return arg;

	if ((TREE_CODE(arg) == ERROR_MARK) ||
		(TREE_CODE(type) == ERROR_MARK) ||
		(TREE_CODE(orig) == ERROR_MARK))
		return si_error_mark_node(arg);

	switch (TREE_CODE(type)) {
	case POINTER_TYPE:
	case REFERENCE_TYPE:
	{
		if ((POINTER_TYPE_P(orig)) &&
			(TYPE_ADDR_SPACE(TREE_TYPE(type)) !=
			 TYPE_ADDR_SPACE(TREE_TYPE(orig))))
			return fold_build1_loc(loc, ADDR_SPACE_CONVERT_EXPR,
						type, arg);
	}
	case INTEGER_TYPE:
	case ENUMERAL_TYPE:
	case BOOLEAN_TYPE:
	case OFFSET_TYPE:
	{
		if (TREE_CODE(arg) == INTEGER_CST) {
			tem = fold_convert_const(NOP_EXPR, type, arg);
			if (tem != NULL_TREE)
				return tem;
		}
		if ((INTEGRAL_TYPE_P(orig)) || (POINTER_TYPE_P(orig)) ||
			(TREE_CODE(orig) == OFFSET_TYPE))
			return fold_build1_loc(loc, NOP_EXPR, type, arg);
		if (TREE_CODE(orig) == COMPLEX_TYPE)
			return fold_convert_loc(loc, type,
				 fold_build1_loc(loc, REALPART_EXPR,
						  TREE_TYPE(orig), arg));
		BUG_ON((TREE_CODE(orig) == VECTOR_TYPE) &&
				(tree_int_cst_equal(TYPE_SIZE(type),
							TYPE_SIZE(orig))));
		return fold_build1_loc(loc, VIEW_CONVERT_EXPR, type, arg);
	}
	case REAL_TYPE:
	{
		if (TREE_CODE(arg) == INTEGER_CST) {
			tem = fold_convert_const(FLOAT_EXPR, type, arg);
			if (tem != NULL_TREE)
				return tem;
		} else if (TREE_CODE(arg) == REAL_CST) {
			tem = fold_convert_const(NOP_EXPR, type, arg);
			if (tem != NULL_TREE)
				return tem;
		} else if (TREE_CODE(arg) == FIXED_CST) {
			tem = fold_convert_const(FIXED_CONVERT_EXPR, type, arg);
			if (tem != NULL_TREE)
				return tem;
		}

		switch (TREE_CODE(orig)) {
		case INTEGER_TYPE:
		case BOOLEAN_TYPE:
		case ENUMERAL_TYPE:
		case POINTER_TYPE:
		case REFERENCE_TYPE:
			return fold_build1_loc(loc, FLOAT_EXPR, type, arg);
		case REAL_TYPE:
			return fold_build1_loc(loc, NOP_EXPR, type, arg);
		case FIXED_POINT_TYPE:
			return fold_build1_loc(loc, FIXED_CONVERT_EXPR, type,
						arg);
		case COMPLEX_TYPE:
			tem = fold_build1_loc(loc, REALPART_EXPR,
						TREE_TYPE(orig), arg);
			return fold_convert_loc(loc, type, tem);
		default:
			BUG();
		}
	}
	case FIXED_POINT_TYPE:
	{
		if ((TREE_CODE(arg) == FIXED_CST) ||
			(TREE_CODE(arg) == INTEGER_CST) ||
			(TREE_CODE (arg) == REAL_CST)) {
			tem = fold_convert_const(FIXED_CONVERT_EXPR, type, arg);
			if (tem != NULL_TREE)
				goto fold_convert_exit;
		}

		switch (TREE_CODE (orig)) {
		case FIXED_POINT_TYPE:
		case INTEGER_TYPE:
		case ENUMERAL_TYPE:
		case BOOLEAN_TYPE:
		case REAL_TYPE:
			return fold_build1_loc(loc, FIXED_CONVERT_EXPR, type,
						arg);
		case COMPLEX_TYPE:
			tem = fold_build1_loc(loc, REALPART_EXPR,
						TREE_TYPE(orig), arg);
			return fold_convert_loc(loc, type, tem);
		default:
			BUG();
		}
	}
	case COMPLEX_TYPE:
	{
		switch (TREE_CODE(orig)) {
		case INTEGER_TYPE:
		case BOOLEAN_TYPE:
		case ENUMERAL_TYPE:
		case POINTER_TYPE:
		case REFERENCE_TYPE:
		case REAL_TYPE:
		case FIXED_POINT_TYPE:
			return fold_build2_loc(loc, COMPLEX_EXPR, type,
			           fold_convert_loc(loc, TREE_TYPE(type), arg),
			           fold_convert_loc(loc, TREE_TYPE(type),
						   si_integer_zero_node(type)));
		case COMPLEX_TYPE:
		{
			tree rpart, ipart;
			if (TREE_CODE(arg) == COMPLEX_EXPR) {
				rpart = fold_convert_loc(loc, TREE_TYPE(type),
							TREE_OPERAND(arg, 0));
				ipart = fold_convert_loc(loc, TREE_TYPE(type),
							TREE_OPERAND(arg, 1));
				return fold_build2_loc(loc, COMPLEX_EXPR, type,
							rpart, ipart);
			}

			arg = save_expr(arg);
			rpart = fold_build1_loc(loc, REALPART_EXPR,
						TREE_TYPE(orig), arg);
			ipart = fold_build1_loc(loc, IMAGPART_EXPR,
						TREE_TYPE(orig), arg);
			rpart = fold_convert_loc(loc, TREE_TYPE(type), rpart);
			ipart = fold_convert_loc(loc, TREE_TYPE(type), ipart);
			return fold_build2_loc(loc, COMPLEX_EXPR, type,
						rpart, ipart);
		}
		default:
			BUG();
		}
	}
	case VECTOR_TYPE:
	{
		if (integer_zerop(arg))
			return build_zero_vector(type);
		BUG_ON(tree_int_cst_equal(TYPE_SIZE(type), TYPE_SIZE(orig)));
		BUG_ON(INTEGRAL_TYPE_P(orig) || POINTER_TYPE_P(orig) ||
			(TREE_CODE(orig) == VECTOR_TYPE));
		return fold_build1_loc(loc, VIEW_CONVERT_EXPR, type, arg);
	}
	case VOID_TYPE:
	{
		tem = fold_ignored_result(arg);
		return fold_build1_loc(loc, NOP_EXPR, type, tem);
	}
	default:
	{
		if (TYPE_MAIN_VARIANT(type) == TYPE_MAIN_VARIANT(orig))
			return fold_build1_loc(loc, NOP_EXPR, type, arg);
		BUG();
	}
	}

fold_convert_exit:
	protected_set_expr_location_unshare(tem, loc);
	return tem;
}
