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
 *	todos and si_log1_todo
 *
 * phase 1-3 are solid now, 4-6 are bad.
 * phase4: init value, mark var parm function ONLY
 * phase5: do direct/indirect calls
 * phase6: do parm calls and some other check
 *
 * dump_function_to_file() in gcc/tree-cfg.c
 *	Dump function_decl fn to file using flags
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
#include "../analysis.h"

/*
 * ************************************************************************
 * main
 * ************************************************************************
 */
static int c_callback(struct sibuf *, int);
static struct lang_ops c_ops;

CLIB_MODULE_NAME(c);
CLIB_MODULE_NEEDED0();

CLIB_MODULE_INIT()
{
	c_ops.callback = c_callback;
	c_ops.type.binary = SI_TYPE_SRC;
	c_ops.type.kernel = SI_TYPE_BOTH;
	c_ops.type.os_type = SI_TYPE_OS_LINUX;
	c_ops.type.type_more = SI_TYPE_MORE_GCC_C;
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
			if (unlikely(fake_addrs[i] == *fake_addr)) {
				*fake_addr = (void *)(unsigned long)
						objs[i].real_addr;
				if (!objs[i].is_adjusted)
					*do_next = 1;
				return;
			}
		}
	}
	for (i = 0; i < loop_limit; i++) {
		if (unlikely(fake_addrs[i] == *fake_addr)) {
			*fake_addr = (void *)(unsigned long)
					objs[i].real_addr;
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
		vec<c_goto_bindings_p, va_gc> *node0 =
					(vec<c_goto_bindings_p, va_gc> *)node;
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

static void __get_type_detail(struct type_node **base, struct list_head *head,
				struct type_node **next, tree node);
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
		struct use_at_list *newua_type;
		struct use_at_list *newua_var;
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
				vnl->var.name = (char *)src_buf_get(
						strlen(name) + 1);
				memcpy(vnl->var.name, name, strlen(name) + 1);
			}
			list_add_tail(&vnl->sibling, &cur_fn->local_vars);
		}

		if (!vnl->var.type)
			__get_type_detail(&vnl->var.type, NULL, NULL,
						TREE_TYPE(n));

		if (vnl->var.type) {
			node_lock_w(vnl->var.type);
			newua_type = use_at_list_find(&vnl->var.type->used_at,
							cur_gimple,
							cur_gimple_op_idx);
			if (!newua_type) {
				newua_type = use_at_list_new();
				newua_type->func_id = cur_fsn->node_id.id;
				newua_type->gimple_stmt = (void *)cur_gimple;
				newua_type->op_idx = cur_gimple_op_idx;
				list_add_tail(&newua_type->sibling,
						&vnl->var.type->used_at);
			}
			node_unlock_w(vnl->var.type);
		}

		node_lock_w(&vnl->var);
		newua_var = use_at_list_find(&vnl->var.used_at,
						cur_gimple,
						cur_gimple_op_idx);
		if (!newua_var) {
			newua_var = use_at_list_new();
			newua_var->func_id = cur_fsn->node_id.id;
			newua_var->gimple_stmt = (void *)cur_gimple;
			newua_var->op_idx = cur_gimple_op_idx;
			list_add_tail(&newua_var->sibling, &vnl->var.used_at);
		}
		node_unlock_w(&vnl->var);

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
		struct use_at_list *newua_type;
		struct use_at_list *newua_var;

		vnl = var_list_find(&cur_fn->args, n);
		if (!vnl) {
			CLIB_DBG_FUNC_EXIT();
			return;
		}

		if (vnl->var.type) {
			node_lock_w(vnl->var.type);
			newua_type = use_at_list_find(&vnl->var.type->used_at,
							cur_gimple,
							cur_gimple_op_idx);
			if (!newua_type) {
				newua_type = use_at_list_new();
				newua_type->func_id = cur_fsn->node_id.id;
				newua_type->gimple_stmt = (void *)cur_gimple;
				newua_type->op_idx = cur_gimple_op_idx;
				list_add_tail(&newua_type->sibling,
						&vnl->var.type->used_at);
			}
			node_unlock_w(vnl->var.type);
		}

		node_lock_w(&vnl->var);
		newua_var = use_at_list_find(&vnl->var.used_at,
						cur_gimple,
						cur_gimple_op_idx);
		if (!newua_var) {
			newua_var = use_at_list_new();
			newua_var->func_id = cur_fsn->node_id.id;
			newua_var->gimple_stmt = (void *)cur_gimple;
			newua_var->op_idx = cur_gimple_op_idx;
			list_add_tail(&newua_var->sibling, &vnl->var.used_at);
		}
		node_unlock_w(&vnl->var);

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

	expanded_location *xloc;
	struct sibuf *b;

	b = find_target_sibuf(n);
	BUG_ON(!b);
	analysis__resfile_load(b);

	xloc = get_location(GET_LOC_VAR, b->payload, n);
	if (si_is_global_var(n, xloc)) {
		/* global vars */
		struct sinode *global_var_sn;
		struct var_node *gvn;
		struct id_list *newgv = NULL;
		long value;
		long val_flag;
		struct use_at_list *newua_type = NULL;
		struct use_at_list *newua_var = NULL;

		get_var_sinode(n, &global_var_sn, 1);
		if (!global_var_sn) {
			value = (long)n;
			val_flag = 1;
		} else {
			value = global_var_sn->node_id.id.id1;
			val_flag = 0;
			gvn = (struct var_node *)global_var_sn->data;
		}

		newgv = id_list_find(&cur_fn->global_vars,
					value, val_flag);
		if (!newgv) {
			newgv = id_list_new();
			newgv->value = value;
			newgv->value_flag = val_flag;
			list_add_tail(&newgv->sibling,
					&cur_fn->global_vars);
		}
		if (val_flag)
			goto out;

		if (gvn->type) {
			node_lock_w(gvn->type);
			newua_type = use_at_list_find(
					&gvn->type->used_at,
					cur_gimple,
					cur_gimple_op_idx);
			if (!newua_type) {
				newua_type = use_at_list_new();
				newua_type->func_id =
						cur_fsn->node_id.id;
				newua_type->gimple_stmt =
						(void *)cur_gimple;
				newua_type->op_idx = cur_gimple_op_idx;
				list_add_tail(&newua_type->sibling,
						&gvn->type->used_at);
			}
			node_unlock_w(gvn->type);
		}

		node_lock_w(gvn);
		newua_var = use_at_list_find(&gvn->used_at,
						cur_gimple,
						cur_gimple_op_idx);
		if (!newua_var) {
			newua_var = use_at_list_new();
			newua_var->func_id = cur_fsn->node_id.id;
			newua_var->gimple_stmt = (void *)cur_gimple;
			newua_var->op_idx = cur_gimple_op_idx;
			list_add_tail(&newua_var->sibling,
					&gvn->used_at);
		}
		node_unlock_w(gvn);

		goto out;
	}

	/* local variable. static variable should be handled */
	while (ctx && (TREE_CODE(ctx) == BLOCK))
		ctx = BLOCK_SUPERCONTEXT(ctx);

	if ((ctx == si_current_function_decl) || (!ctx)) {
		/* current function local vars */
		char name[NAME_MAX];
		struct var_list *newlv = NULL;
		struct use_at_list *newua_type = NULL;
		struct use_at_list *newua_var = NULL;

		newlv = var_list_find(&cur_fn->local_vars,
					(void *)n);
		if (!newlv) {
			newlv = var_list_new((void *)n);
			if (DECL_NAME(n)) {
				memset(name, 0, NAME_MAX);
				get_node_name(DECL_NAME(n), name);
				newlv->var.name = (char *)src_buf_get(
						      strlen(name)+1);
				memcpy(newlv->var.name,
					name, strlen(name)+1);
			}
			list_add_tail(&newlv->sibling,
					&cur_fn->local_vars);
		}

		if (!newlv->var.type)
			__get_type_detail(&newlv->var.type,
						NULL, NULL,
						TREE_TYPE(n));

		if (newlv->var.type) {
			newua_type = use_at_list_find(
					&newlv->var.type->used_at,
					cur_gimple,
					cur_gimple_op_idx);
			if (!newua_type) {
				newua_type = use_at_list_new();
				newua_type->func_id = cur_fsn->node_id.id;
				newua_type->gimple_stmt = (void *)cur_gimple;
				newua_type->op_idx = cur_gimple_op_idx;
				list_add_tail(&newua_type->sibling,
					&newlv->var.type->used_at);
			}
		}

		newua_var = use_at_list_find(&newlv->var.used_at,
						cur_gimple,
						cur_gimple_op_idx);
		if (!newua_var) {
			newua_var = use_at_list_new();
			newua_var->func_id = cur_fsn->node_id.id;
			newua_var->gimple_stmt = (void *)cur_gimple;
			newua_var->op_idx = cur_gimple_op_idx;
			list_add_tail(&newua_var->sibling,
					&newlv->var.used_at);
		}

		if (TREE_STATIC(n) || DECL_INITIAL(n)) {
			do_init_value(&newlv->var, DECL_INITIAL(n));
		}
	}

