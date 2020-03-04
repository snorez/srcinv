/*
 * these functions are copied from gcc for case that don't run as gcc plugin
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

/*
 * ************************************************************************
 * htab
 * ************************************************************************
 */
#ifndef CHAR_BIT
#define	CHAR_BIT 8
#endif

#define si_mix(a,b,c)	\
{	\
	a -= b; a -= c; a ^= (c>>13); \
	b -= c; b -= a; b ^= (a<< 8); \
	c -= a; c -= b; c ^= ((b&0xffffffff)>>13); \
	a -= b; a -= c; a ^= ((c&0xffffffff)>>12); \
	b -= c; b -= a; b = (b ^ (a<<16)) & 0xffffffff; \
	c -= a; c -= b; c = (c ^ (b>> 5)) & 0xffffffff; \
	a -= b; a -= c; a = (a ^ (c>> 3)) & 0xffffffff; \
	b -= c; b -= a; b = (b ^ (a<<10)) & 0xffffffff; \
	c -= a; c -= b; c = (c ^ (b>>15)) & 0xffffffff; \
}

static struct prime_ent const si_prime_tab[] = {
	{          7, 0x24924925, 0x9999999b, 2 },
	{         13, 0x3b13b13c, 0x745d1747, 3 },
	{         31, 0x08421085, 0x1a7b9612, 4 },
	{         61, 0x0c9714fc, 0x15b1e5f8, 5 },
	{        127, 0x02040811, 0x0624dd30, 6 },
	{        251, 0x05197f7e, 0x073260a5, 7 },
	{        509, 0x01824366, 0x02864fc8, 8 },
	{       1021, 0x00c0906d, 0x014191f7, 9 },
	{       2039, 0x0121456f, 0x0161e69e, 10 },
	{       4093, 0x00300902, 0x00501908, 11 },
	{       8191, 0x00080041, 0x00180241, 12 },
	{      16381, 0x000c0091, 0x00140191, 13 },
	{      32749, 0x002605a5, 0x002a06e6, 14 },
	{      65521, 0x000f00e2, 0x00110122, 15 },
	{     131071, 0x00008001, 0x00018003, 16 },
	{     262139, 0x00014002, 0x0001c004, 17 },
	{     524287, 0x00002001, 0x00006001, 18 },
	{    1048573, 0x00003001, 0x00005001, 19 },
	{    2097143, 0x00004801, 0x00005801, 20 },
	{    4194301, 0x00000c01, 0x00001401, 21 },
	{    8388593, 0x00001e01, 0x00002201, 22 },
	{   16777213, 0x00000301, 0x00000501, 23 },
	{   33554393, 0x00001381, 0x00001481, 24 },
	{   67108859, 0x00000141, 0x000001c1, 25 },
	{  134217689, 0x000004e1, 0x00000521, 26 },
	{  268435399, 0x00000391, 0x000003b1, 27 },
	{  536870909, 0x00000019, 0x00000029, 28 },
	{ 1073741789, 0x0000008d, 0x00000095, 29 },
	{ 2147483647, 0x00000003, 0x00000007, 30 },
	/* Avoid "decimal constant so large it is unsigned" for 4294967291.  */
	{ 0xfffffffb, 0x00000006, 0x00000008, 31 }
};

static __maybe_unused unsigned int si_higher_prime_index(unsigned long n)
{
	unsigned int low = 0;
	unsigned int high = sizeof(si_prime_tab) / sizeof(si_prime_tab[0]);

	while (low != high) {
		unsigned int mid = low + (high - low) / 2;
		if (n > si_prime_tab[mid].prime)
			low = mid + 1;
		else
			high = mid;
	}

	BUG_ON(n > si_prime_tab[low].prime);
	return low;
}

static __maybe_unused int si_eq_pointer(const void *p1, const void *p2)
{
	return p1 == p2;
}

static size_t si_htab_size(htab_t htab)
{
	return htab->size;
}

static __maybe_unused size_t si_htab_elements(htab_t htab)
{
	return htab->n_elements - htab->n_deleted;
}

static inline hashval_t si_htab_mod_1(hashval_t x, hashval_t y,
					hashval_t inv, int shift)
{
#ifdef UNSIGNED_64BIT_TYPE
	if ((sizeof(hashval_t) * CHAR_BIT) <= 32) {
		hashval_t t1, t2, t3, t4, q, r;

		t1 = ((UNSIGNED_64BIT_TYPE)x * inv) >> 32;
		t2 = x - t1;
		t3 = t2 >> 1;
		t4 = t1 + t3;
		q = t4 >> shift;
		r = x - (q * y);

		return r;
	}
#endif

	return x % y;
}

