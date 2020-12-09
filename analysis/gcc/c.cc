/*
 * this should match with collect/c.cc
 * NOTE: this should be able to used in multiple thread
 * handle the information collected in lower gimple at ALL_IPA_PASSES_END
 * We assume that a static inline function has the same body in separate source
 *	files.
 *
 * the version of the code used in phase4 of gcc is 8.3.0
 * phase 4 may also be used in c++/... that compiled by gcc
 *
 * FIXME:
 *	PHASE3: is there any race condition?
 *
 * TODO:
 *	rewrite, get_target_field0(), use TERE_CHAIN()
 *	todos and si_log1_todo
 *	TREE_INT_CST_LOW may not be enough to represent the INT_CST
 *	ARRAY_REF size is quite different. array_ref_element_size().
 *	dsv_init:
 *		use possible_list
 *		give a condition, and a proposed result, gen the value
 *		lookup the related functions, to init some kinds of objects.
 *
 * phase 1-3 are solid now, 4-6 are bad.
 * phase4: init value, mark var parm function ONLY
 * phase5: do direct/indirect calls
 * phase6: do parm calls and some other check
 *
 * dump_function_to_file() in gcc/tree-cfg.c
 *	Dump function_decl fn to file using flags
 *
 * UPDATE:
 *   2020-08-07
 *     TYPE_SIZE in bits while TYPE_SIZE_UNIT in bytes
 *
 * Copyright (C) 2018  zerons
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
#ifndef vecpfx
#define	vecpfx m_vecpfx
#endif

#ifndef vecdata
#define	vecdata m_vecdata
#endif

#include "si_gcc.h"

/*
 * ************************************************************************
 * main
 * ************************************************************************
 */
static int c_parse(struct sibuf *, int);
static int dec(struct sample_set *, int idx, struct func_node *);
static void *get_global(struct sibuf *b, const char *string, int *);
static struct lang_ops c_ops;

CLIB_MODULE_NAME(c);
CLIB_MODULE_NEEDEDx(0);

CLIB_MODULE_INIT()
{
	c_ops.parse = c_parse;
	c_ops.dec = dec;
	c_ops.get_global = get_global;
	c_ops.type.binary = SI_TYPE_SRC;
	c_ops.type.kernel = SI_TYPE_BOTH;
	c_ops.type.os_type = SI_TYPE_OS_LINUX;
	c_ops.type.data_fmt = SI_TYPE_DF_GIMPLE;
	register_lang_ops(&c_ops);
	return 0;
}

CLIB_MODULE_EXIT()
{
	unregister_lang_ops(&c_ops);
	return;
}

static __thread int mode = MODE_TEST;
static __thread struct file_obj *objs;
static __thread size_t obj_cnt, obj_idx, obj_adjusted, real_obj_cnt;
static __thread void *addr_base = NULL;
static __thread struct sibuf *cur_sibuf;

/* these two are for optimization purpose */
static __thread void **real_addrs;
static __thread void **fake_addrs;

/* for phase 4 5 6 */
/* cur_fndecl is replaced by current_function_decl */
static __thread gimple_seq cur_gimple = NULL;
static __thread unsigned long cur_gimple_op_idx = 0;
static __thread struct sinode *cur_fsn = NULL;
static __thread struct func_node *cur_fn = NULL;
static __thread tree si_current_function_decl = NULL;
static __thread void **phase4_obj_checked;
static __thread size_t phase4_obj_idx = 0;

/* get obj_cnt without gcc global vars */
static inline void get_real_obj_cnt(void)
{
	for (size_t i = 0; i < obj_cnt; i++) {
		if (!objs[i].gcc_global_varidx)
			continue;
		real_obj_cnt = i;
		return;
	}
	real_obj_cnt = obj_cnt;
	return;
}

static inline void __get_real_addr(void **fake_addr, int *do_next, int reverse)
{
	/*
	 * this function should only return the real addr once,
	 * and then return some other value that means this orig_addr has
	 * already been taken care.
	 */
	size_t i = 0;
	*do_next = 0;
	if (unlikely(!*fake_addr))
		return;

	size_t loop_limit = obj_cnt;
	if (unlikely(reverse && (loop_limit > (MAX_OBJS_PER_FILE / 8)))) {
		loop_limit = MAX_OBJS_PER_FILE / 8;
		for (i = loop_limit; i < obj_cnt; i++) {
			if (unlikely((fake_addrs[i] == *fake_addr) &&
					objs[i].real_addr)) {
				*fake_addr = (void *)(unsigned long)
						objs[i].real_addr;
				if (!objs[i].is_adjusted)
					*do_next = 1;
				return;
			}
		}
	}
	for (i = 0; i < loop_limit; i++) {
		if (unlikely((fake_addrs[i] == *fake_addr) && objs[i].real_addr)) {
			*fake_addr = (void *)(unsigned long)objs[i].real_addr;
			if (!objs[i].is_adjusted)
				*do_next = 1;
			return;
		}
	}
	/* XXX, if not found, which is possible, set it NULL */
	*fake_addr = NULL;
	return;
}

static inline void get_real_addr(void **fake_addr, int *do_next)
{
	__get_real_addr(fake_addr, do_next, 1);
}

static inline void get_real_addr_1(void **fake_addr, int *do_next)
{
	__get_real_addr(fake_addr, do_next, 0);
}

#define	do_nothing() \
	({ do {;}while(0); })
#define	do_real_addr(addr,next) \
	({ \
	 void **ptr = (void **)addr; \
	 int do_next; \
	 get_real_addr(ptr, &do_next); \
	 if (do_next) \
		next; \
	 })

static inline int __is_obj_checked(void *real_addr, int reverse)
{
	switch (mode) {
	case MODE_ADJUST:
	{
		size_t i = 0;
		size_t loop_limit = obj_cnt;
		if (unlikely(reverse &&
				(loop_limit > (MAX_OBJS_PER_FILE / 8)))) {
			loop_limit = MAX_OBJS_PER_FILE / 8;
			for (i = loop_limit; i < obj_cnt; i++) {
				if (unlikely(real_addr == real_addrs[i])) {
					obj_idx = i;
					if (objs[i].is_adjusted)
						return 1;
					objs[i].is_adjusted = 1;
					obj_adjusted++;
					return 0;
				}
			}
		}

		for (i = 0; i < loop_limit; i++) {
			if (unlikely(real_addr == real_addrs[i])) {
				obj_idx = i;
				if (objs[i].is_adjusted)
					return 1;
				objs[i].is_adjusted = 1;
				obj_adjusted++;
				return 0;
			}
		}
		BUG();
	}
	case MODE_GETSTEP4:
	{
		size_t i = 0;
		size_t loop_limit = phase4_obj_idx;
		if (unlikely(reverse &&
				(loop_limit > (MAX_OBJS_PER_FILE / 8)))) {
			loop_limit = MAX_OBJS_PER_FILE / 8;
			for (i = loop_limit; i < phase4_obj_idx; i++) {
				if (unlikely(real_addr ==
						phase4_obj_checked[i])) {
					return 1;
				}
			}
		}

		for (i = 0; i < loop_limit; i++) {
			if (unlikely(real_addr == phase4_obj_checked[i])) {
				return 1;
			}
		}

		phase4_obj_checked[phase4_obj_idx] = real_addr;
		phase4_obj_idx++;
		BUG_ON(phase4_obj_idx >= obj_cnt);

		return 0;
	}
	default:
		BUG();
	}
}

static inline int is_obj_checked(void *real_addr)
{
	return __is_obj_checked(real_addr, 1);
}

static inline int is_obj_checked_1(void *real_addr)
{
	return __is_obj_checked(real_addr, 0);
}

static void do_tree(tree node);
static void do_base(tree node, int flag);
static void do_typed(tree node, int flag);
static void do_common(tree node, int flag);
static void do_int_cst(tree node, int flag);
static void do_real_cst(tree node, int flag);
static void do_fixed_cst(tree node, int flag);
static void do_vector(tree node, int flag);
static void do_string(tree node, int flag);
static void do_complex(tree node, int flag);
static void do_identifier(tree node, int flag);
static void do_c_lang_identifier(tree node, int flag);
static void do_decl_minimal(tree node, int flag);
static void do_decl_common(tree node, int flag);
static void do_decl_with_rtl(tree node, int flag);
static void do_decl_non_common(tree node, int flag);
static void do_parm_decl(tree node, int flag);
static void do_decl_with_vis(tree node, int flag);
static void do_var_decl(tree node, int flag);
static void do_field_decl(tree node, int flag);
static void do_label_decl(tree node, int flag);
static void do_result_decl(tree node, int flag);
static void do_const_decl(tree node, int flag);
static void do_type_decl(tree node, int flag);
static void do_function_decl(tree node, int flag);
static void do_translation_unit_decl(tree node, int flag);
static void do_type_common(tree node, int flag);
static void do_type_with_lang_specific(tree node, int flag);
static void do_type_non_common(tree node, int flag);
static void do_list(tree node, int flag);
static void do_vec(tree node, int flag);
static void do_exp(tree node, int flag);
static void do_ssa_name(tree node, int flag);
static void do_block(tree node, int flag);
static void do_binfo(tree node, int flag);
static void do_statement_list(tree node, int flag);
static void do_constructor(tree node, int flag);
static void do_omp_clause(tree node, int flag);
static void do_optimization_option(tree node, int flag);
static void do_target_option(tree node, int flag);

static inline int __maybe_unused is_location(unsigned long fake_addr)
{
	return fake_addr >= (unsigned long)0x800000000000;
}
static expanded_location *my_expand_location(location_t *loc)
{
	switch (mode) {
	case MODE_ADJUST:
	{
#if 0
		size_t i = 0;
		size_t this_idx = 0;
		for (i = 0; i < obj_cnt; i++) {
			if ((is_location(objs[i].fake_addr)) &&
					(!objs[i].is_adjusted)) {
				this_idx = i;
				break;
			}
		}
		BUG_ON(i == obj_cnt);
#else
		/*
		 * FIXME: there might be an issue here:
		 *	the obj_idx may not be the right location we save
		 */
		size_t __maybe_unused saved_idx = obj_idx;
		while (unlikely(objs[obj_idx].is_adjusted))
			obj_idx++;
		size_t this_idx = obj_idx;
		BUG_ON(!is_location(objs[this_idx].fake_addr));
#endif

		*loc = objs[this_idx].offs;
		objs[this_idx].is_adjusted = 1;
		obj_adjusted++;
		expanded_location *ret;
		ret = (expanded_location *)(
				(unsigned long)objs[this_idx].real_addr);
		return ret;
	}
	default:
		BUG();
	}
}

static void do_location(location_t *loc)
{
	switch (mode) {
	case MODE_ADJUST:
	{
		CLIB_DBG_FUNC_ENTER();
		expanded_location *xloc = my_expand_location(loc);
		do_real_addr(&xloc->file, is_obj_checked((void *)xloc->file));
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	default:
		BUG();
	}
}

static void do_vec_tree(void *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		CLIB_DBG_FUNC_ENTER();
		vec<tree, va_gc, vl_embed> *node0;
		node0 = (vec<tree, va_gc, vl_embed> *)node;

		struct vec_prefix *pfx = (struct vec_prefix *)&node0->vecpfx;
		unsigned long length = pfx->m_num;
		tree *addr = &node0->vecdata[0];
		for (unsigned long i = 0; i < length; i++) {
			do_real_addr(&addr[i], do_tree(addr[i]));
		}
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	case MODE_GETSTEP4:
	{
		return;
	}
	default:
		BUG();
	}
}

static void do_vec_constructor(void *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		CLIB_DBG_FUNC_ENTER();
		vec<constructor_elt, va_gc> *node0 =
			(vec<constructor_elt, va_gc> *)node;
		unsigned long length = node0->vecpfx.m_num;
		struct constructor_elt *addr = &node0->vecdata[0];
		for (unsigned long i = 0; i < length; i++) {
			do_real_addr(&addr[i].index,do_tree(addr[i].index));
			do_real_addr(&addr[i].value,do_tree(addr[i].value));
		}
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	case MODE_GETSTEP4:
	{
		CLIB_DBG_FUNC_ENTER();
		vec<constructor_elt, va_gc> *node0 =
			(vec<constructor_elt, va_gc> *)node;
		unsigned long length = node0->vecpfx.m_num;
		struct constructor_elt *addr = &node0->vecdata[0];
		for (unsigned long i = 0; i < length; i++) {
			do_tree(addr[i].index);
			do_tree(addr[i].value);
		}
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	default:
		BUG();
	}
}

struct GTY((chain_next ("%h.outer"))) c_scope { /* gcc/c/c-decl.c */
	struct c_scope *outer;
	struct c_scope *outer_function;
	struct c_binding *bindings;
	tree blocks;
	tree blocks_last;
	unsigned int depth : 28;
	BOOL_BITFIELD parm_flag : 1;
	BOOL_BITFIELD had_vla_unspec : 1;
	BOOL_BITFIELD warned_forward_parm_decls : 1;
	BOOL_BITFIELD function_body : 1;
	BOOL_BITFIELD keep : 1;
	BOOL_BITFIELD float_const_decimal64 : 1;
	BOOL_BITFIELD has_label_bindings : 1;
	BOOL_BITFIELD has_jump_unsafe_decl : 1;
};
static void do_c_binding(struct c_binding *node, int flag);
static void do_c_scope(struct c_scope *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		CLIB_DBG_FUNC_ENTER();
		do_real_addr(&node->outer, do_c_scope(node->outer, 1));
		do_real_addr(&node->outer_function,
				do_c_scope(node->outer_function, 1));
		do_real_addr(&node->bindings, do_c_binding(node->bindings, 1));
		do_real_addr(&node->blocks, do_tree(node->blocks));
		do_real_addr(&node->blocks_last, do_tree(node->blocks_last));
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	case MODE_GETSTEP4:
	{
		return;
	}
	default:
		BUG();
	}
}

struct GTY(()) c_spot_bindings { /* gcc/c/c-decl.c */
	struct c_scope *scope;
	struct c_binding *bindings_in_scope;
	int stmt_exprs;
	bool left_stmt_expr;
};
static void do_c_spot_bindings(struct c_spot_bindings *node,
						int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		CLIB_DBG_FUNC_ENTER();
		do_real_addr(&node->scope, do_c_scope(node->scope, 1));
		do_real_addr(&node->bindings_in_scope,
				do_c_binding(node->bindings_in_scope, 1));
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	case MODE_GETSTEP4:
	{
		return;
	}
	default:
		BUG();
	}
}

struct GTY(()) c_goto_bindings { /* gcc/c/c-decl.c */
	location_t loc;
	struct c_spot_bindings goto_bindings;
};
typedef struct c_goto_bindings *c_goto_bindings_p;
static void do_c_goto_bindings(struct c_goto_bindings *node,
						int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		CLIB_DBG_FUNC_ENTER();
		do_location(&node->loc);
		do_c_spot_bindings(&node->goto_bindings, 0);
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	case MODE_GETSTEP4:
	{
		return;
	}
	default:
		BUG();
	}
}
static void do_vec_c_goto_bindings(void *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		CLIB_DBG_FUNC_ENTER();
		vec<c_goto_bindings_p, va_gc> *node0;
		node0 = (vec<c_goto_bindings_p, va_gc> *)node;
		unsigned long len = node0->vecpfx.m_num;
		c_goto_bindings_p *addr = node0->vecdata;
		for (unsigned long i = 0; i < len; i++) {
			do_real_addr(&addr[i], do_c_goto_bindings(addr[i], 1));
		}
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	case MODE_GETSTEP4:
	{
		return;
	}
	default:
		BUG();
	}
}

struct GTY(()) c_label_vars { /* gcc/c/c-decl.c */
	struct c_label_vars *shadowed;
	struct c_spot_bindings label_bindings;
	vec<tree, va_gc> *decls_in_scope;
	vec<c_goto_bindings_p, va_gc> *gotos;
};
static void do_c_label_vars(struct c_label_vars *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		CLIB_DBG_FUNC_ENTER();
		do_real_addr(&node->shadowed,
				do_c_label_vars(node->shadowed, 1));
		do_c_spot_bindings(&node->label_bindings, 0);
		do_real_addr(&node->decls_in_scope,
				do_vec_tree((void *)node->decls_in_scope, 1));
		do_real_addr(&node->gotos,
				do_vec_c_goto_bindings((void *)node->gotos,1));
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	case MODE_GETSTEP4:
	{
		return;
	}
	default:
		BUG();
	}
}

struct GTY((chain_next ("%h.prev"))) c_binding { /* gcc/c/c-decl.c */
	union GTY(()) {
		tree GTY((tag ("0"))) type;
		struct c_label_vars * GTY((tag ("1"))) label;
	} GTY((desc ("TREE_CODE (%0.decl) == LABEL_DECL"))) u;
	tree decl;
	tree id;
	struct c_binding *prev;
	struct c_binding *shadowed;
	unsigned int depth : 28;
	BOOL_BITFIELD invisible : 1;
	BOOL_BITFIELD nested : 1;
	BOOL_BITFIELD inner_comp : 1;
	BOOL_BITFIELD in_struct : 1;
	location_t locus;
};
static void do_c_binding(struct c_binding *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		CLIB_DBG_FUNC_ENTER();
		do_location(&node->locus);
		do_real_addr(&node->decl, do_tree(node->decl));
		if (TREE_CODE(node->decl) != LABEL_DECL)
			do_real_addr(&node->u.type, do_tree(node->u.type));
		else
			do_real_addr(&node->u.label,
					do_c_label_vars(node->u.label, 1));
		do_real_addr(&node->id, do_tree(node->id));
		do_real_addr(&node->prev, do_c_binding(node->prev, 1));
		do_real_addr(&node->shadowed, do_c_binding(node->shadowed, 1));
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	case MODE_GETSTEP4:
	{
		return;
	}
	default:
		BUG();
	}
}

static void do_cpp_macro(struct cpp_macro *node, int flag);
static void do_ht_identifier(struct ht_identifier *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		CLIB_DBG_FUNC_ENTER();
		do_real_addr(&node->str, is_obj_checked((void *)node->str));
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	case MODE_GETSTEP4:
	{
		return;
	}
	default:
		BUG();
	}
}

static void do_answer(struct answer *node, int flag);
static void do_cpp_hashnode(struct cpp_hashnode *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		CLIB_DBG_FUNC_ENTER();
		do_ht_identifier(&node->ident, 0);
#ifdef COLLECT_MORE
		struct cpp_hashnode node0 = *node;
		switch (CPP_HASHNODE_VALUE_IDX(node0)) {
		case NTV_MACRO:
			do_real_addr(&node->value.macro,
					do_cpp_macro(node->value.macro, 1));
			break;
		case NTV_ANSWER:
			do_real_addr(&node->value.answers,
					do_answer(node->value.answers, 1));
			break;
		case NTV_BUILTIN:
		case NTV_ARGUMENT:
		case NTV_NONE:
			break;
		default:
			BUG();
		}
#endif
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	case MODE_GETSTEP4:
	{
		return;
	}
	default:
		BUG();
	}
}

static void do_cpp_marco_arg(struct cpp_macro_arg *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		CLIB_DBG_FUNC_ENTER();
		do_real_addr(&node->spelling,
				do_cpp_hashnode(node->spelling, 1));
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	case MODE_GETSTEP4:
	{
		return;
	}
	default:
		BUG();
	}
}

static void do_cpp_string(struct cpp_string *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		CLIB_DBG_FUNC_ENTER();
		do_real_addr(&node->text, is_obj_checked((void *)node->text));
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	case MODE_GETSTEP4:
	{
		return;
	}
	default:
		BUG();
	}
}

static void do_cpp_identifier(struct cpp_identifier *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		CLIB_DBG_FUNC_ENTER();
		do_real_addr(&node->node, do_cpp_hashnode(node->node, 1));
		do_real_addr(&node->spelling,
				do_cpp_hashnode(node->spelling, 1));
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	case MODE_GETSTEP4:
	{
		return;
	}
	default:
		BUG();
	}
}

static void do_cpp_token(struct cpp_token *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		CLIB_DBG_FUNC_ENTER();
		do_location(&node->src_loc);
		switch (*((char *)node + sizeof(*node))) {
		case CPP_TOKEN_FLD_NODE:
			do_cpp_identifier(&node->val.node, 0);
			break;
		case CPP_TOKEN_FLD_SOURCE:
			do_real_addr(&node->val.source,
					do_cpp_token(node->val.source, 1));
			break;
		case CPP_TOKEN_FLD_STR:
			do_cpp_string(&node->val.str, 0);
			break;
		case CPP_TOKEN_FLD_ARG_NO:
			do_cpp_marco_arg(&node->val.macro_arg, 0);
			break;
		case CPP_TOKEN_FLD_TOKEN_NO:
		case CPP_TOKEN_FLD_PRAGMA:
			break;
		default:
			BUG();
		}
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	case MODE_GETSTEP4:
	{
		return;
	}
	default:
		BUG();
	}
}

#if __GNUC__ < 9
struct GTY(()) cpp_macro { /* libcpp/include/cpp-id-data.h */
	cpp_hashnode **GTY((nested_ptr(union tree_node,
			"%h?CPP_HASHNODE(GCC_IDENT_TO_HT_IDENT(%h)):NULL",
			"%h?HT_IDENT_TO_GCC_IDENT(HT_NODE(%h)):NULL"),
			length("%h.paramc"))) params;
	union cpp_macro_u {
		cpp_token *GTY((tag("0"), length("%0.count"))) tokens;
		const unsigned char *GTY((tag("1"))) text;
	} GTY((desc("%1.traditional"))) exp;
	source_location line;
	unsigned int count;
	unsigned short paramc;
	unsigned int fun_like : 1;
	unsigned int variadic : 1;
	unsigned int syshdr : 1;
	unsigned int traditional : 1;
	unsigned int extra_tokens : 1;
};
static void __maybe_unused do_cpp_macro(struct cpp_macro *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		CLIB_DBG_FUNC_ENTER();
		do_location(&node->line);
		if (node->params) {
			do_real_addr(&node->params,
					is_obj_checked(node->params));
			cpp_hashnode **addr = node->params;
			for (unsigned short i = 0; i < node->paramc; i++) {
				do_real_addr(&addr[i],
						do_cpp_hashnode(addr[i], 1));
			}
		}
		switch (node->traditional) {
		case 0:
		{
			if (node->exp.tokens) {
				do_real_addr(&node->exp.tokens,
					is_obj_checked(node->exp.tokens));
				struct cpp_token *addr = node->exp.tokens;
				for (unsigned int i = 0;i < node->count;i++)
					do_cpp_token(&addr[i], 0);
			}
			break;
		}
		case 1:
		{
			if (node->exp.text)
				do_real_addr(&node->exp.text,
						is_obj_checked((void *)
							node->exp.text));
			break;
		}
		default:
			BUG();
		}
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	case MODE_GETSTEP4:
	{
		return;
	}
	default:
		BUG();
	}
}
#else
static void __maybe_unused do_cpp_macro(struct cpp_macro *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		CLIB_DBG_FUNC_ENTER();
		do_location(&node->line);
		if (node->kind == cmk_assert) {
			do_real_addr(&node->parm.next,
					do_cpp_macro(node->parm.next, 1));
		} else {
			cpp_hashnode **addr = node->parm.params;
			if (addr) {
				do_real_addr(&node->parm.params,
						is_obj_checked(addr));
				unsigned short i = 0;
				for (i = 0; i < node->paramc; i++) {
					do_real_addr(&addr[i],
						do_cpp_hashnode(addr[i], 1));
				}
			}
		}
		if (node->kind == cmk_traditional) {
			if (node->exp.text)
				do_real_addr(&node->exp.text,
						is_obj_checked((void *)
							node->exp.text));
		} else {
			if (node->exp.tokens) {
				do_real_addr(&node->exp.tokens,
					is_obj_checked(node->exp.tokens));
				struct cpp_token *addr = node->exp.tokens;
				for (unsigned int i = 0;i < node->count;i++)
					do_cpp_token(&addr[i], 0);
			}
		}
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	case MODE_GETSTEP4:
	{
		return;
	}
	default:
		BUG();
	}
}
#endif

struct GTY(()) answer {
	struct answer *next;
	unsigned int count;
	cpp_token GTY((length("%h.count"))) first[1];
};
static void do_answer(struct answer *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		CLIB_DBG_FUNC_ENTER();
		unsigned int len = node->count;
		do_real_addr(&node->next, do_answer(node->next, 1));
		for (unsigned int i = 0; i < len; i++) {
			do_cpp_token(&node->first[i], 0);
		}
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	case MODE_GETSTEP4:
	{
		return;
	}
	default:
		BUG();
	}
}

static void do_common(tree node, int flag);
static void do_c_common_identifier(struct c_common_identifier *node,
							int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		CLIB_DBG_FUNC_ENTER();
		do_common((tree)&node->common, 0);
		do_cpp_hashnode(&node->node, 0);
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	case MODE_GETSTEP4:
	{
		return;
	}
	default:
		BUG();
	}
}

static void do_real_value(struct real_value *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;
	return;
}

static void do_sorted_fields_type(struct sorted_fields_type *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;

	int cnt = node->len;
	BUG_ON(!cnt);
	switch (mode) {
	case MODE_ADJUST:
	{
		CLIB_DBG_FUNC_ENTER();
		for (int i = 0; i < cnt; i++) {
			do_real_addr(&node->elts[i], do_tree(node->elts[i]));
		}
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	case MODE_GETSTEP4:
	{
		return;
	}
	default:
		BUG();
	}
}

struct GTY(()) c_lang_type {
	struct sorted_fields_type * GTY ((reorder("resort_sorted_fields"))) s;
	tree enum_min;
	tree enum_max;
	tree objc_info;
};
static void do_c_lang_type(void *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		CLIB_DBG_FUNC_ENTER();
		struct c_lang_type *node0 = (struct c_lang_type *)node;
		do_real_addr(&node0->s, do_sorted_fields_type(node0->s, 1));
		do_real_addr(&node0->enum_min, do_tree(node0->enum_min));
		do_real_addr(&node0->enum_max, do_tree(node0->enum_max));
		do_real_addr(&node0->objc_info, do_tree(node0->objc_info));
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	case MODE_GETSTEP4:
	{
		return;
	}
	default:
		BUG();
	}
}

struct GTY(()) c_lang_decl {
	char dummy;
};
static void do_c_lang_decl(struct c_lang_decl *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;
	return;
}

static void do_symtab_node(void *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		CLIB_DBG_FUNC_ENTER();
		struct symtab_node *node0 = (struct symtab_node *)node;
		do_real_addr(&node0->decl, do_tree(node0->decl));
		do_real_addr(&node0->next, do_symtab_node(node0->next, 1));
		do_real_addr(&node0->previous,
				do_symtab_node(node0->previous, 1));
		do_real_addr(&node0->next_sharing_asm_name,
			     do_symtab_node(node0->next_sharing_asm_name, 1));
		do_real_addr(&node0->previous_sharing_asm_name,
			  do_symtab_node(node0->previous_sharing_asm_name, 1));
		do_real_addr(&node0->same_comdat_group,
				do_symtab_node(node0->same_comdat_group, 1));
		do_real_addr(&node0->alias_target,
				do_tree(node0->alias_target));
		do_real_addr(&node0->x_comdat_group,
				do_tree(node0->x_comdat_group));
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	case MODE_GETSTEP4:
	{
		return;
	}
	default:
		BUG();
	}
#if 0
	BUG_ON(node0->ref_list.references);
	vec<ipa_ref_ptr> tmp_referring = node0->ref_list.referring;
	unsigned long length = tmp_referring.length();
	ipa_ref_ptr *addr = tmp_referring.address();
	for (unsigned long i = 0; i < length; i++)
		BUG_ON(addr[i]);
	BUG_ON(node0->lto_file_data);
	BUG_ON(node0->aux);
	BUG_ON(node0->x_section);
#endif
}

#if 0
struct GTY(()) stmt_tree_s {
	vec<tree, va_gc> *x_cur_stmt_list;
	int stmts_are_full_exprs_p;
};
#endif
static void do_stmt_tree_s(struct stmt_tree_s *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		CLIB_DBG_FUNC_ENTER();
		do_real_addr(&node->x_cur_stmt_list,
				do_vec_tree((void *)node->x_cur_stmt_list, 1));
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	case MODE_GETSTEP4:
	{
		return;
	}
	default:
		BUG();
	}
}

#if 0
struct GTY(()) c_language_function {
	struct stmt_tree_s x_stmt_tree;
	vec<tree, va_gc> *local_typedefs;
};
#endif
static void do_c_language_function(struct c_language_function *node,
							int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		CLIB_DBG_FUNC_ENTER();
		do_stmt_tree_s(&node->x_stmt_tree, 0);
		do_real_addr(&node->local_typedefs,
				do_vec_tree((void *)node->local_typedefs, 1));
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	case MODE_GETSTEP4:
	{
		return;
	}
	default:
		BUG();
	}
}

#if 0
typedef uintptr_t splay_tree_key;
typedef uintptr_t splay_tree_value;
struct splay_tree_node_s {
	splay_tree_key key;
	splay_tree_value value;
	splay_tree_node left;
	splay_tree_node right;
};
typedef struct splay_tree_node_s *splay_tree_node;
#endif
static void do_splay_tree_node_s(struct splay_tree_node_s *node,
							int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		CLIB_DBG_FUNC_ENTER();
		do_real_addr(&node->left,
				do_splay_tree_node_s(node->left, 1));
		do_real_addr(&node->right,
				do_splay_tree_node_s(node->right, 1));
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	case MODE_GETSTEP4:
	{
		return;
	}
	default:
		BUG();
	}
}

#if 0
struct splay_tree_s {
	splay_tree_node root;
	splay_tree_compare_fn comp;
	splay_tree_delete_key_fn delete_key;
	splay_tree_delete_value_fn delete_value;
	splay_tree_allocate_fn allocate;
	splay_tree_deallocate_fn deallocate;
	void *allocate_data;
};
typedef struct splay_tree_s *splay_tree;
#endif
static void do_splay_tree_s(struct splay_tree_s *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;
	switch (mode) {
	case MODE_ADJUST:
	{
		CLIB_DBG_FUNC_ENTER();
		do_real_addr(&node->root, do_splay_tree_node_s(node->root, 1));
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	case MODE_GETSTEP4:
	{
		return;
	}
	default:
		BUG();
	}
	/* TODO, allocate_data */
}

struct c_switch {
	tree switch_expr;
	tree orig_type;
	splay_tree cases;
	struct c_spot_bindings *bindings;
	struct c_switch *next;
	bool bool_cond_p;
	bool outside_range_p;
};
static void do_c_switch(struct c_switch *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		CLIB_DBG_FUNC_ENTER();
		do_real_addr(&node->switch_expr, do_tree(node->switch_expr));
		do_real_addr(&node->orig_type, do_tree(node->orig_type));
		do_real_addr(&node->cases, do_splay_tree_s(node->cases, 1));
		do_real_addr(&node->bindings,
				do_c_spot_bindings(node->bindings, 1));
		do_real_addr(&node->next, do_c_switch(node->next, 1));
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	case MODE_GETSTEP4:
	{
		return;
	}
	default:
		BUG();
	}
}

#if 0
struct c_arg_tag { /* gcc/c/c-tree.h */
	tree id;
	tree type;
};
#endif
static void do_c_arg_tag(c_arg_tag *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		CLIB_DBG_FUNC_ENTER();
		do_real_addr(&node->id, do_tree(node->id));
		do_real_addr(&node->type, do_tree(node->type));
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	case MODE_GETSTEP4:
	{
		return;
	}
	default:
		BUG();
	}
}
static void do_vec_c_arg_tag(vec<c_arg_tag, va_gc> *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		CLIB_DBG_FUNC_ENTER();
		unsigned long len = node->vecpfx.m_num;
		BUG_ON(len==0);
		c_arg_tag *addr = node->vecdata;
		for (unsigned long i = 0; i < len; i++) {
			do_c_arg_tag(&addr[i], 0);
		}
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	case MODE_GETSTEP4:
	{
		return;
	}
	default:
		BUG();
	}
}

#if 0
struct c_arg_info { /* gcc/c/c-tree.h */
	tree parms;
	vec<c_arg_tag, va_gc> *tags;
	tree types;
	tree others;
	tree pending_sizes;
	BOOL_BITFIELD had_vla_unspec : 1;
};
#endif
static void do_c_arg_info(struct c_arg_info *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		CLIB_DBG_FUNC_ENTER();
		do_real_addr(&node->parms, do_tree(node->parms));
		do_real_addr(&node->tags, do_vec_c_arg_tag(node->tags, 1));
		do_real_addr(&node->types, do_tree(node->types));
		do_real_addr(&node->others, do_tree(node->others));
		do_real_addr(&node->pending_sizes,
				do_tree(node->pending_sizes));
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	case MODE_GETSTEP4:
	{
		return;
	}
	default:
		BUG();
	}
}

struct GTY(()) language_function {
	struct c_language_function base;
	tree x_break_label;
	tree x_cont_label;
	struct c_switch * GTY((skip)) x_switch_stack;
	struct c_arg_info * GTY((skip)) arg_info;
	int returns_value;
	int returns_null;
	int returns_abnormally;
	int warn_about_return_type;
};
static void do_language_function(struct language_function *node,
							int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		CLIB_DBG_FUNC_ENTER();
		do_c_language_function(&node->base, 0);
		do_real_addr(&node->x_break_label,
				do_tree(node->x_break_label));
		do_real_addr(&node->x_cont_label, do_tree(node->x_cont_label));
		do_real_addr(&node->x_switch_stack,
				do_c_switch(node->x_switch_stack, 1));
		do_real_addr(&node->arg_info,
				do_c_arg_info(node->arg_info, 1));
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	case MODE_GETSTEP4:
	{
		return;
	}
	default:
		BUG();
	}
}

static void do_basic_block(void *node, int flag);
static void do_gimple_seq(gimple_seq gs);
/*
 * following functions are for gsstruct
 * we do NOT check mode in these functions
 */
static __maybe_unused size_t gsstruct_size(gimple_seq gs)
{
	return gsstruct_code_size[gss_for_code(gimple_code(gs))];
}

static __maybe_unused size_t gimple_total_size(gimple_seq gs)
{
	size_t base_size = gsstruct_size(gs);

	/* there is an exception: gphi */
	if (gimple_code(gs) == GIMPLE_PHI) {
		struct gphi *node;
		node = (struct gphi *)gs;

		base_size -= sizeof(*node);
		base_size += sizeof(*node) * node->nargs;
		return base_size;
	}

	if (!gimple_has_ops(gs) || (!gimple_num_ops(gs)))
		return base_size;
	return base_size + (gimple_num_ops(gs)-1) * sizeof(tree);
}

static void do_gimple_base(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;

	CLIB_DBG_FUNC_ENTER();
	do_location(&gs->location);
	do_real_addr(&gs->bb, do_basic_block(gs->bb, 1));
	do_real_addr(&gs->next, do_gimple_seq(gs->next));
	do_real_addr(&gs->prev, do_gimple_seq(gs->prev));
	CLIB_DBG_FUNC_EXIT();

	return;
}

static void do_ssa_use_operand(void *node, int flag);
static void do_use_optype_d(struct use_optype_d *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;

	CLIB_DBG_FUNC_ENTER();
	do_real_addr(&node->next, do_use_optype_d(node->next, 1));
	do_ssa_use_operand(&node->use_ptr, 0);
	CLIB_DBG_FUNC_EXIT();
}

static void do_gs_with_ops_base(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;

	CLIB_DBG_FUNC_ENTER();
	struct gimple_statement_with_ops_base *node0;
	node0 = (struct gimple_statement_with_ops_base *)gs;

	do_gimple_base(gs, 0);
	do_real_addr(&node0->use_ops, do_use_optype_d(node0->use_ops, 1));
	CLIB_DBG_FUNC_EXIT();
}

static inline void do_gs_ops(gimple *gs)
{
	CLIB_DBG_FUNC_ENTER();
	tree *ops;
	ops = gimple_ops(gs);
	for (unsigned int i = 0; i < gimple_num_ops(gs); i++) {
		do_real_addr(&ops[i], do_tree(ops[i]));
	}
	CLIB_DBG_FUNC_EXIT();
}
static void do_gs_with_ops(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;

	CLIB_DBG_FUNC_ENTER();
	struct gimple_statement_with_ops __maybe_unused *node0;
	node0 = (struct gimple_statement_with_ops *)gs;

	do_gs_with_ops_base(gs, 0);
	do_gs_ops(gs);
	CLIB_DBG_FUNC_EXIT();
}

static void do_gs_with_mem_ops_base(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;

	CLIB_DBG_FUNC_ENTER();
	struct gimple_statement_with_memory_ops_base *node0;
	node0 = (struct gimple_statement_with_memory_ops_base *)gs;

	do_gs_with_ops_base(gs, 0);
	do_real_addr(&node0->vdef, do_tree(node0->vdef));
	do_real_addr(&node0->vuse, do_tree(node0->vuse));
	CLIB_DBG_FUNC_EXIT();
}

static void do_gs_with_mem_ops(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;

	CLIB_DBG_FUNC_ENTER();
	struct gimple_statement_with_memory_ops __maybe_unused *node0;
	node0 = (struct gimple_statement_with_memory_ops *)gs;

	do_gs_with_mem_ops_base(gs, 0);
	do_gs_ops(gs);
	CLIB_DBG_FUNC_EXIT();
}

static void do_gcall(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;

	CLIB_DBG_FUNC_ENTER();
	struct gcall __maybe_unused *node0;
	node0 = (struct gcall *)gs;

	do_gs_with_mem_ops_base(gs, 0);

	if (!(gs->subcode & GF_CALL_INTERNAL)) {
		do_real_addr(&node0->u.fntype, do_tree(node0->u.fntype));
	}
	do_gs_ops(gs);
	CLIB_DBG_FUNC_EXIT();
}

static void do_gs_omp(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;

	CLIB_DBG_FUNC_ENTER();
	struct gimple_statement_omp *node0;
	node0 = (struct gimple_statement_omp *)gs;

	do_gimple_base(gs, 0);
	do_real_addr(&node0->body, do_gimple_seq(node0->body));
	CLIB_DBG_FUNC_EXIT();
}

static void do_gbind(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;

	CLIB_DBG_FUNC_ENTER();
	struct gbind *node0;
	node0 = (struct gbind *)gs;

	do_gimple_base(gs, 0);

	do_real_addr(&node0->vars, do_tree(node0->vars));
	do_real_addr(&node0->block, do_tree(node0->block));
	do_real_addr(&node0->body, do_gimple_seq(node0->body));
	CLIB_DBG_FUNC_EXIT();
}

static void do_gcatch(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;

	CLIB_DBG_FUNC_ENTER();
	struct gcatch *node0;
	node0 = (struct gcatch *)gs;

	do_gimple_base(gs, 0);

	do_real_addr(&node0->types, do_tree(node0->types));
	do_real_addr(&node0->handler, do_gimple_seq(node0->handler));
	CLIB_DBG_FUNC_EXIT();
}