out:
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

		analysis__resfile_load(fsn->buf);

		struct func_node *fn;
		fn = (struct func_node *)fsn->data;
		if (!fn)
			goto out;

		struct use_at_list *newua;
		node_lock_w(fn);
		newua = use_at_list_find(&fn->used_at, cur_gimple,
						cur_gimple_op_idx);
		if (!newua) {
			newua = use_at_list_new();
			newua->func_id = cur_fsn->node_id.id;
			newua->gimple_stmt = (void *)cur_gimple;
			newua->op_idx = cur_gimple_op_idx;
			list_add_tail(&newua->sibling, &fn->used_at);
		}
		node_unlock_w(fn);

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

	tree oldtree = (tree)(long)old->obj->real_addr;
	tree newtree = (tree)(long)(objs[obj_idx].real_addr);
	struct tree_decl_with_vis *oldvis, *newvis;
	oldvis = (struct tree_decl_with_vis *)oldtree;
	newvis = (struct tree_decl_with_vis *)newtree;

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

	tree oldtree = (tree)(long)old->obj->real_addr;
	tree newtree = (tree)(long)(objs[obj_idx].real_addr);
	struct tree_decl_with_vis *oldvis, *newvis;
	oldvis = (struct tree_decl_with_vis *)oldtree;
	newvis = (struct tree_decl_with_vis *)newtree;

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
	tree oldt = (tree)(long)old->obj->real_addr;
	tree newt = (tree)(long)(objs[obj_idx].real_addr);

	BUG_ON(TREE_CODE_CLASS(TREE_CODE(newt)) !=
				TREE_CODE_CLASS(TREE_CODE(oldt)));

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
static lock_t __get_base_lock;
static void do_get_base(struct sibuf *buf)
{
	CLIB_DBG_FUNC_ENTER();
	/*
	 * XXX, iter the objs, find out types, vars, functions,
	 * get the locations of them(the file, we sinode_new TYPE_FILE)
	 * sinode_new for the new node
	 */
	struct sinode *sn_new, *sn_tmp, *loc_file;

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
		mutex_lock(&__get_base_lock);

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
			analysis__resfile_load(sn_tmp->buf);
			int chk_val = TREE_NAME_CONFLICT_FAILED;
			chk_val = check_conflict(type, xloc, sn_tmp);
			BUG_ON(chk_val == TREE_NAME_CONFLICT_FAILED);
			if (chk_val == TREE_NAME_CONFLICT_DROP) {
				objs[obj_idx].is_dropped = 1;
				mutex_unlock(&__get_base_lock);
				continue;
			} else if (chk_val == TREE_NAME_CONFLICT_REPLACE) {
				behavior = SINODE_INSERT_BH_REPLACE;
				/*
				 * set the obj is_replaced, while do_replace
				 * finished, the sn_new->obj->is_replaced is
				 * set
				 */
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

		BUG_ON(analysis__sinode_insert(sn_new, behavior));
next_loop:
		mutex_unlock(&__get_base_lock);
	}

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
 * if new field_list should be inserted into a list, then head is not NULL,
 * if new field_list should be pointed by next, then next is not NULL
 */
static void __get_type_detail(struct type_node **base, struct list_head *head,
				struct type_node **next, tree node)
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
		__get_type_detail(NULL, NULL, &new_type->next,
					TREE_TYPE(node));
		break;
	}
	case POINTER_BOUNDS_TYPE:
	case BOOLEAN_TYPE:
	case VOID_TYPE:
	{
		new_type = find_type_node(node);
		if (!new_type)
			break;
		if (new_type->is_set)
			break;
		new_type->is_set = 1;
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

		tree enum_list = TYPE_VALUES(node);
		while (enum_list) {
			memset(name, 0, NAME_MAX);
			get_node_name(TREE_PURPOSE(enum_list), name);
			BUG_ON(!name[0]);
			struct var_list *_new_var;
			_new_var = var_list_new((void *)enum_list);
			_new_var->var.name =
					(char *)src_buf_get(strlen(name)+1);
			memcpy(_new_var->var.name, name, strlen(name)+1);
			long value;
			value = (long)TREE_INT_CST_LOW(TREE_VALUE(enum_list));
			struct possible_list *pv = NULL;
			if (!possible_list_find(&_new_var->var.possible_values,
						VALUE_IS_INT_CST, value)) {
				pv = possible_list_new();
				pv->value_flag = VALUE_IS_INT_CST;
				pv->value = value;
				list_add_tail(&pv->sibling,
					      &_new_var->var.possible_values);
			}
			list_add_tail(&_new_var->sibling, &new_type->children);

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
		new_type->ofsize = TREE_INT_CST_LOW(TYPE_SIZE(node));
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
		new_type->ofsize = TREE_INT_CST_LOW(TYPE_SIZE(node));
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
		__get_type_detail(NULL, NULL, &new_type->next,
						TREE_TYPE(node));

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
		__get_type_detail(NULL, &new_type->children, NULL,
						TREE_TYPE(node));

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
		__get_type_detail(NULL, NULL, &new_type->next,
						TREE_TYPE(node));

		break;
	}
	case ARRAY_TYPE:
	{
		new_type = find_type_node(node);
		if (!new_type)
			break;
		if (new_type->is_set)
			break;

		new_type->is_set = 1;
		if (TYPE_DOMAIN(node) && TYPE_MAX_VALUE(TYPE_DOMAIN(node))) {
			size_t len = 1;
			len += TREE_INT_CST_LOW(TYPE_MAX_VALUE(
							   TYPE_DOMAIN(node)));
			new_type->baselen = len;
		}
		if (TYPE_SIZE(node))
			new_type->ofsize = TREE_INT_CST_LOW(TYPE_SIZE(node));

		__get_type_detail(NULL, NULL, &new_type->next,
						TREE_TYPE(node));

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
		if (TYPE_SIZE(node))
			new_type->ofsize = TREE_INT_CST_LOW(TYPE_SIZE(node));

		tree fields = TYPE_FIELDS(node);
		while (fields) {
			if (unlikely((!DECL_CHAIN(fields)) &&
				(TREE_CODE(TREE_TYPE(fields)) == ARRAY_TYPE)))
				new_type->is_variant = 1;

			struct var_list *newf0;
			newf0 = var_list_new((void *)fields);
			struct var_node *newf1 = &newf0->var;
			__get_type_detail(&newf1->type, NULL, NULL,
						TREE_TYPE(fields));
			if (DECL_NAME(fields)) {
				memset(name, 0, NAME_MAX);
				get_node_name(DECL_NAME(fields), name);
				newf1->name =
					(char *)src_buf_get(strlen(name) + 1);
				memcpy(newf1->name, name, strlen(name)+1);
			}
			list_add_tail(&newf0->sibling, &new_type->children);
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
		/* handle the type of return value */
		__get_type_detail(NULL, &new_type->children, NULL,
					TREE_TYPE(node));

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
				_new->type_name =
					(char *)src_buf_get(strlen(name)+1);
				memcpy(_new->type_name, name, strlen(name)+1);
				list_add_tail(&_new->sibling,
						&new_type->children);
			} else {
				__get_type_detail(NULL, &new_type->children,
						  NULL, TREE_VALUE(args));
			}

			args = TREE_CHAIN(args);
		}
		break;
	}
	}

	if (head && new_type)
		list_add_tail(&new_type->sibling, head);
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

	__get_type_detail(&new_type, NULL, NULL, node);
	if ((long)tn != (long)new_type) {
		BUG();
	}

	CLIB_DBG_FUNC_EXIT();
	return;
}