static inline hashval_t si_htab_mod(hashval_t hash, htab_t htab)
{
	const struct prime_ent *p = &si_prime_tab[htab->size_prime_index];
	return si_htab_mod_1(hash, p->prime, p->inv, p->shift);
}

static inline hashval_t si_htab_mod_m2(hashval_t hash, htab_t htab)
{
	const struct prime_ent *p = &si_prime_tab[htab->size_prime_index];
	return 1 + si_htab_mod_1(hash, p->prime - 2, p->inv_m2, p->shift);
}

/*
 * ************************************************************************
 * histogram_value
 * ************************************************************************
 */
/* from hash_pointer() in libiberty/hashtab.c */
static hashval_t si_htab_hash_pointer(const void *p)
{
	intptr_t v = (intptr_t)p;
	unsigned a, b, c;

	a = b = 0x9e3779b9;
	a += v >> (sizeof(intptr_t) * CHAR_BIT / 2);
	b += v & (((intptr_t)1 << (sizeof(intptr_t) * CHAR_BIT / 2)) - 1);
	c = 0x42135234;
	si_mix(a, b, c);
	return c;
}

static __maybe_unused hashval_t si_histogram_hash(const void *x)
{
	return si_htab_hash_pointer(((const_histogram_value)x)->hvalue.stmt);
}

static int si_histogram_eq(const void *x, const void *y)
{
	return ((const_histogram_value)x)->hvalue.stmt == (const gimple *)y;
}

static void *si_htab_find_with_hash(htab_t htab,
					const void *element,
					hashval_t hash)
{
	hashval_t index, hash2;
	size_t size;
	void *entry;

	htab->searches++;
	size = si_htab_size(htab);
	index = si_htab_mod(hash, htab);

	entry = htab->entries[index];
	if ((entry == HTAB_EMPTY_ENTRY) ||
		((entry != HTAB_DELETED_ENTRY) &&
		 si_histogram_eq(entry, element)))
		return entry;

	hash2 = si_htab_mod_m2(hash, htab);
	for (;;) {
		htab->collisions++;
		index += hash2;
		if (index >= size)
			index -= size;

		entry = htab->entries[index];
		if ((entry == HTAB_EMPTY_ENTRY) ||
			((entry != HTAB_DELETED_ENTRY) &&
			 si_histogram_eq(entry, element)))
			return entry;
	}
}

histogram_value si_gimple_histogram_value(struct function *f,
							gimple *gs)
{
	if (!VALUE_HISTOGRAMS(f))
		return NULL;
	return (histogram_value)si_htab_find_with_hash(VALUE_HISTOGRAMS(f),
						gs, si_htab_hash_pointer(gs));
}

/*
 * ************************************************************************
 * for building tree nodes
 * ************************************************************************
 */
__thread tree si_global_trees[TI_MAX];
__thread tree si_integer_types[itk_none];
__thread tree si_sizetype_tab[(int)stk_type_kind_last];
__thread tree si_current_function_decl;

tree si_build0(enum tree_code code, tree tt)
{
	tree t;
	BUG_ON(!(TREE_CODE_LENGTH(code) == 0));

	t = si_make_node(code);
	TREE_TYPE(t) = tt;
	return t;
}

tree si_build1(enum tree_code code, tree type, tree node)
{
	int length = sizeof(struct tree_exp);
	tree t;

	/* FIXME: record_node_allocation_statistics (code, length); */

	BUG_ON(!(TREE_CODE_LENGTH(code) == 1));

	t = si_ggc_alloc_tree_node_stat(length);
	memset(t, 0, sizeof(struct tree_common));

	TREE_SET_CODE(t, code);

	TREE_TYPE(t) = type;
	SET_EXPR_LOCATION(t, UNKNOWN_LOCATION);
	TREE_OPERAND(t, 0) = node;
	if (node && (!TYPE_P (node))) {
		TREE_SIDE_EFFECTS(t) = TREE_SIDE_EFFECTS(node);
		TREE_READONLY(t) = TREE_READONLY(node);
	}

	if (TREE_CODE_CLASS(code) == tcc_statement) {
		if (code != DEBUG_BEGIN_STMT)
			TREE_SIDE_EFFECTS(t) = 1;
	} else {
		switch (code) {
		case VA_ARG_EXPR:
			TREE_SIDE_EFFECTS(t) = 1;
			TREE_READONLY(t) = 0;
			break;

		case INDIRECT_REF:
			TREE_READONLY(t) = 0;
			break;

		case ADDR_EXPR:
			if (node)
				si_recompute_tree_invariant_for_addr_expr(t);
			break;

		default:
			if (((TREE_CODE_CLASS(code) == tcc_unary) ||
				(code == VIEW_CONVERT_EXPR)) && node &&
				(!TYPE_P(node)) && TREE_CONSTANT(node))
				TREE_CONSTANT(t) = 1;
			if ((TREE_CODE_CLASS(code) == tcc_reference) && node &&
				TREE_THIS_VOLATILE(node))
				TREE_THIS_VOLATILE(t) = 1;
			break;
		}
	}

	return t;
}