static void do_geh_filter(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;

	CLIB_DBG_FUNC_ENTER();
	struct geh_filter *node0;
	node0 = (struct geh_filter *)gs;

	do_gimple_base(gs, 0);

	do_real_addr(&node0->types, do_tree(node0->types));
	do_real_addr(&node0->failure, do_gimple_seq(node0->failure));
	CLIB_DBG_FUNC_EXIT();
}

static void do_geh_else(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;

	CLIB_DBG_FUNC_ENTER();
	struct geh_else *node0;
	node0 = (struct geh_else *)gs;

	do_gimple_base(gs, 0);

	do_real_addr(&node0->n_body, do_gimple_seq(node0->n_body));
	do_real_addr(&node0->e_body, do_gimple_seq(node0->e_body));
	CLIB_DBG_FUNC_EXIT();
}

static void do_geh_mnt(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;

	CLIB_DBG_FUNC_ENTER();
	struct geh_mnt *node0;
	node0 = (struct geh_mnt *)gs;

	do_gimple_base(gs, 0);

	do_real_addr(&node0->fndecl, do_tree(node0->fndecl));
	CLIB_DBG_FUNC_EXIT();
}

static void do_phi_arg_d(struct phi_arg_d *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;

	CLIB_DBG_FUNC_ENTER();
	/* ignore: do_location(&node->locus); */
	do_ssa_use_operand(&node->imm_use, 0);

	do_real_addr(&node->def, do_tree(node->def));
	CLIB_DBG_FUNC_EXIT();
}

static void do_gphi(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;

	CLIB_DBG_FUNC_ENTER();
	struct gphi *node0;
	node0 = (struct gphi *)gs;

	do_gimple_base(gs, 0);

	do_real_addr(&node0->result, do_tree(node0->result));

	for (unsigned i = 0; i < node0->nargs; i++) {
		do_phi_arg_d(&node0->args[i], 0);
	}
	CLIB_DBG_FUNC_EXIT();
}

static void do_gs_eh_ctrl(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;

	CLIB_DBG_FUNC_ENTER();
	struct gimple_statement_eh_ctrl __maybe_unused *node0;
	node0 = (struct gimple_statement_eh_ctrl *)gs;

	do_gimple_base(gs, 0);
	CLIB_DBG_FUNC_EXIT();
}

static void do_gresx(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;

	CLIB_DBG_FUNC_ENTER();
	struct gresx __maybe_unused *node0;
	node0 = (struct gresx *)gs;

	do_gs_eh_ctrl(gs, 0);
	CLIB_DBG_FUNC_EXIT();
}

static void do_geh_dispatch(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;

	CLIB_DBG_FUNC_ENTER();
	struct geh_dispatch __maybe_unused *node0;
	node0 = (struct geh_dispatch *)gs;

	do_gs_eh_ctrl(gs, 0);
	CLIB_DBG_FUNC_EXIT();
}

static void do_gtry(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;

	CLIB_DBG_FUNC_ENTER();
	struct gtry *node0;
	node0 = (struct gtry *)gs;

	do_gimple_base(gs, 0);

	do_real_addr(&node0->eval, do_gimple_seq(node0->eval));
	do_real_addr(&node0->cleanup, do_gimple_seq(node0->cleanup));
	CLIB_DBG_FUNC_EXIT();
}

static __maybe_unused void do_gs_wce(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;

	CLIB_DBG_FUNC_ENTER();
	struct gimple_statement_wce *node0;
	node0 = (struct gimple_statement_wce *)gs;

	do_gimple_base(gs, 0);

	do_real_addr(&node0->cleanup, do_gimple_seq(node0->cleanup));
	CLIB_DBG_FUNC_EXIT();
}

static void do_gasm(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;

	CLIB_DBG_FUNC_ENTER();
	struct gasm *node0;
	node0 = (struct gasm *)gs;
	do_gs_with_mem_ops_base(gs, 0);

	do_real_addr(&node0->string, is_obj_checked((void *)node0->string));

	do_gs_ops(gs);
	CLIB_DBG_FUNC_EXIT();
}

static void do_gomp_critical(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;

	CLIB_DBG_FUNC_ENTER();
	struct gomp_critical *node0;
	node0 = (struct gomp_critical *)gs;

	do_gs_omp(gs, 0);

	do_real_addr(&node0->clauses, do_tree(node0->clauses));
	do_real_addr(&node0->name, do_tree(node0->name));
	CLIB_DBG_FUNC_EXIT();
}

static void do_gomp_for_iter(struct gimple_omp_for_iter *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;

	CLIB_DBG_FUNC_ENTER();
	do_real_addr(&node->index, do_tree(node->index));
	do_real_addr(&node->initial, do_tree(node->initial));
	do_real_addr(&node->final, do_tree(node->final));
	do_real_addr(&node->incr, do_tree(node->incr));
	CLIB_DBG_FUNC_EXIT();
}

static void do_gomp_for(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;

	CLIB_DBG_FUNC_ENTER();
	struct gomp_for *node0;
	node0 = (struct gomp_for *)gs;

	do_gs_omp(gs, 0);

	do_real_addr(&node0->clauses, do_tree(node0->clauses));
	do_real_addr(&node0->iter, do_gomp_for_iter(node0->iter, 1));
	do_real_addr(&node0->pre_body, do_gimple_seq(node0->pre_body));
	CLIB_DBG_FUNC_EXIT();
}

static void do_gs_omp_parallel_layout(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;

	CLIB_DBG_FUNC_ENTER();
	struct gimple_statement_omp_parallel_layout *node0;
	node0 = (struct gimple_statement_omp_parallel_layout *)gs;

	do_gs_omp(gs, 0);

	do_real_addr(&node0->clauses, do_tree(node0->clauses));
	do_real_addr(&node0->child_fn, do_tree(node0->child_fn));
	do_real_addr(&node0->data_arg, do_tree(node0->data_arg));
	CLIB_DBG_FUNC_EXIT();
}

static void do_gs_omp_taskreg(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;

	CLIB_DBG_FUNC_ENTER();
	struct gimple_statement_omp_taskreg __maybe_unused *node0;
	node0 = (struct gimple_statement_omp_taskreg *)gs;

	do_gs_omp_parallel_layout(gs, 0);
	CLIB_DBG_FUNC_EXIT();
}

static void do_gomp_parallel(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;

	CLIB_DBG_FUNC_ENTER();
	struct gomp_parallel __maybe_unused *node0;
	node0 = (struct gomp_parallel *)gs;

	do_gs_omp_taskreg(gs, 0);
	CLIB_DBG_FUNC_EXIT();
}

static void do_gomp_target(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;

	CLIB_DBG_FUNC_ENTER();
	struct gomp_target __maybe_unused *node0;
	node0 = (struct gomp_target *)gs;

	do_gs_omp_parallel_layout(gs, 0);
	CLIB_DBG_FUNC_EXIT();
}

static void do_gomp_task(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;

	CLIB_DBG_FUNC_ENTER();
	struct gomp_task *node0;
	node0 = (struct gomp_task *)gs;

	do_gs_omp_taskreg(gs, 0);

	do_real_addr(&node0->copy_fn, do_tree(node0->copy_fn));
	do_real_addr(&node0->arg_size, do_tree(node0->arg_size));
	do_real_addr(&node0->arg_align, do_tree(node0->arg_align));
	CLIB_DBG_FUNC_EXIT();
}

static void do_gomp_sections(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;

	CLIB_DBG_FUNC_ENTER();
	struct gomp_sections *node0;
	node0 = (struct gomp_sections *)gs;

	do_gs_omp(gs, 0);

	do_real_addr(&node0->clauses, do_tree(node0->clauses));
	do_real_addr(&node0->control, do_tree(node0->control));
	CLIB_DBG_FUNC_EXIT();
}

static void do_gomp_continue(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;

	CLIB_DBG_FUNC_ENTER();
	struct gomp_continue *node0;
	node0 = (struct gomp_continue *)gs;

	do_gimple_base(gs, 0);

	do_real_addr(&node0->control_def, do_tree(node0->control_def));
	do_real_addr(&node0->control_use, do_tree(node0->control_use));
	CLIB_DBG_FUNC_EXIT();
}

static void do_gs_omp_single_layout(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;

	CLIB_DBG_FUNC_ENTER();
	struct gimple_statement_omp_single_layout *node0;
	node0 = (struct gimple_statement_omp_single_layout *)gs;

	do_gs_omp(gs, 0);

	do_real_addr(&node0->clauses, do_tree(node0->clauses));
	CLIB_DBG_FUNC_EXIT();
}

static void do_gomp_single(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;

	CLIB_DBG_FUNC_ENTER();
	struct gomp_single __maybe_unused *node0;
	node0 = (struct gomp_single *)gs;

	do_gs_omp_single_layout(gs, 0);
	CLIB_DBG_FUNC_EXIT();
}

static void do_gomp_teams(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;

	CLIB_DBG_FUNC_ENTER();
	struct gomp_teams __maybe_unused *node0;
	node0 = (struct gomp_teams *)gs;

	do_gs_omp_single_layout(gs, 0);
	CLIB_DBG_FUNC_EXIT();
}

static void do_gomp_ordered(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;

	CLIB_DBG_FUNC_ENTER();
	struct gomp_ordered __maybe_unused *node0;
	node0 = (struct gomp_ordered *)gs;

	do_gs_omp_single_layout(gs, 0);
	CLIB_DBG_FUNC_EXIT();
}

static void do_gomp_atomic_load(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;

	CLIB_DBG_FUNC_ENTER();
	struct gomp_atomic_load *node0;
	node0 = (struct gomp_atomic_load *)gs;

	do_gimple_base(gs, 0);

	do_real_addr(&node0->rhs, do_tree(node0->rhs));
	do_real_addr(&node0->lhs, do_tree(node0->lhs));
	CLIB_DBG_FUNC_EXIT();
}

static void do_gs_omp_atomic_store_layout(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;

	CLIB_DBG_FUNC_ENTER();
	struct gimple_statement_omp_atomic_store_layout *node0;
	node0 = (struct gimple_statement_omp_atomic_store_layout *)gs;

	do_gimple_base(gs, 0);

	do_real_addr(&node0->val, do_tree(node0->val));
	CLIB_DBG_FUNC_EXIT();
}

static void do_gomp_atomic_store(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;

	CLIB_DBG_FUNC_ENTER();
	struct gomp_atomic_store __maybe_unused *node0;
	node0 = (struct gomp_atomic_store *)gs;

	do_gs_omp_atomic_store_layout(gs, 0);
	CLIB_DBG_FUNC_EXIT();
}

static void do_gs_omp_return(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;

	CLIB_DBG_FUNC_ENTER();
	struct gimple_statement_omp_return __maybe_unused *node0;
	node0 = (struct gimple_statement_omp_return *)gs;

	do_gs_omp_atomic_store_layout(gs, 0);
	CLIB_DBG_FUNC_EXIT();
}

static void do_gtransaction(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;

	CLIB_DBG_FUNC_ENTER();
	struct gtransaction *node0;
	node0 = (struct gtransaction *)gs;

	do_gs_with_mem_ops_base(gs, 0);

	do_real_addr(&node0->body, do_gimple_seq(node0->body));
	do_real_addr(&node0->label_norm, do_tree(node0->label_norm));
	do_real_addr(&node0->label_uninst, do_tree(node0->label_uninst));
	do_real_addr(&node0->label_over, do_tree(node0->label_over));
	CLIB_DBG_FUNC_EXIT();
}

static void do_gcond(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;

	CLIB_DBG_FUNC_ENTER();
	struct gcond __maybe_unused *node0;
	node0 = (struct gcond *)gs;

	do_gs_with_ops(gs, 0);
	CLIB_DBG_FUNC_EXIT();
}

static void do_gdebug(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;

	CLIB_DBG_FUNC_ENTER();
	struct gdebug __maybe_unused *node0;
	node0 = (struct gdebug *)gs;

	do_gs_with_ops(gs, 0);
	CLIB_DBG_FUNC_EXIT();
}

static void do_ggoto(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;

	CLIB_DBG_FUNC_ENTER();
	struct ggoto __maybe_unused *node0;
	node0 = (struct ggoto *)gs;

	do_gs_with_ops(gs, 0);
	CLIB_DBG_FUNC_EXIT();
}

static void do_glabel(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;

	CLIB_DBG_FUNC_ENTER();
	struct glabel __maybe_unused *node0;
	node0 = (struct glabel *)gs;

	do_gs_with_ops(gs, 0);
	CLIB_DBG_FUNC_EXIT();
}

static void do_gswitch(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;

	CLIB_DBG_FUNC_ENTER();
	struct gswitch __maybe_unused *node0;
	node0 = (struct gswitch *)gs;

	do_gs_with_ops(gs, 0);
	CLIB_DBG_FUNC_EXIT();
}

static void do_gassign(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;

	CLIB_DBG_FUNC_ENTER();
	struct gassign __maybe_unused *node0;
	node0 = (struct gassign *)gs;

	do_gs_with_mem_ops(gs, 0);
	CLIB_DBG_FUNC_EXIT();
}

static void do_greturn(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;

	CLIB_DBG_FUNC_ENTER();
	struct greturn __maybe_unused *node0;
	node0 = (struct greturn *)gs;

	do_gs_with_mem_ops(gs, 0);
	CLIB_DBG_FUNC_EXIT();
}

static void do_gimple_seq(gimple_seq gs)
{
	if (!gs)
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		CLIB_DBG_FUNC_ENTER();

		BUG_ON(check_gimple_code(gs));

		switch (gimple_code(gs)) {
		case GIMPLE_ASM:
		{
			do_gasm(gs, 1);
			break;
		}
		case GIMPLE_ASSIGN:
		{
			do_gassign(gs, 1);
			break;
		}
		case GIMPLE_CALL:
		{
			do_gcall(gs, 1);
			break;
		}
		case GIMPLE_COND:
		{
			do_gcond(gs, 1);
			break;
		}
		case GIMPLE_LABEL:
		{
			do_glabel(gs, 1);
			break;
		}
		case GIMPLE_GOTO:
		{
			do_ggoto(gs, 1);
			break;
		}
		case GIMPLE_NOP:
		{
			do_gimple_base(gs, 1);
			break;
		}
		case GIMPLE_RETURN:
		{
			do_greturn(gs, 1);
			break;
		}
		case GIMPLE_SWITCH:
		{
			do_gswitch(gs, 1);
			break;
		}
		case GIMPLE_PHI:
		{
			do_gphi(gs, 1);
			break;
		}
		case GIMPLE_OMP_PARALLEL:
		{
			do_gomp_parallel(gs, 1);
			break;
		}
		case GIMPLE_OMP_TASK:
		{
			do_gomp_task(gs, 1);
			break;
		}
		case GIMPLE_OMP_ATOMIC_LOAD:
		{
			do_gomp_atomic_load(gs, 1);
			break;
		}
		case GIMPLE_OMP_ATOMIC_STORE:
		{
			do_gomp_atomic_store(gs, 1);
			break;
		}
		case GIMPLE_OMP_FOR:
		{
			do_gomp_for(gs, 1);
			break;
		}
		case GIMPLE_OMP_CONTINUE:
		{
			do_gomp_continue(gs, 1);
			break;
		}
		case GIMPLE_OMP_SINGLE:
		{
			do_gomp_single(gs, 1);
			break;
		}
		case GIMPLE_OMP_TARGET:
		{
			do_gomp_target(gs, 1);
			break;
		}
		case GIMPLE_OMP_TEAMS:
		{
			do_gomp_teams(gs, 1);
			break;
		}
		case GIMPLE_OMP_RETURN:
		{
			do_gs_omp_return(gs, 1);
			break;
		}
		case GIMPLE_OMP_SECTIONS:
		{
			do_gomp_sections(gs, 1);
			break;
		}
		case GIMPLE_OMP_SECTIONS_SWITCH:
		{
			/* FIXME: is do_gimple_base() right? */
			do_gimple_base(gs, 1);
			break;
		}
		case GIMPLE_OMP_MASTER:
		case GIMPLE_OMP_TASKGROUP:
		case GIMPLE_OMP_SECTION:
		case GIMPLE_OMP_GRID_BODY:
		{
			do_gs_omp(gs, 1);
			break;
		}
		case GIMPLE_OMP_ORDERED:
		{
			do_gomp_ordered(gs, 1);
			break;
		}
		case GIMPLE_OMP_CRITICAL:
		{
			do_gomp_critical(gs, 1);
			break;
		}
		case GIMPLE_BIND:
		{
			do_gbind(gs, 1);
			break;
		}
		case GIMPLE_TRY:
		{
			do_gtry(gs, 1);
			break;
		}
		case GIMPLE_CATCH:
		{
			do_gcatch(gs, 1);
			break;
		}
		case GIMPLE_EH_FILTER:
		{
			do_geh_filter(gs, 1);
			break;
		}
		case GIMPLE_EH_MUST_NOT_THROW:
		{
			do_geh_mnt(gs, 1);
			break;
		}
		case GIMPLE_EH_ELSE:
		{
			do_geh_else(gs, 1);
			break;
		}
		case GIMPLE_RESX:
		{
			do_gresx(gs, 1);
			break;
		}
		case GIMPLE_EH_DISPATCH:
		{
			do_geh_dispatch(gs, 1);
			break;
		}
		case GIMPLE_DEBUG:
		{
			do_gdebug(gs, 1);
			break;
		}
		case GIMPLE_PREDICT:
		{
			/* FIXME: is do_gimple_base() right? */
			do_gimple_base(gs, 1);
			break;
		}
		case GIMPLE_TRANSACTION:
		{
			do_gtransaction(gs, 1);
			break;
		}
		default:
		{
			BUG();
		}
		}

		CLIB_DBG_FUNC_EXIT();
		return;
	}
	case MODE_GETSTEP4:
	{
#if 0
		if (is_obj_checked((void *)gs))
			return;

		CLIB_DBG_FUNC_ENTER();
		/* FIXME: traverse the ops */
		tree *ops = gimple_ops(gs);
		for (unsigned int i = 0; i < gimple_num_ops(gs); i++) {
			do_tree(ops[i]);
		}
		do_basic_block(gs->bb, 1);
		do_gimple_seq(gs->next);
		do_gimple_seq(gs->prev);
		CLIB_DBG_FUNC_EXIT();
#endif
		return;
	}
	default:
		BUG();
	}
	return;
}

static void do_edge(void *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		CLIB_DBG_FUNC_ENTER();
		struct edge_def *node0;
		node0 = (struct edge_def *)node;
		do_location(&node0->goto_locus);
		do_real_addr(&node0->src, do_basic_block(node0->src, 1));
		do_real_addr(&node0->dest, do_basic_block(node0->dest, 1));
		do_real_addr(&node0->insns.g,
				do_gimple_seq(node0->insns.g));
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	case MODE_GETSTEP4:
	{
		CLIB_DBG_FUNC_ENTER();
		struct edge_def *node0;
		node0 = (struct edge_def *)node;
		do_basic_block(node0->src, 1);
		do_basic_block(node0->dest, 1);
		do_gimple_seq(node0->insns.g);
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	default:
		BUG();
	}
}

static void do_vec_edge(void *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		CLIB_DBG_FUNC_ENTER();
		vec<edge, va_gc> *node0;
		node0 = (vec<edge, va_gc> *)node;

		struct vec_prefix *pfx;
		pfx = &node0->vecpfx;
		unsigned long length = pfx->m_num;
		edge *addr = &node0->vecdata[0];
		for (unsigned long i = 0; i < length; i++) {
			do_real_addr(&addr[i], do_edge(addr[i], 1));
		}
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	case MODE_GETSTEP4:
	{
		CLIB_DBG_FUNC_ENTER();
		vec<edge, va_gc> *node0;
		node0 = (vec<edge, va_gc> *)node;

		struct vec_prefix *pfx;
		pfx = &node0->vecpfx;
		unsigned long length = pfx->m_num;
		edge *addr = &node0->vecdata[0];
		for (unsigned long i = 0; i < length; i++) {
			do_edge(addr[i], 1);
		}
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	default:
		BUG();
	}
}

static void do_nb_iter_bound(void *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		CLIB_DBG_FUNC_ENTER();
		struct nb_iter_bound *node0;
		node0 = (struct nb_iter_bound *)node;
		do_real_addr(&node0->stmt, do_gimple_seq(node0->stmt));
		do_real_addr(&node0->next, do_nb_iter_bound(node0->next, 1));
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	case MODE_GETSTEP4:
	{
		CLIB_DBG_FUNC_ENTER();
		struct nb_iter_bound *node0;
		node0 = (struct nb_iter_bound *)node;
		do_gimple_seq(node0->stmt);
		do_nb_iter_bound(node0->next, 1);
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	default:
		BUG();
	}
}

static void do_control_iv(void *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		CLIB_DBG_FUNC_ENTER();
		struct control_iv *node0;
		node0 = (struct control_iv *)node;
		do_real_addr(&node0->base, do_tree(node0->base));
		do_real_addr(&node0->step, do_tree(node0->step));
		do_real_addr(&node0->next,
				do_control_iv(node0->next, 1));
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	case MODE_GETSTEP4:
	{
		CLIB_DBG_FUNC_ENTER();
		struct control_iv *node0;
		node0 = (struct control_iv *)node;
		do_tree(node0->base);
		do_tree(node0->step);
		do_control_iv(node0->next, 1);
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	default:
		BUG();
	}
}

static void do_loop_exit(void *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		CLIB_DBG_FUNC_ENTER();
		struct loop_exit *node0;
		node0 = (struct loop_exit *)node;
		do_real_addr(&node0->e, do_edge(node0->e, 1));
		do_real_addr(&node0->prev, do_loop_exit(node0->prev, 1));
		do_real_addr(&node0->next, do_loop_exit(node0->next, 1));
		do_real_addr(&node0->next_e, do_loop_exit(node0->next_e, 1));
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	case MODE_GETSTEP4:
	{
		CLIB_DBG_FUNC_ENTER();
		struct loop_exit *node0;
		node0 = (struct loop_exit *)node;
		do_edge(node0->e, 1);
		do_loop_exit(node0->prev, 1);
		do_loop_exit(node0->next, 1);
		do_loop_exit(node0->next_e, 1);
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	default:
		BUG();
	}
}

static void do_niter_desc(void *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		CLIB_DBG_FUNC_ENTER();
		struct niter_desc *node0;
		node0 = (struct niter_desc *)node;
		do_real_addr(&node0->out_edge, do_edge(node0->out_edge, 1));
		do_real_addr(&node0->in_edge, do_edge(node0->in_edge, 1));
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	case MODE_GETSTEP4:
	{
		CLIB_DBG_FUNC_ENTER();
		struct niter_desc *node0;
		node0 = (struct niter_desc *)node;
		do_edge(node0->out_edge, 1);
		do_edge(node0->in_edge, 1);
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	default:
		BUG();
	}
}

static void do_vec_loop(void *node, int flag);
static void do_loop(void *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		CLIB_DBG_FUNC_ENTER();
		struct loop *node0;
		node0 = (struct loop *)node;
		do_real_addr(&node0->header,
				do_basic_block(node0->header, 1));
		do_real_addr(&node0->latch,
				do_basic_block(node0->latch, 1));
		do_real_addr(&node0->superloops,
				do_vec_loop(node0->superloops, 1));
		do_real_addr(&node0->inner,
				do_loop(node0->inner, 1));
		do_real_addr(&node0->next,
				do_loop(node0->next, 1));
		do_real_addr(&node0->nb_iterations,
				do_tree(node0->nb_iterations));
		do_real_addr(&node0->simduid,
				do_tree(node0->simduid));
		do_real_addr(&node0->bounds,
				do_nb_iter_bound(node0->bounds, 1));
		do_real_addr(&node0->control_ivs,
				do_control_iv(node0->control_ivs, 1));
		do_real_addr(&node0->exits,
				do_loop_exit(node0->exits, 1));
		do_real_addr(&node0->simple_loop_desc,
				do_niter_desc(node0->simple_loop_desc, 1));
		do_real_addr(&node0->former_header,
				do_basic_block(node0->former_header, 1));
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	case MODE_GETSTEP4:
	{
		CLIB_DBG_FUNC_ENTER();
		struct loop *node0;
		node0 = (struct loop *)node;
		do_basic_block(node0->header, 1);
		do_basic_block(node0->latch, 1);
		do_vec_loop(node0->superloops, 1);
		do_loop(node0->inner, 1);
		do_loop(node0->next, 1);
		do_tree(node0->nb_iterations);
		do_tree(node0->simduid);
		do_nb_iter_bound(node0->bounds, 1);
		do_control_iv(node0->control_ivs, 1);
		do_loop_exit(node0->exits, 1);
		do_niter_desc(node0->simple_loop_desc, 1);
		do_basic_block(node0->former_header, 1);
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	default:
		BUG();
	}
}

static void do_vec_loop(void *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		CLIB_DBG_FUNC_ENTER();
		vec<loop_p,va_gc> *node0;
		node0 = (vec<loop_p,va_gc> *)node;

		struct vec_prefix *pfx;
		pfx = &node0->vecpfx;
		unsigned long length = pfx->m_num;
		loop_p *addr;
		addr = &node0->vecdata[0];
		for (unsigned long i = 0; i < length; i++) {
			do_real_addr(&addr[i],
					do_loop(addr[i], 1));
		}
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	case MODE_GETSTEP4:
	{
		CLIB_DBG_FUNC_ENTER();
		vec<loop_p,va_gc> *node0;
		node0 = (vec<loop_p,va_gc> *)node;

		struct vec_prefix *pfx;
		pfx = &node0->vecpfx;
		unsigned long length = pfx->m_num;
		loop_p *addr;
		addr = &node0->vecdata[0];
		for (unsigned long i = 0; i < length; i++) {
			do_loop(addr[i], 1);
		}
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	default:
		BUG();
	}
}

static void do_basic_block(void *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		CLIB_DBG_FUNC_ENTER();
		struct basic_block_def *node0;
		node0 = (struct basic_block_def *)node;

		do_real_addr(&node0->preds,
				do_vec_edge(node0->preds, 1));
		do_real_addr(&node0->succs,
				do_vec_edge(node0->succs, 1));
		do_real_addr(&node0->loop_father,
				do_loop(node0->loop_father, 1));
		do_real_addr(&node0->prev_bb,
				do_basic_block(node0->prev_bb, 1));
		do_real_addr(&node0->next_bb,
				do_basic_block(node0->next_bb, 1));
		do_real_addr(&node0->il.gimple.seq,
				do_gimple_seq(node0->il.gimple.seq));
		do_real_addr(&node0->il.gimple.phi_nodes,
				do_gimple_seq(node0->il.gimple.phi_nodes));
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	case MODE_GETSTEP4:
	{
		CLIB_DBG_FUNC_ENTER();
		struct basic_block_def *node0;
		node0 = (struct basic_block_def *)node;

		do_vec_edge(node0->preds, 1);
		do_vec_edge(node0->succs, 1);
		do_loop(node0->loop_father, 1);
		do_basic_block(node0->prev_bb, 1);
		do_basic_block(node0->next_bb, 1);
		do_gimple_seq(node0->il.gimple.seq);
		do_gimple_seq(node0->il.gimple.phi_nodes);
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	default:
		BUG();
	}
}

static void do_vec_basic_block(void *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		CLIB_DBG_FUNC_ENTER();
		vec<basic_block, va_gc> *node0;
		node0 = (vec<basic_block, va_gc> *)node;

		struct vec_prefix *pfx;
		pfx = &node0->vecpfx;
		unsigned long length;
		length = pfx->m_num;

		basic_block *addr;
		addr = &node0->vecdata[0];
		for (unsigned long i = 0; i < length; i++) {
			do_real_addr(&addr[i],
					do_basic_block(addr[i], 1));
		}
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	case MODE_GETSTEP4:
	{
		CLIB_DBG_FUNC_ENTER();
		vec<basic_block, va_gc> *node0;
		node0 = (vec<basic_block, va_gc> *)node;

		struct vec_prefix *pfx;
		pfx = &node0->vecpfx;
		unsigned long length;
		length = pfx->m_num;

		basic_block *addr;
		addr = &node0->vecdata[0];
		for (unsigned long i = 0; i < length; i++) {
			do_basic_block(addr[i], 1);
		}
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	default:
		BUG();
	}
}

static void do_cfg(void *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		CLIB_DBG_FUNC_ENTER();
		struct control_flow_graph *node0;
		node0 = (struct control_flow_graph *)node;

		do_real_addr(&node0->x_entry_block_ptr,
				do_basic_block(node0->x_entry_block_ptr, 1));
		do_real_addr(&node0->x_exit_block_ptr,
				do_basic_block(node0->x_exit_block_ptr, 1));
		do_real_addr(&node0->x_basic_block_info,
			     do_vec_basic_block(node0->x_basic_block_info, 1));
		do_real_addr(&node0->x_label_to_block_map,
			   do_vec_basic_block(node0->x_label_to_block_map, 1));
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	case MODE_GETSTEP4:
	{
		CLIB_DBG_FUNC_ENTER();
		struct control_flow_graph *node0;
		node0 = (struct control_flow_graph *)node;

		do_basic_block(node0->x_entry_block_ptr, 1);
		do_basic_block(node0->x_exit_block_ptr, 1);
		do_vec_basic_block(node0->x_basic_block_info, 1);
		do_vec_basic_block(node0->x_label_to_block_map, 1);
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	default:
		BUG();
	}
}

static void do_ssa_operands(void *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		CLIB_DBG_FUNC_ENTER();
		struct ssa_operands __maybe_unused *node0;
		node0 = (struct ssa_operands *)node;
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	case MODE_GETSTEP4:
	{
		return;
	}
	default:
		BUG();
	}
}

static void do_gimple_df(void *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		CLIB_DBG_FUNC_ENTER();
		struct gimple_df *node0;
		node0 = (struct gimple_df *)node;

		do_real_addr(&node0->ssa_names,
				do_vec_tree(node0->ssa_names, 1));
		do_real_addr(&node0->vop,
				do_tree(node0->vop));
		/* TODO: do_hash_map(node0->decls_to_pointers) */

#ifdef GCC_CONTAIN_FREE_SSANAMES
		do_real_addr(&node0->free_ssanames,
				do_vec_tree(node0->free_ssanames, 1));
		do_real_addr(&node0->free_ssanames_queue,
				do_vec_tree(node0->free_ssanames_queue, 1));
#endif

		/* TODO: do_hash_table(node0->default_defs) */
		do_ssa_operands(&node0->ssa_operands, 0);
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	case MODE_GETSTEP4:
	{
		CLIB_DBG_FUNC_ENTER();
		struct gimple_df *node0;
		node0 = (struct gimple_df *)node;

		do_vec_tree(node0->ssa_names, 1);
		do_tree(node0->vop);
		/* TODO: do_hash_map(node0->decls_to_pointers) */

#ifdef GCC_CONTAIN_FREE_SSANAMES
		do_vec_tree(node0->free_ssanames, 1);
		do_vec_tree(node0->free_ssanames_queue, 1);
#endif

		/* TODO: do_hash_table(node0->default_defs) */
		do_ssa_operands(&node0->ssa_operands, 0);
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	default:
		BUG();
	}
}

static void do_loops(void *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		CLIB_DBG_FUNC_ENTER();
		struct loops *node0;
		node0 = (struct loops *)node;

		do_real_addr(&node0->larray,
				do_vec_loop(node0->larray, 1));
		/* TODO: do_hash_table(node0->exits) */
		do_real_addr(&node0->tree_root,
				do_loop(node0->tree_root, 1));
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	case MODE_GETSTEP4:
	{
		CLIB_DBG_FUNC_ENTER();
		struct loops *node0;
		node0 = (struct loops *)node;

		do_vec_loop(node0->larray, 1);
		/* TODO: do_hash_table(node0->exits) */
		do_loop(node0->tree_root, 1);
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	default:
		BUG();
	}
}

static void do_function(struct function *node, int flag);
static void do_histogram_value(void *node)
{
	if (!node)
		return;
	if (is_obj_checked(node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		CLIB_DBG_FUNC_ENTER();
		struct histogram_value_t *node0;
		node0 = (struct histogram_value_t *)node;

		do_real_addr(&node0->hvalue.counters,
			     is_obj_checked((void *)node0->hvalue.counters));

		do_real_addr(&node0->fun, do_function(node0->fun, 1));
		do_real_addr(&node0->hvalue.value,
				do_tree(node0->hvalue.value));
		do_real_addr(&node0->hvalue.stmt,
				do_gimple_seq(node0->hvalue.stmt));
		do_real_addr(&node0->hvalue.next,
				do_histogram_value(node0->hvalue.next));
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	case MODE_GETSTEP4:
	{
		CLIB_DBG_FUNC_ENTER();
		struct histogram_value_t *node0;
		node0 = (struct histogram_value_t *)node;

		do_function(node0->fun, 1);
		do_tree(node0->hvalue.value);
		do_gimple_seq(node0->hvalue.stmt);
		do_histogram_value(node0->hvalue.next);
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	default:
	{
		BUG();
	}
	}
}

static void do_histogram_values(void *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		CLIB_DBG_FUNC_ENTER();
		struct htab *node0;
		node0 = (struct htab *)node;
		do_real_addr(&node0->entries,
				is_obj_checked((void *)node0->entries));
		for (size_t i = 0; i < node0->size; i++) {
			if ((node0->entries[i] == HTAB_EMPTY_ENTRY) ||
				(node0->entries[i] == HTAB_DELETED_ENTRY))
				continue;
			do_real_addr(&node0->entries[i],
				     do_histogram_value(node0->entries[i]));
		}
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	case MODE_GETSTEP4:
	{
		CLIB_DBG_FUNC_ENTER();
		struct htab *node0;
		node0 = (struct htab *)node;
		for (size_t i = 0; i < node0->size; i++) {
			if ((node0->entries[i] == HTAB_EMPTY_ENTRY) ||
				(node0->entries[i] == HTAB_DELETED_ENTRY))
				continue;
			do_histogram_value(node0->entries[i]);
		}
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	default:
	{
		BUG();
	}
	}
}

static void do_function(struct function *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		CLIB_DBG_FUNC_ENTER();
		do_location(&node->function_start_locus);
		do_location(&node->function_end_locus);
		do_real_addr(&node->language,
				do_language_function(node->language, 1));
		do_real_addr(&node->decl, do_tree(node->decl));
		do_real_addr(&node->static_chain_decl,
				do_tree(node->static_chain_decl));
		do_real_addr(&node->nonlocal_goto_save_area,
				do_tree(node->nonlocal_goto_save_area));
		do_real_addr(&node->local_decls,
				do_vec_tree(node->local_decls, 1));
#if __GNUC__ < 8
		do_real_addr(&node->cilk_frame_decl,
				do_tree(node->cilk_frame_decl));
#endif
		do_real_addr(&node->gimple_body,
				do_gimple_seq(node->gimple_body));
		do_real_addr(&node->cfg,
				do_cfg(node->cfg, 1));
		do_real_addr(&node->gimple_df,
				do_gimple_df(node->gimple_df, 1));
		do_real_addr(&node->x_current_loops,
				do_loops(node->x_current_loops, 1));
		do_real_addr(&node->value_histograms,
			     do_histogram_values(node->value_histograms, 1));
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	case MODE_GETSTEP4:
	{
		CLIB_DBG_FUNC_ENTER();
		do_language_function(node->language, 1);
		do_tree(node->decl);
		do_tree(node->static_chain_decl);
		do_tree(node->nonlocal_goto_save_area);
		do_vec_tree(node->local_decls, 1);
#if __GNUC__ < 8
		do_tree(node->cilk_frame_decl);
#endif
		do_gimple_seq(node->gimple_body);
		do_cfg(node->cfg, 1);
		do_gimple_df(node->gimple_df, 1);
		do_loops(node->x_current_loops, 1);
		do_histogram_values(node->value_histograms, 1);
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	default:
		BUG();
	}
#if 0
	/* node0->eh, except.h, ignore it */
	/* node0->su */
	/* node0->used_types_hash */
	/* node0->fde */
	/* node0->cannot_be_copied_reason */
	/* node0->machine, config/i386/i386.h, ignore */
#endif
}

static void do_statement_list_node(void *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		CLIB_DBG_FUNC_ENTER();
		struct tree_statement_list_node *node0 =
				(struct tree_statement_list_node *)node;
		do_real_addr(&node0->prev,
			     do_statement_list_node((void *)node0->prev, 1));
		do_real_addr(&node0->next,
			     do_statement_list_node((void *)node0->next, 1));
		do_real_addr(&node0->stmt, do_tree(node0->stmt));
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	case MODE_GETSTEP4:
	{
		return;
	}
	default:
		BUG();
	}
}

/* like tree_code_size() in gcc/tree.c */
static void do_tree(tree node)
{
	if (!node)
		return;

	CLIB_DBG_FUNC_ENTER();

	BUG_ON(check_tree_code(node));

	enum tree_code code = TREE_CODE(node);
	enum tree_code_class tc = TREE_CODE_CLASS(code);
	switch (code) {
	case INTEGER_CST:
		do_int_cst(node, 1);
		goto out;
	case TREE_BINFO:
		do_binfo(node, 1);
		goto out;
	case TREE_VEC:
		do_vec(node, 1);
		goto out;
	case VECTOR_CST:
		do_vector(node, 1);
		goto out;
	case STRING_CST:
		do_string(node, 1);
		goto out;
	case OMP_CLAUSE:
		do_omp_clause(node, 1);
		goto out;
	default:
		if (tc == tcc_vl_exp) {
			do_exp(node, 1);
			goto out;
		}
	}

	switch (tc) {
	case tcc_declaration:
	{
		switch (code) {
		case FIELD_DECL:
			do_field_decl(node, 1);
			goto out;
		case PARM_DECL:
			do_parm_decl(node, 1);
			goto out;
		case VAR_DECL:
			do_var_decl(node, 1);
			goto out;
		case LABEL_DECL:
			do_label_decl(node, 1);
			goto out;
		case RESULT_DECL:
			do_result_decl(node, 1);
			goto out;
		case CONST_DECL:
			do_const_decl(node, 1);
			goto out;
		case TYPE_DECL:
			do_type_decl(node, 1);
			goto out;
		case FUNCTION_DECL:
			do_function_decl(node, 1);
			goto out;
		case DEBUG_EXPR_DECL:
			do_decl_with_rtl(node, 1);
			goto out;
		case TRANSLATION_UNIT_DECL:
			do_translation_unit_decl(node, 1);
			goto out;
		case NAMESPACE_DECL:
		case IMPORTED_DECL:
		case NAMELIST_DECL:
			do_decl_non_common(node, 1);
			goto out;
		default:
			BUG();
		}
	}
	case tcc_type:
		do_type_non_common(node, 1);
		goto out;
	case tcc_reference:
	case tcc_expression:
	case tcc_statement:
	case tcc_comparison:
	case tcc_unary:
	case tcc_binary:
		do_exp(node, 1);
		goto out;
	case tcc_constant:
	{
		switch (code) {
		case VOID_CST:
			do_typed(node, 1);
			goto out;
		case INTEGER_CST:
			BUG();
		case REAL_CST:
			do_real_cst(node, 1);
			goto out;
		case FIXED_CST:
			do_fixed_cst(node, 1);
			goto out;
		case COMPLEX_CST:
			do_complex(node, 1);
			goto out;
		case VECTOR_CST:
			do_vector(node, 1);
			goto out;
		case STRING_CST:
			BUG();
		default:
			BUG();
		}
	}
	case tcc_exceptional:
	{
		switch (code) {
		case IDENTIFIER_NODE:
			do_c_lang_identifier(node, 1);
			goto out;
#if 0
			do_identifier(node, 1);
			goto out;
#endif
		case TREE_LIST:
			do_list(node, 1);
			goto out;
		case ERROR_MARK:
		case PLACEHOLDER_EXPR:
			do_common(node, 1);
			goto out;
		case TREE_VEC:
		case OMP_CLAUSE:
			BUG();
		case SSA_NAME:
			do_ssa_name(node, 1);
			goto out;
		case STATEMENT_LIST:
			do_statement_list(node, 1);
			goto out;
		case BLOCK:
			do_block(node, 1);
			goto out;
		case CONSTRUCTOR:
			do_constructor(node, 1);
			goto out;
		case OPTIMIZATION_NODE:
			do_optimization_option(node, 1);
			goto out;
		case TARGET_OPTION_NODE:
			do_target_option(node, 1);
			goto out;
		default:
			BUG();
		}
	}
	default:
		BUG();
	}

out:
	CLIB_DBG_FUNC_EXIT();
	return;
}

static void do_base(tree node, int flag)
{
	if (!node)
		return;
#if 0	/* flag will never be 1 */
	if (flag && is_obj_checked((void *)node))
		return;
#endif

	switch (mode) {
	case MODE_ADJUST:
	{
		return;
	}
	case MODE_GETSTEP4:
	{
		return;
	}
	default:
		BUG();
	}
}

static void do_typed(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		CLIB_DBG_FUNC_ENTER();
		struct tree_typed *node0 = (struct tree_typed *)node;
		do_base((tree)&node0->base, 0);
		do_real_addr(&node0->type, do_tree(node0->type));
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	case MODE_GETSTEP4:
	{
		return;
	}
	default:
		BUG();
	}
}