static void get_type_detail(struct sinode *tsn)
{
	CLIB_DBG_FUNC_ENTER();

	/* XXX, get attributes here */
	tree node = (tree)(long)tsn->obj->real_addr;
	get_attributes(&tsn->attributes, TYPE_ATTRIBUTES(node));

	CLIB_DBG_FUNC_EXIT();
}

static void get_var_detail(struct sinode *sn)
{
	CLIB_DBG_FUNC_ENTER();

	analysis__resfile_load(sn->buf);
	tree node = (tree)(long)sn->obj->real_addr;
	struct var_node *new_var;
	new_var = (struct var_node *)sn->data;

	if (new_var->detailed)
		goto out;

	__get_type_detail(&new_var->type, NULL, NULL, TREE_TYPE(node));

	get_attributes(&sn->attributes, DECL_ATTRIBUTES(node));
	new_var->detailed = 1;

out:
	CLIB_DBG_FUNC_EXIT();
	return;
}

/*
 * NOTE: the gimple_body is now converted and added to cfg
 */
static void get_function_detail(struct sinode *sn)
{
	CLIB_DBG_FUNC_ENTER();

	analysis__resfile_load(sn->buf);
	tree node = (tree)(long)sn->obj->real_addr;

	/* XXX: the function body is now in node->f->cfg */
	struct function *f;
	f = DECL_STRUCT_FUNCTION(node);
	if ((!f) || (!f->cfg)) {
		CLIB_DBG_FUNC_EXIT();
		return;
	}

	/* handle the return type and argument list */
	char name[NAME_MAX];
	struct func_node *new_func;
	new_func = (struct func_node *)sn->data;

	if (new_func->detailed) {
		CLIB_DBG_FUNC_EXIT();
		return;
	}

	__get_type_detail(&new_func->ret_type, NULL, NULL,
				TREE_TYPE(TREE_TYPE(node)));
	get_attributes(&sn->attributes, DECL_ATTRIBUTES(node));

	/* parse arguments */
	tree args;
	args = DECL_ARGUMENTS(node);
	while (args) {
		struct var_list *new_arg;
		new_arg = var_list_new((void *)args);

		struct var_node *new_vn;
		new_vn = &new_arg->var;
		__get_type_detail(&new_vn->type, NULL, NULL, TREE_TYPE(args));

		memset(name, 0, NAME_MAX);
		get_node_name(DECL_NAME(args), name);
		BUG_ON(!name[0]);
		new_vn->name = (char *)src_buf_get(strlen(name) + 1);
		memcpy(new_vn->name, name, strlen(name) + 1);

		list_add_tail(&new_arg->sibling, &new_func->args);

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
	struct code_path *cps[block_cnt] = { 0 };

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
		cps[cps_cur]->cq = (void *)b;
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

	new_func->codes = cps[0];
	new_func->detailed = 1;

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

	analysis__resfile_load(b);

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
			analysis__resfile_load(sn->buf);
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
	list_for_each_entry(tmp, &tn->children, sibling) {
		if (!field_b) {
			field_b = (tree)tmp->var.node;
			continue;
		}

		field_e = (tree)tmp->var.node;

		b1 = find_target_sibuf((void *)field_b);
		b2 = find_target_sibuf((void *)field_e);
		BUG_ON(!b1);
		BUG_ON(!b2);
		analysis__resfile_load(b1);
		analysis__resfile_load(b2);

		xloc_b = (expanded_location *)(b1->payload +
						DECL_SOURCE_LOCATION(field_b));
		xloc_e = (expanded_location *)(b2->payload +
						DECL_SOURCE_LOCATION(field_e));

		if ((!strcmp(xloc_b->file, xloc_e->file)) &&
			(xloc_b->line == xloc_e->line) &&
			(xloc_b->column == xloc_e->column)) {
			CLIB_DBG_FUNC_EXIT();
			return 1;
		}

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
	analysis__resfile_load(b1);
	analysis__resfile_load(b2);

	if (!macro_expanded) {
		expanded_location *xloc1, *xloc2;
		xloc1 = (expanded_location *)(b1->payload +
						DECL_SOURCE_LOCATION(field0));
		xloc2 = (expanded_location *)(b2->payload +
						DECL_SOURCE_LOCATION(field1));

		if (!((!strcmp(xloc1->file, xloc2->file)) &&
			(xloc1->line == xloc2->line) &&
			(xloc1->column == xloc2->column))) {
			CLIB_DBG_FUNC_EXIT();
			return 0;
		} else {
			CLIB_DBG_FUNC_EXIT();
			return 1;
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
			CLIB_DBG_FUNC_EXIT();
			return 0;
		}
		if ((tname0) && (!tname1)) {
			CLIB_DBG_FUNC_EXIT();
			return 0;
		}
		if ((!tname0) && (!tname1)) {
			struct type_node *tn0, *tn1;
			tn0 = find_type_node(TREE_TYPE(field0));
			tn1 = find_type_node(TREE_TYPE(field1));
			if (tn0 != tn1) {
				CLIB_DBG_FUNC_EXIT();
				return 0;
			} else {
				CLIB_DBG_FUNC_EXIT();
				return 1;
			}
		}

		get_node_name(tname0, name0);
		get_node_name(tname1, name1);
		if ((!name0[0]) && (name1[0])) {
			CLIB_DBG_FUNC_EXIT();
			return 0;
		}
		if ((name0[0]) && (!name1[0])) {
			CLIB_DBG_FUNC_EXIT();
			return 0;
		}
		if ((!name0[0]) && (!name1[0])) {
			struct type_node *tn0, *tn1;
			tn0 = find_type_node(TREE_TYPE(field0));
			tn1 = find_type_node(TREE_TYPE(field1));
			if (tn0 != tn1) {
				CLIB_DBG_FUNC_EXIT();
				return 0;
			} else {
				CLIB_DBG_FUNC_EXIT();
				return 1;
			}
		}
		if (!strcmp(name0, name1)) {
			CLIB_DBG_FUNC_EXIT();
			return 1;
		} else {
			CLIB_DBG_FUNC_EXIT();
			return 0;
		}
	}

	CLIB_DBG_FUNC_EXIT();
}

static struct var_list *get_target_field0(struct type_node *tn, tree field);
static void do_struct_init(struct type_node *tn, tree init_tree)
{
	CLIB_DBG_FUNC_ENTER();

	BUG_ON(tn->type_code != RECORD_TYPE);

	vec<constructor_elt, va_gc> *init_elts;
	init_elts=(vec<constructor_elt, va_gc> *)(CONSTRUCTOR_ELTS(init_tree));

	/* XXX, some structures may have NULL init elements */
	if (!init_elts) {
		CLIB_DBG_FUNC_EXIT();
		return;
	}

	unsigned long length = init_elts->vecpfx.m_num;
	struct constructor_elt *addr = &init_elts->vecdata[0];

	for (unsigned long i = 0; i < length; i++) {
		BUG_ON(TREE_CODE(addr[i].index) != FIELD_DECL);
		struct var_list *vnl;
		vnl = get_target_field0(tn, (tree)addr[i].index);
		do_init_value(&vnl->var, addr[i].value);
	}

	CLIB_DBG_FUNC_EXIT();
	return;
}

static void do_union_init(struct type_node *tn, tree init_tree)
{
	CLIB_DBG_FUNC_ENTER();

	BUG_ON(tn->type_code != UNION_TYPE);

	vec<constructor_elt, va_gc> *init_elts;
	init_elts=(vec<constructor_elt, va_gc> *)(CONSTRUCTOR_ELTS(init_tree));

	if (!init_elts) {
		CLIB_DBG_FUNC_EXIT();
		return;
	}

	unsigned long length = init_elts->vecpfx.m_num;
	struct constructor_elt *addr = &init_elts->vecdata[0];

	for (unsigned long i = 0; i < length; i++) {
		BUG_ON(TREE_CODE(addr[i].index) != FIELD_DECL);
		struct var_list *vnl;
		vnl = get_target_field0(tn, (tree)addr[i].index);
		do_init_value(&vnl->var, addr[i].value);
	}

	CLIB_DBG_FUNC_EXIT();
	return;
}

static void do_init_value(struct var_node *vn, tree init_tree)
{
	/* XXX, data_node could be var_node or type_node(for structure) */
	if (!init_tree)
		return;

	CLIB_DBG_FUNC_ENTER();

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
		long value = TREE_INT_CST_LOW(init_tree);
		struct possible_list *pv = NULL;
		node_lock_w(vn);
		pv = possible_list_find(&vn->possible_values,
					VALUE_IS_INT_CST,
					value);
		if (!pv) {
			pv = possible_list_new();
			pv->value_flag = VALUE_IS_INT_CST;
			pv->value = value;
			list_add_tail(&pv->sibling, &vn->possible_values);
		}
		node_unlock_w(vn);
		break;
	}
	case REAL_CST:
	{
		long value = (long)init_tree;
		struct possible_list *pv = NULL;
		node_lock_w(vn);
		pv = possible_list_find(&vn->possible_values,
					VALUE_IS_REAL_CST,
					value);
		if (!pv) {
			pv = possible_list_new();
			pv->value_flag = VALUE_IS_REAL_CST;
			pv->value = value;
			list_add_tail(&pv->sibling, &vn->possible_values);
		}
		node_unlock_w(vn);
		break;
	}
	case STRING_CST:
	{
		long value = (long)((struct tree_string *)init_tree)->str;
		struct possible_list *pv = NULL;
		node_lock_w(vn);
		pv = possible_list_find(&vn->possible_values,
					VALUE_IS_STR_CST,
					value);
		if (!pv) {
			pv = possible_list_new();
			pv->value_flag = VALUE_IS_STR_CST;
			pv->value = value;
			list_add_tail(&pv->sibling, &vn->possible_values);
		}
		node_unlock_w(vn);
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
				struct possible_list *pv = NULL;
				node_lock_w(vn);
				pv = possible_list_find(&vn->possible_values,
							VALUE_IS_TREE,
							value);
				if (!pv) {
					pv = possible_list_new();
					pv->value_flag = VALUE_IS_TREE;
					pv->value = value;
					list_add_tail(&pv->sibling,
							&vn->possible_values);
				}
				node_unlock_w(vn);
			} else {
				/* long value = fsn->node_id.id.id1; */
				long value;
				value = sinode_id_all(fsn);
				struct possible_list *pv = NULL;
				node_lock_w(vn);
				pv = possible_list_find(&vn->possible_values,
							VALUE_IS_FUNC,
							value);
				if (!pv) {
					pv = possible_list_new();
					pv->value_flag = VALUE_IS_FUNC;
					pv->value = value;
					list_add_tail(&pv->sibling,
							&vn->possible_values);
				}
				node_unlock_w(vn);
			}
		} else if (TREE_CODE(addr) == VAR_DECL) {
			long value = (long)init_tree;
			struct possible_list *pv = NULL;
			node_lock_w(vn);
			pv = possible_list_find(&vn->possible_values,
						VALUE_IS_VAR_ADDR,
						value);
			if (!pv) {
				pv = possible_list_new();
				pv->value_flag = VALUE_IS_VAR_ADDR;
				pv->value = value;
				list_add_tail(&pv->sibling,
						&vn->possible_values);
			}
			node_unlock_w(vn);
		} else if (TREE_CODE(addr) == STRING_CST) {
			do_init_value(vn, addr);
		} else if (TREE_CODE(addr) == COMPONENT_REF) {
			long value = (long)init_tree;
			struct possible_list *pv = NULL;
			node_lock_w(vn);
			pv = possible_list_find(&vn->possible_values,
						VALUE_IS_VAR_ADDR,
						value);
			if (!pv) {
				pv = possible_list_new();
				pv->value_flag = VALUE_IS_VAR_ADDR;
				pv->value = value;
				list_add_tail(&pv->sibling,
						&vn->possible_values);
			}
			node_unlock_w(vn);
		} else if (TREE_CODE(addr) == ARRAY_REF) {
			long value = (long)init_tree;
			struct possible_list *pv = NULL;
			node_lock_w(vn);
			pv = possible_list_find(&vn->possible_values,
						VALUE_IS_EXPR,
						value);
			if (!pv) {
				pv = possible_list_new();
				pv->value_flag = VALUE_IS_EXPR;
				pv->value = value;
				list_add_tail(&pv->sibling,
						&vn->possible_values);
			}
			node_unlock_w(vn);
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
		struct possible_list *pv = NULL;
		node_lock_w(vn);
		pv = possible_list_find(&vn->possible_values,
					VALUE_IS_EXPR,
					value);
		if (!pv) {
			pv = possible_list_new();
			pv->value_flag = VALUE_IS_EXPR;
			pv->value = value;
			list_add_tail(&pv->sibling, &vn->possible_values);
		}
		node_unlock_w(vn);
		break;
	}
	default:
	{
		si_log1("miss %s\n", tree_code_name[init_tc]);
		break;
	}
	}

	CLIB_DBG_FUNC_EXIT();
}