tree si_fold_build1_loc(location_t loc, enum tree_code code,
			tree type, tree op0)
{
	tree tem;

	tem = si_fold_unary_loc(loc, code, type, op0);
	if (!tem)
		tem = si_build1_loc(loc, code, type, op0);

	return tem;
}

tree si_fold_build2_loc(location_t loc, enum tree_code code,
			tree type, tree op0, tree op1)
{
	tree tem;

	tem = si_fold_binary_loc(loc, code, type, op0, op1);
	if (!tem)
		tem = si_build2_loc(loc, code, type, op0, op1);

	return tem;
}

tree si_build_fixed(tree type, FIXED_VALUE_TYPE f)
{
	tree v;
	FIXED_VALUE_TYPE *fp;

	v = si_make_node(FIXED_CST);
	/* FIXME: just alloc FIXED_VALUE_TYPE size? */
	fp = si_ggc_alloc(sizeof(FIXED_VALUE_TYPE));
	memcpy(fp, &f, sizeof(FIXED_VALUE_TYPE));

	TREE_TYPE(v) = type;
	TREE_FIXED_CST_PTR(v) = fp;

	return v;
}

bool si_fixed_convert_from_int(FIXED_VALUE_TYPE *f, scalar_mode mode,
			double_int a, bool unsigned_p, bool sat_p)
{
	/* TODO */
	BUG();
}

bool si_fixed_convert_from_real(FIXED_VALUE_TYPE *f, scalar_mode mode,
			 const REAL_VALUE_TYPE *a, bool sat_p)
{
	/* TODO */
	BUG();
}

tree si_build_poly_int_cst(tree type, const poly_wide_int_ref &values)
{
	/* TODO */
	BUG();
}

REAL_VALUE_TYPE si_real_value_from_int_cst(const_tree type, const_tree i)
{
	REAL_VALUE_TYPE d;

	memset(&d, 0, sizeof(d));

	si_real_from_integer(&d, type ? TYPE_MODE(type) : VOIDmode,
				wi::to_wide(i), TYPE_SIGN(TREE_TYPE(i)));
	return d;
}

tree si_build_real_from_int_cst(tree type, const_tree i)
{
	tree v;
	int overflow = TREE_OVERFLOW(i);

	v = si_build_real(type, si_real_value_from_int_cst(type, i));

	TREE_OVERFLOW(v) |= overflow;
	return v;
}

static tree si_fold_convert_const_int_from_int(tree type, const_tree arg1)
{
	return si_force_fit_type(type, wi::to_widest(arg1),
				 !POINTER_TYPE_P(TREE_TYPE(arg1)),
				 TREE_OVERFLOW(arg1));
}

static tree si_fold_convert_const_int_from_real(enum tree_code code,
						tree type, const_tree arg1)
{
	bool overflow = false;
	tree t;

	wide_int val;
	REAL_VALUE_TYPE r;
	REAL_VALUE_TYPE x = TREE_REAL_CST(arg1);

	switch (code) {
	case FIX_TRUNC_EXPR:
		si_real_trunc(&r, VOIDmode, &x);
		break;

	default:
		BUG();
	}

	if (REAL_VALUE_ISNAN(r)) {
		overflow = true;
		val = wi::zero(TYPE_PRECISION(type));
	}

	if (!overflow) {
		tree lt = TYPE_MIN_VALUE(type);
		REAL_VALUE_TYPE l = si_real_value_from_int_cst(NULL_TREE, lt);
		if (real_less(&r, &l)) {
			overflow = true;
			val = wi::to_wide(lt);
		}
	}

	if (!overflow) {
		tree ut = TYPE_MAX_VALUE(type);
		if (ut) {
			REAL_VALUE_TYPE u;
			u = si_real_value_from_int_cst(NULL_TREE, ut);
			if (si_real_less(&u, &r)) {
				overflow = true;
				val = wi::to_wide (ut);
			}
		}
	}

	if (!overflow)
		val = si_real_to_integer(&r, &overflow, TYPE_PRECISION(type));

	t = si_force_fit_type(type, val, -1, overflow | TREE_OVERFLOW(arg1));
	return t;
}