static void do_string(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		CLIB_DBG_FUNC_ENTER();
		struct tree_string *node0 = (struct tree_string *)node;
		do_typed((tree)&node0->typed, 0);
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	case MODE_GETSTEP4:
	{
		return;
	}
	default:
		BUG();
	}
}

static void do_real_cst(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		CLIB_DBG_FUNC_ENTER();
		struct tree_real_cst *node0 = (struct tree_real_cst *)node;
		do_typed((tree)&node0->typed, 0);
		do_real_addr(&node0->real_cst_ptr,
				do_real_value(node0->real_cst_ptr, 1));
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	case MODE_GETSTEP4:
	{
		return;
	}
	default:
		BUG();
	}
}

static void do_int_cst(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		CLIB_DBG_FUNC_ENTER();
		struct tree_int_cst *node0 = (struct tree_int_cst *)node;
		do_typed((tree)&node0->typed, 0);
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	case MODE_GETSTEP4:
	{
		return;
	}
	default:
		BUG();
	}
}

static void do_common(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		CLIB_DBG_FUNC_ENTER();
		struct tree_common *node0 = (struct tree_common *)node;
		do_typed((tree)&node0->typed, 0);
		do_real_addr(&node0->chain, do_tree(node0->chain));
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	case MODE_GETSTEP4:
	{
		return;
	}
	default:
		BUG();
	}
}

static void __maybe_unused do_identifier(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		CLIB_DBG_FUNC_ENTER();
		struct tree_identifier *node0 = (struct tree_identifier *)node;
		do_common((tree)&node0->common, 0);
		do_ht_identifier(&node0->id, 0);
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	case MODE_GETSTEP4:
	{
		return;
	}
	default:
		BUG();
	}
}
struct GTY(()) c_lang_identifier { /* gcc/c/c-decl.c */
	struct c_common_identifier common_id;
	struct c_binding *symbol_binding;
	struct c_binding *tag_binding;
	struct c_binding *label_binding;
};
static void do_c_lang_identifier(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		CLIB_DBG_FUNC_ENTER();
		struct c_lang_identifier *node0;
		node0 = (struct c_lang_identifier *)node;
		do_c_common_identifier(&node0->common_id, 0);
#ifdef COLLECT_MORE
		do_real_addr(&node0->symbol_binding,
				do_c_binding(node0->symbol_binding, 1));
		do_real_addr(&node0->tag_binding,
				do_c_binding(node0->tag_binding, 1));
		do_real_addr(&node0->label_binding,
				do_c_binding(node0->label_binding, 1));
#endif
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	case MODE_GETSTEP4:
	{
		return;
	}
	default:
		BUG();
	}
}

static void do_constructor(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		CLIB_DBG_FUNC_ENTER();
		struct tree_constructor *node0;
		node0 = (struct tree_constructor *)node;
		do_typed((tree)&node0->typed, 0);
		do_real_addr(&node0->elts,
				do_vec_constructor((void *)node0->elts, 1));
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	case MODE_GETSTEP4:
	{
		CLIB_DBG_FUNC_ENTER();
		struct tree_constructor *node0;
		node0 = (struct tree_constructor *)node;
		do_typed((tree)&node0->typed, 0);
		do_vec_constructor((void *)node0->elts, 1);
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	default:
		BUG();
	}
}

static void do_statement_list(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		CLIB_DBG_FUNC_ENTER();
		struct tree_statement_list *node0 =
			(struct tree_statement_list *)node;
		do_typed((tree)&node0->typed, 0);

		do_real_addr(&node0->head,
			     do_statement_list_node((void *)node0->head, 1));
		do_real_addr(&node0->tail,
			     do_statement_list_node((void *)node0->tail, 1));
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	case MODE_GETSTEP4:
	{
		return;
	}
	default:
		BUG();
	}
}

static void do_block(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		CLIB_DBG_FUNC_ENTER();
		struct tree_block *node0 = (struct tree_block *)node;
		do_location(&node0->locus);
		do_location(&node0->end_locus);
		do_base((tree)&node0->base, 0);

		do_real_addr(&node0->chain, do_tree(node0->chain));
		do_real_addr(&node0->vars, do_tree(node0->vars));
		do_real_addr(&node0->nonlocalized_vars,
				do_vec_tree(node0->nonlocalized_vars, 1));
		do_real_addr(&node0->subblocks, do_tree(node0->subblocks));
		do_real_addr(&node0->supercontext,
				do_tree(node0->supercontext));
		do_real_addr(&node0->abstract_origin,
				do_tree(node0->abstract_origin));
		do_real_addr(&node0->fragment_origin,
				do_tree(node0->fragment_origin));
		do_real_addr(&node0->fragment_chain,
				do_tree(node0->fragment_chain));
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	case MODE_GETSTEP4:
	{
		return;
	}
	default:
		BUG();
	}
}

static void __4_mark_component_ref(tree op);
static void __4_mark_bit_field_ref(tree op);
static void __4_mark_addr_expr(tree op);
static void __4_mark_mem_ref(tree op);
static void __4_mark_array_ref(tree op);
static void do_exp(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		CLIB_DBG_FUNC_ENTER();
		struct tree_exp *node0 = (struct tree_exp *)node;
		do_location(&node0->locus);
		do_typed((tree)&node0->typed, 0);

		int i = 0;
		if (TREE_CODE_CLASS(TREE_CODE(node)) == tcc_vl_exp) {
			do_real_addr(&node0->operands[0],
					do_tree(node0->operands[0]));
			BUG_ON(!VL_EXP_OPERAND_LENGTH(node));
			for (i = 1; i < VL_EXP_OPERAND_LENGTH(node); i++) {
				do_real_addr(&node0->operands[i],
						do_tree(node0->operands[i]));
			}
		} else {
			for (i = 0;i<TREE_CODE_LENGTH(TREE_CODE(node));i++) {
				do_real_addr(&node0->operands[i],
						do_tree(node0->operands[i]));
			}
		}
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	case MODE_GETSTEP4:
	{
		CLIB_DBG_FUNC_ENTER();
		struct tree_exp *node0 = (struct tree_exp *)node;
		do_typed((tree)&node0->typed, 0);

		int i = 0;
		if (TREE_CODE_CLASS(TREE_CODE(node)) == tcc_vl_exp) {
			do_tree(node0->operands[0]);
			BUG_ON(!VL_EXP_OPERAND_LENGTH(node));
			for (i = 1; i < VL_EXP_OPERAND_LENGTH(node); i++) {
				do_tree(node0->operands[i]);
			}
		} else {
			for (i = 0;i<TREE_CODE_LENGTH(TREE_CODE(node));i++) {
				do_tree(node0->operands[i]);
			}
		}

		switch (TREE_CODE(node)) {
		case COMPONENT_REF:
		{
			__4_mark_component_ref(node);
			break;
		}
		case BIT_FIELD_REF:
		{
			__4_mark_bit_field_ref(node);
			break;
		}
		case ADDR_EXPR:
		{
			__4_mark_addr_expr(node);
			break;
		}
		case MEM_REF:
		{
			__4_mark_mem_ref(node);
			break;
		}
		case ARRAY_REF:
		{
			__4_mark_array_ref(node);
			break;
		}
		default:
			break;
		}

		CLIB_DBG_FUNC_EXIT();
		return;
	}
	default:
		BUG();
	}
}

static void do_list(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		CLIB_DBG_FUNC_ENTER();
		struct tree_list *node0 = (struct tree_list *)node;
		do_common((tree)&node0->common, 0);

		do_real_addr(&node0->purpose, do_tree(node0->purpose));
		do_real_addr(&node0->value, do_tree(node0->value));
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	case MODE_GETSTEP4:
	{
		CLIB_DBG_FUNC_ENTER();
		struct tree_list *node0 = (struct tree_list *)node;
		do_common((tree)&node0->common, 0);

		do_tree(node0->purpose);
		do_tree(node0->value);
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	default:
		BUG();
	}
}

static void do_type_with_lang_specific(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		CLIB_DBG_FUNC_ENTER();
		struct tree_type_with_lang_specific *node0 =
				(struct tree_type_with_lang_specific *)node;
		do_type_common((tree)&node0->common, 0);
		do_real_addr(&node0->lang_specific,
			     do_c_lang_type((void *)node0->lang_specific, 1));
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	case MODE_GETSTEP4:
	{
		return;
	}
	default:
		BUG();
	}
}
static void do_type_common(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		CLIB_DBG_FUNC_ENTER();
		struct tree_type_common *node0;
		node0 = (struct tree_type_common *)node;
		do_common((tree)&node0->common, 0);

		do_real_addr(&node0->size, do_tree(node0->size));
		do_real_addr(&node0->size_unit, do_tree(node0->size_unit));
		do_real_addr(&node0->attributes, do_tree(node0->attributes));
		do_real_addr(&node0->pointer_to, do_tree(node0->pointer_to));
		do_real_addr(&node0->reference_to,
				do_tree(node0->reference_to));
		do_real_addr(&node0->canonical, do_tree(node0->canonical));
		do_real_addr(&node0->next_variant,
				do_tree(node0->next_variant));
		do_real_addr(&node0->main_variant,
				do_tree(node0->main_variant));
		do_real_addr(&node0->context, do_tree(node0->context));
		do_real_addr(&node0->name, do_tree(node0->name));
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	case MODE_GETSTEP4:
	{
		return;
	}
	default:
		BUG();
	}
}

static void do_type_non_common(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		CLIB_DBG_FUNC_ENTER();
		struct tree_type_non_common *node0 =
			(struct tree_type_non_common *)node;

		do_type_with_lang_specific((tree)&node0->with_lang_specific,0);
		do_real_addr(&node0->values, do_tree(node0->values));
		do_real_addr(&node0->minval, do_tree(node0->minval));
		do_real_addr(&node0->maxval, do_tree(node0->maxval));
#if __GNUC__ < 8
		do_real_addr(&node0->binfo, do_tree(node0->binfo));
#else
		do_real_addr(&node0->lang_1, do_tree(node0->lang_1));
#endif

		CLIB_DBG_FUNC_EXIT();
		return;
	}
	case MODE_GETSTEP4:
	{
		return;
	}
	default:
		BUG();
	}
}

static void do_decl_minimal(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		CLIB_DBG_FUNC_ENTER();
		struct tree_decl_minimal *node0 =
			(struct tree_decl_minimal *)node;
		do_location(&node0->locus);
		do_common((tree)&node0->common, 0);

		do_real_addr(&node0->name, do_tree(node0->name));
		do_real_addr(&node0->context, do_tree(node0->context));
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	case MODE_GETSTEP4:
	{
		return;
	}
	default:
		BUG();
	}
}

static void __maybe_unused do_lang_decl(void *node, int flag)
{
	if (!node)
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		BUG();
	}
	case MODE_GETSTEP4:
	{
		BUG();
	}
	default:
		BUG();
	}
}
static void do_decl_common(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		CLIB_DBG_FUNC_ENTER();
		struct tree_decl_common *node0;
		node0 = (struct tree_decl_common *)node;
		do_decl_minimal((tree)&node0->common, 0);

		do_real_addr(&node0->size, do_tree(node0->size));
		do_real_addr(&node0->size_unit, do_tree(node0->size_unit));
		do_real_addr(&node0->initial, do_tree(node0->initial));
		do_real_addr(&node0->attributes, do_tree(node0->attributes));
		do_real_addr(&node0->abstract_origin,
				do_tree(node0->abstract_origin));
		do_real_addr(&node0->lang_specific,
				do_c_lang_decl((struct c_lang_decl*)
						node0->lang_specific,1));
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	case MODE_GETSTEP4:
	{
		return;
	}
	default:
		BUG();
	}
}

static void do_field_decl(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		CLIB_DBG_FUNC_ENTER();
		struct tree_field_decl *node0 = (struct tree_field_decl *)node;
		do_decl_common((tree)&node0->common, 0);

		do_real_addr(&node0->offset, do_tree(node0->offset));
		do_real_addr(&node0->bit_field_type,
				do_tree(node0->bit_field_type));
		do_real_addr(&node0->qualifier, do_tree(node0->qualifier));
		do_real_addr(&node0->bit_offset, do_tree(node0->bit_offset));
		do_real_addr(&node0->fcontext, do_tree(node0->fcontext));
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	case MODE_GETSTEP4:
	{
		return;
	}
	default:
		BUG();
	}
}

static void do_decl_with_rtl(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		CLIB_DBG_FUNC_ENTER();
		struct tree_decl_with_rtl *node0 =
			(struct tree_decl_with_rtl *)node;
		do_decl_common((tree)&node0->common, 0);
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	case MODE_GETSTEP4:
	{
		return;
	}
	default:
		BUG();
	}
}

static void do_label_decl(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		CLIB_DBG_FUNC_ENTER();
		struct tree_label_decl *node0 = (struct tree_label_decl *)node;
		do_decl_with_rtl((tree)&node0->common, 0);
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	case MODE_GETSTEP4:
	{
		return;
	}
	default:
		BUG();
	}
}

static void __get_type_detail(struct type_node **base, struct type_node **next,
				tree node);
static void do_result_decl(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		CLIB_DBG_FUNC_ENTER();
		struct tree_result_decl *node0;
		node0 = (struct tree_result_decl *)node;
		do_decl_with_rtl((tree)&node0->common, 0);
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	case MODE_GETSTEP4:
	{
		tree n = node;
		CLIB_DBG_FUNC_ENTER();

		char name[NAME_MAX];
		struct var_list *vnl;
		if (unlikely(si_is_global_var(n, NULL))) {
			si_log1("in func: %s\n", cur_fsn->name);
			CLIB_DBG_FUNC_EXIT();
			return;
		}

		vnl = var_list_find(&cur_fn->local_vars, n);
		if (!vnl) {
			vnl = var_list_new(n);
			if (DECL_NAME(n)) {
				memset(name, 0, NAME_MAX);
				get_node_name(DECL_NAME(n), name);
				vnl->var.name = name_list_add(name,
							      strlen(name) + 1);
			}
			slist_add_tail(&vnl->sibling, &cur_fn->local_vars);
		}

		if (!vnl->var.type)
			__get_type_detail(&vnl->var.type, NULL,
						TREE_TYPE(n));

		if (vnl->var.type) {
			analysis__type_add_use_at(vnl->var.type,
						  cur_fsn->node_id.id,
						  SI_TYPE_DF_GIMPLE,
						  cur_gimple,
						  cur_gimple_op_idx);
		}

		analysis__var_add_use_at(&vnl->var, cur_fsn->node_id.id,
					 SI_TYPE_DF_GIMPLE,
					 cur_gimple, cur_gimple_op_idx);

		/* add a data_state for this var */
		(void)fn_ds_add(cur_fn, (u64)&vnl->var, DSRT_VN);

		CLIB_DBG_FUNC_EXIT();
		return;

	}
	default:
		BUG();
	}
}

static void do_parm_decl(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		CLIB_DBG_FUNC_ENTER();
		struct tree_parm_decl *node0 = (struct tree_parm_decl *)node;
		do_decl_with_rtl((tree)&node0->common, 0);
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	case MODE_GETSTEP4:
	{
		tree n = node;
		CLIB_DBG_FUNC_ENTER();

		struct var_list *vnl;

		vnl = var_list_find(&cur_fn->args, n);
		if (!vnl) {
			CLIB_DBG_FUNC_EXIT();
			return;
		}

		if (vnl->var.type) {
			analysis__type_add_use_at(vnl->var.type,
						  cur_fsn->node_id.id,
						  SI_TYPE_DF_GIMPLE,
						  cur_gimple,
						  cur_gimple_op_idx);
		}

		analysis__var_add_use_at(&vnl->var, cur_fsn->node_id.id,
					 SI_TYPE_DF_GIMPLE,
					 cur_gimple, cur_gimple_op_idx);

		(void)fn_ds_add(cur_fn, (u64)&vnl->var, DSRT_VN);

		CLIB_DBG_FUNC_EXIT();
		return;
	}
	default:
		BUG();
	}
}

static void do_decl_with_vis(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		CLIB_DBG_FUNC_ENTER();
		struct tree_decl_with_vis *node0 =
			(struct tree_decl_with_vis *)node;
		do_decl_with_rtl((tree)&node0->common, 0);

		do_real_addr(&node0->assembler_name,
				do_tree(node0->assembler_name));
		do_real_addr(&node0->symtab_node,
				do_symtab_node((void *)node0->symtab_node, 1));
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	case MODE_GETSTEP4:
	{
		return;
	}
	default:
		BUG();
	}
}

static void do_init_value(struct var_node *vn, tree init_tree);
static void do_var_decl_phase4(tree n)
{
	CLIB_DBG_FUNC_ENTER();

	tree ctx = DECL_CONTEXT(n);

	char name[NAME_MAX];
	struct var_list *newlv = NULL;

	expanded_location *xloc;
	struct sibuf *b;
	int held = 0;

	b = find_target_sibuf(n);
	BUG_ON(!b);
	held = analysis__sibuf_hold(b);

	xloc = get_location(GET_LOC_VAR, b->payload, n);
	if (si_is_global_var(n, xloc)) {
		/* global vars */
		struct sinode *global_var_sn;
		struct var_node *gvn;
		struct id_list *newgv = NULL;
		long value;
		long val_flag;

		get_var_sinode(n, &global_var_sn, 1);
		if (!global_var_sn) {
			value = (long)n;
			val_flag = 1;
		} else {
			value = global_var_sn->node_id.id.id1;
			val_flag = 0;
			gvn = (struct var_node *)global_var_sn->data;
		}

		newgv = id_list_find(&cur_fn->global_vars, value, val_flag);
		if (!newgv) {
			newgv = id_list_new();
			newgv->value = value;
			newgv->value_flag = val_flag;
			slist_add_tail(&newgv->sibling, &cur_fn->global_vars);
		}
		if (val_flag)
			goto out;

		if (gvn->type) {
			analysis__type_add_use_at(gvn->type,
						  cur_fsn->node_id.id,
						  SI_TYPE_DF_GIMPLE,
						  cur_gimple,
						  cur_gimple_op_idx);
		}

		analysis__var_add_use_at(gvn, cur_fsn->node_id.id,
					 SI_TYPE_DF_GIMPLE,
					 cur_gimple, cur_gimple_op_idx);

		(void)global_ds_base_add((u64)gvn, DSRT_VN);

		goto out;
	}

	/* local variable. static variable should be handled */
	while (ctx && (TREE_CODE(ctx) == BLOCK))
		ctx = BLOCK_SUPERCONTEXT(ctx);

	if ((ctx == si_current_function_decl) || (!ctx)) {
		;
	}

	/* current function local vars */
	newlv = var_list_find(&cur_fn->local_vars, (void *)n);
	if (!newlv) {
		newlv = var_list_new((void *)n);
		if (DECL_NAME(n)) {
			memset(name, 0, NAME_MAX);
			get_node_name(DECL_NAME(n), name);
			newlv->var.name = name_list_add(name,
							strlen(name)+1);
		}
		slist_add_tail(&newlv->sibling, &cur_fn->local_vars);
	}

	if (!newlv->var.type)
		__get_type_detail(&newlv->var.type, NULL, TREE_TYPE(n));

	if (newlv->var.type) {
		analysis__type_add_use_at(newlv->var.type,
					  cur_fsn->node_id.id,
					  SI_TYPE_DF_GIMPLE,
					  cur_gimple,
					  cur_gimple_op_idx);
	}

	analysis__var_add_use_at(&newlv->var, cur_fsn->node_id.id,
				 SI_TYPE_DF_GIMPLE,
				 cur_gimple, cur_gimple_op_idx);

	(void)fn_ds_add(cur_fn, (u64)&newlv->var, DSRT_VN);

	if (TREE_STATIC(n) || DECL_INITIAL(n)) {
		do_init_value(&newlv->var, DECL_INITIAL(n));
	}

out:
	if (!held)
		analysis__sibuf_drop(b);
	CLIB_DBG_FUNC_EXIT();
	return;
}

static void do_var_decl(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		CLIB_DBG_FUNC_ENTER();
		struct tree_var_decl *node0 = (struct tree_var_decl *)node;

		do_decl_with_vis((tree)&node0->common, 0);

		CLIB_DBG_FUNC_EXIT();
		return;
	}
	case MODE_GETSTEP4:
	{
		do_var_decl_phase4(node);
		return;
	}
	default:
		BUG();
	}
}

static void do_decl_non_common(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		CLIB_DBG_FUNC_ENTER();
		struct tree_decl_non_common *node0 =
			(struct tree_decl_non_common *)node;
		do_decl_with_vis((tree)&node0->common, 0);

		do_real_addr(&node0->result, do_tree(node0->result));
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	case MODE_GETSTEP4:
	{
		CLIB_DBG_FUNC_ENTER();
		struct tree_decl_non_common *node0;
		node0 = (struct tree_decl_non_common *)node;
		do_decl_with_vis((tree)&node0->common, 0);

		do_tree(node0->result);
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	default:
		BUG();
	}
}

static void do_type_decl(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		CLIB_DBG_FUNC_ENTER();
		struct tree_type_decl *node0 = (struct tree_type_decl *)node;
		do_decl_non_common((tree)&node0->common, 0);
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	case MODE_GETSTEP4:
	{
		return;
	}
	default:
		BUG();
	}
}

static void do_translation_unit_decl(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		CLIB_DBG_FUNC_ENTER();
		struct tree_translation_unit_decl *node0 =
				(struct tree_translation_unit_decl *)node;
		do_decl_common((tree)&node0->common, 0);

		do_real_addr(&node0->language,
				is_obj_checked((void *)node0->language));
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	case MODE_GETSTEP4:
	{
		return;
	}
	default:
		BUG();
	}
}

static void do_function_decl(tree node, int flag)
{
	if (!node)
		return;

	BUG_ON(!flag);

	if (is_obj_checked((void *)node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		CLIB_DBG_FUNC_ENTER();

		struct tree_function_decl *node0 =
			(struct tree_function_decl *)node;

#if 0
		if (!DECL_STRUCT_FUNCTION(node)) {
			struct tree_decl_minimal *node1;
			node1 = (struct tree_decl_minimal *)node0;
			do_real_addr(&node1->name, do_tree(DECL_NAME(node)));
			CLIB_DBG_FUNC_EXIT();
			return;
		}
#endif

		do_decl_non_common((tree)&node0->common, 0);

		do_real_addr(&node0->f, do_function(node0->f, 1));
		do_real_addr(&node0->arguments, do_tree(node0->arguments));
		do_real_addr(&node0->personality, do_tree(node0->personality));
		do_real_addr(&node0->function_specific_target,
				do_tree(node0->function_specific_target));
		do_real_addr(&node0->function_specific_optimization,
			     do_tree(node0->function_specific_optimization));
		do_real_addr(&node0->saved_tree, do_tree(node0->saved_tree));
		do_real_addr(&node0->vindex, do_tree(node0->vindex));

		CLIB_DBG_FUNC_EXIT();
		return;
	}
	case MODE_GETSTEP4:
	{
		tree n = node;
		CLIB_DBG_FUNC_ENTER();

		struct sinode *fsn;
		get_func_sinode(n, &fsn, 1);
		if (!fsn)
			goto out;
		if (fsn == cur_fsn)
			goto out;

		struct func_node *fn;
		fn = (struct func_node *)fsn->data;
		if (!fn)
			goto out;

		analysis__func_add_use_at(fn, cur_fsn->node_id.id,
					  SI_TYPE_DF_GIMPLE,
					  cur_gimple, cur_gimple_op_idx);

		(void)fn_ds_add(cur_fn, (u64)fn, DSRT_FN);

out:
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	default:
		BUG();
	}
}

static void do_const_decl(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		CLIB_DBG_FUNC_ENTER();
		struct tree_const_decl *node0 = (struct tree_const_decl *)node;
		do_decl_common((tree)&node0->common, 0);
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	case MODE_GETSTEP4:
	{
		return;
	}
	default:
		BUG();
	}
}

static void do_vec(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		CLIB_DBG_FUNC_ENTER();
		struct tree_vec *node0 = (struct tree_vec *)node;
		do_common((tree)&node0->common, 0);

		for (int i = 0; i < TREE_VEC_LENGTH(node); i++) {
			do_real_addr(&node0->a[i], do_tree(node0->a[i]));
		}
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	case MODE_GETSTEP4:
	{
		CLIB_DBG_FUNC_ENTER();
		struct tree_vec *node0 = (struct tree_vec *)node;
		do_common((tree)&node0->common, 0);

		for (int i = 0; i < TREE_VEC_LENGTH(node); i++) {
			do_tree(node0->a[i]);
		}
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	default:
		BUG();
	}
}

static void do_fixed_cst(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		return;
	}
	case MODE_GETSTEP4:
	{
		return;
	}
	default:
		BUG();
	}
}

static void do_vector(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		return;
	}
	case MODE_GETSTEP4:
	{
		return;
	}
	default:
		BUG();
	}
}

static void do_complex(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		return;
	}
	case MODE_GETSTEP4:
	{
		return;
	}
	default:
		BUG();
	}
}

static void do_tree_p(void *node)
{
	if (!node)
		return;
	if (is_obj_checked(node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		CLIB_DBG_FUNC_ENTER();
		tree *node0;
		node0 = (tree *)node;
		do_real_addr(node0, do_tree(*node0));
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	case MODE_GETSTEP4:
	{
		CLIB_DBG_FUNC_ENTER();
		tree *node0;
		node0 = (tree *)node;
		do_tree(*node0);
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	default:
		BUG();
	}
}

static void do_ssa_use_operand(void *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		CLIB_DBG_FUNC_ENTER();
		struct ssa_use_operand_t *node0;
		node0 = (struct ssa_use_operand_t *)node;

		do_real_addr(&node0->prev, do_ssa_use_operand(node0->prev, 1));
		do_real_addr(&node0->next, do_ssa_use_operand(node0->next, 1));

		if (node0->use) {
			do_real_addr(&node0->loc.stmt,
					do_gimple_seq(node0->loc.stmt));
			do_real_addr(&node0->use, do_tree_p(node0->use));
		} else {
			do_real_addr(&node0->loc.ssa_name,
					do_tree(node0->loc.ssa_name));
		}
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	case MODE_GETSTEP4:
	{
		CLIB_DBG_FUNC_ENTER();
		struct ssa_use_operand_t *node0;
		node0 = (struct ssa_use_operand_t *)node;

		do_ssa_use_operand(node0->prev, 1);
		do_ssa_use_operand(node0->next, 1);

		if (node0->use) {
			do_gimple_seq(node0->loc.stmt);
			do_tree_p(node0->use);
		} else {
			do_tree(node0->loc.ssa_name);
		}
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	default:
		BUG();
	}
}

static void do_ptr_info_def(void *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;

	/* nothing to do here */
}

static void do_range_info_def(void *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;

	/* nothing to do here */
}

static void do_ssa_name(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		CLIB_DBG_FUNC_ENTER();
		struct tree_ssa_name *node0 = (struct tree_ssa_name *)node;
		do_typed((tree)&node0->typed, 0);
		do_real_addr(&node0->var, do_tree(node0->var));
		do_real_addr(&node0->def_stmt,
				do_gimple_seq(node0->def_stmt));

		int testv;
		testv = node0->typed.type ?
				(!POINTER_TYPE_P(TREE_TYPE(node))) : 2;
		if (!testv) {
			do_real_addr(&node0->info.ptr_info,
				     do_ptr_info_def(node0->info.ptr_info, 1));
		} else if (testv == 1) {
			do_real_addr(&node0->info.range_info,
				do_range_info_def(node0->info.range_info, 1));
		}

		do_ssa_use_operand(&node0->imm_uses, 0);
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	case MODE_GETSTEP4:
	{
		CLIB_DBG_FUNC_ENTER();
		struct tree_ssa_name *node0 = (struct tree_ssa_name *)node;
		do_typed((tree)&node0->typed, 0);
		do_tree(node0->var);
		do_gimple_seq(node0->def_stmt);

		int testv;
		testv = node0->typed.type ?
				(!POINTER_TYPE_P(TREE_TYPE(node))) : 2;
		if (!testv) {
			do_ptr_info_def(node0->info.ptr_info, 1);
		} else if (testv == 1) {
			do_range_info_def(node0->info.range_info, 1);
		}

		do_ssa_use_operand(&node0->imm_uses, 0);
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	default:
		BUG();
	}
}

static void do_binfo(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		return;
	}
	case MODE_GETSTEP4:
	{
		return;
	}
	default:
		BUG();
	}
}

static void do_omp_clause(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		return;
	}
	case MODE_GETSTEP4:
	{
		return;
	}
	default:
		BUG();
	}
}

static void do_optimization_option(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		CLIB_DBG_FUNC_ENTER();
		struct tree_optimization_option *node0;
		node0 = (struct tree_optimization_option *)node;
#if __GNUC__ < 8
		do_common((tree)&node0->common, 0);
#else
		do_base((tree)&node0->base, 0);
#endif
		CLIB_DBG_FUNC_EXIT();
		return;
	}
	case MODE_GETSTEP4:
	{
		return;
	}
	default:
		BUG();
	}
}

static void do_target_option(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		return;
	}
	case MODE_GETSTEP4:
	{
		return;
	}
	default:
		BUG();
	}
}

/*
 * ************************************************************************
 * seperator
 * ************************************************************************
 */

/*
 * 2020-06-06 UPDATE:
 *	the old sinode could come from OTHER modules, like gcc_asm
 */
static int func_conflict(expanded_location *newl, struct sinode *old)
{
#if 0
	if ((strlen(old->loc_file->name) == strlen(newl->file)) &&
		(!strcmp((char *)old->loc_file->name, newl->file)) &&
		(newl->line == old->loc_line) &&
		(newl->column == old->loc_col))
		BUG();
		return TREE_NAME_CONFLICT_DROP;
#endif

	struct tree_decl_with_vis *oldvis, *newvis;
	tree newtree, oldtree;
	newtree = (tree)(long)(objs[obj_idx].real_addr);
	newvis = (struct tree_decl_with_vis *)newtree;

	if (__si_data_fmt(old->buf) != SI_TYPE_DF_GIMPLE) {
		if (newvis->weak_flag < old->weak_flag)
			return TREE_NAME_CONFLICT_REPLACE;
		else
			return TREE_NAME_CONFLICT_DROP;
	}

	oldtree = (tree)(long)old->obj->real_addr;
	oldvis = (struct tree_decl_with_vis *)oldtree;

	if (oldvis->weak_flag > newvis->weak_flag)
		return TREE_NAME_CONFLICT_REPLACE;
	if ((!DECL_STRUCT_FUNCTION(oldtree)) &&
		(DECL_STRUCT_FUNCTION(newtree)))
		return TREE_NAME_CONFLICT_REPLACE;
	return TREE_NAME_CONFLICT_DROP;
}

static int var_conflict(expanded_location *newl, struct sinode *old)
{
#if 0
	if ((strlen(old->loc_file->name) == strlen(newl->file)) &&
		(!strcmp((char *)old->loc_file->name, newl->file)) &&
		(newl->line == old->loc_line) &&
		(newl->column == old->loc_col))
		return TREE_NAME_CONFLICT_DROP;
#endif

	struct tree_decl_with_vis *oldvis, *newvis;
	tree oldtree, newtree;
	newtree = (tree)(long)(objs[obj_idx].real_addr);
	newvis = (struct tree_decl_with_vis *)newtree;

	if (__si_data_fmt(old->buf) != SI_TYPE_DF_GIMPLE) {
		if (newvis->weak_flag < old->weak_flag)
			return TREE_NAME_CONFLICT_REPLACE;
		else
			return TREE_NAME_CONFLICT_DROP;
	}

	oldtree = (tree)(long)old->obj->real_addr;
	oldvis = (struct tree_decl_with_vis *)oldtree;

	if (oldvis->weak_flag <= newvis->weak_flag)
		return TREE_NAME_CONFLICT_DROP;
	else
		return TREE_NAME_CONFLICT_REPLACE;
}

/* NOTE, TYPE_CONTEXT DECL_CONTEXT to check the scope */
static int type_conflict(expanded_location *newl, struct sinode *old)
{
#if 0
	if (newl && newl->file && old->loc_file &&
		(!strcmp(newl->file, old->loc_file->name)) &&
		(newl->line == old->loc_line) &&
		(newl->column == old->loc_col))
		return TREE_NAME_CONFLICT_DROP;
	if ((!newl) && old->loc_file)
		return TREE_NAME_CONFLICT_SOFT;
	if ((newl) && (!newl->file) && old->loc_file)
		return TREE_NAME_CONFLICT_SOFT;
	if (newl && newl->file && (!old->loc_file))
		return TREE_NAME_CONFLICT_SOFT;
#endif

#if 0
	tree oldtree = (tree)(long)old->obj->real_addr;
	tree newtree = (tree)(long)(objs[obj_idx].real_addr);
	if (TREE_CODE(newtree) != TREE_CODE(oldtree))
		return TREE_NAME_CONFLICT_SOFT;
	if (TREE_CODE(TYPE_NAME(newtree)) != TREE_CODE(TYPE_NAME(oldtree)))
		return TREE_NAME_CONFLICT_SOFT;
#endif

	return TREE_NAME_CONFLICT_DROP;
}

/*
 * RETURN VALUES:
 *	TREE_NAME_CONFLICT_DROP		drop the new node
 *	TREE_NAME_CONFLICT_REPLACE	force insert the new node, replace old
 *	TREE_NAME_CONFLICT_SOFT		this is only for var, to same_name_head
 *	TREE_NAME_CONFLICT_FAILED	-1, check failed
 */
static int check_conflict(enum sinode_type type,
			  expanded_location *newl,
			  struct sinode *old)
{
	if (__si_data_fmt(old->buf) == SI_TYPE_DF_GIMPLE) {
		tree oldt = (tree)(long)old->obj->real_addr;
		tree newt = (tree)(long)(objs[obj_idx].real_addr);

		BUG_ON(TREE_CODE_CLASS(TREE_CODE(newt)) !=
					TREE_CODE_CLASS(TREE_CODE(oldt)));
	}

	int ret = TREE_NAME_CONFLICT_FAILED;
	/* do the first node */
	switch (type) {
	case TYPE_FUNC_STATIC:
	case TYPE_FUNC_GLOBAL:
		ret = func_conflict(newl, old);
		break;
	case TYPE_VAR_GLOBAL:
		ret = var_conflict(newl, old);
		break;
	case TYPE_TYPE:
		ret = type_conflict(newl, old);
		break;
	default:
		BUG();
	}

	return ret;
}

/*
 * SYMBOL could be exported
 */