static void do_phase4_gvar(struct sinode *sn)
{
	CLIB_DBG_FUNC_ENTER();

	analysis__resfile_load(sn->buf);

	tree node;
	struct var_node *new_var;

	node = (tree)(long)sn->obj->real_addr;
	new_var = (struct var_node *)sn->data;

	do_init_value(new_var, DECL_INITIAL(node));

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

	list_for_each_entry(tmp, &tn->children, sibling) {
		tree n1 = (tree)tmp->var.node;
		tree n2 = (tree)field;

		if (is_same_field(n1, n2, macro_expanded)) {
			CLIB_DBG_FUNC_EXIT();
			return tmp;
		}
	}

	list_for_each_entry(tmp, &tn->children, sibling) {
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
		xloc = get_location(GET_LOC_TYPE, b->payload, node);
		si_log1_todo("UNION_TYPE at %s %d %d\n",
				xloc ? xloc->file : NULL,
				xloc ? xloc->line : 0,
				xloc ? xloc->column : 0);
		ret = 0;
		break;
	}
	default:
	{
		si_log1("miss %s\n",
			tree_code_name[vnl->var.type->type_code]);
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

	list_for_each_entry(tmp, &tn->children, sibling) {
		field = (tree)tmp->var.node;
		analysis__resfile_load(find_target_sibuf(field));
		off = get_field_offset(field);

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

	if (list_empty(&tn->children)) {
		si_log1_todo("tn has no children\n");
		CLIB_DBG_FUNC_EXIT();
		return NULL;
	}

	/* check the last field */
	tmp = list_last_entry(&tn->children, struct var_list, sibling);
	field = (tree)tmp->var.node;
	off = get_field_offset(field);

	if (((off + *base_off) < target_offset) &&
		(off + *base_off + tmp->var.type->ofsize) > target_offset) {
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
	struct use_at_list *newua_type = NULL, *newua_var = NULL;

	target_vnl = get_component_ref_vnl(op);
	if (!target_vnl)
		goto out;

	if (target_vnl->var.type) {
		newua_type = use_at_list_find(&target_vnl->var.type->used_at,
						cur_gimple,
						cur_gimple_op_idx);
		if (!newua_type) {
			newua_type = use_at_list_new();
			newua_type->func_id = cur_fsn->node_id.id;
			newua_type->gimple_stmt = (void *)cur_gimple;
			newua_type->op_idx = cur_gimple_op_idx;
			list_add_tail(&newua_type->sibling,
					&target_vnl->var.type->used_at);
		}
	}

	newua_var = use_at_list_find(&target_vnl->var.used_at, cur_gimple,
					cur_gimple_op_idx);
	if (!newua_var) {
		newua_var = use_at_list_new();
		newua_var->func_id = cur_fsn->node_id.id;
		newua_var->gimple_stmt = (void *)cur_gimple;
		newua_var->op_idx = cur_gimple_op_idx;
		list_add_tail(&newua_var->sibling, &target_vnl->var.used_at);
	}

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
	struct use_at_list *newua_type = NULL, *newua_var = NULL;

	CLIB_DBG_FUNC_ENTER();

	exp = (struct tree_exp *)op;
	t0 = exp->operands[0];
	t1 = exp->operands[1];	/* how many bits to read */
	t2 = exp->operands[2];	/* where to read */
	target_offset = TREE_INT_CST_LOW(t2);

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
		case BIT_AND_EXPR:
		case EQ_EXPR:
		{
			BUG_ON((TREE_CODE(ops[2]) != INTEGER_CST) &&
				(TREE_CODE(ops[1]) != INTEGER_CST));

			tree intcst;
			if (TREE_CODE(ops[1]) == INTEGER_CST)
				intcst = ops[1];
			else
				intcst = ops[2];

			unsigned long first_test_bit = 0;
			unsigned long andval;
			andval = TREE_INT_CST_LOW(intcst);
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
			si_log1("miss %s\n", tree_code_name[next_gs_tc]);
			break;
		}
		}
		break;
	}
	default:
	{
		si_log1("miss %s\n", gimple_code_name[gimple_code(next_gs)]);
		break;
	}
	}

	if (unlikely(!target_vnl)) {
		si_log1("recheck target_vnl == NULL\n");
		goto out;
	}

	if (target_vnl->var.type) {
		node_lock_w(target_vnl->var.type);
		newua_type = use_at_list_find(&target_vnl->var.type->used_at,
						cur_gimple,
						cur_gimple_op_idx);
		if (!newua_type) {
			newua_type = use_at_list_new();
			newua_type->func_id = cur_fsn->node_id.id;
			newua_type->gimple_stmt = (void *)cur_gimple;
			newua_type->op_idx = cur_gimple_op_idx;
			list_add_tail(&newua_type->sibling,
					&target_vnl->var.type->used_at);
		}
		node_unlock_w(target_vnl->var.type);
	}

	node_lock_w(&target_vnl->var);
	newua_var = use_at_list_find(&target_vnl->var.used_at, cur_gimple,
					cur_gimple_op_idx);
	if (!newua_var) {
		newua_var = use_at_list_new();
		newua_var->func_id = cur_fsn->node_id.id;
		newua_var->gimple_stmt = (void *)cur_gimple;
		newua_var->op_idx = cur_gimple_op_idx;
		list_add_tail(&newua_var->sibling, &target_vnl->var.used_at);
	}
	node_unlock_w(&target_vnl->var);

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
		if (gimple_code(cur_gimple) != GIMPLE_ASSIGN)
			si_log1_todo("miss %s\n",
				gimple_code_name[gimple_code(cur_gimple)]);
		break;
	}
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
		si_log1("miss %s, loc: %s %d %d\n",
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
	if (si_integer_zerop(t1) &&
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
	default:
	{
		si_log1_todo("miss %s\n", tree_code_name[TREE_CODE(t0)]);
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
		unsigned long offset = TREE_INT_CST_LOW(t1) * 8;
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

		node_lock_w(&vl->var);
		struct use_at_list *ua;
		ua = use_at_list_find(&vl->var.used_at,
					cur_gimple,
					cur_gimple_op_idx);
		if (!ua) {
			ua = use_at_list_new();
			ua->func_id = cur_fsn->node_id.id;
			ua->gimple_stmt = (void *)cur_gimple;
			ua->op_idx = cur_gimple_op_idx;
			list_add_tail(&ua->sibling, &vl->var.used_at);
		}
		node_unlock_w(&vl->var);

		if (!vl->var.type) {
			goto out;
		}

		node_lock_w(vl->var.type);
		ua = use_at_list_find(&vl->var.type->used_at,
					cur_gimple,
					cur_gimple_op_idx);
		if (!ua) {
			ua = use_at_list_new();
			ua->func_id = cur_fsn->node_id.id;
			ua->gimple_stmt = (void *)cur_gimple;
			ua->op_idx = cur_gimple_op_idx;
			list_add_tail(&ua->sibling, &vl->var.type->used_at);
		}
		node_unlock_w(vl->var.type);
	}

out:
	CLIB_DBG_FUNC_EXIT();
	return;
}

static void __4_mark_array_ref(tree op)
{
	struct tree_exp *exp;
	struct type_node *tn;
	struct use_at_list *ua = NULL;

	CLIB_DBG_FUNC_ENTER();

	exp = (struct tree_exp *)op;
	tn = get_array_ref_tn(exp);
	if (!tn)
		goto out;

	node_lock_w(tn);
	ua = use_at_list_find(&tn->used_at, cur_gimple, cur_gimple_op_idx);
	if (!ua) {
		ua = use_at_list_new();
		ua->func_id = cur_fsn->node_id.id;
		ua->gimple_stmt = (void *)cur_gimple;
		ua->op_idx = cur_gimple_op_idx;
		list_add_tail(&ua->sibling, &tn->used_at);
	}
	node_unlock_w(tn);

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

	tree *ops = gimple_ops(gs);
	for (unsigned i = 0; i < gimple_num_ops(gs); i++) {
		/* handle this in direct call */
		if ((gc == GIMPLE_CALL) && (i == 1))
			continue;

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

	CLIB_DBG_FUNC_EXIT();
}

static void __4_mark_var_func(struct sinode *sn)
{
	CLIB_DBG_FUNC_ENTER();

	struct func_node *fn;
	fn = (struct func_node *)sn->data;

	cur_fsn = sn;
	cur_fn = fn;
	si_current_function_decl = (tree)(long)sn->obj->real_addr;

	BUG_ON(!si_function_bb(si_current_function_decl));

	basic_block bb;
	FOR_EACH_BB_FN(bb, DECL_STRUCT_FUNCTION(si_current_function_decl)) {
		gimple_seq gs;
		gs = bb->il.gimple.seq;
		while (gs) {
			cur_gimple = gs;
			__4_mark_gimple(gs);
			gs = gs->next;
		}
	}

	CLIB_DBG_FUNC_EXIT();
	return;
}

/* we only mark the functions variables use position */
static void do_phase4_func(struct sinode *sn)
{
	CLIB_DBG_FUNC_ENTER();

	analysis__resfile_load(sn->buf);
	__4_mark_var_func(sn);

	CLIB_DBG_FUNC_EXIT();
}

static void do_phase4(struct sibuf *b)
{
	CLIB_DBG_FUNC_ENTER();

	analysis__resfile_load(b);

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
	list_for_each_entry(tmp, &fn->local_vars, sibling) {
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
	list_for_each_entry(t, &fn->global_vars, sibling) {
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
	list_for_each_entry(tmp, &fn->args, sibling) {
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

	enum tree_code tc = TREE_CODE(op);
	switch (tc) {
	case VAR_DECL:
	{
		struct var_node *vn;
		vn = get_target_var_node(fsn, op);
		if (!vn)
			break;
		struct possible_list *pv;
		node_lock_w(vn);
		if (!possible_list_find(&vn->possible_values,
					VALUE_IS_FUNC,
					n->node_id.id.id1)) {
			pv = possible_list_new();
			pv->value_flag = VALUE_IS_FUNC;
			pv->value = n->node_id.id.id1;
			list_add_tail(&pv->sibling,
					&vn->possible_values);
		}
		node_unlock_w(vn);
		break;
	}
	case PARM_DECL:
	{
		struct var_node *vn;
		vn = get_target_parm_node(fsn, op);
		if (!vn)
			break;

		struct possible_list *pv;
		node_lock_w(vn);
		if (!possible_list_find(&vn->possible_values,
					VALUE_IS_FUNC,
					n->node_id.id.id1)) {
			pv = possible_list_new();
			pv->value_flag = VALUE_IS_FUNC;
			pv->value = n->node_id.id.id1;
			list_add_tail(&pv->sibling,
					&vn->possible_values);
		}
		node_unlock_w(vn);
		break;
	}
	case COMPONENT_REF:
	{
		struct var_list *vnl;
		vnl = get_component_ref_vnl(op);
		if (!vnl)
			break;

		struct possible_list *pv;
		node_lock_w(&vnl->var);
		if (!possible_list_find(&vnl->var.possible_values,
					VALUE_IS_FUNC,
					n->node_id.id.id1)) {
			pv = possible_list_new();
			pv->value_flag = VALUE_IS_FUNC;
			pv->value = n->node_id.id.id1;
			list_add_tail(&pv->sibling,
					&vnl->var.possible_values);
		}
		node_unlock_w(&vnl->var);
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
		si_log1("miss %s\n", tree_code_name[tc]);
		break;
	}
	}

	CLIB_DBG_FUNC_EXIT();
	return;
}

static void __do_func_used_at(struct sinode *sn, struct func_node *fn)
{
	CLIB_DBG_FUNC_ENTER();

	struct use_at_list *tmp_ua;
	list_for_each_entry(tmp_ua, &fn->used_at, sibling) {
		struct sinode *fsn;
		fsn = analysis__sinode_search(siid_type(&tmp_ua->func_id),
						SEARCH_BY_ID,
						&tmp_ua->func_id);
		BUG_ON(!fsn);
		analysis__resfile_load(fsn->buf);

		gimple_seq gs = (gimple_seq)tmp_ua->gimple_stmt;
		enum gimple_code gc = gimple_code(gs);
		if (gc == GIMPLE_ASSIGN) {
			BUG_ON(tmp_ua->op_idx == 0);
			if (unlikely(gimple_num_ops(gs) != 2))
				continue;
			tree *ops = gimple_ops(gs);
			tree lhs = ops[0];
			__func_assigned(sn, fsn, lhs);
		} else if (gc == GIMPLE_CALL) {
			BUG_ON(tmp_ua->op_idx <= 1);
		} else if (gc == GIMPLE_COND) {
			/* FIXME: we do nothing here */
		} else if (gc == GIMPLE_ASM) {
			/* TODO */
			si_log1_todo("GIMPLE_ASM in phase5\n");
		} else {
			si_log1("miss %s\n", gimple_code_name[gc]);
		}
	}

	CLIB_DBG_FUNC_EXIT();
	return;
}

static void add_caller(struct sinode *callee, struct sinode *caller)
{
	CLIB_DBG_FUNC_ENTER();

	struct func_node *callee_func_node;
	callee_func_node = (struct func_node *)callee->data;
	if (unlikely(!callee_func_node)) {
		/* XXX, check if this function has an alias attribute */
		struct attr_list *tmp;
		int found = 0;
		int no_ins = 0;
		list_for_each_entry(tmp, &callee->attributes, sibling) {
			if (!strcmp(tmp->attr_name, "alias")) {
				found = 1;
				break;
			} else if (!strcmp(tmp->attr_name,
						"no_instrument_function")) {
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
		list_for_each_entry(tmp2, &tmp->values, sibling) {
			memset(name, 0, NAME_MAX);
			tree node = (tree)tmp2->node;
			if (!node)
				continue;

			struct sibuf *b = find_target_sibuf(node);
			analysis__resfile_load(b);
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

			/* FIXME, a static function? */
			struct sinode *new_callee;
			long args[3];
			args[0] = (long)b->rf;
			args[1] = (long)get_builtin_resfile();
			args[2] = (long)name;
			new_callee = analysis__sinode_search(TYPE_FUNC_GLOBAL,
								SEARCH_BY_SPEC,
								(void *)args);
			add_caller(new_callee, caller);
		}

		CLIB_DBG_FUNC_EXIT();
		return;
	}

	if (callf_list_find(&callee_func_node->callers,
				caller->node_id.id.id1, 0)) {
		CLIB_DBG_FUNC_EXIT();
		return;
	}

	struct callf_list *_newc;
	_newc = callf_list_new();
	_newc->value = caller->node_id.id.id1;
	_newc->value_flag = 0;
	/* XXX, no need to add gimple here */

	list_add_tail(&_newc->sibling, &callee_func_node->callers);

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

	struct callf_list *_newc;
	node_lock_w(fn);
	_newc = callf_list_find(&fn->callees, value, val_flag);
	if (!_newc) {
		_newc = callf_list_new();
		_newc->value = value;
		_newc->value_flag = val_flag;
		if (val_flag)
			_newc->body_missing = 1;
		list_add_tail(&_newc->sibling, &fn->callees);
	}
	node_unlock_w(fn);
	callf_gs_list_add(&_newc->gimple_stmts, (void *)gs);

	/* FIXME, what if call_fn_sn has no data? */
	if (!val_flag)
		add_caller(call_fn_sn, sn);

	CLIB_DBG_FUNC_EXIT();
	return;
}

static void add_callee(struct sinode *caller_fsn, gimple_seq gs,
			struct sinode *called_fsn)
{
	CLIB_DBG_FUNC_ENTER();

	struct func_node *caller_fn = (struct func_node *)caller_fsn->data;
	struct func_node *called_fn = (struct func_node *)called_fsn->data;

	struct callf_list *_newc;
	_newc = callf_list_find(&caller_fn->callees,
					called_fsn->node_id.id.id1, 0);
	if (!_newc) {
		_newc = callf_list_new();
		_newc->value = called_fsn->node_id.id.id1;
		_newc->value_flag = 0;
		if (!called_fn)
			_newc->body_missing = 1;
		list_add_tail(&_newc->sibling, &caller_fn->callees);
	}
	callf_gs_list_add(&_newc->gimple_stmts, gs);
	if (called_fn)
		add_caller(called_fsn, caller_fsn);

	CLIB_DBG_FUNC_EXIT();
	return;
}

static void add_possible_callee(struct sinode *caller_fsn,
				gimple_seq gs,
				struct list_head *head)
{
	CLIB_DBG_FUNC_ENTER();

	struct possible_list *tmp_pv;
	list_for_each_entry(tmp_pv, head, sibling) {
		if (tmp_pv->value_flag != VALUE_IS_FUNC)
			continue;
		switch (tmp_pv->value_flag) {
		case VALUE_IS_FUNC:
		{
			union siid *id;
			struct sinode *called_fsn;

			id = (union siid *)&tmp_pv->value;
			called_fsn = analysis__sinode_search(siid_type(id),
							SEARCH_BY_ID, id);

			BUG_ON(!called_fsn);
			add_callee(caller_fsn, gs, called_fsn);
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
		/* TODO */
		break;
	}
	default:
	{
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
	tree *ops = gimple_ops(gs);
	struct gcall *g = NULL;
	tree call_op = NULL;
	if (gc != GIMPLE_CALL)
		return;

	CLIB_DBG_FUNC_ENTER();

	g = (struct gcall *)gs;
	if (gs->subcode & GF_CALL_INTERNAL) {
		struct callf_list *_newc;
		_newc = callf_list_find(&fn->callees,
					(long)g->u.internal_fn,
					0);
		if (!_newc) {
			_newc = callf_list_new();
			_newc->value = (unsigned long)g->u.internal_fn;
			_newc->value_flag = 0;
			_newc->body_missing = 1;
			list_add_tail(&_newc->sibling, &fn->callees);
		}
		callf_gs_list_add(&_newc->gimple_stmts, (void *)gs);
		CLIB_DBG_FUNC_EXIT();
		return;
	}

	call_op = ops[1];
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
	si_current_function_decl = (tree)(long)sn->obj->real_addr;

	basic_block bb;
	FOR_EACH_BB_FN(bb, DECL_STRUCT_FUNCTION(si_current_function_decl)) {
		gimple_seq next_gs;

		next_gs = bb->il.gimple.seq;
		for (; next_gs; next_gs = next_gs->next) {
			cur_gimple = next_gs;
			__trace_call_gs(sn, fn, next_gs);
		}
	}

	CLIB_DBG_FUNC_EXIT();
	return;
}

static void do_phase5_func(struct sinode *n)
{
	CLIB_DBG_FUNC_ENTER();

	struct func_node *fn;

	analysis__resfile_load(n->buf);
	fn = (struct func_node *)n->data;

	__do_func_used_at(n, fn);
	__do_trace_call(n, fn);

	CLIB_DBG_FUNC_EXIT();
	return;
}

static void do_phase5(struct sibuf *b)
{
	CLIB_DBG_FUNC_ENTER();

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

static __thread tree si_global_trees[TI_MAX];
static __thread tree si_integer_types[itk_none];
static __thread tree si_sizetype_tab[stk_type_kind_last];
static void setup_gcc_globals(void)
{
	CLIB_DBG_FUNC_ENTER();

	size_t ggv_idx = 0;
	for (ggv_idx = 0; ggv_idx < obj_cnt; ggv_idx++) {
		if (objs[ggv_idx].gcc_global_varidx)
			break;
	}
	BUG_ON(ggv_idx == obj_cnt);

	for (int i = 0; i < TI_MAX; i++) {
		si_global_trees[i] = (tree)(unsigned long)
					objs[ggv_idx].real_addr;
		ggv_idx++;
	}
	BUG_ON(ggv_idx > obj_cnt);

	for (int i = 0; i < itk_none; i++) {
		si_integer_types[i] = (tree)(unsigned long)
					objs[ggv_idx].real_addr;
		ggv_idx++;
	}
	BUG_ON(ggv_idx > obj_cnt);

	for (int i = 0; i < (int)stk_type_kind_last; i++) {
		si_sizetype_tab[i] = (tree)(unsigned long)
					objs[ggv_idx].real_addr;
		ggv_idx++;
	}
	BUG_ON(ggv_idx > obj_cnt);

	CLIB_DBG_FUNC_EXIT();
	return;
}

static int gcc_ver_major = __GNUC__;
static int gcc_ver_minor = __GNUC_MINOR__;
static int c_callback(struct sibuf *buf, int parse_mode)
{
	CLIB_DBG_FUNC_ENTER();

	struct file_content *fc = (struct file_content *)buf->load_addr;
	if ((fc->gcc_ver_major != gcc_ver_major) ||
		(fc->gcc_ver_minor != gcc_ver_minor)) {
		err_dbg(0, "gcc version not match, need %d.%d",
				fc->gcc_ver_major, fc->gcc_ver_minor);
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
	if (!obj_cnt) {
		CLIB_DBG_FUNC_EXIT();
		return 0;
	}

	switch (mode) {
	case MODE_ADJUST:
	{
		get_real_obj_cnt();
		show_progress_arg[0] = (long)"ADJUST";
		show_progress_arg[1] = (long)fc->path;
		show_progress_arg[2] = (long)&obj_adjusted;
		show_progress_arg[3] = (long)&obj_cnt;
		show_progress_arg[4] = (long)pthread_self();
		mt_print_add();
		mt_add_timer(show_progress_timeout, c_show_progress,
				show_progress_arg, 0, 1);

		void *raddrs[obj_cnt];
		void *faddrs[obj_cnt];
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
		show_progress_arg[1] = (long)fc->path;
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
		show_progress_arg[1] = (long)fc->path;
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
		show_progress_arg[1] = (long)fc->path;
		show_progress_arg[2] = (long)&obj_idx;
		show_progress_arg[3] = (long)&real_obj_cnt;
		show_progress_arg[4] = (long)pthread_self();
		mt_print_add();
		mt_add_timer(show_progress_timeout, c_show_progress,
				show_progress_arg, 0, 1);

		setup_gcc_globals();

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
		show_progress_arg[1] = (long)fc->path;
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
		show_progress_arg[1] = (long)fc->path;
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

	CLIB_DBG_FUNC_EXIT();
	return 0;
}