static tree si_fold_convert_const_int_from_fixed(tree type, const_tree arg1)
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

	if (SIGNED_FIXED_POINT_MODE_P(mode) && temp_trunc.is_negative() &&
			(TREE_FIXED_CST(arg1).data != temp_trunc))
		temp += double_int_one;

	t = si_force_fit_type(type, temp, -1,
		(temp.is_negative() &&
		 (TYPE_UNSIGNED(type) < TYPE_UNSIGNED(TREE_TYPE(arg1)))) |
		TREE_OVERFLOW (arg1));

	return t;
}

static tree si_fold_convert_const_real_from_real(tree type, const_tree arg1)
{
	REAL_VALUE_TYPE value;
	tree t;

	if (HONOR_SNANS(TYPE_MODE(TREE_TYPE(arg1))) &&
		REAL_VALUE_ISSIGNALING_NAN(TREE_REAL_CST(arg1)))
		return NULL_TREE;

	si_real_convert(&value, TYPE_MODE(type), &TREE_REAL_CST(arg1));
	t = si_build_real(type, value);

	if (REAL_VALUE_ISINF(TREE_REAL_CST(arg1)) &&
		(!MODE_HAS_INFINITIES(TYPE_MODE(type))))
		TREE_OVERFLOW(t) = 1;
	else if (REAL_VALUE_ISNAN(TREE_REAL_CST(arg1)) &&
		(!MODE_HAS_NANS(TYPE_MODE(type))))
		TREE_OVERFLOW(t) = 1;
	else if ((!MODE_HAS_INFINITIES(TYPE_MODE(type))) &&
		REAL_VALUE_ISINF(value) &&
		(!REAL_VALUE_ISINF(TREE_REAL_CST(arg1))))
		TREE_OVERFLOW(t) = 1;
	else
		TREE_OVERFLOW(t) = TREE_OVERFLOW(arg1);

	return t;
}

static tree si_fold_convert_const_real_from_fixed(tree type, const_tree arg1)
{
	REAL_VALUE_TYPE value;
	tree t;

	si_real_convert_from_fixed(&value, SCALAR_FLOAT_TYPE_MODE(type),
					&TREE_FIXED_CST(arg1));

	t = si_build_real(type, value);

	TREE_OVERFLOW(t) = TREE_OVERFLOW(arg1);
	return t;
}

static tree si_fold_convert_const_fixed_from_fixed(tree type, const_tree arg1)
{
	FIXED_VALUE_TYPE value;
	tree t;
	bool overflow_p;

	overflow_p = si_fixed_convert(&value, SCALAR_TYPE_MODE(type),
					&TREE_FIXED_CST(arg1),
					TYPE_SATURATING(type));

	t = si_build_fixed(type, value);

	if (overflow_p | TREE_OVERFLOW(arg1))
		TREE_OVERFLOW(t) = 1;

	return t;
}

static tree si_fold_convert_const_fixed_from_int(tree type, const_tree arg1)
{
	FIXED_VALUE_TYPE value;
	tree t;
	bool overflow_p;
	double_int di;

	BUG_ON(!(TREE_INT_CST_NUNITS(arg1) <= 2));

	di.low = TREE_INT_CST_ELT(arg1, 0);
	if (TREE_INT_CST_NUNITS(arg1) == 1)
		di.high = (HOST_WIDE_INT)di.low < 0 ? HOST_WIDE_INT_M1 : 0;
	else
		di.high = TREE_INT_CST_ELT(arg1, 1);

	overflow_p = si_fixed_convert_from_int(&value, SCALAR_TYPE_MODE(type),
						di,
						TYPE_UNSIGNED(TREE_TYPE(arg1)),
						TYPE_SATURATING(type));
	t = si_build_fixed(type, value);

	if (overflow_p | TREE_OVERFLOW(arg1))
		TREE_OVERFLOW(t) = 1;

	return t;
}

static tree si_fold_convert_const_fixed_from_real(tree type, const_tree arg1)
{
	FIXED_VALUE_TYPE value;
	tree t;
	bool overflow_p;

	overflow_p = si_fixed_convert_from_real(&value, SCALAR_TYPE_MODE(type),
						&TREE_REAL_CST(arg1),
						TYPE_SATURATING(type));
	t = si_build_fixed(type, value);

	if (overflow_p | TREE_OVERFLOW(arg1))
		TREE_OVERFLOW(t) = 1;

	return t;
}