static void do_get_base(struct sibuf *buf)
{
	CLIB_DBG_FUNC_ENTER();
	/*
	 * XXX, iter the objs, find out types, vars, functions,
	 * get the locations of them(the file, we sinode_new TYPE_FILE)
	 * sinode_new for the new node
	 */
	struct sinode *sn_new, *sn_tmp, *loc_file;

	int held = analysis__sibuf_hold(buf);

	/* init global_var func sinode, with data; */
	for (obj_idx = 0; obj_idx < real_obj_cnt; obj_idx++) {
		char name[NAME_MAX];
		enum sinode_type type;
		int behavior = SINODE_INSERT_BH_NONE;
		int flag = 0;
		memset(name, 0, NAME_MAX);

		objs[obj_idx].is_dropped = 0;
		objs[obj_idx].is_replaced = 0;

		void *obj_addr = (void *)(long)(objs[obj_idx].real_addr);
		expanded_location *xloc;
		if (objs[obj_idx].is_type) {
			flag = check_file_type((tree)obj_addr);
			if ((flag == TYPE_UNDEFINED) ||
				(flag == TYPE_CANONICALED)) {
				objs[obj_idx].is_dropped = 1;
				continue;
			}

			get_type_name(obj_addr, name);
			xloc = get_location(GET_LOC_TYPE, buf->payload,
						(tree)obj_addr);

			if (name[0] && xloc && xloc->file)
				type = TYPE_TYPE;
			else
				type = TYPE_NONE;
		} else if (objs[obj_idx].is_function) {
			flag = check_file_func((tree)obj_addr);
			if (flag == FUNC_IS_EXTERN) {
				objs[obj_idx].is_dropped = 1;
				continue;
			} else if (flag == FUNC_IS_STATIC) {
				type = TYPE_FUNC_STATIC;
			} else if (flag == FUNC_IS_GLOBAL) {
				type = TYPE_FUNC_GLOBAL;
			} else
				BUG();

			get_function_name(obj_addr, name);
			xloc = get_location(GET_LOC_FUNC, buf->payload,
						(tree)obj_addr);
		} else if (objs[obj_idx].is_global_var) {
			flag = check_file_var((tree)obj_addr);
			if ((flag == VAR_IS_EXTERN) ||
				(flag == VAR_IS_LOCAL)) {
				objs[obj_idx].is_dropped = 1;
				continue;
			} else if (flag == VAR_IS_STATIC) {
				type = TYPE_VAR_STATIC;
			} else if (flag == VAR_IS_GLOBAL) {
				type = TYPE_VAR_GLOBAL;
			} else
				BUG();

			get_var_name(obj_addr, name);
			xloc = get_location(GET_LOC_VAR, buf->payload,
						(tree)obj_addr);
		} else {
			continue;
		}

		sn_new = NULL;
		sn_tmp = NULL;
		loc_file = NULL;
		mutex_lock(&getbase_lock);

		/* handle the location file */
		if (!xloc)	/* happen only on TYPE_TYPE */
			goto step_1;
		if (!xloc->file)
			goto step_1;
		loc_file = analysis__sinode_search(TYPE_FILE, SEARCH_BY_SPEC,
						(void *)xloc->file);
		if (loc_file)
			goto step_1;
		loc_file = analysis__sinode_new(TYPE_FILE,
						(char *)xloc->file,
						strlen(xloc->file)+1,
						NULL, 0);
		BUG_ON(!loc_file);
		BUG_ON(analysis__sinode_insert(loc_file,
						SINODE_INSERT_BH_NONE));

step_1:
		if ((type == TYPE_NONE) && loc_file && (!name[0])) {
			gen_type_name(name, NAME_MAX,
					(tree)obj_addr, loc_file, xloc);
			if (name[0])
				type = TYPE_TYPE;
		}

		if ((type == TYPE_FUNC_GLOBAL) || (type == TYPE_VAR_GLOBAL)) {
			BUG_ON(!name[0]);
			long args[3];
			args[0] = (long)buf->rf;
			args[1] = (long)get_builtin_resfile();
			args[2] = (long)name;
			sn_tmp = analysis__sinode_search(type, SEARCH_BY_SPEC,
							(void *)args);
		} else if ((type == TYPE_FUNC_STATIC) ||
				(type == TYPE_VAR_STATIC) ||
				(type == TYPE_TYPE)) {
			BUG_ON(!loc_file);
			BUG_ON(!name[0]);
			long args[4];
			args[0] = (long)loc_file;
			args[1] = xloc->line;
			args[2] = xloc->column;
			args[3] = (long)name;
			sn_tmp = analysis__sinode_search(type, SEARCH_BY_SPEC,
							(void *)args);
		} else if (type == TYPE_NONE) {
			struct type_node *tn;
			enum tree_code tc = TREE_CODE((tree)obj_addr);
			tn = analysis__sibuf_typenode_search(buf,tc,obj_addr);
			if (tn)
				goto next_loop;

			struct sibuf_typenode *_new;
			_new = sibuf_typenode_new();
			type_node_init(&_new->type, obj_addr, tc);
			BUG_ON(analysis__sibuf_typenode_insert(buf, _new));
			goto next_loop;
		} else {
			BUG();
		}

		if (sn_tmp && (((type == TYPE_FUNC_STATIC) &&
					(sn_tmp->data)) ||
				(type == TYPE_VAR_STATIC) ||
				(type == TYPE_TYPE))) {
			goto next_loop;
		}

		if (sn_tmp && ((type == TYPE_FUNC_GLOBAL) ||
				((type == TYPE_FUNC_STATIC) &&
					(!sn_tmp->data)) ||
				(type == TYPE_VAR_GLOBAL))) {
			int _held = analysis__sibuf_hold(sn_tmp->buf);
			int chk_val = TREE_NAME_CONFLICT_FAILED;
			chk_val = check_conflict(type, xloc, sn_tmp);
			if (!_held)
				analysis__sibuf_drop(sn_tmp->buf);
			BUG_ON(chk_val == TREE_NAME_CONFLICT_FAILED);
			if (chk_val == TREE_NAME_CONFLICT_DROP) {
				objs[obj_idx].is_dropped = 1;
				mutex_unlock(&getbase_lock);
				continue;
			} else if (chk_val == TREE_NAME_CONFLICT_REPLACE) {
				behavior = SINODE_INSERT_BH_REPLACE;
				/*
				 * set the obj is_replaced, while do_replace
				 * finished, the sn_new->obj->is_replaced is
				 * set
				 */
				if (__si_data_fmt(sn_tmp->buf) ==
					SI_TYPE_DF_GIMPLE)
					sn_tmp->obj->is_replaced = 1;
			} else if (chk_val == TREE_NAME_CONFLICT_SOFT) {
				BUG();
			} else
				BUG();
		}

		sn_new = analysis__sinode_new(type, name, strlen(name)+1,
						NULL, 0);
		BUG_ON(!sn_new);
		sn_new->buf = buf;
		sn_new->obj = &objs[obj_idx];
		sn_new->loc_file = loc_file;
		if (loc_file) {
			sn_new->loc_line = xloc->line;
			sn_new->loc_col = xloc->column;
		}
		if (type == TYPE_TYPE) {
			struct type_node *tn;
			tn = type_node_new(obj_addr,TREE_CODE(tree(obj_addr)));
			tn->type_name = sn_new->name;
			sn_new->data = (char *)tn;
			sn_new->datalen = sizeof(*tn);
		} else if ((type == TYPE_VAR_GLOBAL) ||
				(type == TYPE_VAR_STATIC)) {
			struct var_node *vn;
			vn = var_node_new(obj_addr);
			vn->name = sn_new->name;
			sn_new->data = (char *)vn;
			sn_new->datalen = sizeof(*vn);
		} else if ((type==TYPE_FUNC_GLOBAL) ||
				(type==TYPE_FUNC_STATIC)) {
			struct func_node *fn = NULL;
			struct function *func;
			func = DECL_STRUCT_FUNCTION((tree)obj_addr);
			if (func && func->cfg) {
				fn = func_node_new((void *)obj_addr);
				fn->name = sn_new->name;
				sn_new->data = (char *)fn;
				sn_new->datalen = sizeof(*fn);
			}
		}
		if ((type == TYPE_VAR_GLOBAL) || (type == TYPE_VAR_STATIC) ||
		    (type == TYPE_FUNC_GLOBAL) || (type == TYPE_FUNC_STATIC)) {
			struct tree_decl_with_vis *newvis;
			newvis = (struct tree_decl_with_vis *)obj_addr;
			sn_new->weak_flag = newvis->weak_flag;
		}

		BUG_ON(analysis__sinode_insert(sn_new, behavior));
next_loop:
		mutex_unlock(&getbase_lock);
	}

	if (!held)
		analysis__sibuf_drop(buf);
	CLIB_DBG_FUNC_EXIT();
}

static struct type_node *find_type_node(tree type)
{
	CLIB_DBG_FUNC_ENTER();
	struct sinode *sn;
	struct type_node *tn;
	get_type_xnode(type, &sn, &tn);
	if (sn) {
		tn = (struct type_node *)sn->data;
	}

	CLIB_DBG_FUNC_EXIT();
	return tn;
}

/*
 * XXX: if this is the upper call, base MUST NOT be NULL;
 *	otherwise, MUST be NULL
 * if new field_list should be pointed by next, then next is not NULL
 */
static void __get_type_detail(struct type_node **base, struct type_node **next,
				tree node)
{
	CLIB_DBG_FUNC_ENTER();

	int upper_call = 0;
	if (base) {
		upper_call = 1;
	}
	struct type_node *new_type = NULL;
	int code = TREE_CODE(node);
	BUG_ON(TREE_CODE_CLASS(code) != tcc_type);
	char name[NAME_MAX];

	switch (code) {
	case OFFSET_TYPE:
	case NULLPTR_TYPE:
	case FIXED_POINT_TYPE:
	case QUAL_UNION_TYPE:
	case METHOD_TYPE:
	case LANG_TYPE:
	default:
		si_log1("miss %s\n", tree_code_name[code]);
		break;
	case REFERENCE_TYPE:
	{
		new_type = find_type_node(node);
		if (!new_type)
			break;
		if (new_type->is_set)
			break;
		new_type->is_set = 1;
		if (COMPLETE_TYPE_P(node))
			new_type->ofsize =
				TREE_INT_CST_LOW(TYPE_SIZE_UNIT(node));
		__get_type_detail(NULL, &new_type->next, TREE_TYPE(node));
		break;
	}
#if __GNUC__ < 9
	/* removed in gcc-9 */
	case POINTER_BOUNDS_TYPE:
#endif
	case BOOLEAN_TYPE:
	case VOID_TYPE:
	{
		new_type = find_type_node(node);
		if (!new_type)
			break;
		if (new_type->is_set)
			break;
		new_type->is_set = 1;
		if (COMPLETE_TYPE_P(node))
			new_type->ofsize =
				TREE_INT_CST_LOW(TYPE_SIZE_UNIT(node));
		break;
	}
	case ENUMERAL_TYPE:
	{
		new_type = find_type_node(node);
		if (!new_type)
			break;
		if (new_type->is_set)
			break;

		new_type->is_set = 1;
		new_type->is_unsigned = TYPE_UNSIGNED(node);
		new_type->baselen = TYPE_PRECISION(node);
		if (COMPLETE_TYPE_P(node))
			new_type->ofsize =
				TREE_INT_CST_LOW(TYPE_SIZE_UNIT(node));

		tree enum_list = TYPE_VALUES(node);
		while (enum_list) {
			memset(name, 0, NAME_MAX);
			get_node_name(TREE_PURPOSE(enum_list), name);
			BUG_ON(!name[0]);
			struct var_list *_new_var;
			_new_var = var_list_new((void *)enum_list);
			_new_var->var.name = name_list_add(name,
							   strlen(name) + 1);
			long value;
			value = (long)TREE_INT_CST_LOW(TREE_VALUE(enum_list));
			analysis__add_possible(&_new_var->var,
						VALUE_IS_INT_CST,
						value, 0);
			slist_add_tail(&_new_var->sibling,&new_type->children);

			enum_list = TREE_CHAIN(enum_list);
		}
		break;
	}
	case INTEGER_TYPE:
	{
		new_type = find_type_node(node);
		if (!new_type)
			break;
		if (new_type->is_set)
			break;
		new_type->is_set = 1;
		new_type->is_unsigned = TYPE_UNSIGNED(node);
		new_type->baselen = TYPE_PRECISION(node);
		new_type->ofsize = TREE_INT_CST_LOW(TYPE_SIZE_UNIT(node));
		break;
	}
	case REAL_TYPE:
	{
		new_type = find_type_node(node);
		if (!new_type)
			break;
		if (new_type->is_set)
			break;
		new_type->is_set = 1;
		new_type->baselen = TYPE_PRECISION(node);
		new_type->ofsize = TREE_INT_CST_LOW(TYPE_SIZE_UNIT(node));
		break;
	}
	case POINTER_TYPE:
	{
		new_type = find_type_node(node);
		if (!new_type)
			break;
		if (new_type->is_set)
			break;
		new_type->is_set = 1;
		if (COMPLETE_TYPE_P(node))
			new_type->ofsize =
				TREE_INT_CST_LOW(TYPE_SIZE_UNIT(node));
		__get_type_detail(NULL, &new_type->next, TREE_TYPE(node));

		break;
	}
	case COMPLEX_TYPE:
	{
		new_type = find_type_node(node);
		if (!new_type)
			break;
		if (new_type->is_set)
			break;
		new_type->is_set = 1;
		if (COMPLETE_TYPE_P(node))
			new_type->ofsize =
				TREE_INT_CST_LOW(TYPE_SIZE_UNIT(node));
		__get_type_detail(NULL, NULL, TREE_TYPE(node));

		break;
	}
	case VECTOR_TYPE:
	{
		new_type = find_type_node(node);
		if (!new_type)
			break;
		if (new_type->is_set)
			break;
		new_type->is_set = 1;
		if (COMPLETE_TYPE_P(node))
			new_type->ofsize =
				TREE_INT_CST_LOW(TYPE_SIZE_UNIT(node));
		__get_type_detail(NULL, &new_type->next, TREE_TYPE(node));

		break;
	}
	case ARRAY_TYPE:
	{
		new_type = find_type_node(node);
		if (!new_type)
			break;
		if (new_type->is_set)
			break;

		/* FIXME: is the length right? */
		new_type->is_set = 1;
		if (TYPE_DOMAIN(node) && TYPE_MAX_VALUE(TYPE_DOMAIN(node))) {
			size_t len = 1;
			len += TREE_INT_CST_LOW(TYPE_MAX_VALUE(
							   TYPE_DOMAIN(node)));
			new_type->baselen = len;
		}
		if (TYPE_SIZE_UNIT(node))
			new_type->ofsize =
				TREE_INT_CST_LOW(TYPE_SIZE_UNIT(node));

		__get_type_detail(NULL, &new_type->next, TREE_TYPE(node));

		break;
	}
	case RECORD_TYPE:
	case UNION_TYPE:
	{
		new_type = find_type_node(node);
		if (!new_type)
			break;

		if (new_type->is_set)
			break;

		new_type->is_set = 1;
		if (TYPE_SIZE_UNIT(node))
			new_type->ofsize =
				TREE_INT_CST_LOW(TYPE_SIZE_UNIT(node));

		tree fields = TYPE_FIELDS(node);
		while (fields) {
			if (unlikely((!DECL_CHAIN(fields)) &&
				(TREE_CODE(TREE_TYPE(fields)) == ARRAY_TYPE)))
				new_type->is_variant = 1;

			struct var_list *newf0;
			newf0 = var_list_new((void *)fields);
			struct var_node *newf1 = &newf0->var;
			__get_type_detail(&newf1->type, NULL,
						TREE_TYPE(fields));
			if (DECL_NAME(fields)) {
				memset(name, 0, NAME_MAX);
				get_node_name(DECL_NAME(fields), name);
				newf1->name = name_list_add(name,
							    strlen(name) + 1);
			}
			slist_add_tail(&newf0->sibling, &new_type->children);
			fields = DECL_CHAIN(fields);
		}

		break;
	}
	case FUNCTION_TYPE:
	{
		new_type = find_type_node(node);
		if (!new_type)
			break;
		if (new_type->is_set)
			break;

		new_type->is_set = 1;
		if (COMPLETE_TYPE_P(node))
			new_type->ofsize =
				TREE_INT_CST_LOW(TYPE_SIZE_UNIT(node));

		/* handle the type of return value */
		__get_type_detail(NULL, NULL, TREE_TYPE(node));

		/* handle the type of args... */
		tree args = TYPE_ARG_TYPES(node);
		while (args) {
			if ((!TREE_CHAIN(args)) &&
			    (TYPE_MODE_RAW(TREE_VALUE(args)) == VOIDmode)) {
				memset(name, 0, NAME_MAX);
				memcpy(name, ARG_VA_LIST_NAME,
						strlen(ARG_VA_LIST_NAME));
				struct type_node *_new;
				_new = type_node_new((void *)args, VOID_TYPE);
				_new->type_name = name_list_add(name,
								strlen(name)+1);
				slist_add_tail(&_new->sibling,
						&new_type->children);
			} else {
				__get_type_detail(NULL, NULL,
							TREE_VALUE(args));
			}

			args = TREE_CHAIN(args);
		}
		break;
	}
	}

	if (next)
		*next = new_type;
	if (upper_call) {
		*base = new_type;
	}

	CLIB_DBG_FUNC_EXIT();
}

/* XXX, check gcc/c/c-decl.c c-typeck.c for more information */
static void _get_type_detail(struct type_node *tn)
{
	CLIB_DBG_FUNC_ENTER();

	tree node = (tree)tn->node;
	struct type_node *new_type = NULL;

	__get_type_detail(&new_type, NULL, node);
	if (unlikely((long)tn != (long)new_type)) {
		BUG();
	}

	CLIB_DBG_FUNC_EXIT();
	return;
}

static void get_type_detail(struct sinode *tsn)
{
	CLIB_DBG_FUNC_ENTER();

	int held = analysis__sibuf_hold(tsn->buf);

	/* XXX, get attributes here */
	tree node = (tree)(long)tsn->obj->real_addr;
	get_attributes(&tsn->attributes, TYPE_ATTRIBUTES(node));

	if (!held)
		analysis__sibuf_drop(tsn->buf);
	CLIB_DBG_FUNC_EXIT();
}

static void get_var_detail(struct sinode *sn)
{
	CLIB_DBG_FUNC_ENTER();

	int held = analysis__sibuf_hold(sn->buf);
	tree node = (tree)(long)sn->obj->real_addr;
	struct var_node *new_var;
	new_var = (struct var_node *)sn->data;

	if (new_var->detailed)
		goto out;

	__get_type_detail(&new_var->type, NULL, TREE_TYPE(node));

	get_attributes(&sn->attributes, DECL_ATTRIBUTES(node));
	new_var->detailed = 1;

out:
	if (!held)
		analysis__sibuf_drop(sn->buf);
	CLIB_DBG_FUNC_EXIT();
	return;
}

/*
 * NOTE: the gimple_body is now converted and added to cfg
 */
static void get_function_detail(struct sinode *sn)
{
	CLIB_DBG_FUNC_ENTER();

	int held = analysis__sibuf_hold(sn->buf);
	tree node = (tree)(long)sn->obj->real_addr;

	/* XXX: the function body is now in node->f->cfg */
	struct function *f;
	f = DECL_STRUCT_FUNCTION(node);
	if ((!f) || (!f->cfg)) {
		if (!held)
			analysis__sibuf_drop(sn->buf);
		CLIB_DBG_FUNC_EXIT();
		return;
	}

	/* handle the return type and argument list */
	char name[NAME_MAX];
	struct func_node *new_func;
	new_func = (struct func_node *)sn->data;

	if (new_func->detailed) {
		if (!held)
			analysis__sibuf_drop(sn->buf);
		CLIB_DBG_FUNC_EXIT();
		return;
	}

	__get_type_detail(&new_func->ret_type, NULL, TREE_TYPE(TREE_TYPE(node)));
	get_attributes(&sn->attributes, DECL_ATTRIBUTES(node));

	/* parse arguments */
	tree args;
	args = DECL_ARGUMENTS(node);
	while (args) {
		struct var_list *new_arg;
		new_arg = var_list_new((void *)args);

		struct var_node *new_vn;
		new_vn = &new_arg->var;
		__get_type_detail(&new_vn->type, NULL, TREE_TYPE(args));

		memset(name, 0, NAME_MAX);
		get_node_name(DECL_NAME(args), name);
		BUG_ON(!name[0]);
		new_vn->name = name_list_add(name, strlen(name) + 1);

		slist_add_tail(&new_arg->sibling, &new_func->args);

		args = TREE_CHAIN(args);
	}

	/*
	 * TODO: use FOR_EACH_BB_FN()
	 */

	/* XXX: okay, time to parse the control flow graph */
	basic_block bb, b;
	bb = f->cfg->x_entry_block_ptr; /* the first basic_block of the f */

	/*
	 * the gimple statements are normally in basic_block.
	 * However, some may be in edge(check edge_def structure).
	 * the basic_blocks can be searched by bb->next_bb till NULL.
	 * each block may have several preds and 2-succs?, which can
	 * be used for GIMPLE_COND, and that, is the control flow graph.
	 *
	 * extract_true_false_edges_from_block() show that the edge with
	 * EDGE_TRUE_VALUE flag is the true edge
	 */
	int block_cnt = f->cfg->x_n_basic_blocks;
	int edge_cnt = f->cfg->x_n_edges;
	int block_cur = 0, edge_cur = 0, cps_cur = 0;
	basic_block bbs[block_cnt] = { 0 };
	edge edges[edge_cnt] = { 0 };
	struct code_path **__cps;
	__cps = (struct code_path **)
		src_buf_get(block_cnt * sizeof(struct code_path *));
	struct code_path **cps = __cps;

	/* first, check if we know cfg well */
	b = bb;
	while (b) {
		int i;
		for (i = 0; i < block_cur; i++) {
			BUG_ON(bbs[i] == b);
		}
		BUG_ON(block_cur >= block_cnt);
		bbs[block_cur] = b;
		block_cur++;

		unsigned long brs = 0;
		vec<edge,va_gc> *in, *out;
		in = b->preds;
		out = b->succs;

		if (in) {
			struct vec_prefix *pfx;
			pfx = &in->vecpfx;

			size_t cnt;
			cnt = pfx->m_num;

			edge *addr = &in->vecdata[0];
			for (size_t i = 0; i < cnt; i++) {
				int j;
				for (j = 0; j < edge_cur; j++) {
					if (edges[j] == addr[i])
						break;
				}
				if (j == edge_cur) {
					BUG_ON(edge_cur >= edge_cnt);
					edges[edge_cur] = addr[i];
					edge_cur++;
					if (unlikely(addr[i]->insns.g))
						si_log1("edge has gimple\n");
				}
			}
		}

		if (out) {
			struct vec_prefix *pfx;
			pfx = &out->vecpfx;

			size_t cnt;
			cnt = pfx->m_num;
			if (unlikely(cnt > 2))
				brs = cnt - 2;

			edge *addr = &out->vecdata[0];
			for (size_t i = 0; i < cnt; i++) {
				int j;
				for (j = 0; j < edge_cnt; j++) {
					if (edges[j] == addr[i])
						break;
				}
				if (j == edge_cur) {
					BUG_ON(edge_cur >= edge_cnt);
					edges[edge_cur] = addr[i];
					edge_cur++;
					if (unlikely(addr[i]->insns.g))
						si_log1("edge has gimple\n");
				}
			}
		}

		cps[cps_cur] = code_path_new(new_func, brs);
		cps[cps_cur]->cp = (void *)b;
		gimple_seq gs;
		gs = b->il.gimple.seq;
		while (gs) {
			/*
			 * ignore GIMPLE_DEBUG and GIMPLE_NOP
			 */
			if ((gimple_code(gs) == GIMPLE_DEBUG) ||
				(gimple_code(gs) == GIMPLE_NOP)) {
				gs = gs->next;
				continue;
			}

			if ((!gs->next) && (gimple_code(gs) == GIMPLE_COND)) {
				cps[cps_cur]->cond_head = gs;
				break;
			}
			BUG_ON(gimple_code(gs) == GIMPLE_COND);

			/* XXX: nothing to do here */

			gs = gs->next;
		}

		cps_cur++;
		b = b->next_bb;
	}
	/* NOTE: we really handle all the basic_blocks? */
	BUG_ON(block_cur != block_cnt);
	BUG_ON(edge_cur != edge_cnt);

	/* time to handle cps[]->next[] */
	for (int i = 0; i < block_cnt; i++) {
		basic_block b0, b1;
		b0 = bbs[i];

		vec<edge,va_gc> *out;
		out = b0->succs;
		if (!out)
			continue;

		struct vec_prefix *pfx;
		pfx = &out->vecpfx;
		size_t cnt = pfx->m_num;
		edge *addr = &out->vecdata[0];
		int res[cnt];
		for (size_t i = 0; i < cnt; i++)
			res[i] = -1;
		for (size_t j = 0; j < cnt; j++) {
			edge t = addr[j];
			for (int k = 0; k < block_cnt; k++) {
				b1 = bbs[k];
				vec<edge,va_gc> *in;
				in = b1->preds;
				if (!in)
					continue;

				struct vec_prefix *pfx;
				pfx = &in->vecpfx;
				size_t cnt = pfx->m_num;
				edge *addr = &in->vecdata[0];
				for (size_t m = 0; m < cnt; m++) {
					if (addr[m] != t)
						continue;
					BUG_ON(res[j] != -1);
					res[j] = k;
				}
			}
			BUG_ON(res[j] == -1);
		}

		struct code_path *cp;
		cp = cps[i];
		if (cp->cond_head) {
			if (addr[0]->flags & EDGE_TRUE_VALUE) {
				BUG_ON(!(addr[1]->flags & EDGE_FALSE_VALUE));
				cp->next[1] = cps[res[0]];
				cp->next[0] = cps[res[1]];
			} else {
				BUG_ON(!(addr[0]->flags & EDGE_FALSE_VALUE));
				BUG_ON(!(addr[1]->flags & EDGE_TRUE_VALUE));
				cp->next[0] = cps[res[0]];
				cp->next[1] = cps[res[1]];
			}
		} else {
			for (size_t j = 0; j < cnt; j++) {
				cp->next[j] = cps[res[j]];
			}
		}
	}

	new_func->cps = __cps;
	new_func->cp_cnt = block_cnt;
	new_func->detailed = 1;

	if (!held)
		analysis__sibuf_drop(sn->buf);
	CLIB_DBG_FUNC_EXIT();
	return;
}

/*
 * For type, we get global_var with this type.
 * For global var, we know where it is used.
 * For codepath/func, we know what it need and what it does.
 */
static void do_get_detail(struct sibuf *b)
{
	CLIB_DBG_FUNC_ENTER();

	int held = analysis__sibuf_hold(b);

	for (obj_idx = 0; obj_idx < real_obj_cnt; obj_idx++) {
		if (!objs[obj_idx].is_type) {
			continue;
		}

		if (objs[obj_idx].is_dropped || objs[obj_idx].is_replaced)
			continue;

		void *obj_addr = (void *)(long)(objs[obj_idx].real_addr);
		struct sinode *sn;
		struct type_node *tn;
		/* FIXME: do we have a race here? */
		get_type_xnode((tree)obj_addr, &sn, &tn);
		if (sn) {
			tn = (struct type_node *)sn->data;
			get_type_detail(sn);
		}
		BUG_ON(!tn);

		if (tn->is_set) {
			continue;
		}
		_get_type_detail(tn);
	}

	for (obj_idx = 0; obj_idx < real_obj_cnt; obj_idx++) {
		if (objs[obj_idx].is_dropped || objs[obj_idx].is_replaced)
			continue;

		void *obj_addr = (void *)(long)(objs[obj_idx].real_addr);
		struct sinode *n;
		if (objs[obj_idx].is_global_var) {
			get_var_sinode((tree)obj_addr, &n, 0);
			if (!n)
				continue;

			get_var_detail(n);
		} else if (objs[obj_idx].is_function) {
			get_func_sinode((tree)obj_addr, &n, 0);
			if (!n)
				continue;

			get_function_detail(n);
		}
	}

	if (!held)
		analysis__sibuf_drop(b);

	CLIB_DBG_FUNC_EXIT();
	return;
}

/*
 * ************************************************************************
 * phase 4
 * ************************************************************************
 */
static int is_type_from_expand_macro(struct type_node *tn)
{
	CLIB_DBG_FUNC_ENTER();

	BUG_ON((tn->type_code != RECORD_TYPE) &&
		(tn->type_code != UNION_TYPE));

	struct sibuf *b1 = NULL, *b2 = NULL;
	expanded_location *xloc_b = NULL, *xloc_e = NULL;
	tree field_b = NULL, field_e = NULL;

	struct var_list *tmp;
	slist_for_each_entry(tmp, &tn->children, sibling) {
		if (!field_b) {
			field_b = (tree)tmp->var.node;
			continue;
		}

		field_e = (tree)tmp->var.node;

		b1 = find_target_sibuf((void *)field_b);
		b2 = find_target_sibuf((void *)field_e);
		BUG_ON(!b1);
		BUG_ON(!b2);
		int held1 = analysis__sibuf_hold(b1);
		int held2 = analysis__sibuf_hold(b2);

		xloc_b = (expanded_location *)(b1->payload +
						DECL_SOURCE_LOCATION(field_b));
		xloc_e = (expanded_location *)(b2->payload +
						DECL_SOURCE_LOCATION(field_e));

		if (same_location(xloc_b, xloc_e)) {
			if (!held1)
				analysis__sibuf_drop(b1);
			if (!held2)
				analysis__sibuf_drop(b2);
			CLIB_DBG_FUNC_EXIT();
			return 1;
		}

		if (!held1)
			analysis__sibuf_drop(b1);
		if (!held2)
			analysis__sibuf_drop(b2);

		field_b = field_e;
	}

	CLIB_DBG_FUNC_EXIT();
	return 0;
}

/* FIXME: check gimple_compare_field_offset() in gcc/gimple.c */
static int is_same_field(tree field0, tree field1, int macro_expanded)
{
	CLIB_DBG_FUNC_ENTER();

	struct sibuf *b1 = find_target_sibuf((void *)field0);
	struct sibuf *b2 = find_target_sibuf((void *)field1);

	BUG_ON(!b1);
	BUG_ON(!b2);
	int held1 = analysis__sibuf_hold(b1);
	int held2 = analysis__sibuf_hold(b2);
	int ret = 0;

	if (!macro_expanded) {
		expanded_location *xloc1, *xloc2;
		xloc1 = (expanded_location *)(b1->payload +
						DECL_SOURCE_LOCATION(field0));
		xloc2 = (expanded_location *)(b2->payload +
						DECL_SOURCE_LOCATION(field1));

		if (same_location(xloc1, xloc2)) {
			ret = 1;
			goto out;
		} else {
			ret = 0;
			goto out;
		}
	} else {
		/* FIXME */
		char name0[NAME_MAX];
		char name1[NAME_MAX];
		memset(name0, 0, NAME_MAX);
		memset(name1, 0, NAME_MAX);
		tree tname0 = DECL_NAME(field0);
		tree tname1 = DECL_NAME(field1);
		if ((!tname0) && (tname1)) {
			ret = 0;
			goto out;
		}
		if ((tname0) && (!tname1)) {
			ret = 0;
			goto out;
		}
		if ((!tname0) && (!tname1)) {
			struct type_node *tn0, *tn1;

			/* UPDATE: check the DECL_CONTEXT first */
			if (DECL_CONTEXT(field0) && DECL_CONTEXT(field1)) {
				tree type0 = DECL_CONTEXT(field0);
				tree type1 = DECL_CONTEXT(field1);
				tn0 = find_type_node(type0);
				tn1 = find_type_node(type1);
				if (tn0 == tn1) {
					int _idx0 = field_idx(TYPE_FIELDS(type0),
								field0);
					int _idx1 = field_idx(TYPE_FIELDS(type1),
								field1);
					unsigned long offs0, offs1;
					offs0 = get_field_offset(field0);
					offs1 = get_field_offset(field1);
					/* FIXME: dont think this is right!!! */
					if ((_idx0 == _idx1) || (offs0 == offs1)) {
						ret = 1;
						goto out;
					}
				}
			}

			tn0 = find_type_node(TREE_TYPE(field0));
			tn1 = find_type_node(TREE_TYPE(field1));
			if (tn0 != tn1) {
				ret = 0;
				goto out;
			} else {
				ret = 1;
				goto out;
			}
		}

		get_node_name(tname0, name0);
		get_node_name(tname1, name1);
		if ((!name0[0]) && (name1[0])) {
			ret = 0;
			goto out;
		}
		if ((name0[0]) && (!name1[0])) {
			ret = 0;
			goto out;
		}
		if ((!name0[0]) && (!name1[0])) {
			struct type_node *tn0, *tn1;
			tn0 = find_type_node(TREE_TYPE(field0));
			tn1 = find_type_node(TREE_TYPE(field1));
			if (tn0 != tn1) {
				ret = 0;
				goto out;
			} else {
				ret = 1;
				goto out;
			}
		}
		if (!strcmp(name0, name1)) {
			ret = 1;
			goto out;
		} else {
			ret = 0;
			goto out;
		}
	}

out:
	if (!held1)
		analysis__sibuf_drop(b1);
	if (!held2)
		analysis__sibuf_drop(b2);
	CLIB_DBG_FUNC_EXIT();
	return ret;
}

static struct var_list *get_target_field0(struct type_node *tn, tree field);
static void __do_constructor_init(struct type_node *tn, tree init_tree)
{
	vec<constructor_elt, va_gc> *init_elts;
	init_elts=(vec<constructor_elt, va_gc> *)(CONSTRUCTOR_ELTS(init_tree));

	/* XXX, some structures may have NULL init elements */
	if (!init_elts)
		return;

#if 0
	unsigned long length = init_elts->vecpfx.m_num;
	struct constructor_elt *addr = &init_elts->vecdata[0];

	for (unsigned long i = 0; i < length; i++) {
		if (addr[i].index) {
			BUG_ON(TREE_CODE(addr[i].index) != FIELD_DECL);
			struct var_list *vnl;
			vnl = get_target_field0(tn, (tree)addr[i].index);
			do_init_value(&vnl->var, addr[i].value);
		} else
	}
#endif

	tree type = TREE_TYPE(init_tree);
	switch (TREE_CODE(type)) {
	case RECORD_TYPE:
	case UNION_TYPE:
	{
		tree field = NULL_TREE;
		unsigned HOST_WIDE_INT cnt;
		constructor_elt *ce;

		if (TREE_CODE(type) == RECORD_TYPE)
			field = TYPE_FIELDS(type);

		FOR_EACH_VEC_SAFE_ELT(CONSTRUCTOR_ELTS(init_tree), cnt, ce) {
			tree val = ce->value;

			if (ce->index != 0)
				field = ce->index;

			struct var_list *vnl;
			vnl = get_target_field0(tn, (tree)field);
			do_init_value(&vnl->var, val);

			field = DECL_CHAIN(field);
		}

		break;
	}
	default:
	{
		si_log1_todo("miss %s\n", tree_code_name[TREE_CODE(type)]);
		break;
	}
	}

	return;
}

static void do_struct_init(struct type_node *tn, tree init_tree)
{
	CLIB_DBG_FUNC_ENTER();

	BUG_ON(tn->type_code != RECORD_TYPE);

	__do_constructor_init(tn, init_tree);

	CLIB_DBG_FUNC_EXIT();
	return;
}

static void do_union_init(struct type_node *tn, tree init_tree)
{
	CLIB_DBG_FUNC_ENTER();

	BUG_ON(tn->type_code != UNION_TYPE);

	__do_constructor_init(tn, init_tree);

	CLIB_DBG_FUNC_EXIT();
	return;
}

static void do_init_value(struct var_node *vn, tree init_tree)
{
	/* XXX, data_node could be var_node or type_node(for structure) */
	if (!init_tree)
		return;

	CLIB_DBG_FUNC_ENTER();

	struct sibuf *b = find_target_sibuf((void *)init_tree);
	int held = analysis__sibuf_hold(b);

	enum tree_code init_tc = TREE_CODE(init_tree);
	switch (init_tc) {
	case CONSTRUCTOR:
	{
		vec<constructor_elt, va_gc> *init_elts;
		init_elts = (vec<constructor_elt, va_gc> *)
				(CONSTRUCTOR_ELTS(init_tree));

		if (!init_elts)
			break;

		unsigned long length = init_elts->vecpfx.m_num;
		struct constructor_elt *addr = &init_elts->vecdata[0];

		if (vn->type->type_code == RECORD_TYPE) {
			do_struct_init(vn->type, init_tree);
			break;
		} else if (vn->type->type_code == UNION_TYPE) {
			do_union_init(vn->type, init_tree);
			break;
		}

		/* this is an array */
		if (vn->type->next->type_code != RECORD_TYPE) {
			for (unsigned long i = 0; i < length; i++) {
				do_init_value(vn, addr[i].value);
			}
			break;
		}

		/* this is a structure array */
		for (unsigned long i = 0; i < length; i++) {
			do_struct_init(vn->type->next, addr[i].value);
		}

		break;
	}
	case INTEGER_CST:
	{
		if (TREE_INT_CST_EXT_NUNITS(init_tree) > 1)
			si_log1_todo("EXT_NUNITS > 1\n");
		long value = TREE_INT_CST_LOW(init_tree);
		analysis__add_possible(vn, VALUE_IS_INT_CST, value, 0);
		break;
	}
	case REAL_CST:
	{
		long value = (long)init_tree;
		analysis__add_possible(vn, VALUE_IS_REAL_CST, value, 0);
		break;
	}
	case STRING_CST:
	{
		/* long value = (long)((struct tree_string *)init_tree)->str; */
		long value = (long)TREE_STRING_POINTER(init_tree);
		u32 length = TREE_STRING_LENGTH(init_tree);
		analysis__add_possible(vn, VALUE_IS_STR_CST, value, length);
		break;
	}
	case ADDR_EXPR:
	{
		tree addr = ((struct tree_exp *)init_tree)->operands[0];
		if (!addr)
			break;

		if (TREE_CODE(addr) == FUNCTION_DECL) {
			struct sinode *fsn;
			get_func_sinode(addr, &fsn, 1);
			if (!fsn) {
				/* FIXME, this function not found yet */
				long value = (long)addr;
				analysis__add_possible(vn, VALUE_IS_TREE,
						       value, 0);
			} else {
				/* long value = fsn->node_id.id.id1; */
				long value;
				value = sinode_id_all(fsn);
				analysis__add_possible(vn, VALUE_IS_FUNC,
						       value, 0);
			}
		} else if (TREE_CODE(addr) == VAR_DECL) {
			long value = (long)init_tree;
			analysis__add_possible(vn, VALUE_IS_VAR_ADDR,
					       value, 0);
		} else if (TREE_CODE(addr) == STRING_CST) {
			do_init_value(vn, addr);
		} else if (TREE_CODE(addr) == COMPONENT_REF) {
			long value = (long)init_tree;
			analysis__add_possible(vn, VALUE_IS_VAR_ADDR, value, 0);
		} else if (TREE_CODE(addr) == ARRAY_REF) {
			long value = (long)init_tree;
			analysis__add_possible(vn, VALUE_IS_EXPR, value, 0);
		} else if (TREE_CODE(addr) == COMPOUND_LITERAL_EXPR) {
			tree vd = COMPOUND_LITERAL_EXPR_DECL(addr);
			BUG_ON(!vd);
			do_init_value(vn, DECL_INITIAL(vd));
		} else {
			si_log1("miss %s, %s\n",
					tree_code_name[TREE_CODE(addr)],
					vn->name);
		}
		break;
	}
	case NOP_EXPR:
	{
		tree value = ((struct tree_exp *)init_tree)->operands[0];
		do_init_value(vn, value);
		break;
	}
	case POINTER_PLUS_EXPR:
	case PLUS_EXPR:
	{
		long value = (long)init_tree;
		analysis__add_possible(vn, VALUE_IS_EXPR, value, 0);
		break;
	}
	default:
	{
		si_log1("miss %s\n", tree_code_name[init_tc]);
		break;
	}
	}

	if (!held)
		analysis__sibuf_drop(b);
	CLIB_DBG_FUNC_EXIT();
}

static void do_phase4_gvar(struct sinode *sn)
{
	CLIB_DBG_FUNC_ENTER();

	tree node;
	struct var_node *new_var;

	int held = analysis__sibuf_hold(sn->buf);

	node = (tree)(long)sn->obj->real_addr;
	new_var = (struct var_node *)sn->data;

	do_init_value(new_var, DECL_INITIAL(node));

	if (!held)
		analysis__sibuf_drop(sn->buf);

	CLIB_DBG_FUNC_EXIT();
	return;
}

#if 0
static struct type_node *get_mem_ref_tn(tree op)
{
	CLIB_DBG_FUNC_ENTER();

	struct type_node *tn = NULL;
	struct tree_exp *exp = (struct tree_exp *)op;
	tree t0 = exp->operands[0];
	tree __maybe_unused t1 = exp->operands[1];