static tree si_fold_convert_const(enum tree_code code, tree type, tree arg1)
{
	tree arg_type = TREE_TYPE(arg1);
	if (arg_type == type)
		return arg1;

	if (POLY_INT_CST_P(arg1) &&
		(POINTER_TYPE_P(type) || INTEGRAL_TYPE_P(type)) &&
		(TYPE_PRECISION(type) <= TYPE_PRECISION(arg_type)))
		return si_build_poly_int_cst(type,
				poly_wide_int::from(poly_int_cst_value(arg1),
						TYPE_PRECISION(type),
						TYPE_SIGN(arg_type)));

	if (POINTER_TYPE_P(type) || INTEGRAL_TYPE_P(type) ||
		(TREE_CODE(type) == OFFSET_TYPE)) {
		if (TREE_CODE(arg1) == INTEGER_CST)
			return si_fold_convert_const_int_from_int(type, arg1);
		else if (TREE_CODE(arg1) == REAL_CST)
			return si_fold_convert_const_int_from_real(code,
							type, arg1);
		else if (TREE_CODE(arg1) == FIXED_CST)
			return si_fold_convert_const_int_from_fixed(type, arg1);
	} else if (TREE_CODE(type) == REAL_TYPE) {
		if (TREE_CODE(arg1) == INTEGER_CST)
			return si_build_real_from_int_cst(type, arg1);
		else if (TREE_CODE(arg1) == REAL_CST)
			return si_fold_convert_const_real_from_real(type, arg1);
		else if (TREE_CODE(arg1) == FIXED_CST)
			return si_fold_convert_const_real_from_fixed(type,arg1);
	} else if (TREE_CODE(type) == FIXED_POINT_TYPE) {
		if (TREE_CODE(arg1) == FIXED_CST)
			return si_fold_convert_const_fixed_from_fixed(type,
									arg1);
		else if (TREE_CODE(arg1) == INTEGER_CST)
			return si_fold_convert_const_fixed_from_int(type, arg1);
		else if (TREE_CODE(arg1) == REAL_CST)
			return si_fold_convert_const_fixed_from_real(type,arg1);
	} else if (TREE_CODE(type) == VECTOR_TYPE) {
		/* FIXME: tree_vector_builder */
		BUG();
	}

	return NULL_TREE;
}

tree si_fold_ignored_result(tree t)
{
	if (!TREE_SIDE_EFFECTS(t))
		return si_integer_zero_node;

	for (;;) {
		switch (TREE_CODE_CLASS(TREE_CODE(t))) {
		case tcc_unary:
			t = TREE_OPERAND(t, 0);
			break;
		case tcc_binary:
		case tcc_comparison:
		{
			if (!TREE_SIDE_EFFECTS(TREE_OPERAND(t, 1)))
				t = TREE_OPERAND(t, 0);
			else if (!TREE_SIDE_EFFECTS(TREE_OPERAND(t, 0)))
				t = TREE_OPERAND(t, 1);
			else
				return t;
			break;
		}
		case tcc_expression:
		{
			switch (TREE_CODE(t)) {
			case COMPOUND_EXPR:
			{
				if (TREE_SIDE_EFFECTS(TREE_OPERAND(t, 1)))
					return t;
				t = TREE_OPERAND(t, 0);
				break;
			}
			case COND_EXPR:
			{
				if (TREE_SIDE_EFFECTS(TREE_OPERAND(t, 1)) ||
					TREE_SIDE_EFFECTS(TREE_OPERAND(t, 2)))
					return t;
				t = TREE_OPERAND(t, 0);
				break;
			}
			default:
				return t;
			}
			break;
		}
		default:
			return t;
		}
	}
}

tree si_build_vector_from_val(tree vectype, tree sc)
{
	/* TODO */
	BUG();
}

static tree si_build_zero_vector(tree type)
{
	tree t;

	t = si_fold_convert_const(NOP_EXPR, TREE_TYPE(type),
					si_integer_zero_node);
	return si_build_vector_from_val(type, t);
}

tree si_decl_function_context(const_tree decl)
{
	tree context;

	if (TREE_CODE(decl) == ERROR_MARK)
		return 0;
	else if ((TREE_CODE(decl) == FUNCTION_DECL) &&
			DECL_VINDEX(decl))
		context = TYPE_MAIN_VARIANT(TREE_TYPE(
				TREE_VALUE(TYPE_ARG_TYPES(TREE_TYPE(decl)))));
	else
		context = DECL_CONTEXT(decl);

	while (context && (TREE_CODE(context) != FUNCTION_DECL)) {
		if (TREE_CODE(context) == BLOCK)
			context = BLOCK_SUPERCONTEXT(context);
		else
			context = si_get_containing_scope(context);
	}

	return context;
}