	if ((TREE_CODE(t0) == PARM_DECL) || (TREE_CODE(t0) == VAR_DECL)) {
		struct type_node *tn0 = find_type_node(TREE_TYPE(t0));
		struct type_node *tn1 = find_type_node(TREE_TYPE(op));
		tn = tn0;
		if (tn0 != tn1)
			tn = tn1;
	} else if (TREE_CODE(t0) == ADDR_EXPR) {
		struct tree_exp *exp1 = (struct tree_exp *)t0;
		tree tvar = exp1->operands[0];
		BUG_ON((TREE_CODE(tvar) != PARM_DECL) &&
			(TREE_CODE(tvar) != VAR_DECL));
		struct type_node *tn0 = find_type_node(TREE_TYPE(tvar));
		tn = tn0;
	} else if (TREE_CODE(t0) == SSA_NAME) {
		tn = find_type_node(TREE_TYPE(t0));
	} else {
		si_log1("miss %s\n", tree_code_name[TREE_CODE(t0)]);
	}

	CLIB_DBG_FUNC_EXIT();
	return tn;
}
#endif

static struct type_node *get_array_ref_tn(struct tree_exp *exp)
{
	CLIB_DBG_FUNC_ENTER();

	tree op0 = exp->operands[0];
	tree __maybe_unused op1 = exp->operands[1];
	tree __maybe_unused op2 = exp->operands[2];
	tree __maybe_unused op3 = exp->operands[3];

	tree type = TREE_TYPE(op0);
	BUG_ON(TREE_CODE(type) != ARRAY_TYPE);
	type = TREE_TYPE(type);
	while (TREE_CODE(type) == ARRAY_TYPE)
		type = TREE_TYPE(type);
	struct type_node *tn = NULL;
	tn = find_type_node(type);

	CLIB_DBG_FUNC_EXIT();
	return tn;
}

static struct var_list *get_target_field0(struct type_node *tn, tree field)
{
	if (!tn)
		return NULL;

	CLIB_DBG_FUNC_ENTER();

	struct var_list *tmp = NULL;
	while (tn->type_code == POINTER_TYPE) {
		tn = tn->next;
		if (!tn) {
			CLIB_DBG_FUNC_EXIT();
			return NULL;
		}
	}
	if ((tn->type_code != RECORD_TYPE) && (tn->type_code != UNION_TYPE)) {
		if (tn->type_code != VOID_TYPE)
			si_log1_todo("miss %s\n",
					tree_code_name[tn->type_code]);
		CLIB_DBG_FUNC_EXIT();
		return NULL;
	}
	BUG_ON(TREE_CODE(field) != FIELD_DECL);
	int macro_expanded = is_type_from_expand_macro(tn);

	slist_for_each_entry(tmp, &tn->children, sibling) {
		tree n1 = (tree)tmp->var.node;
		tree n2 = (tree)field;

		if (is_same_field(n1, n2, macro_expanded)) {
			CLIB_DBG_FUNC_EXIT();
			return tmp;
		}
	}

	slist_for_each_entry(tmp, &tn->children, sibling) {
		struct type_node *t = tmp->var.type;
		struct var_list *ret = NULL;

		if (!t)
			continue;

		if ((t != tn) &&
			((t->type_code == RECORD_TYPE) ||
			 (t->type_code == UNION_TYPE)))
			ret = get_target_field0(t, field);

		if (ret) {
			CLIB_DBG_FUNC_EXIT();
			return ret;
		}
	}

#if 0
	/* typedef struct xxx_s xxx */
	tree type_tree = (tree)tn->node;
	tree main_type = TYPE_CANONICAL(type_tree);
	struct type_node *tn_tmp;
	tn_tmp = find_type_node(main_type);
	if (tn_tmp == tn)
		BUG();
	tmp = get_target_field0(tn_tmp, field);
	BUG_ON(!tmp);
#endif

	CLIB_DBG_FUNC_EXIT();
	return NULL;
}

/*
 * return value:
 * -1: do nothing
 *  0: vnl is the target
 *  1: call calculate_field_offset again
 */
static int __check_field(struct var_list *vnl)
{
	CLIB_DBG_FUNC_ENTER();

	int ret = 0;
	switch (vnl->var.type->type_code) {
	case INTEGER_TYPE:
	case POINTER_TYPE:
	case ARRAY_TYPE:
	case ENUMERAL_TYPE:
	{
		ret = 0;
		break;
	}
	case RECORD_TYPE:
	{
		ret = 1;
		break;
	}
	case UNION_TYPE:
	{
		/* TODO: how to get the target field in UNION_TYPE */
		tree node;
		struct sibuf *b;
		expanded_location *xloc;

		node = (tree)vnl->var.node;
		b = find_target_sibuf(node);
		int held = analysis__sibuf_hold(b);
		xloc = get_location(GET_LOC_TYPE, b->payload, node);
		if (!held)
			analysis__sibuf_drop(b);
		si_log1_todo("UNION_TYPE at %s %d %d\n",
				xloc ? xloc->file : NULL,
				xloc ? xloc->line : 0,
				xloc ? xloc->column : 0);
		ret = 0;
		break;
	}
	default:
	{
		si_log1("miss %s\n", tree_code_name[vnl->var.type->type_code]);
		ret = -1;
		break;
	}
	}

	CLIB_DBG_FUNC_EXIT();
	return ret;
}

static struct var_list *calculate_field_offset(struct type_node *tn,
			unsigned long *prev_off, unsigned long *base_off,
			unsigned long target_offset)
{
	CLIB_DBG_FUNC_ENTER();

	struct var_list *prev_vnl = NULL;
	unsigned long off = 0;
	struct var_list *tmp;
	tree field = NULL;

	slist_for_each_entry(tmp, &tn->children, sibling) {
		struct sibuf *b;
		field = (tree)tmp->var.node;
		b = find_target_sibuf(field);
		int held = analysis__sibuf_hold(b);
		off = get_field_offset(field);
		if (!held)
			analysis__sibuf_drop(b);

		/*
		 * some structures contains other structures
		 * base_off is the child structure's offset in the base struct
		 * off is current field's offset
		 */
		if ((off + *base_off) == target_offset) {
			CLIB_DBG_FUNC_EXIT();
			return tmp;
		}

		if (((*prev_off + *base_off) < target_offset) &&
			((off + *base_off) > target_offset)) {
			switch (__check_field(prev_vnl)) {
			case 0:
			{
				CLIB_DBG_FUNC_EXIT();
				return prev_vnl;
			}
			case 1:
			{
				tree node;
				node = (tree)prev_vnl->var.node;
				*base_off += get_field_offset(node);
				*prev_off = 0;
				tn = prev_vnl->var.type;
				CLIB_DBG_FUNC_EXIT();
				return calculate_field_offset(tn, prev_off,
						       base_off,target_offset);
			}
			default:
			{
				break;
			}
			}
		}

		*prev_off = off;
		prev_vnl = tmp;

		/* check if the offset is already larger than target_offset */
		if ((*prev_off + *base_off) > target_offset)
			break;
	}

	if (slist_empty(&tn->children)) {
		si_log1_todo("tn has no children\n");
		CLIB_DBG_FUNC_EXIT();
		return NULL;
	}

	/* check the last field */
	tmp = slist_last_entry(&tn->children, struct var_list, sibling);
	field = (tree)tmp->var.node;
	off = get_field_offset(field);

	if (((off + *base_off) < target_offset) &&
		/* Fix: napi_struct_extended_rh structure is empty */
		tmp->var.type &&
		(off + *base_off +
		 tmp->var.type->ofsize * BITS_PER_UNIT) > target_offset) {
		switch (__check_field(tmp)) {
		case 0:
		{
			CLIB_DBG_FUNC_EXIT();
			return tmp;
		}
		case 1:
		{
			tree node;
			node = (tree)tmp->var.node;
			*base_off += get_field_offset(node);
			*prev_off = 0;
			tn = tmp->var.type;
			CLIB_DBG_FUNC_EXIT();
			return calculate_field_offset(tn, prev_off,
						      base_off, target_offset);
		}
		default:
		{
			break;
		}
		}
	}

	CLIB_DBG_FUNC_EXIT();
	return NULL;
}

static struct var_list *get_target_field1(struct type_node *tn,
					  unsigned long target_offset)
{
	if (!tn)
		return NULL;

	struct var_list *ret = NULL;
	unsigned long prev_off = 0;
	unsigned long base_off = 0;

	CLIB_DBG_FUNC_ENTER();

	ret = calculate_field_offset(tn, &prev_off, &base_off, target_offset);

	CLIB_DBG_FUNC_EXIT();
	return ret;

}

/*
 * this is a RECORD/UNION, we just get the field's var_list,
 * and add use_at
 */
static struct var_list *get_component_ref_vnl(tree op)
{
	CLIB_DBG_FUNC_ENTER();

	struct tree_exp *exp;
	exp = (struct tree_exp *)op;
	tree t0 = exp->operands[0];
	tree t1 = exp->operands[1];
	tree t2 = exp->operands[2];
	BUG_ON(t2);
	enum tree_code tc = TREE_CODE(t0);

	struct type_node *tn = NULL;
	switch (tc) {
	case COMPONENT_REF:
	{
		/*
		 * FIXME: if t0 is a COMPONENT_REF in MODE_GETSTEP4, we mark
		 * it too.
		 */
		if (mode == MODE_GETSTEP4) {
			__4_mark_component_ref(t0);
		}

		tn = find_type_node(TREE_TYPE(t0));
		break;
	}
	case MEM_REF:
	{
		if (mode == MODE_GETSTEP4) {
			__4_mark_mem_ref(t0);
		}

		tn = find_type_node(TREE_TYPE(t0));
		break;
	}
	case ARRAY_REF:
	{
		tn = get_array_ref_tn((struct tree_exp *)t0);
		break;
	}
	case VAR_DECL:
	case PARM_DECL:
	case RESULT_DECL:
	case INDIRECT_REF:
	{
		tn = find_type_node(TREE_TYPE(t0));
		break;
	}
	default:
	{
		enum gimple_code gc = gimple_code(cur_gimple);
		expanded_location *xloc;
		xloc = get_gimple_loc(cur_fsn->buf->payload,
					&cur_gimple->location);
		si_log1("%s in %s, loc: %s %d %d\n",
				tree_code_name[tc],
				gimple_code_name[gc],
				xloc ? xloc->file : NULL,
				xloc ? xloc->line : 0,
				xloc ? xloc->column : 0);
		break;
	}
	}

	if (!tn) {
		CLIB_DBG_FUNC_EXIT();
		return NULL;
	}

	struct var_list *target_vnl;
	target_vnl = get_target_field0(tn, t1);

	CLIB_DBG_FUNC_EXIT();
	return target_vnl;
}

static void __4_mark_component_ref(tree op)
{
	CLIB_DBG_FUNC_ENTER();

	struct var_list *target_vnl;

	target_vnl = get_component_ref_vnl(op);
	if (!target_vnl)
		goto out;

	if (target_vnl->var.type) {
		analysis__type_add_use_at(target_vnl->var.type,
					  cur_fsn->node_id.id,
					  SI_TYPE_DF_GIMPLE,
					  cur_gimple,
					  cur_gimple_op_idx);
	}

	analysis__var_add_use_at(&target_vnl->var, cur_fsn->node_id.id,
				 SI_TYPE_DF_GIMPLE,
				 cur_gimple, cur_gimple_op_idx);

	/* XXX: Do not add data_state here, the data is in other data state */
out:
	CLIB_DBG_FUNC_EXIT();
	return;
}

static void __4_mark_bit_field_ref(tree op)
{
	struct tree_exp *exp;
	tree t0;
	tree __maybe_unused t1;
	tree t2;
	struct type_node *tn = NULL;
	struct var_list __maybe_unused *vnl = NULL;
	struct var_list *target_vnl = NULL;
	gimple_seq next_gs;
	unsigned long target_offset = 0;

	CLIB_DBG_FUNC_ENTER();

	exp = (struct tree_exp *)op;
	t0 = exp->operands[0];
	t1 = exp->operands[1];	/* how many bits to read */
	t2 = exp->operands[2];	/* where to read */
	target_offset = TREE_INT_CST_LOW(t2);
	if (TREE_INT_CST_EXT_NUNITS(t2) > 1)
		si_log1_todo("EXT_NUNITS > 1\n");

	switch (TREE_CODE(t0)) {
	case MEM_REF:
	{
		tn = find_type_node(TREE_TYPE(t0));
		break;
	}
	case VAR_DECL:
	case PARM_DECL:
	{
		tn = find_type_node(TREE_TYPE(t0));
		break;
	}
	case COMPONENT_REF:
	{
		/*
		 * drivers/acpi/acpica/dscontrol.c
		 *	(*acpi_operand_object)->
		 */
		tn = find_type_node(TREE_TYPE(t0));
		break;
	}
	case ARRAY_REF:
	{
		tn = get_array_ref_tn((struct tree_exp *)t0);
		break;
	}
	case SSA_NAME:
	{
		/* v0.4-test */
		/*
		 * print_IO_APIC() in linux kernel
		 * the t0 is a INTEGER_TYPE, how to handle this situation??
		 * this may happen when some codes are:
		 *	union {
		 *		u32 dword;
		 *		struct {
		 *			...
		 *		};
		 *	} some_type;
		 */
		tn = find_type_node(TREE_TYPE(t0));
		if (tn && (tn->type_code == INTEGER_TYPE))
			tn = NULL;
		break;
	}
	default:
	{
		enum gimple_code gc;
		expanded_location *xloc;

		/*
		 * TODO: view_convert_expr in gimple_assign,
		 * loc: linux-5.4.0/drivers/iommu/intel_irq_remapping.c 176 46
		 */
		gc = gimple_code(cur_gimple);
		xloc = get_gimple_loc(cur_fsn->buf->payload,
					&cur_gimple->location);
		si_log1("%s in %s, loc: %s %d %d\n",
				tree_code_name[TREE_CODE(t0)],
				gimple_code_name[gc],
				xloc ? xloc->file : NULL,
				xloc ? xloc->line : 0,
				xloc ? xloc->column : 0);
		break;
	}
	}

	if (!tn)
		goto out;

	/*
	 * find out the var_node that represent this bit_field
	 * cur_gimple not enough to get the field
	 * peek next gimple
	 */
	next_gs = cur_gimple->next;
	switch (gimple_code(next_gs)) {
	case GIMPLE_ASSIGN:
	{
		tree *ops0 = gimple_ops(cur_gimple);
		tree *ops1 = gimple_ops(next_gs);

		tree lhs = ops0[0];
		int found = 0;
		for (unsigned int i = 1; i < gimple_num_ops(next_gs); i++) {
			if (ops1[i] == lhs) {
				found = 1;
				break;
			}
		}

		if (unlikely(!found)) {
			/*
			 * TODO, bit_field_0 == bit_field_1,
			 * four GIMPLE_ASSIGNs
			 */
			si_log1_todo("lhs not found\n");
			break;
		}

		enum tree_code next_gs_tc;
		tree *ops = gimple_ops(next_gs);
		int op_cnt = gimple_num_ops(next_gs);
		if (op_cnt == 2) {
			target_vnl = get_target_field1(tn, target_offset);
			break;
		}

		BUG_ON(op_cnt != 3);
		next_gs_tc = (enum tree_code)next_gs->subcode;
		switch (next_gs_tc) {
		case EQ_EXPR:
		case NE_EXPR:
		{
			/* no idea what to do here. */
			break;
		}
		case BIT_AND_EXPR:
		{
			if (unlikely((TREE_CODE(ops[2]) != INTEGER_CST) &&
				(TREE_CODE(ops[1]) != INTEGER_CST))) {
				si_log1_todo("miss %s %s\n",
					tree_code_name[TREE_CODE(ops[1])],
					tree_code_name[TREE_CODE(ops[2])]);
				break;
			}

			tree intcst;
			if (TREE_CODE(ops[1]) == INTEGER_CST)
				intcst = ops[1];
			else
				intcst = ops[2];

			unsigned long first_test_bit = 0;
			unsigned long andval;
			andval = TREE_INT_CST_LOW(intcst);
			if (TREE_INT_CST_EXT_NUNITS(intcst) > 1)
				si_log1_todo("EXT_NUNITS > 1\n");
			while (1) {
				if (andval &
				    (((unsigned long)1) << first_test_bit))
					break;
				first_test_bit++;
			}
			target_offset += first_test_bit;

			target_vnl = get_target_field1(tn, target_offset);

			break;
		}
		case BIT_XOR_EXPR:
		{
			si_log1_todo("recheck BIT_XOR_EXPR\n");
			break;
		}
		case RSHIFT_EXPR:
		{
			si_log1_todo("recheck RSHIFT_EXPR\n");
			break;
		}
		default:
		{
			char loc[1024];
			gimple_loc_string(loc, 1024, cur_gimple);
			si_log1("miss %s, loc: %s\n",
					tree_code_name[next_gs_tc],
					loc);
			break;
		}
		}
		break;
	}
	case GIMPLE_COND:
	{
		/* FIXME: no idea what to do here */
		break;
	}
	case GIMPLE_DEBUG:
	{
		/* nothing to do here */
		break;
	}
	default:
	{
		char loc[1024];
		gimple_loc_string(loc, 1024, cur_gimple);
		si_log1("miss %s, loc: %s\n",
				gimple_code_name[gimple_code(next_gs)],
				loc);
		break;
	}
	}

	if (unlikely(!target_vnl)) {
		si_log1("recheck target_vnl == NULL\n");
		goto out;
	}

	if (target_vnl->var.type) {
		analysis__type_add_use_at(target_vnl->var.type,
					  cur_fsn->node_id.id,
					  SI_TYPE_DF_GIMPLE,
					  cur_gimple,
					  cur_gimple_op_idx);
	}

	analysis__var_add_use_at(&target_vnl->var, cur_fsn->node_id.id,
				 SI_TYPE_DF_GIMPLE,
				 cur_gimple, cur_gimple_op_idx);

	/* XXX: like COMPONENT_REF, do not add data_state here */

out:
	CLIB_DBG_FUNC_EXIT();
	return;
}

static void __4_mark_addr_expr(tree op)
{
	struct tree_exp *exp;
	tree op0;

	CLIB_DBG_FUNC_ENTER();

	exp = (struct tree_exp *)op;
	op0 = exp->operands[0];
	switch (TREE_CODE(op0)) {
	case COMPONENT_REF:
	{
		__4_mark_component_ref(op0);
		break;
	}
	case ARRAY_REF:
	{
		__4_mark_array_ref(op0);
		break;
	}
	case MEM_REF:
	{
		__4_mark_mem_ref(op0);
		break;
	}
	case LABEL_DECL:
	{
		/* TODO: miss gimple_call? */
		if (gimple_code(cur_gimple) != GIMPLE_ASSIGN)
			si_log1_todo("miss %s\n",
				gimple_code_name[gimple_code(cur_gimple)]);
		break;
	}
	case RESULT_DECL:
	case VAR_DECL:
	case PARM_DECL:
	case FUNCTION_DECL:
	case STRING_CST:
	{
		break;
	}
	default:
	{
		expanded_location *xloc;
		xloc = get_gimple_loc(cur_fsn->buf->payload,
					&cur_gimple->location);
		si_log1_todo("miss %s, loc: %s %d %d\n",
				tree_code_name[TREE_CODE(op0)],
				xloc ? xloc->file : NULL,
				xloc ? xloc->line : 0,
				xloc ? xloc->column : 0);
		break;
	}
	}

	CLIB_DBG_FUNC_EXIT();
	return;
}

/*
 * check MEM_REF in dump_generic_node() in tree-pretty-print.c
 */
static void __4_mark_mem_ref(tree op)
{
	CLIB_DBG_FUNC_ENTER();

	tree t0 = TREE_OPERAND(op, 0);
	tree t1 = TREE_OPERAND(op, 1);
	struct type_node *tn;
	tree type;

#if 0
	if (integer_zerop(t1) &&
		(TREE_CODE(t0) != INTEGER_CST) &&
		(TREE_TYPE(t0) != NULL_TREE) &&
		(TREE_TYPE(TREE_TYPE(t0)) == TREE_TYPE(TREE_TYPE(t1))) &&
		(TYPE_MODE(TREE_TYPE(t0)) == TYPE_MODE(TREE_TYPE(t1))) &&
		(TYPE_REF_CAN_ALIAS_ALL(TREE_TYPE(t0)) ==
		 TYPE_REF_CAN_ALIAS_ALL(TREE_TYPE(t1))) &&
		(TYPE_MAIN_VARIANT(TREE_TYPE(op)) ==
		 TYPE_MAIN_VARIANT(TREE_TYPE(TREE_TYPE(t1))))) {
		/* ignore MR_DEPENDENCE_CLIQUE() */
		if (TREE_CODE(t0) != ADDR_EXPR) {
			/* handle t0, and ignore t1 */
		} else {
			/* TREE_OPERAND(t0, 0), handled later in do_tree() */
		}

		goto out;
	}
#endif

	/*
	 * it seems the type of MEM_REF is from
	 *	TYPE_MAIN_VARIANT(TREE_TYPE(TREE_OPERAND(op, 1)));
	 */
	switch (TREE_CODE(t0)) {
	case ADDR_EXPR:
	{
		__4_mark_addr_expr(t0);
		break;
	}
	case SSA_NAME:
	{
		break;
	}
	case PARM_DECL:
	case VAR_DECL:
	{
		break;
	}
	case INTEGER_CST:
	{
		/*
		 * FIXME: in linux kernel, native_apic_mem_read(u32 reg).
		 * return *((volatile u32 *)(APIC_BASE + reg));
		 *
		 * The reg is definite an address, but cast to u32.
		 */
		break;
	}
	default:
	{
		char loc[1024];
		gimple_loc_string(loc, 1024, cur_gimple);
		si_log1_todo("miss %s, loc: %s\n",
				tree_code_name[TREE_CODE(t0)],
				loc);
		break;
	}
	}

	type = TREE_TYPE(t0);
	if (type && (TREE_CODE(type) == POINTER_TYPE)) {
		type = TREE_TYPE(type);
	}

	if (type && ((TREE_CODE(type) == RECORD_TYPE) ||
			(TREE_CODE(type) == UNION_TYPE)) &&
		    t1 && (TREE_CODE(t1) == INTEGER_CST)) {
		unsigned long offset = TREE_INT_CST_LOW(t1) * BITS_PER_UNIT;
		if (TREE_INT_CST_EXT_NUNITS(t1) > 1)
			si_log1_todo("EXT_NUNITS > 1\n");
		tn = find_type_node(type);
		if (!tn) {
			si_log1_todo("tn not found\n");
			goto out;
		}

		struct var_list *vl;
		vl = get_target_field1(tn, offset);
		if (!vl) {
			si_log1_todo("vl not found\n");
			goto out;
		}

		analysis__var_add_use_at(&vl->var, cur_fsn->node_id.id,
					 SI_TYPE_DF_GIMPLE,
					 cur_gimple, cur_gimple_op_idx);

		if (vl->var.type) {
			analysis__type_add_use_at(vl->var.type,
						  cur_fsn->node_id.id,
						  SI_TYPE_DF_GIMPLE,
						  cur_gimple,
						  cur_gimple_op_idx);
		}

		/* XXX: like BIT_FIELD_REF, do not add data state */
	}

out:
	CLIB_DBG_FUNC_EXIT();
	return;
}

static void __4_mark_array_ref(tree op)
{
	struct tree_exp *exp;
	struct type_node *tn;

	CLIB_DBG_FUNC_ENTER();

	exp = (struct tree_exp *)op;
	tn = get_array_ref_tn(exp);
	if (!tn)
		goto out;

	analysis__type_add_use_at(tn, cur_fsn->node_id.id,
				  SI_TYPE_DF_GIMPLE,
				  cur_gimple, cur_gimple_op_idx);

out:
	CLIB_DBG_FUNC_EXIT();
	return;
}

static void __4_mark_gimple(gimple_seq gs)
{
	enum gimple_code gc = gimple_code(gs);
	if ((gc == GIMPLE_DEBUG) || (gc == GIMPLE_NOP)) {
		return;
	} else if (gc == GIMPLE_LABEL) {
#if 0
		expanded_location *xloc;
		xloc = get_gimple_loc(cur_fsn->buf->payload, &gs->location);
		si_log1("GIMPLE_LABEL at %s %d %d\n",
				xloc->file, xloc->line, xloc->column);
#endif
		return;
	}

	CLIB_DBG_FUNC_ENTER();

	for (size_t i = 0; i < obj_cnt; i++)
		phase4_obj_checked[i] = NULL;

	tree *ops = gimple_ops(gs);
	for (unsigned i = 0; i < gimple_num_ops(gs); i++) {
		if (!ops[i])
			continue;

		cur_gimple_op_idx = i;
		phase4_obj_idx = 0;
		for (size_t i = 0; i < obj_cnt; i++)
			phase4_obj_checked[i] = NULL;

		/*
		 * before v0.6.2, we handle variables and functions in do_tree,
		 * then, what else should be marked too?
		 *	: FIELD_DECL in RECORD_TYPE / UNION_TYPE
		 * thus, we don't need __4_mark_gimple_op() any longer, do all
		 * in do_tree().
		 */
		do_tree(ops[i]);
	}

	/* need to handle GIMPLE_PHIs */
	if (gc == GIMPLE_PHI) {
		unsigned nargs = gimple_phi_num_args(gs);
		for (unsigned i = 0; i < nargs; i++) {
			tree def;
			def = gimple_phi_arg_def(gs, i);
			do_tree(def);
		}
		do_tree(gimple_phi_result(gs));
		/* TODO: ssa_use_operand_t not handled */
	}

	CLIB_DBG_FUNC_EXIT();
}

static void __4_mark_var_func(struct sinode *sn)
{
	CLIB_DBG_FUNC_ENTER();

	struct func_node *fn;
	fn = (struct func_node *)sn->data;

	int held = analysis__sibuf_hold(sn->buf);

	cur_fsn = sn;
	cur_fn = fn;
	si_current_function_decl = (tree)(long)sn->obj->real_addr;

	BUG_ON(!si_function_bb(si_current_function_decl));

	basic_block bb;
	FOR_EACH_BB_FN(bb, DECL_STRUCT_FUNCTION(si_current_function_decl)) {
		gimple_seq gs;
		gs = bb->il.gimple.phi_nodes;
		while (gs) {
			cur_gimple = gs;
			__4_mark_gimple(gs);
			gs = gs->next;
		}

		gs = bb->il.gimple.seq;
		while (gs) {
			cur_gimple = gs;
			__4_mark_gimple(gs);
			gs = gs->next;
		}
	}

	if (!held)
		analysis__sibuf_drop(sn->buf);

	CLIB_DBG_FUNC_EXIT();
	return;
}

/* we only mark the functions variables use position */
static void do_phase4_func(struct sinode *sn)
{
	CLIB_DBG_FUNC_ENTER();

	__4_mark_var_func(sn);

	CLIB_DBG_FUNC_EXIT();
}

static void do_phase4(struct sibuf *b)
{
	CLIB_DBG_FUNC_ENTER();

	int held = analysis__sibuf_hold(b);

	for (obj_idx = 0; obj_idx < real_obj_cnt; obj_idx++) {

		if (objs[obj_idx].is_dropped || objs[obj_idx].is_replaced)
			continue;

		void *obj_addr;
		obj_addr = (void *)(long)(objs[obj_idx].real_addr);

		struct sinode *n;
		if (objs[obj_idx].is_global_var) {
			get_var_sinode((tree)obj_addr, &n, 0);
			if (!n)
				continue;

			/* init possible value */
			do_phase4_gvar(n);
		} else if (objs[obj_idx].is_function) {
			get_func_sinode((tree)obj_addr, &n, 0);
			if ((!n) || (!n->data))
				continue;

			do_phase4_func(n);
		} else {
			continue;
		}
	}

	if (!held)
		analysis__sibuf_drop(b);

	CLIB_DBG_FUNC_EXIT();
}

/*
 * ************************************************************************
 * phase 5
 * ************************************************************************
 */
/* NULL: the target var is extern, not defined in this source files */
static struct var_node *get_target_var_node(struct sinode *fsn, tree node)
{
	CLIB_DBG_FUNC_ENTER();

	BUG_ON(TREE_CODE(node) != VAR_DECL);
	struct func_node *fn;
	fn = (struct func_node *)fsn->data;

	struct var_list *tmp;
	slist_for_each_entry(tmp, &fn->local_vars, sibling) {
		if (tmp->var.node == (void *)node) {
			CLIB_DBG_FUNC_EXIT();
			return &tmp->var;
		}
	}

	struct sinode *target_vsn = NULL;
	get_var_sinode(node, &target_vsn, 1);
	if (!target_vsn) {
		CLIB_DBG_FUNC_EXIT();
		return NULL;
	}

	struct id_list *t;
	slist_for_each_entry(t, &fn->global_vars, sibling) {
		struct var_node *vn;
		if (t->value_flag)
			continue;

		union siid *tid = (union siid *)&t->value;
		if (tid->id1 != target_vsn->node_id.id.id1)
			continue;

		vn = (struct var_node *)target_vsn->data;
		CLIB_DBG_FUNC_EXIT();
		return vn;
	}

	CLIB_DBG_FUNC_EXIT();
	return NULL;
}

static struct var_node *get_target_parm_node(struct sinode *fsn, tree node)
{
	CLIB_DBG_FUNC_ENTER();

	BUG_ON(TREE_CODE(node) != PARM_DECL);
	struct func_node *fn = (struct func_node *)fsn->data;

	struct var_list *tmp;
	slist_for_each_entry(tmp, &fn->args, sibling) {
		if (tmp->var.node == (void *)node) {
			CLIB_DBG_FUNC_EXIT();
			return &tmp->var;
		}
	}

	BUG();
}

static void __func_assigned(struct sinode *n, struct sinode *fsn, tree op)
{
	CLIB_DBG_FUNC_ENTER();

	int held = analysis__sibuf_hold(fsn->buf);

	enum tree_code tc = TREE_CODE(op);
	switch (tc) {
	case VAR_DECL:
	{
		struct var_node *vn;
		vn = get_target_var_node(fsn, op);
		if (!vn)
			break;

		analysis__add_possible(vn, VALUE_IS_FUNC, n->node_id.id.id1, 0);
		break;
	}
	case PARM_DECL:
	{
		struct var_node *vn;
		vn = get_target_parm_node(fsn, op);
		if (!vn)
			break;

		analysis__add_possible(vn, VALUE_IS_FUNC, n->node_id.id.id1, 0);
		break;
	}
	case COMPONENT_REF:
	{
		struct var_list *vnl;
		vnl = get_component_ref_vnl(op);
		if (!vnl)
			break;

		analysis__add_possible(&vnl->var, VALUE_IS_FUNC,
					n->node_id.id.id1, 0);
		break;
	}
	case ARRAY_REF:
	case MEM_REF:
	{
		struct tree_exp *exp = (struct tree_exp *)op;
		tree op0 = exp->operands[0];
		__func_assigned(n, fsn, op0);
		break;
	}
	case SSA_NAME:
	{
		/* testcase-1.c */
		/* FIXME: do nothing here */
		break;
	}
	default:
	{
		/* TODO: miss ADDR_EXPR */
		si_log1("miss %s\n", tree_code_name[tc]);
		break;
	}
	}

	if (!held)
		analysis__sibuf_drop(fsn->buf);

	CLIB_DBG_FUNC_EXIT();
	return;
}

static void __do_func_used_at(struct sinode *sn, struct func_node *fn)
{
	CLIB_DBG_FUNC_ENTER();

	struct use_at_list *tmp_ua;
	slist_for_each_entry(tmp_ua, &fn->used_at, sibling) {
		struct sinode *fsn;
		fsn = analysis__sinode_search(siid_type(&tmp_ua->func_id),
						SEARCH_BY_ID,
						&tmp_ua->func_id);
		BUG_ON(!fsn);

		if (tmp_ua->type == SI_TYPE_DF_GIMPLE) {
			gimple_seq gs = (gimple_seq)tmp_ua->where;
			int held = analysis__sibuf_hold(fsn->buf);
			enum gimple_code gc = gimple_code(gs);
			if (gc == GIMPLE_ASSIGN) {
				BUG_ON(tmp_ua->extra_info == 0);
				if (unlikely(gimple_num_ops(gs) != 2)) {
					if (!held)
						analysis__sibuf_drop(fsn->buf);
					continue;
				}
				tree *ops = gimple_ops(gs);
				tree lhs = ops[0];
				__func_assigned(sn, fsn, lhs);
			} else if (gc == GIMPLE_CALL) {
				/* do nothing here */;
			} else if (gc == GIMPLE_COND) {
				/* FIXME: we do nothing here */
			} else if (gc == GIMPLE_ASM) {
				/* TODO */
				si_log1_todo("GIMPLE_ASM in phase5\n");
			} else {
				/* TODO: miss gimple_phi */
				si_log1("miss %s\n", gimple_code_name[gc]);
			}
			if (!held)
				analysis__sibuf_drop(fsn->buf);
		}
	}

	CLIB_DBG_FUNC_EXIT();
	return;
}

static void callee_alias_add_caller(struct sinode *callee,
				    struct sinode *caller)
{
	CLIB_DBG_FUNC_ENTER();

	struct func_node *callee_fn;
	callee_fn = (struct func_node *)callee->data;
	BUG_ON(callee_fn);

	/* XXX, check if this function has an alias attribute */
	struct attr_list *tmp;
	int found = 0;
	int no_ins = 0;
	slist_for_each_entry(tmp, &callee->attributes, sibling) {
		if (!strcmp(tmp->attr_name, "alias")) {
			found = 1;
			break;
		} else if (!strcmp(tmp->attr_name, "no_instrument_function")) {
			no_ins = 1;
		}
	}

	if (!found) {
		CLIB_DBG_FUNC_EXIT();
		if (no_ins)
			return;
		return;
	}

	char name[NAME_MAX];
	struct attrval_list *tmp2;
	slist_for_each_entry(tmp2, &tmp->values, sibling) {
		memset(name, 0, NAME_MAX);
		tree node = (tree)tmp2->node;
		if (!node)
			continue;

		struct sibuf *b = find_target_sibuf(node);
		int held = analysis__sibuf_hold(b);
		if (TREE_CODE(node) == IDENTIFIER_NODE) {
			get_node_name(node, name);
		} else if (TREE_CODE(node) == STRING_CST) {
			struct tree_string *tstr;
			tstr = (struct tree_string *)node;
			BUG_ON(tstr->length >= NAME_MAX);
			memcpy(name, tstr->str, tstr->length);
		} else {
			BUG();
		}
		if (!held)
			analysis__sibuf_drop(b);

		/* FIXME, a static function? */
		struct sinode *new_callee;
		long args[3];
		args[0] = (long)b->rf;
		args[1] = (long)get_builtin_resfile();
		args[2] = (long)name;
		new_callee = analysis__sinode_search(TYPE_FUNC_GLOBAL,
							SEARCH_BY_SPEC,
							(void *)args);
		analysis__add_caller(new_callee, caller,
					callee_alias_add_caller);
	}

	CLIB_DBG_FUNC_EXIT();
	return;
}

static void call_func_decl(struct sinode *sn, struct func_node *fn,
			gimple_seq gs, tree n)
{
	CLIB_DBG_FUNC_ENTER();

	struct sinode *call_fn_sn = NULL;
	long value;
	long val_flag;

	get_func_sinode(n, &call_fn_sn, 1);
	if (!call_fn_sn) {
		value = (long)n;
		val_flag = 1;
	} else {
		value = call_fn_sn->node_id.id.id1;
		val_flag = 0;
	}

	/* this piece of code is quite similar to add_callee */
	struct callf_list *newc;
	node_lock_w(fn);
	newc = __add_call(&fn->callees, value, val_flag, val_flag);
	callf_stmt_list_add(&newc->stmts, SI_TYPE_DF_GIMPLE, (void *)gs);
	node_unlock_w(fn);

	/* FIXME, what if call_fn_sn has no data? */
	if (!val_flag)
		analysis__add_caller(call_fn_sn, sn, callee_alias_add_caller);

	CLIB_DBG_FUNC_EXIT();
	return;
}

static void add_possible_callee(struct sinode *caller_fsn,
				gimple_seq gs,
				struct slist_head *head)
{
	CLIB_DBG_FUNC_ENTER();

	struct possible_list *tmp_pv;
	slist_for_each_entry(tmp_pv, head, sibling) {
		if (tmp_pv->value_flag != VALUE_IS_FUNC)
			continue;
		switch (tmp_pv->value_flag) {
		case VALUE_IS_FUNC:
		{
			union siid *id;
			struct sinode *callee_fsn;

			id = (union siid *)&tmp_pv->value;
			callee_fsn = analysis__sinode_search(siid_type(id),
							SEARCH_BY_ID, id);

			BUG_ON(!callee_fsn);
			analysis__add_callee(caller_fsn, callee_fsn, gs,
						SI_TYPE_DF_GIMPLE);
			analysis__add_caller(callee_fsn, caller_fsn,
						callee_alias_add_caller);
			break;
		}
		default:
		{
			si_log1_todo("miss %d\n", tmp_pv->value_flag);
			break;
		}
		}
	}

	CLIB_DBG_FUNC_EXIT();
	return;
}

static void call_parm_decl(struct sinode *fsn, struct func_node *fn,
				gimple_seq gs, tree parm)
{
	if (mode != MODE_GETINDCFG2)
		return;

	CLIB_DBG_FUNC_ENTER();

	si_log1_todo("parm call happened in %s\n", fsn->name);

	CLIB_DBG_FUNC_EXIT();
	return;
}

static void call_var_decl(struct sinode *fsn, struct func_node *fn,
			gimple_seq gs, tree var)
{
	CLIB_DBG_FUNC_ENTER();

	struct var_node *vn = NULL;
	vn = get_target_var_node(fsn, var);

	if (vn) {
		add_possible_callee(fsn, gs, &vn->possible_values);
	}

	CLIB_DBG_FUNC_EXIT();
	return;
}

static void call_ssaname_gassign_component_ref(struct sinode *sn,
				struct func_node *fn,
				gimple_seq orig_gs,
				tree ref_op)
{
	CLIB_DBG_FUNC_ENTER();

	struct var_list *vnl;

	vnl = get_component_ref_vnl(ref_op);
	if (!vnl) {
		si_log1_todo("target vnl not found\n");
		goto out;
	}

	add_possible_callee(sn, orig_gs, &vnl->var.possible_values);

out:
	CLIB_DBG_FUNC_EXIT();
	return;
}

static void call_addr_expr(struct sinode *sn, struct func_node *fn,
			gimple_seq gs, tree call_op);