bool si_decl_address_invariant_p(const_tree op)
{
	switch (TREE_CODE(op)) {
	case PARM_DECL:
	case RESULT_DECL:
	case LABEL_DECL:
	case FUNCTION_DECL:
		return true;

	case VAR_DECL:
		if ((TREE_STATIC(op) || DECL_EXTERNAL(op)) ||
			DECL_THREAD_LOCAL_P(op) ||
			(DECL_CONTEXT(op) == si_current_function_decl) ||
			(si_decl_function_context(op) ==
			 si_current_function_decl))
			return true;
		break;

	case CONST_DECL:
		if ((TREE_STATIC(op) || DECL_EXTERNAL(op)) ||
			(si_decl_function_context(op) ==
			 si_current_function_decl))
			return true;
		break;
	default:
		break;
	}

	return false;
}

static bool si_tree_invariant_p_1(tree t)
{
	tree op;

	if (TREE_CONSTANT(t) ||
		(TREE_READONLY(t) && (!TREE_SIDE_EFFECTS(t))))
		return true;

	switch (TREE_CODE(t)) {
	case SAVE_EXPR:
		return true;

	case ADDR_EXPR:
	{
		op = TREE_OPERAND(t, 0);
		while (handled_component_p(op)) {
			switch (TREE_CODE(op)) {
			case ARRAY_REF:
			case ARRAY_RANGE_REF:
				if ((!si_tree_invariant_p(TREE_OPERAND(op, 1)))
					|| (TREE_OPERAND(op, 2) != NULL_TREE)
					|| (TREE_OPERAND(op, 3) != NULL_TREE))
					return false;
				break;
			case COMPONENT_REF:
				if (TREE_OPERAND(op, 2) != NULL_TREE)
					return false;
				break;
			default:
				;
			}
			op = TREE_OPERAND(op, 0);
		}
		return CONSTANT_CLASS_P(op) || si_decl_address_invariant_p(op);
	}
	default:
		break;
	}

	return false;
}

bool si_tree_invariant_p(tree t)
{
	tree inner = si_skip_simple_arithmetic(t);
	return si_tree_invariant_p_1(inner);
}

tree si_skip_simple_arithmetic(tree expr)
{
	while (TREE_CODE(expr) == NON_LVALUE_EXPR)
		expr = TREE_OPERAND(expr, 0);

	while (1) {
		if (UNARY_CLASS_P(expr))
			expr = TREE_OPERAND(expr, 0);
		else if (BINARY_CLASS_P(expr)) {
			if (si_tree_invariant_p(TREE_OPERAND(expr, 1)))
				expr = TREE_OPERAND(expr, 0);
			else if (si_tree_invariant_p(TREE_OPERAND(expr, 0)))
				expr = TREE_OPERAND(expr, 1);
			else
				break;
		} else
			break;
	}

	return expr;
}

bool si_contains_placeholder_p(const_tree exp)
{
	enum tree_code code;

	if (!exp)
		return 0;

	code = TREE_CODE(exp);
	if (code == PLACEHOLDER_EXPR)
		return 1;

	switch (TREE_CODE_CLASS(code)) {
	case tcc_reference:
		return CONTAINS_PLACEHOLDER_P(TREE_OPERAND(exp, 0));

	case tcc_exceptional:
		if (code == TREE_LIST)
			return (CONTAINS_PLACEHOLDER_P(TREE_VALUE(exp)) ||
				CONTAINS_PLACEHOLDER_P(TREE_CHAIN(exp)));
		break;

	case tcc_unary:
	case tcc_binary:
	case tcc_comparison:
	case tcc_expression:
	{
		switch (code) {
		case COMPOUND_EXPR:
			return CONTAINS_PLACEHOLDER_P(TREE_OPERAND(exp, 1));

		case COND_EXPR:
			return (CONTAINS_PLACEHOLDER_P(TREE_OPERAND(exp, 0)) ||
				CONTAINS_PLACEHOLDER_P(TREE_OPERAND(exp, 1)) ||
				CONTAINS_PLACEHOLDER_P(TREE_OPERAND(exp, 2)));

		case SAVE_EXPR:
			return 0;

		default:
			break;
		}

		switch (TREE_CODE_LENGTH(code)) {
		case 1:
			return CONTAINS_PLACEHOLDER_P(TREE_OPERAND(exp, 0));
		case 2:
			return (CONTAINS_PLACEHOLDER_P(TREE_OPERAND(exp, 0)) ||
				CONTAINS_PLACEHOLDER_P(TREE_OPERAND(exp, 1)));
		default:
			return 0;
		}
	}

	case tcc_vl_exp:
		switch (code) {
		case CALL_EXPR:
		{
			const_tree arg;
			const_call_expr_arg_iterator iter;
			FOR_EACH_CONST_CALL_EXPR_ARG(arg, iter, exp)
				if (CONTAINS_PLACEHOLDER_P(arg))
					return 1;
			return 0;
		}
		default:
			return 0;
		}

	default:
		return 0;
	}

	return 0;
}