static void call_ssaname_gassign(struct sinode *sn,
				struct func_node *fn,
				gimple_seq orig_gs,
				gimple_seq def_stmt)
{
	struct gassign *ga;
	enum tree_code tc;

	ga = (struct gassign *)def_stmt;
	tc = gimple_assign_rhs_code(ga);

	CLIB_DBG_FUNC_ENTER();

	/* ignore the lhs, which is op[0] */
	switch (tc) {
	case COMPONENT_REF:
	{
		BUG_ON(TREE_CODE(gimple_assign_rhs1(ga)) != COMPONENT_REF);
		call_ssaname_gassign_component_ref(sn, fn, orig_gs,
					gimple_assign_rhs1(ga));
		break;
	}
	case NOP_EXPR:
	{
		break;
	}
	case VAR_DECL:
	{
		/*
		 * machine_check_poll() in linux kernel
		 */
		BUG_ON(TREE_CODE(gimple_assign_rhs1(ga)) != VAR_DECL);
		call_var_decl(sn, fn, orig_gs, gimple_assign_rhs1(ga));
		break;
	}
	case ADDR_EXPR:
	{
		/* testcase-1.c */
		BUG_ON(TREE_CODE(gimple_assign_rhs1(ga)) != ADDR_EXPR);
		call_addr_expr(sn, fn, orig_gs, gimple_assign_rhs1(ga));
		break;
	}
	case MEM_REF:
	{
		si_log1_todo("MEM_REF\n");
		break;
	}
	default:
	{
		/* TODO: miss ARRAY_REF/SSA_NAME */
		si_log1_todo("miss subcode %s\n", tree_code_name[tc]);
		break;
	}
	}

	CLIB_DBG_FUNC_EXIT();
	return;
}

#define	SSANAME_DEPTH	256
static __thread tree ssaname_node[SSANAME_DEPTH];
static __thread int ssaname_node_idx = 0;
static int ssaname_node_check(tree ssa)
{
	BUG_ON(ssaname_node_idx >= (SSANAME_DEPTH-1));
	for (int i = 0; i < ssaname_node_idx; i++) {
		if (ssaname_node[i] == ssa)
			return 1;
	}
	return 0;
}

static void ssaname_node_push(tree ssa)
{
	ssaname_node[ssaname_node_idx] = ssa;
	ssaname_node_idx++;
}

static void ssaname_node_pop(void)
{
	BUG_ON(!ssaname_node_idx);
	ssaname_node_idx--;
}

static void call_ssaname(struct sinode *sn, struct func_node *fn,
				gimple_seq gs, tree call_op, int flag);
static void call_ssaname_gphi_op(struct sinode *sn, struct func_node *fn,
				gimple_seq orig_gs, tree phi_op)
{
	CLIB_DBG_FUNC_ENTER();

	switch (TREE_CODE(phi_op)) {
	case SSA_NAME:
	{
		call_ssaname(sn, fn, orig_gs, phi_op, 1);
		break;
	}
	case INTEGER_CST:
	{
		break;
	}
	default:
	{
		/* TODO: miss ADDR_EXPR */
		si_log1_todo("miss %s\n", tree_code_name[TREE_CODE(phi_op)]);
		break;
	}
	}

	CLIB_DBG_FUNC_EXIT();
	return;
}

static void call_ssaname_gphi(struct sinode *sn, struct func_node *fn,
				gimple_seq orig_gs, gimple_seq def_stmt)
{
	/*
	 * check dump_gimple_phi() in gcc/gimple-pretty-print.c
	 */
	CLIB_DBG_FUNC_ENTER();

	size_t i;
	for (i = 0; i < gimple_phi_num_args(def_stmt); i++) {
		call_ssaname_gphi_op(sn, fn, orig_gs,
					gimple_phi_arg_def(def_stmt, i));
	}

	CLIB_DBG_FUNC_EXIT();
	return;
}

static void call_ssaname(struct sinode *sn, struct func_node *fn,
			gimple_seq gs, tree call_op, int flag)
{
	CLIB_DBG_FUNC_ENTER();

	gimple_seq def_gs;
	tree var;

	if (!flag) {
		ssaname_node_idx = 0;
	} else {
		if (ssaname_node_check(call_op))
			goto out;
		ssaname_node_push(call_op);
	}

	def_gs = SSA_NAME_DEF_STMT(call_op);
	if ((!def_gs) || (gimple_code(def_gs) == GIMPLE_NOP))
		goto ssa_var;

	switch (gimple_code(def_gs)) {
	case GIMPLE_ASSIGN:
	{
		call_ssaname_gassign(sn, fn, gs, def_gs);
		break;
	}
	case GIMPLE_PHI:
	{
		/*
		 * this might happen when the code does this:
		 *	void (*cb)(int);
		 *	if (...)
		 *		cb = cb1;
		 *	else
		 *		cb = cb2;
		 *	cb(1);		=> PHI node
		 */
		call_ssaname_gphi(sn, fn, gs, def_gs);
		break;
	}
	default:
	{
		/* TODO: miss GIMPLE_CALL */
		si_log1_todo("miss %s\n",
			     gimple_code_name[gimple_code(def_gs)]);
		break;
	}
	}

ssa_var:
	var = SSA_NAME_VAR(call_op);
	if (!var)
		goto out;

	switch (TREE_CODE(var)) {
	case PARM_DECL:
	{
		call_parm_decl(sn, fn, gs, var);
		break;
	}
	case VAR_DECL:
	{
		call_var_decl(sn, fn, gs, var);
		break;
	}
	default:
	{
		si_log1_todo("miss %s\n", tree_code_name[TREE_CODE(var)]);
		break;
	}
	}

	if (flag) {
		BUG_ON(ssaname_node[ssaname_node_idx-1] != call_op);
		ssaname_node_pop();
	}

out:
	CLIB_DBG_FUNC_EXIT();
	return;
}

/*
 * TODO: check gimple_call_addr_fndecl() in gcc/gimple-expr.h
 */
static void call_addr_expr(struct sinode *sn, struct func_node *fn,
			gimple_seq gs, tree call_op)
{
	CLIB_DBG_FUNC_ENTER();

#if 0
	struct tree_exp *cfn = NULL;
	tree op0;

	cfn = (struct tree_exp *)call_op;
	if (!cfn->operands[0])
		goto out;
	op0 = cfn->operands[0];
#endif
	tree op0 = gimple_call_addr_fndecl(call_op);
	if (!op0)
		goto out;

	switch (TREE_CODE(op0)) {
	case FUNCTION_DECL:
	{
		call_func_decl(sn, fn, gs, op0);
		break;
	}
	default:
	{
		si_log1_todo("miss %s\n", tree_code_name[TREE_CODE(op0)]);
		break;
	}
	}

out:
	CLIB_DBG_FUNC_EXIT();
	return;
}

static void __trace_call_gs(struct sinode *sn, struct func_node *fn,
			    gimple_seq gs)
{
	enum gimple_code gc = gimple_code(gs);
	struct gcall *g = NULL;
	tree call_op = NULL;
	if (gc != GIMPLE_CALL)
		return;

	CLIB_DBG_FUNC_ENTER();

	g = (struct gcall *)gs;
	if (gimple_call_internal_p(g)) {
		struct callf_list *newc;
		node_lock_w(fn);
		newc = __add_call(&fn->callees, (long)g->u.internal_fn, 0, 1);
		callf_stmt_list_add(&newc->stmts, SI_TYPE_DF_GIMPLE, gs);
		node_unlock_w(fn);
		CLIB_DBG_FUNC_EXIT();
		return;
	}

	call_op = gimple_call_fn(g);
	BUG_ON(!call_op);
	switch (TREE_CODE(call_op)) {
	case ADDR_EXPR:
	{
		call_addr_expr(sn, fn, gs, call_op);
		break;
	}
	case PARM_DECL:
	{
		call_parm_decl(sn, fn, gs, call_op);
		break;
	}
	case VAR_DECL:
	{
		call_var_decl(sn, fn, gs, call_op);
		break;
	}
	case SSA_NAME:
	{
		call_ssaname(sn, fn, gs, call_op, 0);
		break;
	}
	default:
	{
		si_log1_todo("miss %s\n", tree_code_name[TREE_CODE(call_op)]);
		break;
	}
	}

	CLIB_DBG_FUNC_EXIT();
	return;
}

/*
 * just get the obviously one function it calls.
 * The PARM_DECL/VAR_DECL are traced later
 */
static void __do_trace_call(struct sinode *sn, struct func_node *fn)
{
	CLIB_DBG_FUNC_ENTER();

	cur_fsn = sn;
	cur_fn = fn;
	int held = analysis__sibuf_hold(sn->buf);
	si_current_function_decl = (tree)(long)sn->obj->real_addr;

	basic_block bb;
	FOR_EACH_BB_FN(bb, DECL_STRUCT_FUNCTION(si_current_function_decl)) {
		gimple_seq next_gs;
		next_gs = bb->il.gimple.phi_nodes;
		for (; next_gs; next_gs = next_gs->next) {
			cur_gimple = next_gs;
			__trace_call_gs(sn, fn, next_gs);
		}

		next_gs = bb->il.gimple.seq;
		for (; next_gs; next_gs = next_gs->next) {
			cur_gimple = next_gs;
			__trace_call_gs(sn, fn, next_gs);
		}
	}

	if (!held)
		analysis__sibuf_drop(sn->buf);
	CLIB_DBG_FUNC_EXIT();
	return;
}

static void do_phase5_func(struct sinode *n)
{
	CLIB_DBG_FUNC_ENTER();

	struct func_node *fn;

	fn = (struct func_node *)n->data;

	__do_func_used_at(n, fn);
	__do_trace_call(n, fn);

	CLIB_DBG_FUNC_EXIT();
	return;
}

static void do_phase5(struct sibuf *b)
{
	CLIB_DBG_FUNC_ENTER();

	int held = analysis__sibuf_hold(b);

	for (obj_idx = 0; obj_idx < obj_cnt; obj_idx++) {

		if (objs[obj_idx].is_dropped || objs[obj_idx].is_replaced)
			continue;

		void *obj_addr = (void *)(long)(objs[obj_idx].real_addr);
		if (!objs[obj_idx].is_function) {
			continue;
		}

		struct sinode *n;
		get_func_sinode((tree)obj_addr, &n, 0);
		if (!n)
			continue;

		if (!n->data)
			continue;

		do_phase5_func(n);
	}

	if (!held)
		analysis__sibuf_drop(b);

	CLIB_DBG_FUNC_EXIT();
	return;
}

/*
 * ************************************************************************
 * phase 6
 * ************************************************************************
 */
static void do_phase6(struct sibuf *b)
{
	do_phase5(b);
	return;
}

static __thread long show_progress_arg[0x10];
static int show_progress_timeout = 3;
static void c_show_progress(int signo, siginfo_t *si, void *arg, int last)
{
	long *args = (long *)arg;
	char __maybe_unused *status = (char *)args[0];
	char *parsing_file = (char *)args[1];
	long cur = *(long *)args[2];
	long goal = *(long *)args[3];
	pthread_t id = (pthread_t)args[4];

	long percent = (cur) * 100 / (goal);
#if 0
	char *buf;
	buf = clib_ap_start("%s %s... %.3f%%\n", status,
				parsing_file, percent);
	mt_print0(id, buf);
	clib_ap_end(buf);
#else
	mt_print1(id, "0x%lx %s (%ld%%)\n", id, parsing_file, percent);
#endif
}

static void do_gcc_global_var_adjust(void)
{
	CLIB_DBG_FUNC_ENTER();

	size_t i = 0;
	for (i = 0; i < obj_cnt; i++) {
		if (!objs[i].gcc_global_varidx)
			continue;

		if (!objs[i].fake_addr) {
			/* gcc global var is NULL */
			objs[i].real_addr = 0;
			objs[i].is_adjusted = 1;
			obj_adjusted++;
		} else if (!objs[i].size) {
			/* gcc global var should be adjusted */
			void *this_fake;
			this_fake = (void *)(unsigned long)objs[i].fake_addr;
			int do_next;
			get_real_addr_1(&this_fake, &do_next);
			BUG_ON(!this_fake);

			objs[i].real_addr = (unsigned long)this_fake;
			objs[i].is_adjusted = 1;
			obj_adjusted++;
		} else {
			size_t old_adjusted = obj_adjusted;
			do_tree((tree)(unsigned long)objs[i].real_addr);
			BUG_ON(old_adjusted >= obj_adjusted);
		}
	}

	CLIB_DBG_FUNC_EXIT();
	return;
}

static void *get_global(struct sibuf *b, const char *string, int *len)
{
	int held = analysis__sibuf_hold(b);

	void *ret = NULL;
	tree *t0 = (tree *)b->globals;
	tree *t1 = (tree *)(&t0[TI_MAX]);
	tree *t2 = (tree *)(&t1[itk_none]);

	if (len)
		*len = 0;

	if (!strcmp(string, "global_trees")) {
		if (len)
			*len = TI_MAX;
		ret = (void *)t0;
	} else if (!strcmp(string, "integer_types")) {
		if (len)
			*len = itk_none;
		ret = (void *)t1;
	} else if (!strcmp(string, "sizetype_tab")) {
		if (len)
			*len = stk_type_kind_last;
		ret = (void *)t2;
	}

	if (!held)
		analysis__sibuf_drop(b);
	return ret;
}

static void setup_gcc_globals(struct sibuf *buf)
{
	CLIB_DBG_FUNC_ENTER();

	size_t len = 0;
	len += TI_MAX * sizeof(tree);
	len += itk_none * sizeof(tree);
	len += stk_type_kind_last * sizeof(tree);

	buf->globals = (void *)src_buf_get(len);

	int held = analysis__sibuf_hold(buf);

	size_t ggv_idx = 0;
	for (ggv_idx = 0; ggv_idx < obj_cnt; ggv_idx++) {
		if (objs[ggv_idx].gcc_global_varidx)
			break;
	}
	BUG_ON(ggv_idx == obj_cnt);

	tree *ptr = NULL;
	for (int i = 0; i < TI_MAX; i++) {
		ptr = (tree *)get_global(buf, "global_trees", NULL);
		ptr[i] = (tree)(unsigned long)objs[ggv_idx].real_addr;
		ggv_idx++;
	}
	BUG_ON(ggv_idx > obj_cnt);

	for (int i = 0; i < itk_none; i++) {
		ptr = (tree *)get_global(buf, "integer_types", NULL);
		ptr[i] = (tree)(unsigned long)objs[ggv_idx].real_addr;
		ggv_idx++;
	}
	BUG_ON(ggv_idx > obj_cnt);

	for (int i = 0; i < (int)stk_type_kind_last; i++) {
		ptr = (tree *)get_global(buf, "sizetype_tab", NULL);
		ptr[i] = (tree)(unsigned long)objs[ggv_idx].real_addr;
		ggv_idx++;
	}
	BUG_ON(ggv_idx > obj_cnt);

	if (!held)
		analysis__sibuf_drop(buf);

	CLIB_DBG_FUNC_EXIT();
	return;
}

static int gcc_ver_major = __GNUC__;
static int gcc_ver_minor = __GNUC_MINOR__;
static int c_parse(struct sibuf *buf, int parse_mode)
{
	CLIB_DBG_FUNC_ENTER();

	struct file_content *fc = (struct file_content *)buf->load_addr;

	int held = analysis__sibuf_hold(buf);

	if ((fc->gcc_ver_major != gcc_ver_major) ||
		(fc->gcc_ver_minor > gcc_ver_minor)) {
		si_log1_warn("gcc version not match, need %d.%d\n",
				fc->gcc_ver_major, fc->gcc_ver_minor);
		if (!held)
			analysis__sibuf_drop(buf);
		CLIB_DBG_FUNC_EXIT();
		return -1;
	}

	mode = parse_mode;
	cur_sibuf = buf;
	addr_base = buf->payload;
	obj_cnt = buf->obj_cnt;
	real_obj_cnt = obj_cnt;
	objs = buf->objs;
	obj_idx = 0;
	obj_adjusted = 0;

	/*
	 * XXX: we need to copy the path to current thread stack.
	 * Because the current sibuf may be unload during the process,
	 * Then c_show_progress() will cause SIGSEGV.
	 */
	char sp_path[PATH_MAX];
	snprintf(sp_path, PATH_MAX, "%s", fc->path + fc->srcroot_len);

	if (!obj_cnt) {
		if (!held)
			analysis__sibuf_drop(buf);

		CLIB_DBG_FUNC_EXIT();
		return 0;
	}

	switch (mode) {
	case MODE_ADJUST:
	{
		get_real_obj_cnt();
		show_progress_arg[0] = (long)"ADJUST";
		show_progress_arg[1] = (long)sp_path;
		show_progress_arg[2] = (long)&obj_adjusted;
		show_progress_arg[3] = (long)&obj_cnt;
		show_progress_arg[4] = (long)pthread_self();
		mt_print_add();
		mt_add_timer(show_progress_timeout, c_show_progress,
				show_progress_arg, 0, 1);

		void *raddrs[obj_cnt];
		void *faddrs[obj_cnt];
		/* add an extra check on the obj_cnt */
		BUG_ON((obj_cnt * sizeof(raddrs[0]) * 2) >= THREAD_STACKSZ);

		for (size_t i = 0; i < obj_cnt; i++) {
			objs[i].is_adjusted = 0;
			/* XXX: fixup the gcc global vars real_addr */
			if (objs[i].gcc_global_varidx && (!objs[i].fake_addr))
				objs[i].real_addr = 0;
			if (objs[i].gcc_global_varidx && (!objs[i].size))
				objs[i].real_addr = 0;	/* fix later */
			raddrs[i] = (void *)(unsigned long)objs[i].real_addr;
			faddrs[i] = (void *)(unsigned long)objs[i].fake_addr;
		}
		real_addrs = raddrs;
		fake_addrs = faddrs;

		/*
		 * we collect the lower gimple at ALL_IPA_PASSES_END,
		 * all functions have been chained
		 */
		do_tree((tree)(unsigned long)objs[0].real_addr);

		/*
		 * Do NOT forget the gcc global vars
		 */
		do_gcc_global_var_adjust();

		BUG_ON(obj_adjusted != obj_cnt);

		mt_del_timer(0);
		mt_print_del();
		break;
	}
	case MODE_GETBASE:
	{
		get_real_obj_cnt();
		show_progress_arg[0] = (long)"GETBASE";
		show_progress_arg[1] = (long)sp_path;
		show_progress_arg[2] = (long)&obj_idx;
		show_progress_arg[3] = (long)&real_obj_cnt;
		show_progress_arg[4] = (long)pthread_self();
		mt_print_add();
		mt_add_timer(show_progress_timeout, c_show_progress,
				show_progress_arg, 0, 1);

		do_get_base(buf);

		mt_del_timer(0);
		mt_print_del();
		break;
	}
	case MODE_GETDETAIL:
	{
		get_real_obj_cnt();
		show_progress_arg[0] = (long)"GETDETAIL";
		show_progress_arg[1] = (long)sp_path;
		show_progress_arg[2] = (long)&obj_idx;
		show_progress_arg[3] = (long)&real_obj_cnt;
		show_progress_arg[4] = (long)pthread_self();
		mt_print_add();
		mt_add_timer(show_progress_timeout, c_show_progress,
				show_progress_arg, 0, 1);

		/* XXX: get details till codepath */
		do_get_detail(buf);

		mt_del_timer(0);
		mt_print_del();
		break;
	}
	case MODE_GETSTEP4:
	{
		get_real_obj_cnt();
		show_progress_arg[0] = (long)"GETSTEP4";
		show_progress_arg[1] = (long)sp_path;
		show_progress_arg[2] = (long)&obj_idx;
		show_progress_arg[3] = (long)&real_obj_cnt;
		show_progress_arg[4] = (long)pthread_self();
		mt_print_add();
		mt_add_timer(show_progress_timeout, c_show_progress,
				show_progress_arg, 0, 1);

		setup_gcc_globals(buf);

		void *xaddrs[obj_cnt];
		phase4_obj_checked = xaddrs;

		/* XXX, get func_node's callees/callers/global/local_vars... */
		do_phase4(buf);

		mt_del_timer(0);
		mt_print_del();
		break;
	}
	case MODE_GETINDCFG1:
	{
		show_progress_arg[0] = (long)"GETINDCFG1";
		show_progress_arg[1] = (long)sp_path;
		show_progress_arg[2] = (long)&obj_idx;
		show_progress_arg[3] = (long)&obj_cnt;
		show_progress_arg[4] = (long)pthread_self();
		mt_print_add();
		mt_add_timer(show_progress_timeout, c_show_progress,
				show_progress_arg, 0, 1);

		do_phase5(buf);

		mt_del_timer(0);
		mt_print_del();
		break;
	}
	case MODE_GETINDCFG2:
	{
		show_progress_arg[0] = (long)"GETINDCFG2";
		show_progress_arg[1] = (long)sp_path;
		show_progress_arg[2] = (long)&obj_idx;
		show_progress_arg[3] = (long)&obj_cnt;
		show_progress_arg[4] = (long)pthread_self();
		mt_print_add();
		mt_add_timer(show_progress_timeout, c_show_progress,
				show_progress_arg, 0, 1);

		do_phase6(buf);

		mt_del_timer(0);
		mt_print_del();
		break;
	}
	default:
		break;
	}

	if (!held)
		analysis__sibuf_drop(buf);

	CLIB_DBG_FUNC_EXIT();
	return 0;
}

/*
 * ************************************************************************
 * dec functions
 * ************************************************************************
 */
static struct data_state_rw *get_ds_via_tree(struct sample_set *sset, int idx,
						struct fn_list *fnl, tree n,
						int *complete_type_p,
						int *ssa_write);
static int do_dec(struct sample_set *sset, int idx, struct fn_list *fnl,
		  gimple_seq gs);
static int dsv_fill(struct sample_set *sset, int idx, struct fn_list *fnl,
		    struct data_state_val *dsv, tree node);

static int get_dsv_via_constructor(struct sample_set *sset, int idx,
					struct fn_list *fnl,
					struct data_state_val *dsv, tree n)
{
	/* check dump_generic_node() and native_encode_initializer() */
	tree type = TREE_TYPE(n);
	if (!CONSTRUCTOR_ELTS(n))
		return 0;

	switch (TREE_CODE(type)) {
	case RECORD_TYPE:
	case UNION_TYPE:
	{
		tree field = NULL_TREE;
		unsigned HOST_WIDE_INT cnt;
		constructor_elt *ce;

		if (TREE_CODE(type) == RECORD_TYPE)
			field = TYPE_FIELDS(type);

		if (DSV_TYPE(dsv) != DSVT_CONSTRUCTOR) {
			u8 subtype = DSV_SUBTYPE_DEF;
			if (TREE_CODE(type) == UNION_TYPE)
				subtype = DSV_SUBTYPE_UNION;
			dsv_alloc_data(dsv, DSVT_CONSTRUCTOR, subtype, 0);
		}
		FOR_EACH_VEC_SAFE_ELT(CONSTRUCTOR_ELTS(n), cnt, ce) {
			tree val = ce->value;
			int pos, fieldsize;

			if (ce->index != 0)
				field = ce->index;

			if (val)
				STRIP_NOPS(val);

			if (field == NULL_TREE || DECL_BIT_FIELD(field)) {
				si_log1_todo("not handled\n");
				return -1;
			}

			if ((TREE_CODE(TREE_TYPE(field)) == ARRAY_TYPE) &&
				TYPE_DOMAIN(TREE_TYPE(field)) &&
			      (!TYPE_MAX_VALUE(TYPE_DOMAIN(TREE_TYPE(field))))) {
				si_log1_todo("not handled\n");
				return -1;
			} else if ((DECL_SIZE_UNIT(field) == NULL_TREE) ||
				    (!tree_fits_shwi_p(DECL_SIZE_UNIT(field)))) {
				si_log1_todo("not handled\n");
				return -1;
			}

			fieldsize = tree_to_shwi(DECL_SIZE_UNIT(field));
			/*
			 * native_encode_initializer() use int_byte_position().
			 * However, we need to get the bit position.
			 * FIXME: Can we use bit_position()?
			 * XXX: int_bit_position() use LOG2_BITS_PER_UNIT, which
			 * is determined by BITS_PER_UNIT.
			 */
			pos = int_bit_position(field);

			int err;
			struct data_state_val1 *tmp;
			struct data_state_val *tmp_dsv, *union_dsv;
			err = dsv_find_constructor_elem(dsv, pos,
						fieldsize * BITS_PER_UNIT,
						&tmp, &tmp_dsv, &union_dsv);
			if (err == -1) {
				si_log1_warn("dsv_find_constructor_elem err\n");
				return -1;
			}
			if (!tmp) {
				si_log1_warn("Should not happen\n");
				return -1;
			}
			dsv_set_raw(tmp_dsv, val);
			if (dsv_fill(sset, idx, fnl, tmp_dsv, val) == -1) {
				si_log1_todo("dsv_fill err\n");
				return -1;
			}
		}
		break;
	}
	case ARRAY_TYPE:
	{
		HOST_WIDE_INT min_index;
		unsigned HOST_WIDE_INT cnt;
		int curpos = 0, fieldsize;
		constructor_elt *ce;

		if ((TYPE_DOMAIN(type) == NULL_TREE) ||
			(!tree_fits_shwi_p(TYPE_MIN_VALUE(TYPE_DOMAIN(type))))) {
			si_log1_todo("not handled\n");
			return -1;
		}

		fieldsize = int_size_in_bytes(TREE_TYPE(type));
		if (fieldsize <= 0) {
			si_log1_warn("should not happen\n");
			return -1;
		}

		min_index = tree_to_shwi(TYPE_MIN_VALUE(TYPE_DOMAIN(type)));
		if (DSV_TYPE(dsv) != DSVT_CONSTRUCTOR)
			dsv_alloc_data(dsv, DSVT_CONSTRUCTOR, 0, 0);
		FOR_EACH_VEC_SAFE_ELT(CONSTRUCTOR_ELTS(n), cnt, ce) {
			tree val = ce->value;
			tree index = ce->index;
			int pos = curpos;
			if (index && (TREE_CODE(index) == RANGE_EXPR))
				pos = (tree_to_shwi(TREE_OPERAND(index, 0)) - 
					min_index) * fieldsize;
			else if (index)
				pos = (tree_to_shwi(index) - min_index) *
					fieldsize;

			if (val && (TREE_CODE(val) == ADDR_EXPR) &&
			    (TREE_CODE(TREE_OPERAND(val, 0)) == FUNCTION_DECL))
				val = TREE_OPERAND(val, 0);

			int err;
			struct data_state_val1 *tmp;
			struct data_state_val *tmp_dsv, *union_dsv;
			err = dsv_find_constructor_elem(dsv,
							pos * BITS_PER_UNIT,
						      fieldsize * BITS_PER_UNIT,
						      &tmp, &tmp_dsv,
						      &union_dsv);
			if (err == -1) {
				si_log1_warn("dsv_find_constructor_elem err\n");
				return -1;
			}
			if (!tmp) {
				si_log1_warn("Should not happen\n");
				return -1;
			}
			dsv_set_raw(tmp_dsv, val);
			if (dsv_fill(sset, idx, fnl, tmp_dsv, val) == -1) {
				si_log1_todo("dsv_fill err\n");
				return -1;
			}

			curpos = pos + fieldsize;
			if (index && (TREE_CODE(index) == RANGE_EXPR)) {
				int count;
				count = tree_to_shwi(TREE_OPERAND(index, 1)) -
					tree_to_shwi(TREE_OPERAND(index, 0));
				while (count-- > 0) {
					if (val) {
						si_log1_todo("not handled\n");
					}
					curpos += fieldsize;
				}
			}
		}
		break;
	}
	default:
	{
		si_log1_todo("miss %s\n", tree_code_name[TREE_CODE(type)]);
		return -1;
	}
	}

	return 0;
}

static int dsv_fill(struct sample_set *sset, int idx, struct fn_list *fnl,
		    struct data_state_val *dsv, tree node)
{
	tree n = node;
	if (!n)
		return 0;

	struct sibuf *b;
	int ret = 0;
	b = find_target_sibuf((void *)n);
	int held = analysis__sibuf_hold(b);

	switch (TREE_CODE(n)) {
	case INTEGER_CST:
	{
		/* FIXME: what if TREE_INT_CST_EXT_NUNITS(n)>1 */
		u32 this_bytes = get_type_bytes(TREE_TYPE(n));
		if (DSV_TYPE(dsv) != DSVT_INT_CST)
			dsv_alloc_data(dsv, DSVT_INT_CST, 0, this_bytes);
		clib_memcpy_bits(DSV_SEC1_VAL(dsv), this_bytes * BITS_PER_UNIT,
				 &TREE_INT_CST_ELT(n, 0),
				 sizeof(TREE_INT_CST_ELT(n, 0)) * BITS_PER_UNIT);
		break;
	}
	case ADDR_EXPR:
	{
		struct data_state_rw *target_ds;
		struct data_state_val *target_dsv;
		target_ds = get_ds_via_tree(sset, idx, fnl, n, NULL, NULL);
		if (!target_ds) {
			si_log1_warn("Should not happen\n");
			ret = -1;
			break;
		}

		/* we know the value is DSVT_ADDR */
		target_dsv = &target_ds->val;

		ret = dsv_copy_data(dsv, target_dsv);
		if (ret == -1) {
			si_log1_warn("dsv_copy_data err\n");
		}
		break;
	}
	case CONSTRUCTOR:
	{
		ret = get_dsv_via_constructor(sset, idx, fnl, dsv, n);
		break;
	}
	case FIELD_DECL:
	case INTEGER_TYPE:
	case PARM_DECL:
	case RECORD_TYPE:
	case UNION_TYPE:
	case ARRAY_TYPE:
	case SSA_NAME:
	case VAR_DECL:
	case POINTER_TYPE:
	case ENUMERAL_TYPE:
	case BOOLEAN_TYPE:
	case NOP_EXPR:
	case STRING_CST:
	{
		ret = 0;
		break;
	}
	default:
	{
		si_log1_todo("miss %s\n", tree_code_name[TREE_CODE(n)]);
		ret = -1;
		break;
	}
	}

	if (!held)
		analysis__sibuf_drop(b);
	return ret;
}

static int dsv_init(struct sample_set *sset, int idx, struct fn_list *fnl,
			struct data_state_val *dsv, tree node)
{
	int err = 0;

	struct sibuf *b;
	b = find_target_sibuf((void *)node);
	int held = analysis__sibuf_hold(b);
	/*
	 * FIXME: what if the node is CONSTRUCTOR, and the val is not set?
	 * Is it right to check the type?
	 * If node is SSA_NAME, and DSV_TYPE is DSVT_UNK, then the def_stmt
	 * is not parsed yet.
	 */
	if (TREE_CODE(node) == SSA_NAME) {
		if (!gimple_in_func_stmts(fnl->fn, SSA_NAME_DEF_STMT(node))) {
			err = do_dec(sset, idx, fnl, SSA_NAME_DEF_STMT(node));
			if (!held)
				analysis__sibuf_drop(b);
			return err;
		}
	}

	tree type = NULL;
	if (TREE_CODE_CLASS(TREE_CODE(node)) == tcc_type)
		type = node;
	else
		type = TREE_TYPE(node);

	enum tree_code tc_type = TREE_CODE(type);
	switch (tc_type) {
	case UNION_TYPE:
	case RECORD_TYPE:
	{
		tree field = TYPE_FIELDS(type);
		unsigned long this_offset = 0;
		u32 this_bits = 0;
		u8 subtype = DSV_SUBTYPE_DEF;
		if (tc_type == UNION_TYPE)
			subtype = DSV_SUBTYPE_UNION;
		dsv_alloc_data(dsv, DSVT_CONSTRUCTOR, subtype, 0);
		while (field) {
			tree this_type = TREE_TYPE(field);
			struct data_state_val1 *dsv1;
			this_offset = get_field_offset(field);
			if (TYPE_SIZE(this_type)) {
				this_bits = TREE_INT_CST_LOW(
						TYPE_SIZE(this_type));
			} else if (TREE_CODE(this_type) == ARRAY_TYPE) {
				/* The size is calculated in runtime */
				this_bits = 0;
			} else {
				si_log1_todo("miss %s\n",
					   tree_code_name[TREE_CODE(this_type)]);
				this_bits = 0;
			}
			dsv1 = dsv1_alloc((void *)field, (s32)this_offset,
					  this_bits);
			slist_add_tail(&dsv1->sibling, DSV_SEC3_VAL(dsv));

			/* for each element, run dsv_init() */
			err = dsv_init(sset, idx, fnl, &dsv1->val, this_type);
			if (err)
				break;

			field = DECL_CHAIN(field);
		}
		break;
	}
	case ARRAY_TYPE:
	{
		size_t elem_cnt = 1;
		if (TYPE_DOMAIN(type) && TYPE_MAX_VALUE(TYPE_DOMAIN(type)))
			elem_cnt += TREE_INT_CST_LOW(TYPE_MAX_VALUE(
							TYPE_DOMAIN(type)));

		/* FIXME: alignment */
		tree elem_type = TREE_TYPE(type);
		size_t elem_size = TREE_INT_CST_LOW(TYPE_SIZE_UNIT(elem_type));
		unsigned long this_offset = 0;
		u32 this_bits = (u32)elem_size * BITS_PER_UNIT;
		dsv_alloc_data(dsv, DSVT_CONSTRUCTOR, 0, 0);
		for (size_t i = 0; i < elem_cnt; i++) {
			struct data_state_val1 *dsv1;
			this_offset = i * this_bits;
			dsv1 = dsv1_alloc(elem_type, this_offset, this_bits);
			slist_add_tail(&dsv1->sibling, DSV_SEC3_VAL(dsv));

			err = dsv_init(sset, idx, fnl, &dsv1->val, elem_type);
			if (err)
				break;
		}
		break;
	}
	case BOOLEAN_TYPE:
	{
		dsv_alloc_data(dsv, DSVT_INT_CST, 0, sizeof(int));
		*(int *)DSV_SEC1_VAL(dsv) = s_random() % 2;
		break;
	}
	case ENUMERAL_TYPE:
	{
		tree enum_list = TYPE_VALUES(type);

		int cnt = 0;
		while (enum_list) {
			cnt++;
			enum_list = TREE_CHAIN(enum_list);
		}

		int idx = cnt ? (s_random() % cnt) : 0;
		cnt = 0;
		enum_list = TYPE_VALUES(type);
		long value = s_random();
		dsv_alloc_data(dsv, DSVT_INT_CST, 0, sizeof(int));
		*(int *)DSV_SEC1_VAL(dsv) = (int)value;
		while (enum_list) {
			value = (long)TREE_INT_CST_LOW(TREE_VALUE(enum_list));
			if (idx == cnt) {
				*(int *)DSV_SEC1_VAL(dsv) = (int)value;
				break;
			}
			cnt++;
			enum_list = TREE_CHAIN(enum_list);
		}
		break;
	}
	case INTEGER_TYPE:
	case REAL_TYPE:
	{
		size_t len_bytes, len_bits __maybe_unused;
		len_bytes = TREE_INT_CST_LOW(TYPE_SIZE_UNIT(type));
		len_bits = TREE_INT_CST_LOW(TYPE_SIZE(type));

		if (tc_type == INTEGER_TYPE)
			dsv_alloc_data(dsv, DSVT_INT_CST, 0, len_bytes);
		else if (tc_type == REAL_TYPE)
			dsv_alloc_data(dsv, DSVT_REAL_CST, 0, len_bytes);
#ifdef GUESS_DSV_RANDOM
		random_bits(DSV_SEC1_VAL(dsv), len_bits);
#endif
		break;
	}
	case POINTER_TYPE:
	{
		/* FIXME: init NULL value? */
		size_t bytes = sizeof(void *);
		if (TYPE_SIZE_UNIT(type))
			bytes = TREE_INT_CST_LOW(TYPE_SIZE_UNIT(type));
		dsv_alloc_data(dsv, DSVT_INT_CST, 0, bytes);
		*(void **)DSV_SEC1_VAL(dsv) = (void *)0;
		break;
	}
	default:
	{
		err = -1;
		si_log1_todo("miss %s\n", tree_code_name[tc_type]);
		break;
	}
	}

	dsv_fill(sset, idx, fnl, dsv, node);

	if (!held)
		analysis__sibuf_drop(b);
	return err;
}

/*
 * @get_ds_val: get the target data_state_val the given @dsv reference to.
 * If the dsv is not initialised, init it.
 */
static struct data_state_val *get_ds_val(struct sample_set *sset, int idx,
					 struct fn_list *fnl,
					 struct data_state_val *dsv,
					 s32 offset, u32 bits)
{
	struct data_state_val *ret = NULL;

	if ((!dsv) || ((long)dsv == (long)offsetof(struct data_state_rw, val)))
		return ret;

	if ((DSV_TYPE(dsv) == DSVT_UNK) || (!DSV_SEC1_VAL(dsv))) {
		tree n = (tree)(long)dsv->raw;
		struct sibuf *b;
		b = find_target_sibuf((void *)n);
		int held = analysis__sibuf_hold(b);

		if (dsv_init(sset, idx, fnl, dsv, n)) {
			si_log1_warn("dsv_init err\n");
		} else {
			ret = get_ds_val(sset, idx, fnl, dsv, offset, bits);
		}

		if (!held)
			analysis__sibuf_drop(b);
	} else if (DSV_TYPE(dsv) == DSVT_REF) {
		ret = get_ds_val(sset, idx, fnl, &DSV_SEC2_VAL(dsv)->ds->val,
				 DSV_SEC2_VAL(dsv)->offset,
				 DSV_SEC2_VAL(dsv)->bits);
	} else if (DSV_TYPE(dsv) == DSVT_CONSTRUCTOR) {
		int err;
		struct data_state_val1 *tmp;
		struct data_state_val *tmp_dsv, *union_dsv;
		err = dsv_find_constructor_elem(dsv, offset, bits,
						&tmp, &tmp_dsv, &union_dsv);
		if ((!err) && tmp) {
			ret = tmp_dsv;
#if 0
		} else if (err == -1) {
			si_log1_warn("dsv_find_constructor_elem err\n");
		} else if (!tmp) {
			si_log1_warn("Should not happen\n");
#endif
		}
	} else {
		/*
		 * TODO: If dsv is a string, and it is used as an array,
		 * how to return the value? Should be better to use ARRAY
		 * to represent STRING_CST
		 */
		ret = dsv;
	}

	/* if target dsv not found, return the given one */
	if (!ret)
		ret = dsv;
	return ret;
}

static struct data_state_rw *get_ds_via_constructor(struct sample_set *sset,
							int idx,
							struct fn_list *fnl,
							tree n)
{
	struct data_state_rw *ret = NULL;
	enum tree_code tc = TREE_CODE(n);
	if (tc != CONSTRUCTOR) {
		si_log1_warn("should not happen\n");
		return ret;
	}

	ret = ds_rw_new((u64)n, DSRT_RAW, (void *)n);

	(void)dsv_init(sset, idx, fnl, &ret->val, n);

	return ret;
}

static struct data_state_rw *get_ds_via_tree(struct sample_set *sset, int idx,
						struct fn_list *fnl, tree n,
						int *complete_type_p,
						int *ssa_write)
{
	if (complete_type_p)
		*complete_type_p = 1;
	int old_ssa_write = 0;
	if (ssa_write) {
		old_ssa_write = *ssa_write;
		*ssa_write = 0;
	}

	struct data_state_rw *ret = NULL;
	if (!n)
		return ret;

	if (TREE_CODE_CLASS(TREE_CODE(n)) == tcc_type) {
		si_log1_warn("should not happen: %s\n",
				tree_code_name[TREE_CODE(n)]);
		return ret;
	}

	if (!COMPLETE_TYPE_P(TREE_TYPE(n))) {
		if (complete_type_p)
			*complete_type_p = 0;
#if 0
		si_log1_warn("tree(%s) type(%s) is not a COMPLETE_TYPE\n",
				tree_code_name[TREE_CODE(n)],
				tree_code_name[TREE_CODE(TREE_TYPE(n))]);
#endif
		return ret;
	}

	enum tree_code tc = TREE_CODE(n);
#if 0
	u32 bits = 0;
	bits = TREE_INT_CST_LOW(TYPE_SIZE(TREE_TYPE(n)));
#endif

	switch (tc) {
	case INTEGER_CST:
	{
		/* FIXME: gcc use wide_int */
		int i = TREE_INT_CST_EXT_NUNITS(n), j;
		unsigned long val[i];
		memset(val, 0, sizeof(val[0]) * i);
		u32 bytes = sizeof(val[0]);

		val[0] = TREE_INT_CST_LOW(n);
		for (j = 1; j < i; j++) {
			val[j] = TREE_INT_CST_ELT(n, j);
			if (val[j])
				bytes = sizeof(val[0]) * (j+1);
		}
		ret = ds_rw_new((u64)n, DSRT_RAW, n);
		dsv_alloc_data(&ret->val, DSVT_INT_CST, 0, bytes);
		memcpy(DS_SEC1_VAL(ret), val, bytes);
		break;
	}
	case STRING_CST:
	{
		char *str;
		str = (char *)TREE_STRING_POINTER(n);
		u32 srclen = TREE_STRING_LENGTH(n);

		ret = ds_rw_new((u64)n, DSRT_RAW, n);
		dsv_alloc_data(&ret->val, DSVT_CONSTRUCTOR, 0, 0);
		dsv_fill_str_data(&ret->val, n, str, srclen);
		break;
	}
	case REAL_CST:
	{
		/* FIXME: we just copy the real_value structure */
		u32 bytes = sizeof(struct real_value);
		ret = ds_rw_new((u64)n, DSRT_RAW, n);
		dsv_alloc_data(&ret->val, DSVT_REAL_CST, 0, bytes);
		memcpy(DS_SEC1_VAL(ret), TREE_REAL_CST_PTR(n), bytes);
		break;
	}
	case PARM_DECL:
	{
		struct var_list *vnl;
		struct func_node *fn;
		u32 bits = 0;

		fn = fnl->fn;
		vnl = var_list_find(&fn->args, n);
		if (!vnl)
			break;
		else if (vnl->var.type)
			bits = vnl->var.type->ofsize * BITS_PER_UNIT;

		struct data_state_rw *ds;
		ds = ds_rw_find(sset, idx, fnl, (u64)&vnl->var, DSRT_VN);
		ret = ds_dup_base(&ds->base);
		dsv_alloc_data(&ret->val, DSVT_REF, 0, 0);
		if (ds_vref_setv(&ret->val, ds, 0, bits) == -1) {
			si_log1_warn("ds_vref_setv err\n");
			ds_drop(ret);
			ret = NULL;
		}
		ds_drop(ds);

		break;
	}
	case VAR_DECL:
	{
		u32 bits = 0;
		tree fndecl = NULL;
		struct func_node *fn;
		fndecl = (tree)fnl->fn->node;
		fn = fnl->fn;

		expanded_location *xloc;
		struct sibuf *b;
		b = find_target_sibuf(n);
		int held = analysis__sibuf_hold(b);
		if (!held)
			analysis__sibuf_drop(b);
		xloc = get_location(GET_LOC_VAR, b->payload, n);

		tree init_node;
		init_node = n;
		if (DECL_INITIAL(n))
			init_node = DECL_INITIAL(n);

		if (si_is_global_var(n, xloc)) {
			struct sinode *gvsn;
			struct var_node *gvn;
			struct data_state_rw *ds;

			get_var_sinode(n, &gvsn, 1);
			if (!gvsn) {
				/*
				 * some external variables may not be defined
				 * in the source. e.g. stdout in standard lib
				 */
				char name[NAME_MAX];
				memset(name, 0, NAME_MAX);
				get_var_name((void *)n, name);
				si_log1_warn("global var not found, %s\n",
						name);
				ret = ds_rw_new((u64)n, DSRT_RAW,
							init_node);
				/* TODO: init the value? */
				break;
			}
			int held = analysis__sibuf_hold(gvsn->buf);
			gvn = (struct var_node *)gvsn->data;
			if (gvn->type)
				bits = gvn->type->ofsize * BITS_PER_UNIT;
			else
				bits = 0;

			ds = ds_rw_find(sset, idx, fnl,
						(u64)gvn, DSRT_VN);
			if (unlikely(!ds)) {
				/* like FUNCTION_DECL */
				struct data_state_base *base;
				base = global_ds_base_add((u64)gvn,
								  DSRT_VN);
				ds = ds_dup_base(base);
				si_lock_w();
				slist_add_tail(&ds->base.sibling,
						&si->global_data_rw_states);
				si_unlock_w();
				ds_hold(ds);
			}
			if (DECL_INITIAL((tree)gvn->node))
				init_node = DECL_INITIAL((tree)gvn->node);
			dsv_set_raw(&ds->val, (void *)init_node);

			ret = ds_dup_base(&ds->base);
			dsv_alloc_data(&ret->val, DSVT_REF, 0, 0);
			if (ds_vref_setv(&ret->val, ds, 0, bits) == -1) {
				si_log1_warn("ds_vref_setv err\n");
				ds_drop(ret);
				ret = NULL;
			}
			ds_drop(ds);
			if (!held)
				analysis__sibuf_drop(gvsn->buf);

			break;
		}

		/* local var */
		tree ctx = DECL_CONTEXT(n);
		while (ctx && (TREE_CODE(ctx) == BLOCK))
			ctx = BLOCK_SUPERCONTEXT(ctx);

		if ((ctx == fndecl) || (!ctx)) {
			;
		}

		struct var_list *vnl;
		vnl = var_list_find(&fn->local_vars, (void *)n);
		if (unlikely(!vnl)) {
			vnl = var_list_new((void *)n);
			if (DECL_NAME(n)) {
				char name[NAME_MAX];
				memset(name, 0, NAME_MAX);
				get_node_name(DECL_NAME(n), name);
				vnl->var.name = name_list_add(name,
								strlen(name)+1);
			}
			slist_add_tail(&vnl->sibling, &fn->local_vars);

			/* XXX: need to add fn ds and fnl ds */
			(void)fn_ds_add(fn, (u64)&vnl->var, DSRT_VN);
			(void)fnl_ds_add(fnl, (u64)&vnl->var, DSRT_VN,
					 (void *)n);
		}
		if (vnl->var.type)
			bits = vnl->var.type->ofsize * BITS_PER_UNIT;
		else
			bits = 0;
		struct data_state_rw *ds;
		ds = ds_rw_find(sset, idx, fnl, (u64)&vnl->var,
					DSRT_VN);
		dsv_set_raw(&ds->val, (void *)init_node);

		ret = ds_dup_base(&ds->base);
		dsv_alloc_data(&ret->val, DSVT_REF, 0, 0);
		if (ds_vref_setv(&ret->val, ds, 0, bits) == -1) {
			si_log1_warn("ds_vref_setv err\n");
			ds_drop(ret);
			ret = NULL;
		}
		ds_drop(ds);

		break;
	}
	case FUNCTION_DECL:
	{
		struct sinode *fsn;
		get_func_sinode(n, &fsn, 1);
		if ((!fsn) || (!fsn->data)) {
			si_log1_warn("FUNCTION_DECL sinode not right, %p %p\n",
					n, fsn);
			break;
		}

		struct data_state_rw *tmp;
		tmp = ds_rw_find(sset, idx, fnl, (u64)fsn->data,
					 DSRT_FN);
		if (unlikely(!tmp)) {
			/*
			 * FIXME: this function probably is used in some global
			 * var, we should handle this in parse PHASE4.
			 */
			struct data_state_base *base;
			base = global_ds_base_add((u64)fsn->data,
							  DSRT_FN);
			tmp = ds_dup_base(base);
			si_lock_w();
			slist_add_tail(&tmp->base.sibling,
					&si->global_data_rw_states);
			si_unlock_w();
			ds_hold(tmp);
		}
		ret = ds_dup_base(&tmp->base);
		ds_drop(tmp);

		break;
	}
	case ADDR_EXPR:
	{
		struct data_state_rw *tmp;
		u32 bits;

		tmp = get_ds_via_tree(sset, idx, fnl, TREE_OPERAND(n, 0), NULL,
					NULL);
		if (!tmp) {
			/*
			 * TODO: in linux kernel in_lock_functions(), the
			 * __lock_text_start seems to be defined in .S file,
			 * which is not implemented by now. 2020-10-29.
			 */
			break;
		}
		bits = get_type_bits(TREE_OPERAND(n, 0));

		ret = ds_rw_new((u64)n, DSRT_RAW, n);
		dsv_alloc_data(&ret->val, DSVT_ADDR, 0, 0);
		if (ds_vref_setv(&ret->val, tmp, 0, bits) == -1) {
			si_log1_warn("ds_vref_setv err\n");
			ds_drop(ret);
			ret = NULL;
		}
		ds_drop(tmp);
		break;
	}
	case SSA_NAME:
	{
		/*
		 * If this ssa_name is a temporary SSA_NAME, and it is not
		 * in fnl, we create a new one and add it into fnl.
		 */
		tree var = SSA_NAME_VAR(n);
		struct data_state_rw *var_ds = NULL;

		if (var) {
			var_ds = get_ds_via_tree(sset, idx, fnl, var, NULL,
						 NULL);
			if (!var_ds) {
				si_log1_todo("Should not happen\n");
				break;
			}
		} else {
			ret = fnl_ds_add(fnl, (u64)n, DSRT_RAW, n);
			goto ssa_name_out;
		}

		if (old_ssa_write) {
			u32 bits;

			bits = get_type_bits(TREE_TYPE(var));

			ret = fnl_ds_add(fnl, (u64)n, DSRT_RAW, n);
			dsv_alloc_data(&ret->val, DSVT_REF, 0, 0);
			if (ds_vref_setv(&ret->val, var_ds, 0, bits) == -1) {
				si_log1_warn("ds_vref_setv err\n");
				ds_drop(ret);
				ret = NULL;
			}
			*ssa_write = 1;
			goto ssa_name_out;
		}

		/* !old_ssa_write && var */
		ret = fnl_ds_add(fnl, (u64)n, DSRT_RAW, n);
		if (gimple_nop_p(SSA_NAME_DEF_STMT(n))) {
			if (!SSA_NAME_IS_DEFAULT_DEF(n)) {
				si_log1_warn("Should not happen\n");
			} else if (DSV_TYPE(&ret->val) == DSVT_UNK) {
				struct data_state_val *var_dsv;
				var_dsv = get_ds_val(sset, idx, fnl,
						     &var_ds->val, 0, 0);
				if (dsv_copy_data(&ret->val, var_dsv) == -1) {
					si_log1_warn("dsv_copy_data err\n");
					ds_drop(ret);
					ret = NULL;
				}
			}
		}

ssa_name_out:
		ds_drop(var_ds);
		break;
	}
	case ARRAY_REF:
	{
		/*
		 * op0: the array
		 * op1: index
		 * op2: lower bound
		 */
		struct data_state_rw *tmp;
		tmp = get_ds_via_tree(sset, idx, fnl, TREE_OPERAND(n, 0), NULL,
					NULL);
		if (!tmp) {
			si_log1_todo("not handled\n");
			break;
		}

		/* we got the array, now we need to get the offset and bits */
		s32 this_offset = 0;
		u32 this_bits = 0;
		u64 elem_size = TREE_INT_CST_LOW(array_ref_element_size(n));
		u64 low_idx = TREE_INT_CST_LOW(array_ref_low_bound(n));
		u64 up_idx __maybe_unused = 0;
		if (array_ref_up_bound(n))
			up_idx = TREE_INT_CST_LOW(array_ref_up_bound(n));

		/* TODO: the op1 may be a variable. */
		u64 index = 0;
		struct data_state_rw *index_ds;
		index_ds = get_ds_via_tree(sset, idx, fnl, TREE_OPERAND(n, 1),
						NULL, NULL);
		if (index_ds) {
			struct data_state_val *index_dsv;
			index_dsv = get_ds_val(sset, idx, fnl, &index_ds->val,
						0, 0);
			if (DSV_TYPE(index_dsv) != DSVT_INT_CST) {
				si_log1_todo("Should not happen\n");
			} else {
				clib_memcpy_bits(&index,
						 sizeof(index) * BITS_PER_UNIT,
						 DSV_SEC1_VAL(index_dsv),
						 index_dsv->info.v1_info.bytes *
							BITS_PER_UNIT);
				if (up_idx && (index > up_idx)) {
					/* OOB */
					sample_set_set_flag(sset,
							    SAMPLE_SF_OOBR);
				}
			}
			ds_drop(index_ds);
			index_ds = NULL;
		}

		this_offset = elem_size * BITS_PER_UNIT * (index - low_idx);
		this_bits = elem_size * BITS_PER_UNIT;

		ret = ds_rw_new((u64)n, DSRT_RAW, n);
		dsv_alloc_data(&ret->val, DSVT_REF, 0, 0);
		if (ds_vref_setv(&ret->val, tmp, this_offset, this_bits) == -1) {
			si_log1_warn("ds_vref_setv err\n");
			ds_drop(ret);
			ret = NULL;
		}
		ds_drop(tmp);
		break;
	}
	case BIT_FIELD_REF:
	{
		/*
		 * op0: the container
		 * op1: how many bits it take
		 * op2: where to start
		 */
		struct data_state_rw *tmp;
		tmp = get_ds_via_tree(sset, idx, fnl, TREE_OPERAND(n, 0), NULL,
					NULL);
		if (!tmp) {
			si_log1_todo("not handled\n");
			break;
		}

		BUG_ON(NUM_POLY_INT_COEFFS > 1);
		/* check bit_field_offset and bit_field_size */
		u64 this_offset = TREE_INT_CST_LOW(TREE_OPERAND(n, 1));
		u64 this_bits = TREE_INT_CST_LOW(TREE_OPERAND(n, 2));

		ret = ds_rw_new((u64)n, DSRT_RAW, n);
		dsv_alloc_data(&ret->val, DSVT_REF, 0, 0);
		if (ds_vref_setv(&ret->val, tmp, this_offset, this_bits) == -1) {
			si_log1_warn("ds_vref_setv err\n");
			ds_drop(ret);
			ret = NULL;
		}
		ds_drop(tmp);
		break;
	}
	case COMPONENT_REF:
	{
		/*
		 * op0:
		 * op1: field
		 * op2: aligned_offset
		 */
		struct data_state_rw *tmp;
		tmp = get_ds_via_tree(sset, idx, fnl, TREE_OPERAND(n, 0), NULL,
					NULL);
		if (!tmp) {
			si_log1_todo("not handled\n");
			break;
		}

		u64 this_offset = get_field_offset(TREE_OPERAND(n, 1));
		u64 this_bits = TREE_INT_CST_LOW(
				TYPE_SIZE(TREE_TYPE(TREE_OPERAND(n, 1))));

		ret = ds_rw_new((u64)n, DSRT_RAW, n);
		dsv_alloc_data(&ret->val, DSVT_REF, 0, 0);
		if (ds_vref_setv(&ret->val, tmp, this_offset, this_bits) == -1) {
			si_log1_warn("ds_vref_setv err\n");
			ds_drop(ret);
			ret = NULL;
		}
		ds_drop(tmp);

		break;
	}
	case MEM_REF:
	{
		/*
		 * op0: base
		 * op1: offset
		 */
		struct data_state_rw *tmp, *newtmp = NULL;
		struct data_state_val *tmp_dsv;
		tmp = get_ds_via_tree(sset, idx, fnl, TREE_OPERAND(n, 0), NULL,
					NULL);
		if (!tmp) {
			si_log1_todo("not handled\n");
			break;
		}

		BUG_ON(NUM_POLY_INT_COEFFS > 1);
		u64 this_offset = *(mem_ref_offset(n).coeffs[0].get_val()) *
					BITS_PER_UNIT;

		tree ptype;
		/* FIXME: what exactly is the type to calculate the bits? */
		/* ptype = TYPE_MAIN_VARIANT(TREE_TYPE(TREE_OPERAND(n, 1))); */
		/* ptype = TREE_TYPE(n); */
		ptype = TREE_TYPE(TREE_OPERAND(n, 0));
		if (TREE_TYPE(TREE_OPERAND(n, 0)) !=
		    TREE_TYPE(TREE_OPERAND(n, 1))) {
			ptype = TREE_TYPE(TREE_OPERAND(n, 1));
		}

		u64 this_bits = TREE_INT_CST_LOW(TYPE_SIZE(ptype));

		tmp_dsv = get_ds_val(sset, idx, fnl, &tmp->val,
					this_offset, this_bits);
		/* check NULL pointer deref */
		if (DSV_TYPE(tmp_dsv) == DSVT_INT_CST) {
			void **pptr = (void **)DSV_SEC1_VAL(tmp_dsv);
			if (sample_set_check_nullptr(pptr)) {
				sample_set_set_flag(sset, SAMPLE_SF_NULLREF);
			}
		} else if (DSV_TYPE(tmp_dsv) == DSVT_ADDR) {
			newtmp = DSV_SEC2_VAL(tmp_dsv)->ds;
			this_offset += DSV_SEC2_VAL(tmp_dsv)->offset;
			ds_hold(newtmp);
		}

		ret = ds_rw_new((u64)n, DSRT_RAW, n);
		dsv_alloc_data(&ret->val, DSVT_REF, 0, 0);
		if (newtmp) {
			int err;
			err = ds_vref_setv(&ret->val, newtmp,
					   this_offset, this_bits);
			if (err == -1) {
				si_log1_warn("ds_vref_setv err\n");
				ds_drop(ret);
				ret = NULL;
			}
		} else {
			int err;
			err = ds_vref_setv(&ret->val, tmp,
					   this_offset, this_bits);
			if (err == -1) {
				si_log1_warn("ds_vref_setv err\n");
				ds_drop(ret);
				ret = NULL;
			}
		}
		ds_drop(tmp);
		ds_drop(newtmp);

		break;
	}
	case CONSTRUCTOR:
	{
		ret = get_ds_via_constructor(sset, idx, fnl, n);
		break;
	}
	case VOID_CST:
	case FIXED_CST:
	case COMPLEX_CST:
	case VECTOR_CST:
	default:
	{
		si_log1_todo("miss %s\n", tree_code_name[tc]);
		break;
	}
	}

	return ret;
}

static void dsv_extend(struct data_state_val *dsv)
{
	if (DSV_TYPE(dsv) != DSVT_INT_CST) {
		/* DSVT_INT_CST only */
		return;
	}

	/* TODO: endian? */
	tree n = (tree)(long)dsv->raw;
	struct sibuf *b;
	b = find_target_sibuf((void *)n);
	int held = analysis__sibuf_hold(b);

	enum tree_code tc = TREE_CODE(n);
	tree type;
	if (TREE_CODE_CLASS(TREE_CODE(n)) != tcc_type)
		type = TREE_TYPE(n);
	else
		type = n;
	u32 msb_pos = TREE_INT_CST_LOW(TYPE_SIZE(type)) - 1;
	int sign = 1;

	switch (tc) {
	case FIELD_DECL:
	{
		if (DECL_BIT_FIELD(n)) {
			type = DECL_BIT_FIELD_TYPE(n);
			msb_pos = TREE_INT_CST_LOW(DECL_SIZE(n)) - 1;
		}
		break;
	}
	default:
	{
		break;
	}
	}

	int msb_bit;
	u32 bits;

	if (!type)
		goto out;

	if (TYPE_UNSIGNED(type))
		sign = 0;
	dsv->info.v1_info.sign = sign;

	if (!sign)
		goto out;

	/* XXX: maybe we could use clib_memset_bits() */
	BUG_ON(msb_pos == (u32)-1);
	msb_bit = test_bit(msb_pos, (long *)DSV_SEC1_VAL(dsv));
	if (!msb_bit)
		goto out;

	bits = TREE_INT_CST_LOW(TYPE_SIZE(type));
	if (type != TREE_TYPE(n)) {
		u32 _bits;
		_bits = TREE_INT_CST_LOW(TYPE_SIZE(TREE_TYPE(n)));
		if (bits > _bits)
			bits = _bits;
	}

	for (u32 i = msb_pos + 1; i < bits; i++)
		test_and_set_bit(i, (long *)DSV_SEC1_VAL(dsv));

out:
	if (!held)
		analysis__sibuf_drop(b);
	return;
}

static struct data_state_val *get_ds_sec3_data(struct data_state_val *base,
						s32 *offset)
{
	struct data_state_val *dsv = base, *ret = NULL;
	s32 _offset = *offset;
	int flag = 0;
	while (1) {
		struct data_state_val1 *tmp;
		slist_for_each_entry(tmp, DSV_SEC3_VAL(dsv), sibling) {
			s32 this_fieldoffs = tmp->offset;
			u32 this_fieldbits = tmp->bits;
			if ((_offset < this_fieldoffs) ||
			    ((u32)_offset >= (this_fieldoffs + this_fieldbits)))
				continue;

			/* the value is in this data_state_val1 */
			*offset = _offset - this_fieldoffs;
			_offset = *offset;

			switch (dsv_get_section(DSV_TYPE(&tmp->val))) {
			case DSV_SEC_3:
			{
				flag = 1;
				dsv = &tmp->val;
				break;
			}
			case DSV_SEC_2:
			case DSV_SEC_1:
			case DSV_SEC_UNK:
			default:
			{
				ret = &tmp->val;
				flag = 2;
				break;
			}
			}
			break;
		}

		if (flag == 1)
			continue;
		else if (flag == 2)
			break;
		else
			break;
	}
	return ret;
}

static void *get_dsv_raw_func(struct sample_set *sset, int idx,
			    struct fn_list *fnl,
			    struct data_state_rw *ds, int ref_type)
{
	int done = 0;
	s32 offset = 0;
	void *ret = NULL;

	ds_hold(ds);
	while (!done) {
		/* check the ref_type first */
		switch (ref_type) {
		case DSRT_FN:
		{
			if (ds->base.ref_type == DSRT_FN) {
				struct func_node *fn;
				fn = (struct func_node *)
					(long)ds->base.ref_base;
				ret = fn->node;
				done = 1;
			} else if (ds->base.ref_type == DSRT_RAW) {
				tree n;
				n = (tree)(long)ds->base.ref_base;
				/* FIXME: not only FUNCTION_DECL tree node */
				if (TREE_CODE(n) == FUNCTION_DECL) {
					ret = (void *)n;
					done = 1;
				}
			}
			break;
		}
		default:
		{
			si_log1_todo("miss %d\n", ref_type);
			done = 1;
			break;
		}
		}

		if (done)
			break;

		struct data_state_val_ref *dsvr = NULL;
		switch (dsv_get_section(DS_VTYPE(ds))) {
		case DSV_SEC_3:
		{
			struct data_state_val *sec3_val;
			sec3_val = get_ds_sec3_data(&ds->val, &offset);
			if (!sec3_val) {
				done = 1;
				break;
			}
			if (dsv_get_section(DSV_TYPE(sec3_val)) != DSV_SEC_2) {
				/* FIXME: don't know what to do here */
				done = 1;
				break;
			}
			dsvr = DSV_SEC2_VAL(sec3_val);
		}
		case DSV_SEC_2:
		{
			struct data_state_rw *tmpds;
			if (!dsvr)
				dsvr = DS_SEC2_VAL(ds);
			ds_hold(dsvr->ds);
			tmpds = dsvr->ds;
			offset = dsvr->offset;
			ds_drop(ds);
			ds = tmpds;;
			break;
		}
		case DSV_SEC_1:
		case DSV_SEC_UNK:
		default:
		{
			done = 1;
			break;
		}
		}
	}
	ds_drop(ds);
	return ret;
}

static int fnl_init(struct sample_set *sset, int idx, struct fn_list *fnl)
{
	struct sample_state *sample = sset->samples[idx];
	fnl->curpos = fn_first_gimple(fnl->fn);

	/* XXX: should check if this function has a GIMPLE_RETURN */
	basic_block bb;
	tree fndecl = (tree)fnl->fn->node;
	int has_greturn = 0;
	FOR_EACH_BB_FN(bb, DECL_STRUCT_FUNCTION(fndecl)) {
		gimple_seq gs;

		gs = bb->il.gimple.phi_nodes;
		while (gs) {
			if (gimple_code(gs) == GIMPLE_RETURN) {
				has_greturn = 1;
				break;
			}
			gs = gs->next;
		}
		if (has_greturn)
			break;

		gs = bb->il.gimple.seq;
		while (gs) {
			if (gimple_code(gs) == GIMPLE_RETURN) {
				has_greturn = 1;
				break;
			}
			gs = gs->next;
		}
		if (has_greturn)
			break;
	}

	if (!has_greturn) {
		/*
		 * Some functions may not have a GIMPLE_RETURN stmt.
		 * e.g. call siglongjmp(), exit(), ...
		 */
		si_log1_todo("%s has no GIMPLE_RETURN\n", fnl->fn->name);
		return -1;
	}

	(void)sample_add_new_cp(sample, fnl->fn->cps[0]);
	(void)fnl_add_new_cp(fnl, fnl->fn->cps[0]);

	/* TODO: init the data_states, e.g. params. use DSVT_REF carefully. */
	return 0;
}

static void fnl_deinit(struct sample_set *sset, int idx, struct fn_list *fnl)
{
	/* TODO: decrease some refcounts, etc. */
}

static int dec_gimple_asm(struct sample_set *sset, int idx,
				struct fn_list *fnl, gimple_seq gs)
{
	/* TODO: parse_ssa_operands() get_asm_stmt_operands() */
	int err = -1;
	si_log1_todo("GIMPLE_ASM in %s\n", fnl->fn->name);
	fnl->curpos = (void *)cp_next_gimple(fnl->fn, gs);
	return err;
}

static int dec_internal_call(struct sample_set *sset, int idx,
				struct fn_list *fnl, gimple_seq gs)
{
	/* TODO: handle the internal call */
	sset->samples[idx]->retval = (struct data_state_rw *)VOID_RETVAL;
	return 0;
}

static int dec_gimple_call(struct sample_set *sset, int idx,
				struct fn_list *fnl, gimple_seq gs)
{
	/* parse_ssa_operands get_expr_operands */
	int err = 0;
	struct gcall *stmt;
	struct sinode *target_fsn = NULL;
	struct func_node *target_fn = NULL;
	stmt = (struct gcall *)gs;
	struct sample_state *sample = sset->samples[idx];

	tree lhs, callee, __callee;
	lhs = gimple_call_lhs(stmt);
	callee = gimple_call_fn(stmt);
	__callee = callee;

	if (sample->retval) {
		struct data_state_rw *orig_ds = sample->retval;

		if (!lhs) {
			if ((void *)orig_ds != VOID_RETVAL) {
				char loc[1024];
				gimple_loc_string(loc, 1024, gs);
				si_log1("ignore retval? %s\n", loc);
				sample_set_set_flag(sset, SAMPLE_SF_NCHKRV);
			}
		} else if ((void *)orig_ds == VOID_RETVAL) {
			char loc[1024];
			gimple_loc_string(loc, 1024, gs);
			si_log1("take void retval? %s\n", loc);
			sample_set_set_flag(sset, SAMPLE_SF_VOIDRV);
		} else {
			struct data_state_rw *dstmp;
			struct data_state_val *dsvtmp;
			int ssa_write = 1;
			dstmp = get_ds_via_tree(sset, idx, fnl, lhs, NULL,
						&ssa_write);
			if (!dstmp) {
				si_log1_warn("Should not happen\n");
				return -1;
			}

			dsvtmp = get_ds_val(sset, idx, fnl, &dstmp->val, 0, 0);

			err = dsv_copy_data(dsvtmp, &orig_ds->val);
			if (err == -1) {
				si_log1_warn("dsv_copy_data err\n");
				ds_drop(dstmp);
				return -1;
			}
			if (ssa_write) {
				if (dsvtmp == &dstmp->val) {
					si_log1_warn("Should not happen\n");
				} else {
					err = dsv_copy_data(&dstmp->val, dsvtmp);
					if (err == -1) {
						si_log1_warn("dsv_copy_data "
								"err\n");
						ds_drop(dstmp);
						return -1;
					}
				}
			}
			ds_drop(dstmp);
		}

		if ((void *)orig_ds != VOID_RETVAL)
			ds_drop(orig_ds);
		sample->retval = NULL;
		fnl->curpos = (void *)cp_next_gimple(fnl->fn, gs);
		return 0;
	}

	if (gimple_call_internal_p(stmt))
		return dec_internal_call(sset, idx, fnl, gs);

	/*
	 * *) find the called fn
	 * *) give dec_special_call() a chance to handle this call
	 * *) do not set the current fnl curpos to the next gimple sentence
	 * *) check if the arg_head is empty, setup the arg_head.
	 * *) create a new fnl, insert into fn_list_head, setup that fnl curpos
	 */
	switch (TREE_CODE(callee)) {
	case ADDR_EXPR:
	{
		__callee = gimple_call_addr_fndecl(callee);
		if (!__callee) {
			si_log1_warn("fndecl not found\n");
			break;
		}
		/* fall through */
	}
	case FUNCTION_DECL:
	{
		get_func_sinode(__callee, &target_fsn, 1);
		/* some function are external, in some other libraries */
		break;
	}
	case SSA_NAME:
	{
		__callee = SSA_NAME_VAR(callee);
		if (!__callee) {
			/* This ssa has been set. Should be in fnl. */
			struct data_state_rw *ds_ssa;
			ds_ssa = get_ds_via_tree(sset, idx, fnl, callee, NULL,
						 NULL);
			if (!ds_ssa) {
				si_log1_warn("Should not happen\n");
				break;
			}

			__callee = (tree)get_dsv_raw_func(sset, idx, fnl, ds_ssa,
							DSRT_FN);
			ds_drop(ds_ssa);
			if (!__callee) {
				si_log1_todo("FUNCTION_DECL is still NULL\n");
				break;
			}
			get_func_sinode(__callee, &target_fsn, 1);
			break;
		}
		/* fall through */
	}
	case PARM_DECL:
	case VAR_DECL:
	{
		/* The var should've been set */
		struct data_state_rw *ds;
		ds = get_ds_via_tree(sset, idx, fnl, __callee, NULL, NULL);
		if (!ds) {
			si_log1_todo("target data_state not found\n");
			break;
		}

		__callee = (tree)get_dsv_raw_func(sset, idx, fnl, ds, DSRT_FN);
		ds_drop(ds);
		if (!__callee) {
			si_log1_todo("FUNCTION_DECL not found\n");
			break;
		}

		get_func_sinode(__callee, &target_fsn, 1);
		break;
	}
	default:
	{
		si_log1_todo("miss callee[%s]\n",
				tree_code_name[TREE_CODE(callee)]);
		return -1;
	}
	}

	if (target_fsn) {
		/*
		 * XXX: check if target_fsn has an alias
		 */
		struct attr_list *attr;
		int has_alias = 0;
		if (unlikely(slist_empty(&target_fsn->attributes))) {
			tree fsn_node;
			struct sibuf *b;
			b = target_fsn->buf;
			int held = analysis__sibuf_hold(b);
			fsn_node = (tree)(long)target_fsn->obj->real_addr;
			get_attributes(&target_fsn->attributes,
					DECL_ATTRIBUTES(fsn_node));
			if (!held)
				analysis__sibuf_drop(b);
		}
		slist_for_each_entry(attr, &target_fsn->attributes, sibling) {
			if (!strcmp("alias", attr->attr_name)) {
				has_alias = 1;
				break;
			}
		}

		if (has_alias) {
			struct attrval_list *attrval;
			char name[NAME_MAX];
			slist_for_each_entry(attrval, &attr->values, sibling) {
				char *ret = si_get_alias_name(name, NAME_MAX,
							(tree)attrval->node);
				if (!ret)
					continue;

				long args[3];
				struct sibuf *b;
				b = find_target_sibuf(attrval->node);
				args[0] = (long)b->rf;
				args[1] = (long)get_builtin_resfile();
				args[2] = (long)name;
				target_fsn = analysis__sinode_search(
							TYPE_FUNC_GLOBAL,
							SEARCH_BY_SPEC,
							(void *)args);
				break;
			}
		}
		struct sinode *caller_fsn;
		get_func_sinode((tree)fnl->fn->node, &caller_fsn, 1);
		analysis__add_callee(caller_fsn, target_fsn, gs,
					SI_TYPE_DF_GIMPLE);
		analysis__add_caller(target_fsn, caller_fsn,
					callee_alias_add_caller);;

		target_fn = (struct func_node *)target_fsn->data;
	}
	if (!target_fn) {
		const char *name = NULL;
		if (__callee) {
			name = IDENTIFIER_POINTER(DECL_NAME(__callee));
		}
		si_log1_todo("target func_node not found. %s, %s\n",
				tree_code_name[TREE_CODE(callee)], name);
		return -1;
	}

	struct sibuf *b = find_target_sibuf((void *)target_fn->node);
	int held = analysis__sibuf_hold(b);
	if (!held)
		analysis__sibuf_drop(b);

	/* FIXME: what is static chain for a GIMPLE_CALL, op[2]? */
	sample_empty_arg_head(sample);
	for (unsigned i = 0; i < gimple_call_num_args(stmt); i++) {
		tree arg = gimple_call_arg(stmt, i);
		struct data_state_rw *tmpds;
		tmpds = get_ds_via_tree(sset, idx, fnl, arg, NULL, NULL);
		if (!tmpds) {
			si_log1_todo("data state not found\n");
			return -1;
		}

		/*
		 * XXX: note the tmpds may already insert into fnl.
		 */
		if (!slist_in_head(&tmpds->base.sibling,
					&fnl->data_state_list)) {
			ds_hold(tmpds);
			slist_add_tail(&tmpds->base.sibling,
					&sample->arg_head);
		} else {
			struct data_state_rw *tmpds1;
			u32 bits = get_type_bits(arg);

			tmpds1 = ds_dup_base(&tmpds->base);
			dsv_alloc_data(&tmpds1->val, DSVT_REF, 0, 0);
			if (ds_vref_setv(&tmpds1->val, tmpds, 0, bits) == -1) {
				si_log1_warn("ds_vref_setv err\n");
				ds_drop(tmpds1);
				tmpds1 = NULL;
			} else {
				ds_hold(tmpds1);
				slist_add_tail(&tmpds1->base.sibling,
						&sample->arg_head);
				ds_drop(tmpds1);
			}
		}
		ds_drop(tmpds);
	}

	if (!analysis__dec_special_call(sset, idx, fnl, target_fn))
		return 0;

	struct fn_list *fnl_callee;
	fnl_callee = sample_add_new_fn(sample, target_fn);
	if (fnl_init(sset, idx, fnl_callee))
		err = -1;

	return err;
}

static int dec_gimple_return(struct sample_set *sset, int idx,
				struct fn_list *fnl, gimple_seq gs)
{
	struct greturn *stmt;
	stmt = (struct greturn *)gs;
	struct sample_state *sample = sset->samples[idx];

	if ((!sample) || (!fnl)) {
		si_log1_todo("sample and fnl should not be NULL\n");
		return -1;
	}

	/*
	 * Okay, this function is EOL, set the retval, remove its fnl,
	 * that will be all.
	 */
	tree rvtree = gimple_return_retval(stmt);
	struct data_state_rw *dstmp;
	dstmp = get_ds_via_tree(sset, idx, fnl, rvtree, NULL, NULL);
	if (dstmp) {
		ds_hold(dstmp);
		sample->retval = dstmp;
	} else
		sample->retval = (struct data_state_rw *)VOID_RETVAL;
	ds_drop(dstmp);

	slist_del(&fnl->sibling, &sample->fn_list_head);
	fnl_deinit(sset, idx, fnl);
	fn_list_free(fnl);

	return 0;
}

/*
 * return value:
 *	-1: err
 *	1: true
 *	0: false
 */
static int gimple_cond_compare(struct sample_set *sset, int idx,
				struct fn_list *fnl,
				struct data_state_rw *lhs,
				struct data_state_rw *rhs,
				enum tree_code cond_code)
{
	/* If lhs or rhs is unknown, we guess a value */
	int err = -1;
	cur_max_signint _retval;
	struct data_state_val *lhs_val, *rhs_val;
	lhs_val = get_ds_val(sset, idx, fnl, &lhs->val, 0, 0);
	rhs_val = get_ds_val(sset, idx, fnl, &rhs->val, 0, 0);
	if ((!lhs_val) || (!rhs_val)) {
		si_log1_todo("get_ds_val err\n");
		return -1;
	}

	dsv_extend(lhs_val);
	dsv_extend(rhs_val);

	switch (cond_code) {
	case EQ_EXPR:
	{
		err = analysis__dsv_compute(lhs_val, rhs_val,
					    CLIB_COMPUTE_F_COMPARE,
					    DSV_COMP_F_EQ,
					    &_retval);
		break;
	}
	case GE_EXPR:
	{
		err = analysis__dsv_compute(lhs_val, rhs_val,
					    CLIB_COMPUTE_F_COMPARE,
					    DSV_COMP_F_GE,
					    &_retval);
		break;
	}
	case GT_EXPR:
	{
		err = analysis__dsv_compute(lhs_val, rhs_val,
					    CLIB_COMPUTE_F_COMPARE,
					    DSV_COMP_F_GT,
					    &_retval);
		break;
	}
	case LE_EXPR:
	{
		err = analysis__dsv_compute(lhs_val, rhs_val,
					    CLIB_COMPUTE_F_COMPARE,
					    DSV_COMP_F_LE,
					    &_retval);
		break;
	}
	case LT_EXPR:
	{
		err = analysis__dsv_compute(lhs_val, rhs_val,
					    CLIB_COMPUTE_F_COMPARE,
					    DSV_COMP_F_LT,
					    &_retval);
		break;
	}
	case NE_EXPR:
	{
		err = analysis__dsv_compute(lhs_val, rhs_val,
					    CLIB_COMPUTE_F_COMPARE,
					    DSV_COMP_F_NE,
					    &_retval);
		break;
	}
	default:
	{
		si_log1_todo("miss %s\n", tree_code_name[cond_code]);
		break;
	}
	}

	if (err == -1) {
		si_log1_warn("analysis__dsv_compute err\n");
		return -1;
	}

	return _retval ? 1 : 0;
}

static int dec_gassign_array_to_pointer(tree result, tree rhs1,
					struct data_state_val *rhs1_dsv)
{
	if ((TREE_CODE(TREE_TYPE(result)) == POINTER_TYPE) &&
		(TREE_CODE(rhs1) == ADDR_EXPR) &&
		(TREE_CODE(TREE_TYPE(TREE_OPERAND(rhs1, 0))) == ARRAY_TYPE)) {
		if (DSV_TYPE(rhs1_dsv) != DSVT_ADDR) {
			si_log1_warn("Should not happen\n");
			return -1;
		}

		if (DS_VTYPE(DSV_SEC2_VAL(rhs1_dsv)->ds) != DSVT_CONSTRUCTOR) {
			si_log1_warn("Should not happen\n");
			return -1;
		}

		size_t this_bits;
		this_bits = get_type_bytes(TREE_TYPE(TREE_TYPE(result))) *
				BITS_PER_UNIT;
		DSV_SEC2_VAL(rhs1_dsv)->bits = this_bits;

		return 1;
	} else {
		return 0;
	}
}