tree si_save_expr(tree expr)
{
	tree inner;

	inner = si_skip_simple_arithmetic(expr);
	if (TREE_CODE(inner) == ERROR_MARK)
		return inner;

	if (si_tree_invariant_p_1(inner))
		return expr;

	if (si_contains_placeholder_p(inner))
		return expr;

	/* FIXME: EXPR_LOCATION(expr) */
	expr = si_build1_loc(UNKNOWN_LOCATION, SAVE_EXPR,
				TREE_TYPE(expr), expr);

	TREE_SIDE_EFFECTS(expr) = 1;
	return expr;
}

tree si_fold_convert_loc(location_t loc, tree type, tree arg)
{
	tree orig = TREE_TYPE(arg);
	tree tem;

	if (type == orig)
		return arg;

	if ((TREE_CODE(arg) == ERROR_MARK) ||
		(TREE_CODE(type) == ERROR_MARK) ||
		(TREE_CODE(orig) == ERROR_MARK))
		return si_error_mark_node;

	switch (TREE_CODE(type)) {
	case POINTER_TYPE:
	case REFERENCE_TYPE:
	{
		if ((POINTER_TYPE_P(orig)) &&
			(TYPE_ADDR_SPACE(TREE_TYPE(type)) !=
			 TYPE_ADDR_SPACE(TREE_TYPE(orig))))
			return si_fold_build1_loc(loc, ADDR_SPACE_CONVERT_EXPR,
							type, arg);
		/* fall through */
	}
	case INTEGER_TYPE:
	case ENUMERAL_TYPE:
	case BOOLEAN_TYPE:
	case OFFSET_TYPE:
	{
		if (TREE_CODE(arg) == INTEGER_CST) {
			tem = si_fold_convert_const(NOP_EXPR, type, arg);
			if (tem != NULL_TREE)
				return tem;
		}

		if (INTEGRAL_TYPE_P(orig) || POINTER_TYPE_P(orig) ||
			(TREE_CODE(orig) == OFFSET_TYPE))
			return si_fold_build1_loc(loc, NOP_EXPR, type, arg);

		if (TREE_CODE(orig) == COMPLEX_TYPE)
			return si_fold_convert_loc(loc, type,
					si_fold_build1_loc(loc, REALPART_EXPR,
						TREE_TYPE(orig), arg));
		BUG_ON(!((TREE_CODE(orig) == VECTOR_TYPE) &&
			si_tree_int_cst_equal(TYPE_SIZE(type),
						TYPE_SIZE(orig))));
		return si_fold_build1_loc(loc, VIEW_CONVERT_EXPR, type, arg);
	}
	case REAL_TYPE:
	{
		if (TREE_CODE(arg) == INTEGER_CST) {
			tem = si_fold_convert_const(FLOAT_EXPR, type, arg);
			if (tem != NULL_TREE)
				return tem;
		} else if (TREE_CODE(arg) == REAL_CST) {
			tem = si_fold_convert_const(NOP_EXPR, type, arg);
			if (tem != NULL_TREE)
				return tem;
		} else if (TREE_CODE(arg) == FIXED_CST) {
			tem = si_fold_convert_const(FIXED_CONVERT_EXPR,
							type, arg);
			if (tem != NULL_TREE)
				return tem;
		}

		switch (TREE_CODE(orig)) {
		case INTEGER_TYPE:
		case BOOLEAN_TYPE:
		case ENUMERAL_TYPE:
		case POINTER_TYPE:
		case REFERENCE_TYPE:
			return si_fold_build1_loc(loc, FLOAT_EXPR, type, arg);
		case REAL_TYPE:
			return si_fold_build1_loc(loc, NOP_EXPR, type, arg);
		case FIXED_POINT_TYPE:
			return si_fold_build1_loc(loc, FIXED_CONVERT_EXPR,
							type, arg);
		case COMPLEX_TYPE:
			tem = si_fold_build1_loc(loc, REALPART_EXPR,
						 TREE_TYPE(orig), arg);
			return si_fold_convert_loc(loc, type, tem);
		default:
			BUG();
		}
	}
	case FIXED_POINT_TYPE:
	{
		if ((TREE_CODE(arg) == FIXED_CST) ||
			(TREE_CODE(arg) == INTEGER_CST) ||
			(TREE_CODE(arg) == REAL_CST)) {
			tem = si_fold_convert_const(FIXED_CONVERT_EXPR,
							type, arg);
			if (tem != NULL_TREE)
				goto out;
		}

		switch (TREE_CODE(orig)) {
		case FIXED_POINT_TYPE:
		case INTEGER_TYPE:
		case ENUMERAL_TYPE:
		case BOOLEAN_TYPE:
		case REAL_TYPE:
			return si_fold_build1_loc(loc, FIXED_CONVERT_EXPR,
							type, arg);
		case COMPLEX_TYPE:
			tem = si_fold_build1_loc(loc, REALPART_EXPR,
						 TREE_TYPE(orig), arg);
			return si_fold_convert_loc(loc, type, tem);
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
			return si_fold_build2_loc(loc, COMPLEX_EXPR, type,
					si_fold_convert_loc(loc,
						TREE_TYPE(type), arg),
					si_fold_convert_loc(loc,
						TREE_TYPE(type),
						si_integer_zero_node));
		case COMPLEX_TYPE:
		{
			tree rpart, ipart;
			if (TREE_CODE(arg) == COMPLEX_EXPR) {
				rpart = si_fold_convert_loc(loc,
						TREE_TYPE(type),
						TREE_OPERAND(arg, 0));
				ipart = si_fold_convert_loc(loc,
						TREE_TYPE(type),
						TREE_OPERAND(arg, 1));
				return si_fold_build2_loc(loc, COMPLEX_EXPR,
						type, rpart, ipart);
			}

			arg = si_save_expr(arg);
			rpart = si_fold_build1_loc(loc, REALPART_EXPR,
					TREE_TYPE(orig), arg);
			ipart = si_fold_build1_loc(loc, IMAGPART_EXPR,
					TREE_TYPE(orig), arg);
			rpart = si_fold_convert_loc(loc, TREE_TYPE(type),
					rpart);
			ipart = si_fold_convert_loc(loc, TREE_TYPE(type),
					ipart);
			return si_fold_build2_loc(loc, COMPLEX_EXPR,
					type, rpart, ipart);
		}
		default:
			BUG();
		}
	}
	case VECTOR_TYPE:
	{
		if (si_integer_zerop(arg))
			return si_build_zero_vector(type);
		BUG_ON(!(si_tree_int_cst_equal(TYPE_SIZE(type),
						TYPE_SIZE(orig))));
		BUG_ON(!(INTEGRAL_TYPE_P(orig) || POINTER_TYPE_P(orig) ||
				(TREE_CODE(orig) == VECTOR_TYPE)));
		return si_fold_build1_loc(loc, VIEW_CONVERT_EXPR, type, arg);
	}
	case VOID_TYPE:
	{
		tem = si_fold_ignored_result(arg);
		return si_fold_build1_loc(loc, NOP_EXPR, type, tem);
	}
	default:
	{
		if (TYPE_MAIN_VARIANT(type) == TYPE_MAIN_VARIANT(orig))
			return si_fold_build1_loc(loc, NOP_EXPR, type, arg);
		BUG();
	}
	}

out:
	/*
	 * FIXME: the gcc ignore the return value,,,
	 * si_protected_set_expr_loc_unshare(tem, loc);
	 */
	return tem;
}

tree si_component_ref_field_offset(tree exp)
{
	tree aligned_offset = TREE_OPERAND(exp, 2);
	tree field = TREE_OPERAND(exp, 1);
	location_t loc = UNKNOWN_LOCATION;

	if (aligned_offset) {
		if (TREE_TYPE(aligned_offset) != si_sizetype)
			aligned_offset = si_fold_convert_loc(loc, sizetype,
					aligned_offset);
		return size_binop_loc(loc, MULT_EXPR, aligned_offset,
				size_int(DECL_OFFSET_ALIGN(field) /
					BITS_PER_UNIT));
	} else
		return SI_SS_PH_IN_EXPR(DECL_FIELD_OFFSET(field), exp);
}