static int dec_gimple_assign(struct sample_set *sset, int idx,
				struct fn_list *fnl, gimple_seq gs)
{
	int err = 0;
	struct gassign *stmt;
	stmt = (struct gassign *)gs;

	tree lhs, rhs1, rhs2, rhs3;
	enum tree_code rhs_code;
	lhs = gimple_assign_lhs(stmt);
	rhs1 = gimple_assign_rhs1(stmt);
	rhs2 = gimple_assign_rhs2(stmt);
	rhs3 = gimple_assign_rhs3(stmt);
	rhs_code = gimple_assign_rhs_code(stmt);

	u32 bytes = get_type_bytes(lhs);

	struct data_state_rw *lhs_state, *rhs1_state, *rhs2_state, *rhs3_state;
	int ssa_write = 1;
	lhs_state = get_ds_via_tree(sset, idx, fnl, lhs, NULL, &ssa_write);
	rhs1_state = get_ds_via_tree(sset, idx, fnl, rhs1, NULL, NULL);
	rhs2_state = get_ds_via_tree(sset, idx, fnl, rhs2, NULL, NULL);
	rhs3_state = get_ds_via_tree(sset, idx, fnl, rhs3, NULL, NULL);
	if ((lhs && (!lhs_state)) ||
			(rhs1 && (!rhs1_state)) ||
			(rhs2 && (!rhs2_state)) ||
			(rhs3 && (!rhs3_state))) {
		si_log1_todo("not handled\n");
		return -1;
	}

	struct data_state_val *rhs1_val, *rhs2_val, *rhs3_val __maybe_unused;
	struct data_state_val *lhs_val;
	lhs_val = get_ds_val(sset, idx, fnl, &lhs_state->val, 0, 0);
	rhs1_val = get_ds_val(sset, idx, fnl, &rhs1_state->val, 0, 0);
	rhs2_val = get_ds_val(sset, idx, fnl, &rhs2_state->val, 0, 0);
	rhs3_val = get_ds_val(sset, idx, fnl, &rhs3_state->val, 0, 0);

	switch (rhs_code) {
	case GT_EXPR:
	case GE_EXPR:
	case LE_EXPR:
	case LT_EXPR:
	case NE_EXPR:
	case EQ_EXPR:
	{
		if (rhs3) {
			err = -1;
			si_log1_warn("rhs3 is supposed to be NULL\n");
			break;
		}

		dsv_extend(rhs1_val);
		dsv_extend(rhs2_val);

		int extra_flag = DSV_COMP_F_UNK;
		cur_max_signint _retval;
		if (rhs_code == GT_EXPR)
			extra_flag = DSV_COMP_F_GT;
		else if (rhs_code == GE_EXPR)
			extra_flag = DSV_COMP_F_GE;
		else if (rhs_code == LE_EXPR)
			extra_flag = DSV_COMP_F_LE;
		else if (rhs_code == LT_EXPR)
			extra_flag = DSV_COMP_F_LT;
		else if (rhs_code == NE_EXPR)
			extra_flag = DSV_COMP_F_NE;
		else if (rhs_code == EQ_EXPR)
			extra_flag = DSV_COMP_F_EQ;
		BUG_ON(extra_flag == DSV_COMP_F_UNK);

		err = analysis__dsv_compute(rhs1_val, rhs2_val,
					    CLIB_COMPUTE_F_COMPARE,
					    extra_flag,
					    &_retval);
		if (err == -1) {
			si_log1_warn("analysis__dsv_compute err\n");
			break;
		}

		dsv_alloc_data(lhs_val, DSVT_INT_CST, 0, bytes);
		*(char *)(DSV_SEC1_VAL(lhs_val)) = _retval ? 1 : 0;
		break;
	}
	case BIT_NOT_EXPR:
	{
		if (rhs2 || rhs3) {
			err = -1;
			si_log1_warn("rhs2/rhs3 is supposed to be NULL\n");
			break;
		}

		dsv_extend(rhs1_val);

		int flag = CLIB_COMPUTE_F_BITNOT;
		cur_max_signint _retval;

		/* XXX: need to alloc BEFORE dsv_compute() */
		dsv_alloc_data(lhs_val, DSVT_INT_CST, 0, bytes);
		err = analysis__dsv_compute(rhs1_val, lhs_val, flag, 0,
					    &_retval);
		if (err == -1) {
			si_log1_warn("analysis__dsv_compute err\n");
			break;
		}
		clib_memcpy_bits(DSV_SEC1_VAL(lhs_val), bytes * BITS_PER_UNIT,
				 &_retval, sizeof(_retval) * BITS_PER_UNIT);
		break;
	}
	case BIT_IOR_EXPR:
	case BIT_XOR_EXPR:
	case BIT_AND_EXPR:
	case PLUS_EXPR:
	case MINUS_EXPR:
	case MULT_EXPR:
	case RDIV_EXPR:
	case TRUNC_DIV_EXPR:
	case TRUNC_MOD_EXPR:
	{
		if (rhs3) {
			err = -1;
			si_log1_warn("rhs3 is supposed to be NULL\n");
			break;
		}

		dsv_extend(rhs1_val);
		dsv_extend(rhs2_val);

		int flag = CLIB_COMPUTE_F_UNK;
		cur_max_signint _retval;
		if (rhs_code == BIT_IOR_EXPR)
			flag = CLIB_COMPUTE_F_BITIOR;
		else if (rhs_code == BIT_XOR_EXPR)
			flag = CLIB_COMPUTE_F_BITXOR;
		else if (rhs_code == BIT_AND_EXPR)
			flag = CLIB_COMPUTE_F_BITAND;
		else if (rhs_code == PLUS_EXPR)
			flag = CLIB_COMPUTE_F_ADD;
		else if (rhs_code == MINUS_EXPR)
			flag = CLIB_COMPUTE_F_SUB;
		else if (rhs_code == MULT_EXPR)
			flag = CLIB_COMPUTE_F_MUL;
		else if (rhs_code == RDIV_EXPR)
			flag = CLIB_COMPUTE_F_DIV;
		else if (rhs_code == TRUNC_DIV_EXPR)
			flag = CLIB_COMPUTE_F_DIV;
		else if (rhs_code == TRUNC_MOD_EXPR)
			flag = CLIB_COMPUTE_F_MOD;
		BUG_ON(flag == CLIB_COMPUTE_F_UNK);

		err = analysis__dsv_compute(rhs1_val, rhs2_val, flag, 0,
					    &_retval);
		if (err == -1) {
			si_log1_warn("analysis__dsv_compute err\n");
			break;
		}

		dsv_alloc_data(lhs_val, DSVT_INT_CST, 0, bytes);
		clib_memcpy_bits(DSV_SEC1_VAL(lhs_val), bytes * BITS_PER_UNIT,
				 &_retval, sizeof(_retval) * BITS_PER_UNIT);
		break;
	}
	case LSHIFT_EXPR:
	case RSHIFT_EXPR:
#if 0
	case LROTATE_EXPR:
	case RROTATE_EXPR:
#endif
	{
		/*
		 * From gccint-9.3.0
		 * These nodes represent left and right shifts, respectively.
		 * The first operand is the value to shift;
		 * it will always be of integral type. The second operand is
		 * an expression for the number of bits by which to shift.
		 * Right shift should be treated as arithmetic, i.e.,
		 * the high-order bits should be zero-filled when the expression
		 * has unsigned type and filled with the sign bit when the
		 * expression has signed type. Note that the result is undefined
		 * if the second operand is larger than or equal to the first
		 * operand’s type size. Unlike most nodes, these can have a
		 * vector as first operand and a scalar as second operand.
		 */
		if (rhs3) {
			err = -1;
			si_log1_warn("rhs3 is supposed to be NULL\n");
			break;
		}

		if (DSV_TYPE(rhs1_val) != DSVT_INT_CST) {
			err = -1;
			si_log1_warn("rhs1 should be INT_CST\n");
			break;
		}

		if (DSV_TYPE(rhs2_val) != DSVT_INT_CST) {
			err = -1;
			si_log1_todo("rhs2 should be INT_CST, now is %d\n",
					DSV_TYPE(rhs2_val));
			break;
		}

		dsv_extend(rhs1_val);
		dsv_extend(rhs2_val);

		int flag = CLIB_COMPUTE_F_UNK;
		cur_max_signint _retval;
		if (rhs_code == LSHIFT_EXPR)
			flag = CLIB_COMPUTE_F_SHL;
		else if (rhs_code == RSHIFT_EXPR)
			flag = CLIB_COMPUTE_F_SHR;
		else if (rhs_code == LROTATE_EXPR)
			flag = CLIB_COMPUTE_F_ROL;
		else if (rhs_code == RROTATE_EXPR)
			flag = CLIB_COMPUTE_F_ROR;
		BUG_ON(flag == CLIB_COMPUTE_F_UNK);

		err = analysis__dsv_compute(rhs1_val, rhs2_val, flag, 0,
						&_retval);
		if (err == -1) {
			si_log1_warn("analysis__dsv_compute err\n");
			break;
		}

		dsv_alloc_data(lhs_val, DSVT_INT_CST, 0, bytes);
		clib_memcpy_bits(DSV_SEC1_VAL(lhs_val), bytes * BITS_PER_UNIT,
				 &_retval, sizeof(_retval) * BITS_PER_UNIT);
		break;
	}
	case POINTER_PLUS_EXPR:
	{
		/*
		 * From gccint-9.3.0
		 * POINTER_PLUS_EXPR:
		 *	This node represents pointer arithmetic.
		 *	The first operand is always a pointer/reference type.
		 *	The second operand is always an unsigned integer type
		 *	compatible with sizetype. This and POINTER DIFF EXPR
		 *	are the only binary arithmetic operators that can
		 *	operate on pointer types.
		 */
		if (rhs3) {
			err = -1;
			si_log1_warn("rhs3 is supposed to be NULL\n");
			break;
		}

		if (DSV_TYPE(rhs2_val) != DSVT_INT_CST) {
			err = -1;
			si_log1_warn("rhs2_val type must be INT_CST\n");
			break;
		}

		dsv_extend(rhs2_val);

		if (DSV_TYPE(rhs1_val) == DSVT_ADDR) {
			/*
			 * Looks like we do NOT need to get the element size
			 * this pointer point to. The rhs2 is the right offset.
			 *
			 * Also, if we want to get the element size, we should
			 * use rhs1, not the rhs1_val->raw.
			 */
#if 0
			tree n = (tree)(long)rhs1_val->raw;
			tree type = TREE_TYPE(n);
			if (TREE_CODE(type) != POINTER_TYPE) {
				err = -1;
				si_log1_todo("miss %s\n",
						tree_code_name[TREE_CODE(type)]);
				break;
			}

			tree ptype = TREE_TYPE(type);
			size_t sz = TREE_INT_CST_LOW(TYPE_SIZE_UNIT(ptype));
#endif
			size_t idx = 0;
			clib_memcpy_bits(&idx, sizeof(idx) * BITS_PER_UNIT,
					 DSV_SEC1_VAL(rhs2_val),
					 rhs2_val->info.v1_info.bytes *
						BITS_PER_UNIT);

			struct data_state_rw *newds;
			s32 new_offset;
			u32 new_bits;
			newds = DSV_SEC2_VAL(rhs1_val)->ds;
			new_offset = DSV_SEC2_VAL(rhs1_val)->offset +
					idx * BITS_PER_UNIT;
			new_bits = DSV_SEC2_VAL(rhs1_val)->bits;

			if (lhs_val == rhs1_val) {
				DSV_SEC2_VAL(lhs_val)->offset = new_offset;
			} else {
				dsv_alloc_data(lhs_val, DSVT_ADDR, 0, 0);
				err = ds_vref_hold_setv(lhs_val, newds,
							new_offset, new_bits);
				if (err == -1) {
					si_log1_warn("ds_vref_hold_setv err\n");
					break;
				}
			}
		} else if (DSV_TYPE(rhs1_val) == DSVT_INT_CST) {
			/* FIXME: rhs1_val MUST be NULL. */
			for (u32 i = 0; i < rhs1_val->info.v1_info.bytes; i++) {
				if (((char *)DSV_SEC1_VAL(rhs1_val))[i]) {
					si_log1_warn("should not happen\n");
					err = -1;
					break;
				}
			}
			if (err == -1)
				break;

			/* zero lhs_val */
			dsv_alloc_data(lhs_val, DSVT_INT_CST,
					0, rhs1_val->info.v1_info.bytes);
		} else {
			err = -1;
			si_log1_todo("miss %d\n", DSV_TYPE(rhs1_val));
		}

		break;
	}
	case POINTER_DIFF_EXPR:
	{
		/*
		 * From gccint-9.3.0:
		 * POINTER_DIFF_EXPR
		 *	This node represents pointer subtraction.
		 *	The two operands always have pointer/reference type.
		 *	It returns a signed integer of the same precision as the
		 *	pointers. The behavior is undefined if the difference of
		 *	the two pointers, seen as infinite precision
		 *	non-negative integers, does not fit in the result type.
		 *	The result does not depend on the pointer type,
		 *	it is not divided by the size of the pointed-to type.
		 */
		if (rhs3) {
			err = -1;
			si_log1_warn("rhs3 is supposed to be NULL\n");
			break;
		}

		if (DSV_TYPE(rhs1_val) != DSV_TYPE(rhs2_val)) {
			err = -1;
			si_log1_warn("rhs1 and rhs2 have different vtype\n");
			break;
		}

		if (DSV_TYPE(rhs1_val) == DSVT_ADDR) {
			if (DSV_SEC2_VAL(rhs1_val)->ds !=
				DSV_SEC2_VAL(rhs2_val)->ds) {
				err = -1;
				si_log1_todo("rhs1 rhs2 have different base\n");
				break;
			}

			size_t sub_val;
			sub_val = DSV_SEC2_VAL(rhs1_val)->offset -
					DSV_SEC2_VAL(rhs2_val)->offset;
			dsv_alloc_data(lhs_val, DSVT_INT_CST, 0, bytes);
			clib_memcpy_bits(DSV_SEC1_VAL(lhs_val),
					 bytes * BITS_PER_UNIT,
					 &sub_val,
					 sizeof(sub_val) * BITS_PER_UNIT);
		} else {
			err = -1;
			si_log1_todo("miss %d\n", DSV_TYPE(rhs1_val));
		}

		break;
	}
	case MEM_REF:
	{
		if (rhs2 || rhs3) {
			err = -1;
			si_log1_warn("rhs2 and rhs3 are supposed to be NULL\n");
			break;
		}

		/* The rhs1_val should be it */
		switch (DSV_TYPE(rhs1_val)) {
		case DSVT_INT_CST:
		{
			dsv_extend(rhs1_val);

			dsv_alloc_data(lhs_val, DSVT_INT_CST, 0, bytes);
			clib_memcpy_bits(DSV_SEC1_VAL(lhs_val),
					 bytes * BITS_PER_UNIT,
					 DSV_SEC1_VAL(rhs1_val),
					 rhs1_val->info.v1_info.bytes *
						BITS_PER_UNIT);
			break;
		}
		case DSVT_ADDR:
		case DSVT_CONSTRUCTOR:
		{
			err = dsv_copy_data(lhs_val, rhs1_val);
			if (err == -1) {
				si_log1_warn("dsv_copy_data err\n");
			}
			break;
		}
		default:
		{
			err = -1;
			si_log1_todo("miss %d\n", DSV_TYPE(rhs1_val));
			break;
		}
		}

		break;
	}
	case ARRAY_REF:
	case COMPONENT_REF:
	{
		if (rhs2 || rhs3) {
			err = -1;
			si_log1_warn("rhs2 and rhs3 are supposed to be NULL\n");
			break;
		}

		dsv_extend(rhs1_val);

		/*
		 * XXX: we can not just setup a DSVT_REF here
		 * we need to copy that field data to lhs_state
		 */
		tree n = (tree)(long)rhs1_val->raw;
		tree type;
		struct sibuf *b;
		b = find_target_sibuf((void *)n);
		int held = analysis__sibuf_hold(b);
		if (TREE_CODE_CLASS(TREE_CODE(n)) != tcc_type)
			type = TREE_TYPE(n);
		else
			type = n;

		switch (DSV_TYPE(rhs1_val)) {
		case DSVT_INT_CST:
		{
			dsv_alloc_data(lhs_val, DSVT_INT_CST, 0, bytes);
			clib_memcpy_bits(DSV_SEC1_VAL(lhs_val),
					 bytes * BITS_PER_UNIT,
					 DSV_SEC1_VAL(rhs1_val),
					 TREE_INT_CST_LOW(TYPE_SIZE(type)));
			break;
		}
		case DSVT_ADDR:
		case DSVT_REF:
		{
			/* in case the lhs_val and rhs1_val are the same */
			struct data_state_rw *target_ds;
			s32 target_offset = DSV_SEC2_VAL(rhs1_val)->offset;
			u32 target_bits = DSV_SEC2_VAL(rhs1_val)->bits;
			target_ds = DSV_SEC2_VAL(rhs1_val)->ds;

			if (lhs_val != rhs1_val) {
				dsv_alloc_data(lhs_val, DSV_TYPE(rhs1_val),
					       0, 0);
				err = ds_vref_hold_setv(lhs_val, target_ds,
							target_offset,
							target_bits);
				if (err == -1) {
					si_log1_warn("ds_vref_hold_setv err\n");
				}
			}
			break;
		}
		case DSVT_CONSTRUCTOR:
		{
			/* TODO: be careful */
			dsv_copy_data(lhs_val, rhs1_val);
			break;
		}
		default:
		{
			err = -1;
			si_log1_todo("miss %d\n", DSV_TYPE(rhs1_val));
			break;
		}
		}
		if (!held)
			analysis__sibuf_drop(b);
		break;
	}
	case MIN_EXPR:
	case MAX_EXPR:
	{
		if (rhs3) {
			err = -1;
			si_log1_warn("rhs3 is supposed to be NULL\n");
			break;
		}

		if (DSV_TYPE(rhs1_val) != DSV_TYPE(rhs2_val)) {
			err = -1;
			si_log1_warn("rhs1 rhs2 have different vtype\n");
			break;
		}

		if (DSV_TYPE(rhs1_val) != DSVT_INT_CST) {
			err = -1;
			si_log1_todo("rhs1 rhs2 are not INT_CST");
			break;
		}

		dsv_extend(rhs1_val);
		dsv_extend(rhs2_val);

		cur_max_signint _retval;
		err = analysis__dsv_compute(rhs1_val, rhs2_val,
						CLIB_COMPUTE_F_COMPARE,
						DSV_COMP_F_GE, &_retval);
		if (err == -1) {
			si_log1_warn("analysis__dsv_compute err\n");
			break;
		}

		struct data_state_val *target_dsv;
		if (rhs_code == MIN_EXPR) {
			if (_retval)
				target_dsv = rhs2_val;
			else
				target_dsv = rhs1_val;
		} else if (rhs_code == MAX_EXPR) {
			if (_retval)
				target_dsv = rhs1_val;
			else
				target_dsv = rhs2_val;
		}

		tree n = (tree)(long)target_dsv->raw;
		size_t bytes = get_type_bytes(n);
		dsv_alloc_data(lhs_val, DSVT_INT_CST, 0, bytes);
		clib_memcpy_bits(DSV_SEC1_VAL(lhs_val), bytes * BITS_PER_UNIT,
				 DSV_SEC1_VAL(target_dsv),
				 bytes * BITS_PER_UNIT);
		break;
	}
	case SSA_NAME:
	case VAR_DECL:
	case PARM_DECL:
	{
		if (rhs2 || rhs3) {
			err = -1;
			si_log1_warn("rhs2 rhs3 are supposed to be NULL\n");
			break;
		}

		dsv_extend(rhs1_val);
		err = dsv_copy_data(lhs_val, rhs1_val);
		if (err == -1) {
			si_log1_warn("dsv_copy_data err\n");
		}
		break;
	}
	case INTEGER_CST:
	{
		if (rhs2 || rhs3) {
			err = -1;
			si_log1_warn("rhs2 rhs3 are supposed to be NULL\n");
			break;
		}

		dsv_extend(rhs1_val);
		err = dsv_copy_data(lhs_val, rhs1_val);
		if (err == -1) {
			si_log1_warn("dsv_copy_data err\n");
		}
		break;
	}
	case STRING_CST:
	{
		if (rhs2 || rhs3) {
			err = -1;
			si_log1_warn("rhs2 rhs3 are supposed to be NULL\n");
			break;
		}

		/*
		 * one special case here in linux kernel ecryptfs_from_hex():
		 *	char tmp[3] = { 0, };
		 * the value is represented as a STRING_CST, but the
		 * lhs_state is an ARRAY.
		 */
		if (DSV_TYPE(lhs_val) != DSV_TYPE(rhs1_val)) {
			si_log1_warn("Should not happen, (%d %d)\n",
					DSV_TYPE(lhs_val), DSV_TYPE(rhs1_val));
		} else {
			err = dsv_copy_data(lhs_val, rhs1_val);
			if (err == -1) {
				si_log1_warn("dsv_copy_data err\n");
			}
		}

		break;
	}
	case ADDR_EXPR:
	{
		if (rhs2 || rhs3) {
			err = -1;
			si_log1_warn("rhs2 rhs3 are supposed to be NULL\n");
			break;
		}

		/*
		 * FIXME: I can not remember why
		 * `do not use rhs1_val as the src`
		 */
		int _rv;
		_rv = dec_gassign_array_to_pointer(lhs, rhs1, rhs1_val);
		if (!_rv) {
			err = dsv_copy_data(lhs_val, &rhs1_state->val);
			if (err == -1) {
				si_log1_warn("dsv_copy_data err\n");
			}
		} else if (_rv == -1) {
			err = -1;
			break;
		} else {
			err = dsv_copy_data(lhs_val, rhs1_val);
			if (err == -1) {
				si_log1_warn("dsv_copy_data err\n");
			}
		}
		break;
	}
	case BIT_FIELD_REF:
	{
		if (rhs2 || rhs3) {
			err = -1;
			si_log1_warn("rhs2 rhs3 are supposed to be NULL\n");
			break;
		}

		dsv_extend(rhs1_val);
		err = dsv_copy_data(lhs_val, rhs1_val);
		if (err == -1) {
			si_log1_warn("dsv_copy_data err\n");
		}
		break;
	}
	case CONSTRUCTOR:
	{
		if (rhs2 || rhs3) {
			err = -1;
			si_log1_warn("rhs2 rhs3 are supposed to be NULL\n");
			break;
		}

		if ((DSV_TYPE(&rhs1_state->val) == DSVT_CONSTRUCTOR) &&
				DSV_SEC3_VAL(&rhs1_state->val)) {
			err = dsv_copy_data(lhs_val, &rhs1_state->val);
			if (err == -1) {
				si_log1_warn("dsv_copy_data err\n");
			}
		} else if ((DSV_TYPE(rhs1_val) == DSVT_CONSTRUCTOR) &&
				DSV_SEC3_VAL(rhs1_val)) {
			err = dsv_copy_data(lhs_val, rhs1_val);
			if (err == -1) {
				si_log1_warn("dsv_copy_data err\n");
			}
		}
		break;
	}
	case NOP_EXPR:
	{
		if (rhs2 || rhs3) {
			err = -1;
			si_log1_warn("rhs2 rhs3 are supposed to be NULL\n");
			break;
		}

		if (DSV_TYPE(lhs_val) == DSV_TYPE(rhs1_val)) {
			err = dsv_copy_data(lhs_val, rhs1_val);
			if (err == -1) {
				si_log1_warn("dsv_copy_data err\n");
			}
		} else {
			si_log1_todo("not handled. "
					"lhs_state: %p, rhs1_state: %p\n",
					lhs_state, rhs1_state);
			err = -1;
			break;
		}
		break;
	}
	default:
	{
		char loc[1024];
		gimple_loc_string(loc, 1024, gs);
		si_log1_todo("miss %s, %s\n", tree_code_name[rhs_code], loc);
		err = -1;
		break;
	}
	}

	if (ssa_write) {
		if (lhs_val == &lhs_state->val) {
			si_log1_warn("Should not happen\n");
		} else {
			err = dsv_copy_data(&lhs_state->val, lhs_val);
			if (err == -1) {
				si_log1_warn("dsv_copy_data err\n");
			}
		}
	}

	ds_drop(lhs_state);
	ds_drop(rhs1_state);
	ds_drop(rhs2_state);
	ds_drop(rhs3_state);

	if (!err)
		fnl->curpos = (void *)cp_next_gimple(fnl->fn, gs);

	return err;
}

static int dec_gimple_cond(struct sample_set *sset, int idx,
				struct fn_list *fnl, gimple_seq gs)
{
	struct gcond *stmt;
	stmt = (struct gcond *)gs;
	enum tree_code cond_code;
	cond_code = gimple_cond_code(gs);
	tree lhs = gimple_cond_lhs(gs);
	tree rhs = gimple_cond_rhs(gs);
	struct data_state_rw *lhs_state = NULL, *rhs_state = NULL;
	struct data_state_val *lhs_dsv = NULL, *rhs_dsv = NULL;

	/* XXX: decide which cp to go */
	struct code_path *this_cp = find_cp_by_gs(fnl->fn, gs);
	struct code_path *next_cp = NULL;
	if (si_gimple_cond_true_p(stmt)) {
		next_cp = this_cp->next[1];
	} else if (si_gimple_cond_false_p(stmt)) {
		next_cp = this_cp->next[0];
	} else {
		lhs_state = get_ds_via_tree(sset, idx, fnl, lhs, NULL, NULL);
		rhs_state = get_ds_via_tree(sset, idx, fnl, rhs, NULL, NULL);
		if ((lhs && (!lhs_state)) || (rhs && (!rhs_state))) {
			si_log1_warn("Should not happen\n");
			return -1;
		}

		int err = gimple_cond_compare(sset, idx, fnl,
					  lhs_state, rhs_state, cond_code);
		if (err == 1) {
			next_cp = this_cp->next[1];
		} else if (err == 0) {
			next_cp = this_cp->next[0];
		}

		if (next_cp) {
			lhs_dsv = get_ds_val(sset, idx, fnl,
						&lhs_state->val, 0, 0);
			rhs_dsv = get_ds_val(sset, idx, fnl,
						&rhs_state->val, 0, 0);

			int _err;
			_err = analysis__sample_state_check_loop(sset, idx,
						lhs_dsv, rhs_dsv, next_cp);
			if (_err == -1) {
				si_log1_warn("analysis__sample_state_check_loop"
						" err\n");
				err = -1;
			} else if (_err == 1) {
				sample_set_set_flag(sset, SAMPLE_SF_INFLOOP);
			}
		}

		ds_drop(lhs_state);
		ds_drop(rhs_state);
	}

	if (!next_cp) {
		char loc[1024];
		gimple_loc_string(loc, 1024, gs);
		si_log1_todo("next_cp NULL(gimple_cond_compare err?), %s\n",
				loc);
		return -1;
	}

	/*
	 * XXX: check infinite loop here.
	 * First, we need to know if we are in a loop now,
	 * Then, we need to know the condition to jump out this loop.
	 * Then, we should check if the lhs_state/rhs_state different from the
	 *	saved ones.
	 */

	fnl->curpos = (void *)cp_first_gimple(next_cp);
	sample_add_new_cp(sset->samples[idx], next_cp);
	fnl_add_new_cp(fnl, next_cp);

	return 0;
}

static int dec_gimple_switch(struct sample_set *sset, int idx,
				struct fn_list *fnl, gimple_seq gs)
{
	struct gswitch *stmt = (struct gswitch *)gs;
	tree index = gimple_switch_index(stmt);
	struct data_state_rw *index_ds;
	struct data_state_val *index_dsv;
	index_ds = get_ds_via_tree(sset, idx, fnl, index, NULL, NULL);
	if (index && (!index_ds)) {
		si_log1_todo("not handled\n");
		return -1;
	}

	/* get the index value first */
	u64 val = 0;
	u32 bits = get_type_bits(index);
	index_dsv = get_ds_val(sset, idx, fnl, &index_ds->val, 0, 0);
	BUG_ON(DSV_TYPE(index_dsv) != DSVT_INT_CST);
	clib_memcpy_bits(&val, sizeof(val) * BITS_PER_UNIT,
			 DSV_SEC1_VAL(index_dsv), bits);
	ds_drop(index_ds);

	/* fake a tree_int_cst node */
	struct tree_int_cst fake_tree;
	memcpy(&fake_tree, si_integer_zero_node((void *)gs),
		sizeof(fake_tree));
	TREE_INT_CST_ELT((tree)&fake_tree, 0) = val;

	edge t = si_find_taken_edge_switch_expr(fnl->fn, stmt,
						(tree)&fake_tree);
	if (!t) {
		si_log1_todo("target edge not found\n");
		return -1;
	}

	struct code_path *next_cp;
	next_cp = find_cp_by_bb(fnl->fn, t->dest);
	if (!next_cp) {
		si_log1_todo("next_cp not found\n");
		return -1;
	}

	fnl->curpos = (void *)cp_first_gimple(next_cp);
	sample_add_new_cp(sset->samples[idx], next_cp);
	fnl_add_new_cp(fnl, next_cp);

	return 0;
}

static int dec_gimple_phi(struct sample_set *sset, int idx,
				struct fn_list *fnl, gimple_seq gs)
{
	int err = 0;
	struct gphi *stmt;
	stmt = (struct gphi *)gs;

	struct code_path *prev_cp;
	struct cp_list *prev_cpl;
	prev_cpl = slist_prev_entry(fnl_last_cpl(fnl), &fnl->cp_list, sibling);
	prev_cp = prev_cpl->cp;

	int complete_type_p;
	tree result = gimple_phi_result(gs);
	struct data_state_rw *result_ds, *src_ds = NULL;
	struct data_state_val *result_dsv, *src_dsv;
	unsigned nargs = gimple_phi_num_args(gs);
	unsigned i = 0;
	tree def;
	basic_block src, prev_bb = (basic_block)prev_cp->cp;

	int ssa_write = 1;
	result_ds = get_ds_via_tree(sset, idx, fnl, result, &complete_type_p,
				    &ssa_write);
	if (!complete_type_p) {
		/* ignore VOID_TYPE */
		goto out;
	} else if (!result_ds) {
		si_log1_warn("result_ds not found\n");
		return -1;
	}

	for (i = 0; i < nargs; i++) {
		basic_block _prev_bb = prev_bb;
		def = gimple_phi_arg_def(gs, i);
		src = gimple_phi_arg_edge(stmt, i)->src;

		int found = 0;
		while (1) {
			if (_prev_bb == src) {
				found = 1;
				break;
			}

			vec<edge,va_gc> *out = _prev_bb->succs;
			struct vec_prefix *pfx = &out->vecpfx;
			size_t cnt = pfx->m_num;
			edge *addr = &out->vecdata[0];
			edge t = addr[0];
			if ((cnt == 1) && (t->flags & EDGE_FALLTHRU))
				_prev_bb = t->dest;
			else
				break;
		}
		if (!found)
			continue;

		/* Okay, we found the previous code_path */
		src_ds = get_ds_via_tree(sset, idx, fnl, def, &complete_type_p,
					 NULL);
		if (!complete_type_p)
			goto out;

		break;
	}
	if (!src_ds) {
		si_log1_warn("src_ds not found\n");
		ds_drop(result_ds);
		return -1;
	}

	result_dsv = get_ds_val(sset, idx, fnl, &result_ds->val, 0, 0);
	src_dsv = get_ds_val(sset, idx, fnl, &src_ds->val, 0, 0);

	/*
	 * If assign an ARRAY to a pointer, we should get the [0] element's
	 * offset and bits.
	 */
	if (dec_gassign_array_to_pointer(result, def, src_dsv) == -1) {
		ds_drop(src_ds);
		ds_drop(result_ds);
		return -1;
	}

	err = dsv_copy_data(result_dsv, src_dsv);
	if (err == -1) {
		si_log1_warn("dsv_copy_data err\n");
		ds_drop(src_ds);
		ds_drop(result_ds);
		return -1;
	}
	if (ssa_write) {
		if (result_dsv == &result_ds->val) {
			si_log1_warn("Should not happen\n");
		} else {
			err = dsv_copy_data(&result_ds->val, result_dsv);
			if (err == -1) {
				si_log1_warn("dsv_copy_data err\n");
				ds_drop(src_ds);
				ds_drop(result_ds);
				return -1;
			}
		}
	}

out:
	ds_drop(src_ds);
	ds_drop(result_ds);
	fnl->curpos = (void *)cp_next_gimple(fnl->fn, gs);
	return 0;
}

static int dec_ignore(struct sample_set *sset, int idx,
				struct fn_list *fnl, gimple_seq gs)
{
	fnl->curpos = (void *)cp_next_gimple(fnl->fn, gs);
	return 0;
}

static int dec_nop(struct sample_set *sset, int idx, struct fn_list *fnl,
			gimple_seq gs)
{
	return dec_ignore(sset, idx, fnl, gs);
}

static int dec_adjust_cp(struct sample_set *sset, int idx, struct fn_list *fnl)
{
	struct code_path *prev_cp;
	struct code_path *dest_cp;
	prev_cp = fnl_last_cpl(fnl)->cp;

	do {
		if (!prev_cp) {
			si_log1_todo("no previous code_path found\n");
			return -1;
		}

		basic_block bb = (basic_block)prev_cp->cp;
		vec<edge,va_gc> *out;
		out = bb->succs;
		if (!out) {
			si_log1_warn("no succs in bb(%p), func(%s)\n",
					bb, fnl->fn->name);
			return -1;
		}

		struct vec_prefix *pfx;
		edge *addr;
		size_t cnt;
		edge t;

		pfx = &out->vecpfx;
		cnt = pfx->m_num;
		addr = &out->vecdata[0];
		t = addr[0];
		if ((cnt != 1) || (!(t->flags & EDGE_FALLTHRU))) {
			si_log1_todo("bb is supposed to have a FALLTHRU edge\n");
			return -1;
		}

		dest_cp = find_cp_by_bb(fnl->fn, t->dest);
		if (!dest_cp) {
			si_log1_todo("dest code_path not found\n");
			return -1;
		}

		fnl->curpos = cp_first_gimple(dest_cp);
		prev_cp = dest_cp;
	} while (!fnl->curpos);

	sample_add_new_cp(sset->samples[idx], dest_cp);
	fnl_add_new_cp(fnl, dest_cp);

	return 0;
}

static struct {
	enum gimple_code	gc;
	int			(*cb)(struct sample_set *, int,
					struct fn_list *, gimple_seq);
} dec_cbs[] = {
	{GIMPLE_ASM,			dec_gimple_asm},
	{GIMPLE_CALL,			dec_gimple_call},
	{GIMPLE_RETURN,			dec_gimple_return},
	{GIMPLE_ASSIGN,			dec_gimple_assign},
	{GIMPLE_COND,			dec_gimple_cond},
	{GIMPLE_PHI,			dec_gimple_phi},
	{GIMPLE_GOTO,			NULL},
	{GIMPLE_SWITCH,			dec_gimple_switch},
	{GIMPLE_OMP_PARALLEL,		NULL},
	{GIMPLE_OMP_TASK,		NULL},
	{GIMPLE_OMP_ATOMIC_LOAD,	NULL},
	{GIMPLE_OMP_ATOMIC_STORE,	NULL},
	{GIMPLE_OMP_FOR,		NULL},
	{GIMPLE_OMP_CONTINUE,		NULL},
	{GIMPLE_OMP_SINGLE,		NULL},
	{GIMPLE_OMP_TARGET,		NULL},
	{GIMPLE_OMP_TEAMS,		NULL},
	{GIMPLE_OMP_RETURN,		NULL},
	{GIMPLE_OMP_SECTIONS,		NULL},
	{GIMPLE_OMP_SECTIONS_SWITCH,	NULL},
	{GIMPLE_OMP_MASTER,		NULL},
	{GIMPLE_OMP_TASKGROUP,		NULL},
	{GIMPLE_OMP_SECTION,		NULL},
	{GIMPLE_OMP_GRID_BODY,		NULL},
	{GIMPLE_OMP_ORDERED,		NULL},
	{GIMPLE_OMP_CRITICAL,		NULL},
	{GIMPLE_BIND,			NULL},
	{GIMPLE_TRY,			NULL},
	{GIMPLE_CATCH,			NULL},
	{GIMPLE_EH_FILTER,		NULL},
	{GIMPLE_EH_MUST_NOT_THROW,	NULL},
	{GIMPLE_EH_ELSE,		NULL},
	{GIMPLE_RESX,			NULL},
	{GIMPLE_EH_DISPATCH,		NULL},
	{GIMPLE_DEBUG,			dec_ignore},
	{GIMPLE_PREDICT,		dec_ignore},
	{GIMPLE_TRANSACTION,		NULL},
	{GIMPLE_LABEL,			dec_ignore},
	{GIMPLE_NOP,			dec_nop},
};

static int do_dec(struct sample_set *sset, int idx, struct fn_list *fnl,
		  gimple_seq gs)
{
	int ret = 0;
	enum gimple_code gc = gimple_code(gs);
	struct func_node *fn = fnl->fn;
	unsigned i = 0;
	for (; i < (sizeof(dec_cbs) / sizeof(dec_cbs[0])); i++) {
		if (gc != dec_cbs[i].gc)
			continue;
		if (dec_cbs[i].cb) {
			ret = dec_cbs[i].cb(sset, idx, fnl, gs);
		} else {
			si_log1_todo("miss %s in %s at %p\n",
					gimple_code_name[gc],
					fnl->fn->name, gs);
		}
		break;
	}

	if (i == (sizeof(dec_cbs) / sizeof(dec_cbs[0]))) {
		si_log1_todo("miss %s in %s at %p\n",
				gimple_code_name[gc], fn->name, gs);
		ret = -1;
	}

	return ret;
}

static int dec(struct sample_set *sset, int idx, struct func_node *start_fn)
{
	struct sample_state *sample = sset->samples[idx];
	struct fn_list *fnl = sample_state_last_fnl(sample);
	if (!fnl) {
		fnl = sample_add_new_fn(sample, start_fn);
		if (fnl_init(sset, idx, fnl))
			return -1;
	}

	int ret = 0;
	gimple_seq gs = (gimple_seq)fnl->curpos;
	if (!gs) {
		/* XXX: we may reach the end of a basic block gimple seqs */
		if (dec_adjust_cp(sset, idx, fnl)) {
			si_log1_todo("dec_adjust_cp failed\n");
			return -1;
		} else {
			gs = (gimple_seq)fnl->curpos;
		}
	}
	if (!gs) {
		si_log1_todo("curpos still NULL\n");
		return -1;
	}

	ret = do_dec(sset, idx, fnl, gs);

	return ret;
}
