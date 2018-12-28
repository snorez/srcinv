/*
 * this should match with collect/c.cc
 * NOTE, this should be able to used in multiple thread
 * this handle the information collected in lower gimple before cfg pass
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
#define	vecpfx m_vecpfx
#define	vecdata m_vecdata
#include "../si_core.h"

#define	DEFTREECODE(SYM, NAME, TYPE, LENGTH) TYPE,
#define	END_OF_BASE_TREE_CODES tcc_exceptional,
const enum tree_code_class tree_code_type[] = {
#include <all-tree.def>
};
#undef DEFTREECODE
#undef END_OF_BASE_TREE_CODES

#define DEFTREECODE(SYM, NAME, TYPE, LENGTH) LENGTH,
#define END_OF_BASE_TREE_CODES	0,
const unsigned char tree_code_length[] = {
#include <all-tree.def>
};
#undef DEFTREECODE
#undef END_OF_BASE_TREE_CODES

#define DEFTREECODE(SYM, NAME, TYPE, LEN) NAME,
#define END_OF_BASE_TREE_CODES "@dummy",
static const char *const tree_code_name[] = {
#include "all-tree.def"
};
#undef DEFTREECODE
#undef END_OF_BASE_TREE_CODES

#define DEFGSSTRUCT(SYM, STRUCT, HAS_TREE_OP) \
	(HAS_TREE_OP ? sizeof (struct STRUCT) - sizeof (tree) : 0),
EXPORTED_CONST size_t gimple_ops_offset_[] = {
#include "gsstruct.def"
};
#undef DEFGSSTRUCT

#define DEFGSSTRUCT(SYM, STRUCT, HAS_TREE_OP) sizeof (struct STRUCT),
static const size_t gsstruct_code_size[] = {
#include "gsstruct.def"
};
#undef DEFGSSTRUCT

#define DEFGSCODE(SYM, NAME, GSSCODE)	NAME,
const char *const gimple_code_name[] = {
#include "gimple.def"
};
#undef DEFGSCODE

#define DEFGSCODE(SYM, NAME, GSSCODE)	GSSCODE,
EXPORTED_CONST enum gimple_statement_structure_enum gss_for_code_[] = {
#include "gimple.def"
};
#undef DEFGSCODE

static int c_callback(struct sibuf *, int);
static struct lang_ops c_ops;

CLIB_PLUGIN_NAME(c);
CLIB_PLUGIN_NEEDED1(getinfo);

CLIB_PLUGIN_INIT()
{
	c_ops.type = (unsigned char)SI_LANG_TYPE_C;
	c_ops.callback = c_callback;
	register_lang_ops(&c_ops);
	return 0;
}

CLIB_PLUGIN_EXIT()
{
	return;
}

static __thread int mode = MODE_TEST;
static __thread struct file_obj *objs;
static __thread size_t obj_cnt, obj_idx, obj_adjusted;
static __thread void *addr_base = NULL;
static __thread struct sibuf *cur_sibuf;

static __thread tree cur_func = NULL;
static __thread gimple_seq cur_gimple = NULL;
static __thread unsigned long cur_gimple_op_idx = 0;
static __thread struct sinode *cur_fn = NULL;
static __thread struct func_node *cur_func_node = NULL;
#if 0
static __thread void *xrefs_obj_checked[MAX_OBJS_PER_FILE];
#endif
static __thread void **xrefs_obj_checked;
static __thread size_t xrefs_obj_idx = 0;

/* these two are for optimization purpose */
#if 0
static __thread void *real_addrs[MAX_OBJS_PER_FILE];
static __thread void *fake_addrs[MAX_OBJS_PER_FILE];
#endif
static __thread void **real_addrs;
static __thread void **fake_addrs;

#if 0
static inline void get_real_addr(void **fake_addr, int *do_next)
{
	/*
	 * this function should only return the real addr once,
	 * and then return some other value that means this orig_addr has
	 * already been taken care.
	 */
	int i = 0;
	*do_next = 0;
	if (unlikely(!*fake_addr))
		return;

	int loop_limit = obj_idx;
	if (unlikely(loop_limit > (MAX_OBJS_PER_FILE / 8))) {
		loop_limit = MAX_OBJS_PER_FILE / 8;
		for (i = loop_limit; i < obj_idx; i++) {
			if (unlikely(fake_addrs[i] == *fake_addr)) {
				*fake_addr = (void *)
						(unsigned long)objs[i].real_addr;
				return;
			}
		}
	}
	for (i = 0; i < loop_limit; i++) {
		if (unlikely(fake_addrs[i] == *fake_addr)) {
			*fake_addr = (void *)(unsigned long)objs[i].real_addr;
			return;
		}
	}

	while (unlikely(objs[obj_idx].is_adjusted))
		obj_idx++;

	if (unlikely((void *)(unsigned long)objs[obj_idx].fake_addr != *fake_addr)) {
		fprintf(stderr, "hello world\n");
		BUG();
	}
	*fake_addr = (void *)(unsigned long)objs[obj_idx].real_addr;
	objs[obj_idx].is_adjusted = 1;
	*do_next = 1;
	obj_idx++;
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

static inline int is_obj_checked(void *real_addr)
{
	int i = 0;
	int loop_limit;
	if (unlikely(!obj_idx))
		loop_limit = 0;
	else
		loop_limit = obj_idx-1;
	if (unlikely(loop_limit > (MAX_OBJS_PER_FILE / 8))) {
		loop_limit = MAX_OBJS_PER_FILE / 8;
		for (i = loop_limit; i <  obj_idx-1; i++) {
			if (unlikely(real_addr == real_addrs[i]))
				return 1;
		}
	}

	for (i = 0; i < loop_limit; i++) {
		if (unlikely(real_addr == real_addrs[i]))
			return 1;
	}
	return 0;
}
#endif
static inline void get_real_addr(void **fake_addr, int *do_next)
{
	/*
	 * this function should only return the real addr once,
	 * and then return some other value that means this orig_addr has
	 * already been taken care.
	 */
	int i = 0;
	*do_next = 0;
	if (unlikely(!*fake_addr))
		return;

	int loop_limit = obj_cnt;
	if (unlikely(loop_limit > (MAX_OBJS_PER_FILE / 8))) {
		loop_limit = MAX_OBJS_PER_FILE / 8;
		for (i = loop_limit; i < obj_cnt; i++) {
			if (unlikely(fake_addrs[i] == *fake_addr)) {
				*fake_addr = (void *)
						(unsigned long)objs[i].real_addr;
				if (!objs[i].is_adjusted)
					*do_next = 1;
				return;
			}
		}
	}
	for (i = 0; i < loop_limit; i++) {
		if (unlikely(fake_addrs[i] == *fake_addr)) {
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

static inline int is_obj_checked(void *real_addr)
{
	switch (mode) {
	case MODE_ADJUST:
	{
		int i = 0;
		int loop_limit = obj_cnt;
		if (unlikely(loop_limit > (MAX_OBJS_PER_FILE / 8))) {
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
	case MODE_GETXREFS:
	{
		int i = 0;
		int loop_limit = xrefs_obj_idx;
		if (unlikely(loop_limit > (MAX_OBJS_PER_FILE / 8))) {
			loop_limit = MAX_OBJS_PER_FILE / 8;
			for (i = loop_limit; i < xrefs_obj_idx; i++) {
				if (unlikely(real_addr == xrefs_obj_checked[i])) {
					return 1;
				}
			}
		}

		for (i = 0; i < loop_limit; i++) {
			if (unlikely(real_addr == xrefs_obj_checked[i])) {
				return 1;
			}
		}

		xrefs_obj_checked[xrefs_obj_idx] = real_addr;
		xrefs_obj_idx++;
		BUG_ON(xrefs_obj_idx >= obj_cnt);

		return 0;
	}
	default:
		BUG();
	}
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

static expanded_location *my_expand_location(location_t *loc)
{
	switch (mode) {
	case MODE_ADJUST:
	{
		while (unlikely(objs[obj_idx].is_adjusted))
			obj_idx++;
		BUG_ON((unsigned long)objs[obj_idx].fake_addr <
				(unsigned long)0x800000000000);
		*loc = objs[obj_idx].offs;
		objs[obj_idx].is_adjusted = 1;
		obj_adjusted++;
		expanded_location *ret =
		      (expanded_location *)((unsigned long)objs[obj_idx].real_addr);
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
		expanded_location *xloc = my_expand_location(loc);
		do_real_addr(&xloc->file, is_obj_checked((void *)xloc->file));
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
		vec<tree, va_gc, vl_embed> *node0 =
			(vec<tree, va_gc, vl_embed> *)node;

		struct vec_prefix *pfx = (struct vec_prefix *)&node0->vecpfx;
		unsigned long length = pfx->m_num;
		BUG_ON(!length);
		tree *addr = &node0->vecdata[0];
		for (unsigned long i = 0; i < length; i++) {
			do_real_addr(&addr[i], do_tree(addr[i]));
		}
		return;
	}
	case MODE_GETXREFS:
	{
		vec<tree, va_gc, vl_embed> *node0 =
			(vec<tree, va_gc, vl_embed> *)node;

		struct vec_prefix *pfx = (struct vec_prefix *)&node0->vecpfx;
		unsigned long length = pfx->m_num;
		BUG_ON(!length);
		tree *addr = &node0->vecdata[0];
		for (unsigned long i = 0; i < length; i++) {
			do_tree(addr[i]);
		}
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
		vec<constructor_elt, va_gc> *node0 =
			(vec<constructor_elt, va_gc> *)node;
		unsigned long length = node0->vecpfx.m_num;
		BUG_ON(!length);
		struct constructor_elt *addr = &node0->vecdata[0];
		for (unsigned long i = 0; i < length; i++) {
			do_real_addr(&addr[i].index,do_tree(addr[i].index));
			do_real_addr(&addr[i].value,do_tree(addr[i].value));
		}
		return;
	}
	case MODE_GETXREFS:
	{
		vec<constructor_elt, va_gc> *node0 =
			(vec<constructor_elt, va_gc> *)node;
		unsigned long length = node0->vecpfx.m_num;
		BUG_ON(!length);
		struct constructor_elt *addr = &node0->vecdata[0];
		for (unsigned long i = 0; i < length; i++) {
			do_tree(addr[i].index);
			do_tree(addr[i].value);
		}
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
		do_real_addr(&node->outer, do_c_scope(node->outer, 1));
		do_real_addr(&node->outer_function,
				do_c_scope(node->outer_function, 1));
		do_real_addr(&node->bindings, do_c_binding(node->bindings, 1));
		do_real_addr(&node->blocks, do_tree(node->blocks));
		do_real_addr(&node->blocks_last, do_tree(node->blocks_last));
		return;
	}
	case MODE_GETXREFS:
	{
#if 0
		do_c_scope(node->outer, 1);
		do_c_scope(node->outer_function, 1);
		do_c_binding(node->bindings, 1);
		do_tree(node->blocks);
		do_tree(node->blocks_last);
#endif
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
		do_real_addr(&node->scope, do_c_scope(node->scope, 1));
		do_real_addr(&node->bindings_in_scope,
				do_c_binding(node->bindings_in_scope, 1));
		return;
	}
	case MODE_GETXREFS:
	{
#if 0
		do_c_scope(node->scope, 1);
		do_c_binding(node->bindings_in_scope, 1);
#endif
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
		do_location(&node->loc);
		do_c_spot_bindings(&node->goto_bindings, 0);
		return;
	}
	case MODE_GETXREFS:
	{
#if 0
		do_c_spot_bindings(&node->goto_bindings, 0);
#endif
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
		vec<c_goto_bindings_p, va_gc> *node0 =
					(vec<c_goto_bindings_p, va_gc> *)node;
		unsigned long len = node0->vecpfx.m_num;
		BUG_ON(!len);
		c_goto_bindings_p *addr = node0->vecdata;
		for (unsigned long i = 0; i < len; i++) {
			do_real_addr(&addr[i], do_c_goto_bindings(addr[i], 1));
		}
		return;
	}
	case MODE_GETXREFS:
	{
#if 0
		vec<c_goto_bindings_p, va_gc> *node0 =
					(vec<c_goto_bindings_p, va_gc> *)node;
		unsigned long len = node0->vecpfx.m_num;
		BUG_ON(!len);
		c_goto_bindings_p *addr = node0->vecdata;
		for (unsigned long i = 0; i < len; i++) {
			do_c_goto_bindings(addr[i], 1);
		}
#endif
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
		do_real_addr(&node->shadowed, do_c_label_vars(node->shadowed, 1));
		do_c_spot_bindings(&node->label_bindings, 0);
		do_real_addr(&node->decls_in_scope,
				do_vec_tree((void *)node->decls_in_scope, 1));
		do_real_addr(&node->gotos,
				do_vec_c_goto_bindings((void *)node->gotos, 1));
		return;
	}
	case MODE_GETXREFS:
	{
#if 0
		do_c_label_vars(node->shadowed, 1);
		do_c_spot_bindings(&node->label_bindings, 0);
		do_vec_tree((void *)node->decls_in_scope, 1);
		do_vec_c_goto_bindings((void *)node->gotos, 1);
#endif
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
		return;
	}
	case MODE_GETXREFS:
	{
#if 0
		do_tree(node->decl);
		if (TREE_CODE(node->decl) != LABEL_DECL)
			do_tree(node->u.type);
		else
			do_c_label_vars(node->u.label, 1);
		do_tree(node->id);
		do_c_binding(node->prev, 1);
		do_c_binding(node->shadowed, 1);
#endif
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
		do_real_addr(&node->str, is_obj_checked((void *)node->str));
		return;
	}
	case MODE_GETXREFS:
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
		return;
	}
	case MODE_GETXREFS:
	{
#if 0
		do_ht_identifier(&node->ident, 0);
#ifdef COLLECT_MORE
		struct cpp_hashnode node0 = *node;
		switch (CPP_HASHNODE_VALUE_IDX(node0)) {
		case NTV_MACRO:
			do_cpp_macro(node->value.macro, 1);
			break;
		case NTV_ANSWER:
			do_answer(node->value.answers, 1);
			break;
		case NTV_BUILTIN:
		case NTV_ARGUMENT:
		case NTV_NONE:
			break;
		default:
			BUG();
		}
#endif
#endif
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
		do_real_addr(&node->spelling, do_cpp_hashnode(node->spelling, 1));
		return;
	}
	case MODE_GETXREFS:
	{
#if 0
		do_cpp_hashnode(node->spelling, 1);
#endif
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
		do_real_addr(&node->text, is_obj_checked((void *)node->text));
		return;
	}
	case MODE_GETXREFS:
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
		do_real_addr(&node->node, do_cpp_hashnode(node->node, 1));
		do_real_addr(&node->spelling, do_cpp_hashnode(node->spelling, 1));
		return;
	}
	case MODE_GETXREFS:
	{
#if 0
		do_cpp_hashnode(node->node, 1);
		do_cpp_hashnode(node->spelling, 1);
#endif
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
		return;
	}
	case MODE_GETXREFS:
	{
#if 0
		switch (*((char *)node + sizeof(*node))) {
		case CPP_TOKEN_FLD_NODE:
			do_cpp_identifier(&node->val.node, 0);
			break;
		case CPP_TOKEN_FLD_SOURCE:
			do_cpp_token(node->val.source, 1);
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
#endif
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
static void do_cpp_macro(struct cpp_macro *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		do_location(&node->line);
		if (node->params) {
			do_real_addr(&node->params, is_obj_checked(node->params));
			cpp_hashnode **addr = node->params;
			for (unsigned short i = 0; i < node->paramc; i++) {
				do_real_addr(&addr[i], do_cpp_hashnode(addr[i], 1));
			}
		}
		switch (node->traditional) {
		case 0:
		{
			if (node->exp.tokens) {
				do_real_addr(&node->exp.tokens,
						is_obj_checked(node->exp.tokens));
				struct cpp_token *addr = node->exp.tokens;
				for (unsigned int i = 0; i < node->count; i++) {
					do_cpp_token(&addr[i], 0);
				}
			}
			break;
		}
		case 1:
		{
			if (node->exp.text)
				do_real_addr(&node->exp.text,
					is_obj_checked((void *)node->exp.text));
			break;
		}
		default:
			BUG();
		}
		return;
	}
	case MODE_GETXREFS:
	{
#if 0
		if (node->params) {
			cpp_hashnode **addr = node->params;
			for (unsigned short i = 0; i < node->paramc; i++) {
				do_cpp_hashnode(addr[i], 1);
			}
		}
		switch (node->traditional) {
		case 0:
		{
			if (node->exp.tokens) {
				struct cpp_token *addr = node->exp.tokens;
				for (unsigned int i = 0; i < node->count; i++) {
					do_cpp_token(&addr[i], 0);
				}
			}
			break;
		}
		case 1:
		{
			break;
		}
		default:
			BUG();
		}
#endif
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
		unsigned int len = node->count;
		BUG_ON(!len);
		do_real_addr(&node->next, do_answer(node->next, 1));
		for (unsigned int i = 0; i < len; i++) {
			do_cpp_token(&node->first[i], 0);
		}
		return;
	}
	case MODE_GETXREFS:
	{
#if 0
		unsigned int len = node->count;
		BUG_ON(!len);
		do_answer(node->next, 1);
		for (unsigned int i = 0; i < len; i++) {
			do_cpp_token(&node->first[i], 0);
		}
#endif
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
		do_common((tree)&node->common, 0);
		do_cpp_hashnode(&node->node, 0);
		return;
	}
	case MODE_GETXREFS:
	{
#if 0
		do_common((tree)&node->common, 0);
		do_cpp_hashnode(&node->node, 0);
#endif
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

#if 0
struct GTY(()) sorted_fields_type {
	int len;
	tree GTY((length("%h.len"))) elts[1];
};
#endif
static void do_sorted_fields_type(struct sorted_fields_type *node,
							int flag)
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
		for (int i = 0; i < cnt; i++) {
			do_real_addr(&node->elts[i], do_tree(node->elts[i]));
		}
		return;
	}
	case MODE_GETXREFS:
	{
#if 0
		for (int i = 0; i < cnt; i++) {
			do_tree(node->elts[i]);
		}
#endif
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
		struct c_lang_type *node0 = (struct c_lang_type *)node;
		do_real_addr(&node0->s, do_sorted_fields_type(node0->s, 1));
		do_real_addr(&node0->enum_min, do_tree(node0->enum_min));
		do_real_addr(&node0->enum_max, do_tree(node0->enum_max));
		do_real_addr(&node0->objc_info, do_tree(node0->objc_info));
		return;
	}
	case MODE_GETXREFS:
	{
#if 0
		struct c_lang_type *node0 = (struct c_lang_type *)node;
		do_sorted_fields_type(node0->s, 1);
		do_tree(node0->enum_min);
		do_tree(node0->enum_max);
		do_tree(node0->objc_info);
#endif
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
		struct symtab_node *node0 = (struct symtab_node *)node;
		do_real_addr(&node0->decl, do_tree(node0->decl));
		do_real_addr(&node0->next, do_symtab_node(node0->next, 1));
		do_real_addr(&node0->previous, do_symtab_node(node0->previous, 1));
		do_real_addr(&node0->next_sharing_asm_name,
				do_symtab_node(node0->next_sharing_asm_name, 1));
		do_real_addr(&node0->previous_sharing_asm_name,
				do_symtab_node(node0->previous_sharing_asm_name, 1));
		do_real_addr(&node0->same_comdat_group,
				do_symtab_node(node0->same_comdat_group, 1));
		do_real_addr(&node0->alias_target, do_tree(node0->alias_target));
		do_real_addr(&node0->x_comdat_group, do_tree(node0->x_comdat_group));
		return;
	}
	case MODE_GETXREFS:
	{
#if 0
		struct symtab_node *node0 = (struct symtab_node *)node;
		do_tree(node0->decl);
		do_symtab_node(node0->next, 1);
		do_symtab_node(node0->previous, 1);
		do_symtab_node(node0->next_sharing_asm_name, 1);
		do_symtab_node(node0->previous_sharing_asm_name, 1);
		do_symtab_node(node0->same_comdat_group, 1);
		do_tree(node0->alias_target);
		do_tree(node0->x_comdat_group);
#endif

		return;
	}
	default:
		BUG();
	}
	/* TODO */
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
		do_real_addr(&node->x_cur_stmt_list,
				do_vec_tree((void *)node->x_cur_stmt_list, 1));
		return;
	}
	case MODE_GETXREFS:
	{
#if 0
		do_vec_tree((void *)node->x_cur_stmt_list, 1);
#endif
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
		do_stmt_tree_s(&node->x_stmt_tree, 0);
		do_real_addr(&node->local_typedefs,
				do_vec_tree((void *)node->local_typedefs, 1));
		return;
	}
	case MODE_GETXREFS:
	{
#if 0
		do_stmt_tree_s(&node->x_stmt_tree, 0);
		do_vec_tree((void *)node->local_typedefs, 1);
#endif
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
		do_real_addr(&node->left, do_splay_tree_node_s(node->left, 1));
		do_real_addr(&node->right, do_splay_tree_node_s(node->right, 1));
		return;
	}
	case MODE_GETXREFS:
	{
#if 0
		do_splay_tree_node_s(node->left, 1);
		do_splay_tree_node_s(node->right, 1);
#endif
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
		do_real_addr(&node->root, do_splay_tree_node_s(node->root, 1));
		return;
	}
	case MODE_GETXREFS:
	{
#if 0
		do_splay_tree_node_s(node->root, 1);
#endif
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
		do_real_addr(&node->switch_expr, do_tree(node->switch_expr));
		do_real_addr(&node->orig_type, do_tree(node->orig_type));
		do_real_addr(&node->cases, do_splay_tree_s(node->cases, 1));
		do_real_addr(&node->bindings, do_c_spot_bindings(node->bindings, 1));
		do_real_addr(&node->next, do_c_switch(node->next, 1));
		return;
	}
	case MODE_GETXREFS:
	{
#if 0
		do_tree(node->switch_expr);
		do_tree(node->orig_type);
		do_splay_tree_s(node->cases, 1);
		do_c_spot_bindings(node->bindings, 1);
		do_c_switch(node->next, 1);
#endif
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
static void do_c_arg_tag(struct c_arg_tag *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		do_real_addr(&node->id, do_tree(node->id));
		do_real_addr(&node->type, do_tree(node->type));
		return;
	}
	case MODE_GETXREFS:
	{
#if 0
		do_tree(node->id);
		do_tree(node->type);
#endif
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

	unsigned long len = node->vecpfx.m_num;
	BUG_ON(len==0);
	c_arg_tag *addr = node->vecdata;
	switch (mode) {
	case MODE_ADJUST:
	{
		for (unsigned long i = 0; i < len; i++) {
			do_c_arg_tag(&addr[i], 0);
		}
		return;
	}
	case MODE_GETXREFS:
	{
#if 0
		for (unsigned long i = 0; i < len; i++) {
			do_c_arg_tag(&addr[i], 0);
		}
#endif
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
		do_real_addr(&node->parms, do_tree(node->parms));
		do_real_addr(&node->tags, do_vec_c_arg_tag(node->tags, 1));
		do_real_addr(&node->types, do_tree(node->types));
		do_real_addr(&node->others, do_tree(node->others));
		do_real_addr(&node->pending_sizes, do_tree(node->pending_sizes));
		return;
	}
	case MODE_GETXREFS:
	{
#if 0
		do_tree(node->parms);
		do_vec_c_arg_tag(node->tags, 1);
		do_tree(node->types);
		do_tree(node->others);
		do_tree(node->pending_sizes);
#endif
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
		do_c_language_function(&node->base, 0);
		do_real_addr(&node->x_break_label, do_tree(node->x_break_label));
		do_real_addr(&node->x_cont_label, do_tree(node->x_cont_label));
		do_real_addr(&node->x_switch_stack,
				do_c_switch(node->x_switch_stack, 1));
		do_real_addr(&node->arg_info, do_c_arg_info(node->arg_info, 1));
		return;
	}
	case MODE_GETXREFS:
	{
#if 0
		do_c_language_function(&node->base, 0);
		do_tree(node->x_break_label);
		do_tree(node->x_cont_label);
		do_c_switch(node->x_switch_stack, 1);
		do_c_arg_info(node->arg_info, 1);
#endif
		return;
	}
	default:
		BUG();
	}
}

static void do_gimple_seq(gimple_seq gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		do_location(&gs->location);
		tree *ops = gimple_ops(gs);
		for (int i = 0; i < gimple_num_ops(gs); i++) {
			do_real_addr(&ops[i], do_tree(ops[i]));
		}
		do_real_addr(&gs->next, do_gimple_seq(gs->next, 1));
		do_real_addr(&gs->prev, do_gimple_seq(gs->prev, 1));
		return;
	}
	case MODE_GETXREFS:
	{
		return;
	}
	default:
		BUG();
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
		do_location(&node->function_start_locus);
		do_location(&node->function_end_locus);
		do_real_addr(&node->language,
				do_language_function(node->language, 1));
		do_real_addr(&node->decl, do_tree(node->decl));
		do_real_addr(&node->static_chain_decl,
				do_tree(node->static_chain_decl));
		do_real_addr(&node->nonlocal_goto_save_area,
				do_tree(node->nonlocal_goto_save_area));
		do_real_addr(&node->local_decls, do_vec_tree(node->local_decls, 1));
		do_real_addr(&node->cilk_frame_decl,
				do_tree(node->cilk_frame_decl));
		do_real_addr(&node->gimple_body,
				do_gimple_seq(node->gimple_body, 1));
		return;
	}
	case MODE_GETXREFS:
	{
#if 0
		do_language_function(node->language, 1);
		do_tree(node->decl);
		do_tree(node->static_chain_decl);
		do_tree(node->nonlocal_goto_save_area);
		do_vec_tree(node->local_decls, 1);
		do_tree(node->cilk_frame_decl);
		do_gimple_seq(node->gimple_body, 1);
#endif
		return;
	}
	default:
		BUG();
	}
#if 0
	/* TODO, node0->eh, except.h, ignore it */
	/* TODO, node0->cfg, ignore */
	/* TODO, node0->gimple_body */
	/* TODO, node0->gimple_df */
	/* TODO, node0->x_current_loops */
	/* TODO, node0->su */
	/* TODO, node0->value_histograms */
	/* TODO, node0->used_types_hash */
	/* TODO, node0->fde */
	/* TODO, node0->cannot_be_copied_reason */
	/* TODO, node0->machine, config/i386/i386.h, ignore */
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
		struct tree_statement_list_node *node0 =
				(struct tree_statement_list_node *)node;
		do_real_addr(&node0->prev,
				do_statement_list_node((void *)node0->prev, 1));
		do_real_addr(&node0->next,
				do_statement_list_node((void *)node0->next, 1));
		do_real_addr(&node0->stmt, do_tree(node0->stmt));
		return;
	}
	case MODE_GETXREFS:
	{
#if 0
		struct tree_statement_list_node *node0 =
				(struct tree_statement_list_node *)node;
		do_statement_list_node((void *)node0->prev, 1);
		do_statement_list_node((void *)node0->next, 1);
		do_tree(node0->stmt);
#endif
		return;
	}
	default:
		BUG();
	}
}

static void do_tree(tree node)
{
	if (!node)
		return;

	enum tree_code code = TREE_CODE(node);
	enum tree_code_class tc = TREE_CODE_CLASS(code);
	switch (code) {
	case INTEGER_CST:
		return do_int_cst(node, 1);
	case TREE_BINFO:
		return do_binfo(node, 1);
	case TREE_VEC:
		return do_vec(node, 1);
	case VECTOR_CST:
		return do_vector(node, 1);
	case STRING_CST:
		return do_string(node, 1);
	case OMP_CLAUSE:
		return do_omp_clause(node, 1);
	default:
		if (tc == tcc_vl_exp)
			return do_exp(node, 1);
	}

	switch (tc) {
	case tcc_declaration:
	{
		switch (code) {
		case FIELD_DECL:
			return do_field_decl(node, 1);
		case PARM_DECL:
			return do_parm_decl(node, 1);
		case VAR_DECL:
			return do_var_decl(node, 1);
		case LABEL_DECL:
			return do_label_decl(node, 1);
		case RESULT_DECL:
			return do_result_decl(node, 1);
		case CONST_DECL:
			return do_const_decl(node, 1);
		case TYPE_DECL:
			return do_type_decl(node, 1);
		case FUNCTION_DECL:
			return do_function_decl(node, 1);
		case DEBUG_EXPR_DECL:
			return do_decl_with_rtl(node, 1);
		case TRANSLATION_UNIT_DECL:
			return do_translation_unit_decl(node, 1);
		case NAMESPACE_DECL:
		case IMPORTED_DECL:
		case NAMELIST_DECL:
			return do_decl_non_common(node, 1);
		default:
			BUG();
		}
	}
	case tcc_type:
		return do_type_non_common(node, 1);
	case tcc_reference:
	case tcc_expression:
	case tcc_statement:
	case tcc_comparison:
	case tcc_unary:
	case tcc_binary:
		return do_exp(node, 1);
	case tcc_constant:
	{
		switch (code) {
		case VOID_CST:
			return do_typed(node, 1);
		case INTEGER_CST:
			BUG();
		case REAL_CST:
			return do_real_cst(node, 1);
		case FIXED_CST:
			return do_fixed_cst(node, 1);
		case COMPLEX_CST:
			return do_complex(node, 1);
		case VECTOR_CST:
			return do_vector(node, 1);
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
			return do_c_lang_identifier(node, 1);
#if 0
			return do_identifier(node, 1);
#endif
		case TREE_LIST:
			return do_list(node, 1);
		case ERROR_MARK:
		case PLACEHOLDER_EXPR:
			return do_common(node, 1);
		case TREE_VEC:
		case OMP_CLAUSE:
			BUG();
		case SSA_NAME:
			return do_ssa_name(node, 1);
		case STATEMENT_LIST:
			return do_statement_list(node, 1);
		case BLOCK:
			return do_block(node, 1);
		case CONSTRUCTOR:
			return do_constructor(node, 1);
		case OPTIMIZATION_NODE:
			return do_optimization_option(node, 1);
		case TARGET_OPTION_NODE:
			return do_target_option(node, 1);
		default:
			BUG();
		}
	}
	default:
		BUG();
	}
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
	case MODE_GETXREFS:
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
		struct tree_typed *node0 = (struct tree_typed *)node;
		do_base((tree)&node0->base, 0);
		do_real_addr(&node0->type, do_tree(node0->type));
		return;
	}
	case MODE_GETXREFS:
	{
#if 0
		struct tree_typed *node0 = (struct tree_typed *)node;
		do_base((tree)&node0->base, 0);
		do_tree(node0->type);
#endif
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
		struct tree_string *node0 = (struct tree_string *)node;
		do_typed((tree)&node0->typed, 0);
		return;
	}
	case MODE_GETXREFS:
	{
#if 0
		struct tree_string *node0 = (struct tree_string *)node;
		do_typed((tree)&node0->typed, 0);
#endif
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
		struct tree_real_cst *node0 = (struct tree_real_cst *)node;
		do_typed((tree)&node0->typed, 0);
		do_real_addr(&node0->real_cst_ptr,
				do_real_value(node0->real_cst_ptr, 1));
		return;
	}
	case MODE_GETXREFS:
	{
#if 0
		struct tree_real_cst *node0 = (struct tree_real_cst *)node;
		do_typed((tree)&node0->typed, 0);
		do_real_value(node0->real_cst_ptr, 1);
#endif
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
		struct tree_int_cst *node0 = (struct tree_int_cst *)node;
		do_typed((tree)&node0->typed, 0);
		return;
	}
	case MODE_GETXREFS:
	{
#if 0
		struct tree_int_cst *node0 = (struct tree_int_cst *)node;
		do_typed((tree)&node0->typed, 0);
#endif
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
		struct tree_common *node0 = (struct tree_common *)node;
		do_typed((tree)&node0->typed, 0);
		do_real_addr(&node0->chain, do_tree(node0->chain));
		return;
	}
	case MODE_GETXREFS:
	{
#if 0
		struct tree_common *node0 = (struct tree_common *)node;
		do_typed((tree)&node0->typed, 0);
#endif
		return;
	}
	default:
		BUG();
	}
}

static void do_identifier(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		struct tree_identifier *node0 = (struct tree_identifier *)node;
		do_common((tree)&node0->common, 0);
		do_ht_identifier(&node0->id, 0);
		return;
	}
	case MODE_GETXREFS:
	{
#if 0
		struct tree_identifier *node0 = (struct tree_identifier *)node;
		do_common((tree)&node0->common, 0);
		do_ht_identifier(&node0->id, 0);
#endif
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
		struct c_lang_identifier *node0 = (struct c_lang_identifier *)node;
		do_c_common_identifier(&node0->common_id, 0);
#ifdef COLLECT_MORE
		do_real_addr(&node0->symbol_binding,
				do_c_binding(node0->symbol_binding, 1));
		do_real_addr(&node0->tag_binding,
				do_c_binding(node0->tag_binding, 1));
		do_real_addr(&node0->label_binding,
				do_c_binding(node0->label_binding, 1));
#endif
		return;
	}
	case MODE_GETXREFS:
	{
#if 0
		struct c_lang_identifier *node0 = (struct c_lang_identifier *)node;
		do_c_common_identifier(&node0->common_id, 0);
#ifdef COLLECT_MORE
		do_c_binding(node0->symbol_binding, 1);
		do_c_binding(node0->tag_binding, 1);
		do_c_binding(node0->label_binding, 1);
#endif
#endif
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
		struct tree_constructor *node0 =
			(struct tree_constructor *)node;
		do_typed((tree)&node0->typed, 0);
		do_real_addr(&node0->elts,
				do_vec_constructor((void *)node0->elts, 1));
		return;
	}
	case MODE_GETXREFS:
	{
		struct tree_constructor *node0 =
			(struct tree_constructor *)node;
		do_typed((tree)&node0->typed, 0);
		do_vec_constructor((void *)node0->elts, 1);
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
		struct tree_statement_list *node0 =
			(struct tree_statement_list *)node;
		do_typed((tree)&node0->typed, 0);

		do_real_addr(&node0->head,
				do_statement_list_node((void *)node0->head, 1));
		do_real_addr(&node0->tail,
				do_statement_list_node((void *)node0->tail, 1));
		return;
	}
	case MODE_GETXREFS:
	{
#if 0
		struct tree_statement_list *node0 =
			(struct tree_statement_list *)node;
		do_typed((tree)&node0->typed, 0);

		do_statement_list_node((void *)node0->head, 1);
		do_statement_list_node((void *)node0->tail, 1);
#endif
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
		struct tree_block *node0 = (struct tree_block *)node;
		do_location(&node0->locus);
		do_location(&node0->end_locus);
		do_base((tree)&node0->base, 0);

		do_real_addr(&node0->chain, do_tree(node0->chain));
		do_real_addr(&node0->vars, do_tree(node0->vars));
		do_real_addr(&node0->nonlocalized_vars,
				do_vec_tree(node0->nonlocalized_vars, 1));
		do_real_addr(&node0->subblocks, do_tree(node0->subblocks));
		do_real_addr(&node0->supercontext, do_tree(node0->supercontext));
		do_real_addr(&node0->abstract_origin,
				do_tree(node0->abstract_origin));
		do_real_addr(&node0->fragment_origin,
				do_tree(node0->fragment_origin));
		do_real_addr(&node0->fragment_chain,
				do_tree(node0->fragment_chain));
		return;
	}
	case MODE_GETXREFS:
	{
#if 0
		struct tree_block *node0 = (struct tree_block *)node;
		do_base((tree)&node0->base, 0);

		do_tree(node0->chain);
		do_tree(node0->vars);
		do_vec_tree(node0->nonlocalized_vars, 1);
		do_tree(node0->subblocks);
		do_tree(node0->supercontext);
		do_tree(node0->abstract_origin);
		do_tree(node0->fragment_origin);
		do_tree(node0->fragment_chain);
#endif
		return;
	}
	default:
		BUG();
	}
}

static void do_exp(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		struct tree_exp *node0 = (struct tree_exp *)node;
		do_location(&node0->locus);
		do_typed((tree)&node0->typed, 0);

		if (TREE_CODE_CLASS(TREE_CODE(node)) == tcc_vl_exp) {
			do_real_addr(&node0->operands[0],
					do_tree(node0->operands[0]));
			for (int i = 1; i < VL_EXP_OPERAND_LENGTH(node); i++) {
				do_real_addr(&node0->operands[i],
						do_tree(node0->operands[i]));
			}
		} else {
			for (int i = 0;i<TREE_CODE_LENGTH(TREE_CODE(node));i++) {
				do_real_addr(&node0->operands[i],
						do_tree(node0->operands[i]));
			}
		}
		return;
	}
	case MODE_GETXREFS:
	{
		struct tree_exp *node0 = (struct tree_exp *)node;
		do_typed((tree)&node0->typed, 0);

		if (TREE_CODE_CLASS(TREE_CODE(node)) == tcc_vl_exp) {
			do_tree(node0->operands[0]);
			for (int i = 1; i < VL_EXP_OPERAND_LENGTH(node); i++) {
				do_tree(node0->operands[i]);
			}
		} else {
			for (int i = 0;i<TREE_CODE_LENGTH(TREE_CODE(node));i++) {
				do_tree(node0->operands[i]);
			}
		}
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
		struct tree_list *node0 = (struct tree_list *)node;
		do_common((tree)&node0->common, 0);

		do_real_addr(&node0->purpose, do_tree(node0->purpose));
		do_real_addr(&node0->value, do_tree(node0->value));
		return;
	}
	case MODE_GETXREFS:
	{
		struct tree_list *node0 = (struct tree_list *)node;
		do_common((tree)&node0->common, 0);

		do_tree(node0->purpose);
		do_tree(node0->value);
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
		struct tree_type_with_lang_specific *node0 =
				(struct tree_type_with_lang_specific *)node;
		do_type_common((tree)&node0->common, 0);
		do_real_addr(&node0->lang_specific,
				do_c_lang_type((void *)node0->lang_specific, 1));
		return;
	}
	case MODE_GETXREFS:
	{
#if 0
		struct tree_type_with_lang_specific *node0 =
				(struct tree_type_with_lang_specific *)node;
		do_type_common((tree)&node0->common, 0);
		do_c_lang_type((void *)node0->lang_specific, 1);
#endif
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
		struct tree_type_common *node0 = (struct tree_type_common *)node;
		do_common((tree)&node0->common, 0);

		do_real_addr(&node0->size, do_tree(node0->size));
		do_real_addr(&node0->size_unit, do_tree(node0->size_unit));
		do_real_addr(&node0->attributes, do_tree(node0->attributes));
		do_real_addr(&node0->pointer_to, do_tree(node0->pointer_to));
		do_real_addr(&node0->reference_to, do_tree(node0->reference_to));
		do_real_addr(&node0->canonical, do_tree(node0->canonical));
		do_real_addr(&node0->next_variant, do_tree(node0->next_variant));
		do_real_addr(&node0->main_variant, do_tree(node0->main_variant));
		do_real_addr(&node0->context, do_tree(node0->context));
		do_real_addr(&node0->name, do_tree(node0->name));
		return;
	}
	case MODE_GETXREFS:
	{
#if 0
		struct tree_type_common *node0 = (struct tree_type_common *)node;
		do_common((tree)&node0->common, 0);

		do_tree(node0->size);
		do_tree(node0->size_unit);
		do_tree(node0->attributes);
		do_tree(node0->pointer_to);
		do_tree(node0->reference_to);
		do_tree(node0->canonical);
		do_tree(node0->next_variant);
		do_tree(node0->main_variant);
		do_tree(node0->context);
		do_tree(node0->name);
#endif
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
		struct tree_type_non_common *node0 =
			(struct tree_type_non_common *)node;

		do_type_with_lang_specific((tree)&node0->with_lang_specific, 0);
		do_real_addr(&node0->values, do_tree(node0->values));
		do_real_addr(&node0->minval, do_tree(node0->minval));
		do_real_addr(&node0->maxval, do_tree(node0->maxval));
		do_real_addr(&node0->binfo, do_tree(node0->binfo));

		return;
	}
	case MODE_GETXREFS:
	{
#if 0
		struct tree_type_non_common *node0 =
			(struct tree_type_non_common *)node;

		do_type_with_lang_specific((tree)&node0->with_lang_specific, 0);
		do_tree(node0->values);
		do_tree(node0->minval);
		do_tree(node0->maxval);
		do_tree(node0->binfo);
#endif

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
		struct tree_decl_minimal *node0 =
			(struct tree_decl_minimal *)node;
		do_location(&node0->locus);
		do_common((tree)&node0->common, 0);

		do_real_addr(&node0->name, do_tree(node0->name));
		do_real_addr(&node0->context, do_tree(node0->context));
		return;
	}
	case MODE_GETXREFS:
	{
#if 0
		struct tree_decl_minimal *node0 =
			(struct tree_decl_minimal *)node;
		do_common((tree)&node0->common, 0);

		do_tree(node0->name);
		do_tree(node0->context);
#endif
		return;
	}
	default:
		BUG();
	}
}

static void do_lang_decl(void *node, int flag)
{
	if (!node)
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		BUG();
	}
	case MODE_GETXREFS:
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
		struct tree_decl_common *node0 = (struct tree_decl_common *)node;
		do_decl_minimal((tree)&node0->common, 0);

		do_real_addr(&node0->size, do_tree(node0->size));
		do_real_addr(&node0->size_unit, do_tree(node0->size_unit));
		do_real_addr(&node0->initial, do_tree(node0->initial));
		do_real_addr(&node0->attributes, do_tree(node0->attributes));
		do_real_addr(&node0->abstract_origin,
				do_tree(node0->abstract_origin));
		do_real_addr(&node0->lang_specific,
			do_c_lang_decl((struct c_lang_decl*)node0->lang_specific,1));
		return;
	}
	case MODE_GETXREFS:
	{
#if 0
		struct tree_decl_common *node0 = (struct tree_decl_common *)node;
		do_decl_minimal((tree)&node0->common, 0);

		do_tree(node0->size);
		do_tree(node0->size_unit);
		do_tree(node0->initial);
		do_tree(node0->attributes);
		do_tree(node0->abstract_origin);
		do_c_lang_decl((struct c_lang_decl*)node0->lang_specific,1);
#endif
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
		struct tree_field_decl *node0 = (struct tree_field_decl *)node;
		do_decl_common((tree)&node0->common, 0);

		do_real_addr(&node0->offset, do_tree(node0->offset));
		do_real_addr(&node0->bit_field_type,
				do_tree(node0->bit_field_type));
		do_real_addr(&node0->qualifier, do_tree(node0->qualifier));
		do_real_addr(&node0->bit_offset, do_tree(node0->bit_offset));
		do_real_addr(&node0->fcontext, do_tree(node0->fcontext));
		return;
	}
	case MODE_GETXREFS:
	{
#if 0
		struct tree_field_decl *node0 = (struct tree_field_decl *)node;
		do_decl_common((tree)&node0->common, 0);

		do_tree(node0->offset);
		do_tree(node0->bit_field_type);
		do_tree(node0->qualifier);
		do_tree(node0->bit_offset);
		do_tree(node0->fcontext);
#endif
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
		struct tree_decl_with_rtl *node0 =
			(struct tree_decl_with_rtl *)node;
		do_decl_common((tree)&node0->common, 0);
		return;
	}
	case MODE_GETXREFS:
	{
#if 0
		struct tree_decl_with_rtl *node0 =
			(struct tree_decl_with_rtl *)node;
		do_decl_common((tree)&node0->common, 0);
#endif
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
		struct tree_label_decl *node0 = (struct tree_label_decl *)node;
		do_decl_with_rtl((tree)&node0->common, 0);
		return;
	}
	case MODE_GETXREFS:
	{
#if 0
		struct tree_label_decl *node0 = (struct tree_label_decl *)node;
		do_decl_with_rtl((tree)&node0->common, 0);
#endif
		return;
	}
	default:
		BUG();
	}
}

static void do_result_decl(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		struct tree_result_decl *node0 = (struct tree_result_decl *)node;
		do_decl_with_rtl((tree)&node0->common, 0);
		return;
	}
	case MODE_GETXREFS:
	{
#if 0
		struct tree_result_decl *node0 = (struct tree_result_decl *)node;
		do_decl_with_rtl((tree)&node0->common, 0);
#endif
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
		struct tree_parm_decl *node0 = (struct tree_parm_decl *)node;
		do_decl_with_rtl((tree)&node0->common, 0);
		return;
	}
	case MODE_GETXREFS:
	{
		struct var_node_list *vnl;
		struct use_at_list *newua_type;
		struct use_at_list *newua_var;
		vnl = var_node_list_find(&cur_func_node->args, node);
		if (!vnl)
			return;
		newua_type = use_at_list_find(&vnl->var.type->used_at, cur_gimple,
						cur_gimple_op_idx);
		if (!newua_type) {
			newua_type = use_at_list_new();
			newua_type->func_id = cur_fn->node_id.id;
			newua_type->gimple_stmt = (void *)cur_gimple;
			newua_type->op_idx = cur_gimple_op_idx;
			list_add_tail(&newua_type->sibling, &vnl->var.type->used_at);
		}
		newua_var = use_at_list_find(&vnl->var.used_at, cur_gimple,
						cur_gimple_op_idx);
		if (!newua_var) {
			newua_var = use_at_list_new();
			newua_var->func_id = cur_fn->node_id.id;
			newua_var->gimple_stmt = (void *)cur_gimple;
			newua_var->op_idx = cur_gimple_op_idx;
			list_add_tail(&newua_var->sibling, &vnl->var.used_at);
		}
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
		struct tree_decl_with_vis *node0 =
			(struct tree_decl_with_vis *)node;
		do_decl_with_rtl((tree)&node0->common, 0);

		do_real_addr(&node0->assembler_name,
				do_tree(node0->assembler_name));
		do_real_addr(&node0->symtab_node,
				do_symtab_node((void *)node0->symtab_node, 1));
		return;
	}
	case MODE_GETXREFS:
	{
#if 0
		struct tree_decl_with_vis *node0 =
			(struct tree_decl_with_vis *)node;
		do_decl_with_rtl((tree)&node0->common, 0);

		do_tree(node0->assembler_name);
		//do_symtab_node((void *)node0->symtab_node, 1);
#endif
		return;
	}
	default:
		BUG();
	}
}

static void __get_type_detail(struct type_node **base, struct list_head *head,
				struct type_node **next, tree node);
static void do_init_value(struct var_node *vn, tree init_tree);
static void do_var_decl(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;

	switch (mode) {
	case MODE_ADJUST:
	{
		struct tree_var_decl *node0 = (struct tree_var_decl *)node;

		do_decl_with_vis((tree)&node0->common, 0);

		return;
	}
	case MODE_GETXREFS:
	{
		struct tree_var_decl *node0 = (struct tree_var_decl *)node;
		tree ctx = DECL_CONTEXT(node);

		if (is_global_var(node) &&
			((!ctx) || (TREE_CODE(ctx) == TRANSLATION_UNIT_DECL))) {
			/* global vars */
			struct sinode *global_var_sn;
			struct var_node *gvn;
			struct id_list *newgv = NULL;
			long value;
			long val_flag;
			struct use_at_list *newua_type = NULL, *newua_var = NULL;

			get_var_sinode(node, &global_var_sn, 1);
			if (!global_var_sn) {
				value = (long)node;
				val_flag = 1;
			} else {
				value = global_var_sn->node_id.id.id1;
				val_flag = 0;
				gvn = (struct var_node *)global_var_sn->data;
			}

			newgv = id_list_find(&cur_func_node->global_vars,
						value, val_flag);
			if (!newgv) {
				newgv = id_list_new();
				newgv->value = value;
				newgv->value_flag = val_flag;
				list_add_tail(&newgv->sibling,
						&cur_func_node->global_vars);
			}
			if (val_flag)
				return;

			newua_type = use_at_list_find(&gvn->type->used_at,
							cur_gimple,
							cur_gimple_op_idx);
			if (!newua_type) {
				newua_type = use_at_list_new();
				newua_type->func_id = cur_fn->node_id.id;
				newua_type->gimple_stmt = (void *)cur_gimple;
				newua_type->op_idx = cur_gimple_op_idx;
				list_add_tail(&newua_type->sibling,
						&gvn->type->used_at);
				newua_var = use_at_list_new();
				newua_var->func_id = cur_fn->node_id.id;
				newua_var->gimple_stmt = (void *)cur_gimple;
				newua_var->op_idx = cur_gimple_op_idx;
				list_add_tail(&newua_var->sibling,
						&gvn->used_at);
			}

			return;
		}

		/* so, a local variable. static local variable should be handled */
		while (TREE_CODE(ctx) == BLOCK)
			ctx = BLOCK_SUPERCONTEXT(ctx);
		if (ctx == cur_func) {
			/* current function local vars */
			char name[NAME_MAX];
			struct var_node_list *newlv = NULL;
			struct use_at_list *newua_type = NULL, *newua_var = NULL;
			newlv = var_node_list_find(&cur_func_node->local_vars,
							(void *)node);
			if (!newlv) {
				newlv = var_node_list_new((void *)node);
				if (DECL_NAME(node)) {
					memset(name, 0, NAME_MAX);
					get_node_name(DECL_NAME(node), name);
					newlv->var.name = (char *)src__src_buf_get(
								strlen(name)+1);
					memcpy(newlv->var.name, name,
								strlen(name)+1);
				}
				list_add_tail(&newlv->sibling,
						&cur_func_node->local_vars);
			}
			if (!newlv->var.type)
				__get_type_detail(&newlv->var.type, NULL, NULL,
							TREE_TYPE(node));
			newua_type = use_at_list_find(&newlv->var.type->used_at,
							cur_gimple,
							cur_gimple_op_idx);
			if (!newua_type) {
				newua_type = use_at_list_new();
				newua_type->func_id = cur_fn->node_id.id;
				newua_type->gimple_stmt = (void *)cur_gimple;
				newua_type->op_idx = cur_gimple_op_idx;
				list_add_tail(&newua_type->sibling,
						&newlv->var.type->used_at);
				newua_var = use_at_list_new();
				newua_var->func_id = cur_fn->node_id.id;
				newua_var->gimple_stmt = (void *)cur_gimple;
				newua_var->op_idx = cur_gimple_op_idx;
				list_add_tail(&newua_var->sibling,
						&newlv->var.used_at);
			}

			if (TREE_STATIC(node)) {
				do_init_value(&newlv->var, DECL_INITIAL(node));
			}

			return;
		}

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
		struct tree_decl_non_common *node0 =
			(struct tree_decl_non_common *)node;
		do_decl_with_vis((tree)&node0->common, 0);

		do_real_addr(&node0->result, do_tree(node0->result));
		return;
	}
	case MODE_GETXREFS:
	{
		struct tree_decl_non_common *node0 =
			(struct tree_decl_non_common *)node;
		do_decl_with_vis((tree)&node0->common, 0);

		do_tree(node0->result);
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
		struct tree_type_decl *node0 = (struct tree_type_decl *)node;
		do_decl_non_common((tree)&node0->common, 0);
		return;
	}
	case MODE_GETXREFS:
	{
		struct tree_type_decl *node0 = (struct tree_type_decl *)node;
		do_decl_non_common((tree)&node0->common, 0);
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
		struct tree_translation_unit_decl *node0 =
				(struct tree_translation_unit_decl *)node;
		do_decl_common((tree)&node0->common, 0);

		do_real_addr(&node0->language,
				is_obj_checked((void *)node0->language));
		return;
	}
	case MODE_GETXREFS:
	{
		struct tree_translation_unit_decl *node0 =
				(struct tree_translation_unit_decl *)node;
		do_decl_common((tree)&node0->common, 0);
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
		struct tree_function_decl *node0 =
			(struct tree_function_decl *)node;
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
		return;
	}
	case MODE_GETXREFS:
	{
		struct tree_function_decl *node0 =
			(struct tree_function_decl *)node;

		struct sinode *fsn;
		get_func_sinode(node, &fsn, 1);
		if (!fsn)
			return;
		if (fsn == cur_fn)
			return;
		struct func_node *fn = (struct func_node *)fsn->data;
		struct use_at_list *newua = use_at_list_find(&fn->used_at,
								cur_gimple,
								cur_gimple_op_idx);
		if (!newua) {
			newua = use_at_list_new();
			newua->func_id = cur_fn->node_id.id;
			newua->gimple_stmt = (void *)cur_gimple;
			newua->op_idx = cur_gimple_op_idx;
			list_add_tail(&newua->sibling, &fn->used_at);
		}

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
		struct tree_const_decl *node0 = (struct tree_const_decl *)node;
		do_decl_common((tree)&node0->common, 0);
		return;
	}
	case MODE_GETXREFS:
	{
		struct tree_const_decl *node0 = (struct tree_const_decl *)node;
		do_decl_common((tree)&node0->common, 0);
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
		struct tree_vec *node0 = (struct tree_vec *)node;
		do_common((tree)&node0->common, 0);

		for (int i = 0; i < TREE_VEC_LENGTH(node); i++) {
			do_real_addr(&node0->a[i], do_tree(node0->a[i]));
		}
		return;
	}
	case MODE_GETXREFS:
	{
		struct tree_vec *node0 = (struct tree_vec *)node;
		do_common((tree)&node0->common, 0);

		for (int i = 0; i < TREE_VEC_LENGTH(node); i++) {
			do_tree(node0->a[i]);
		}
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
		BUG();
	}
	case MODE_GETXREFS:
	{
		BUG();
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
		BUG();
	}
	case MODE_GETXREFS:
	{
		BUG();
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
		BUG();
	}
	case MODE_GETXREFS:
	{
		BUG();
	}
	default:
		BUG();
	}
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
		BUG();
	}
	case MODE_GETXREFS:
	{
		BUG();
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
		BUG();
	}
	case MODE_GETXREFS:
	{
		BUG();
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
		BUG();
	}
	case MODE_GETXREFS:
	{
		BUG();
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
		struct tree_optimization_option *node0 =
					(struct tree_optimization_option *)node;
		do_common((tree)&node0->common, 0);
		return;
	}
	case MODE_GETXREFS:
	{
#if 0
		struct tree_optimization_option *node0 =
					(struct tree_optimization_option *)node;
		do_common((tree)&node0->common, 0);
#endif
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
		BUG();
	}
	case MODE_GETXREFS:
	{
		BUG();
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
		(newl->line == old->loc_line) && (newl->column == old->loc_col))
		BUG();
		return TREE_NAME_CONFLICT_DROP;
#endif

	tree oldtree = (tree)(long)old->obj->real_addr;
	tree newtree = (tree)(long)(objs[obj_idx].real_addr);
	struct tree_decl_with_vis *oldvis = (struct tree_decl_with_vis *)oldtree;
	struct tree_decl_with_vis *newvis = (struct tree_decl_with_vis *)newtree;

	if (oldvis->weak_flag <= newvis->weak_flag)
		return TREE_NAME_CONFLICT_DROP;
	else
		return TREE_NAME_CONFLICT_REPLACE;
}

static int var_conflict(expanded_location *newl, struct sinode *old)
{
#if 0
	if ((strlen(old->loc_file->name) == strlen(newl->file)) &&
		(!strcmp((char *)old->loc_file->name, newl->file)) &&
		(newl->line == old->loc_line) && (newl->column == old->loc_col))
		return TREE_NAME_CONFLICT_DROP;
#endif

	tree oldtree = (tree)(long)old->obj->real_addr;
	tree newtree = (tree)(long)(objs[obj_idx].real_addr);
	struct tree_decl_with_vis *oldvis = (struct tree_decl_with_vis *)oldtree;
	struct tree_decl_with_vis *newvis = (struct tree_decl_with_vis *)newtree;

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

	tree oldtree = (tree)(long)old->obj->real_addr;
	tree newtree = (tree)(long)(objs[obj_idx].real_addr);
	if (TREE_CODE(newtree) != TREE_CODE(oldtree))
		return TREE_NAME_CONFLICT_SOFT;
	if (TREE_CODE(TYPE_NAME(newtree)) != TREE_CODE(TYPE_NAME(oldtree)))
		return TREE_NAME_CONFLICT_SOFT;

	return TREE_NAME_CONFLICT_DROP;
}

/*
 * RETURN VALUES:
 *	TREE_NAME_CONFLICT_DROP		drop the new node
 *	TREE_NAME_CONFLICT_REPLACE	force insert the new node, replace old one
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
	case TYPE_FUNC_GLOBAL:
		ret = func_conflict(newl, old);
		break;
	case TYPE_VAR_GLOBAL:
		ret = var_conflict(newl, old);
		break;
	case TYPE_TYPE_NAME:
		ret = type_conflict(newl, old);
		break;
	default:
		BUG();
	}
	if (ret != TREE_NAME_CONFLICT_SOFT)
		return ret;

	struct sinode *tmp;
	list_for_each_entry(tmp, &old->same_name_head, same_name_sibling) {
		resfile__resfile_load(tmp->buf);
		switch (type) {
		case TYPE_FUNC_GLOBAL:
			ret = func_conflict(newl, tmp);
			break;
		case TYPE_VAR_GLOBAL:
			ret = var_conflict(newl, tmp);
			break;
		case TYPE_TYPE_NAME:
			ret = type_conflict(newl, tmp);
			break;
		default:
			BUG();
		}
		if (ret != TREE_NAME_CONFLICT_SOFT)
			break;
	}
	return ret;
}

/*
 * SYMBOL could be exported
 */
static lock_t _sinode_lock;
static void do_get_base(struct sibuf *buf)
{
	/*
	 * XXX, iter the objs, find out types, vars, functions,
	 * get the locations of them(the file, we sinode_new TYPE_FILE)
	 * sinode_new for the new node
	 */
	struct sinode *sn_new, *sn_tmp, *loc_file;

	/* init global_var func sinode, without data; type with data */
	for (obj_idx = 0; obj_idx < obj_cnt; obj_idx++) {
		char name[NAME_MAX];
		enum sinode_type type;
		int behavior = SINODE_INSERT_BH_NONE;
		int flag = 0;
		memset(name, 0, NAME_MAX);

		void *obj_addr = (void *)(long)(objs[obj_idx].real_addr);
		expanded_location *xloc;
		if (objs[obj_idx].is_type) {
			get_type_name(obj_addr, name);
			xloc = get_location(GET_LOC_TYPE,
						buf->payload, (tree)obj_addr);
			if (xloc && xloc->file &&
				(strcmp(xloc->file, "<built-in>")))
				type = TYPE_TYPE_LOC;
			else if (name[0])
				type = TYPE_TYPE_NAME;
			else
				type = TYPE_NONE;
		} else if (objs[obj_idx].is_function) {
			flag = check_file_func((tree)obj_addr);
			if (flag == FUNC_IS_EXTERN)
				continue;
			else if (flag == FUNC_IS_STATIC)
				type = TYPE_FUNC_STATIC;
			else if (flag == FUNC_IS_GLOBAL)
				type = TYPE_FUNC_GLOBAL;
			else
				BUG();

			get_function_name(obj_addr, name);
			xloc = get_location(GET_LOC_FUNC,
						buf->payload, (tree)obj_addr);
		} else if (objs[obj_idx].is_global_var) {
			flag = check_file_var((tree)obj_addr);
			if (flag == VAR_IS_EXTERN)
				continue;
			else if (flag == VAR_IS_STATIC)
				type = TYPE_VAR_STATIC;
			else if (flag == VAR_IS_GLOBAL)
				type = TYPE_VAR_GLOBAL;
			else
				BUG();

			get_var_name(obj_addr, name);
			xloc = get_location(GET_LOC_VAR,
						buf->payload, (tree)obj_addr);
		} else {
			continue;
		}

		sn_new = NULL;
		sn_tmp = NULL;
		loc_file = NULL;
		mutex_lock(&_sinode_lock);

		/* handle the location file */
		if (!xloc)	/* happen only on TYPE_TYPE */
			goto step_1;
		if (!xloc->file)
			goto step_1;
		loc_file = sinode__sinode_search(TYPE_FILE, SEARCH_BY_NAME,
						(void *)xloc->file);
		if (loc_file)
			goto step_1;
		loc_file = sinode__sinode_new(TYPE_FILE,
						(char *)xloc->file,
						strlen(xloc->file)+1,
						NULL, 0);
		BUG_ON(!loc_file);
		BUG_ON(sinode__sinode_insert(loc_file, SINODE_INSERT_BH_NONE));

step_1:
		if ((type == TYPE_FUNC_GLOBAL) || (type == TYPE_VAR_GLOBAL) ||
			(type == TYPE_TYPE_NAME)) {
			BUG_ON(!name[0]);
			sn_tmp = sinode__sinode_search(type, SEARCH_BY_NAME, name);
		} else if ((type == TYPE_FUNC_STATIC) || (type == TYPE_VAR_STATIC) ||
				(type == TYPE_TYPE_LOC)) {
			BUG_ON(!loc_file);
			long args[3];
			args[0] = (long)xloc->file;
			args[1] = xloc->line;
			args[2] = xloc->column;
			sn_tmp = sinode__sinode_search(type, SEARCH_BY_LOC,
							(void *)args);
		} else if (type == TYPE_NONE) {
			struct type_node *tn;
			enum tree_code tc = TREE_CODE((tree)obj_addr);
			tn = sibuf__sibuf_type_node_search(buf, tc, obj_addr);
			if (tn)
				goto next_loop;

			struct sibuf_type_node *_new;
			_new = sibuf_type_node_new();
			type_node_init(&_new->type, obj_addr, tc);
			BUG_ON(sibuf__sibuf_type_node_insert(buf, _new));
			goto next_loop;
		} else {
			BUG();
		}

		if (sn_tmp && ((type == TYPE_FUNC_STATIC) ||
				(type == TYPE_VAR_STATIC) ||
				(type == TYPE_TYPE_LOC))) {
			goto next_loop;
		}

		if (sn_tmp && ((type == TYPE_FUNC_GLOBAL) ||
				(type == TYPE_VAR_GLOBAL) ||
				(type == TYPE_TYPE_NAME))) {
			resfile__resfile_load(sn_tmp->buf);
			int chk_val = TREE_NAME_CONFLICT_FAILED;
			chk_val = check_conflict(type, xloc, sn_tmp);
			BUG_ON(chk_val == TREE_NAME_CONFLICT_FAILED);
			if (chk_val == TREE_NAME_CONFLICT_DROP) {
				objs[obj_idx].is_dropped = 1;
				mutex_unlock(&_sinode_lock);
				continue;
			} else if (chk_val == TREE_NAME_CONFLICT_REPLACE) {
				behavior = SINODE_INSERT_BH_REPLACE;
				sn_tmp->obj->is_replaced = 1;
			} else if (chk_val == TREE_NAME_CONFLICT_SOFT) {
				behavior = SINODE_INSERT_BH_SOFT;
			} else
				BUG();
		}

		sn_new = sinode__sinode_new(type, name, strlen(name)+1, NULL, 0);
		BUG_ON(!sn_new);
		sn_new->buf = buf;
		sn_new->obj = &objs[obj_idx];
		sn_new->loc_file = loc_file;
		if (loc_file) {
			sn_new->loc_line = xloc->line;
			sn_new->loc_col = xloc->column;
		}
		if ((type == TYPE_TYPE_NAME) || (type == TYPE_TYPE_LOC)) {
			struct type_node *tn;
			tn = type_node_new(obj_addr, TREE_CODE(tree(obj_addr)));
			tn->type_name = sn_new->name;
			sn_new->data = (char *)tn;
			sn_new->datalen = sizeof(*tn);
		} else if ((type == TYPE_VAR_GLOBAL) || (type == TYPE_VAR_STATIC)) {
			struct var_node *vn;
			vn = var_node_new(obj_addr);
			vn->name = sn_new->name;
			sn_new->data = (char *)vn;
			sn_new->datalen = sizeof(*vn);
		} else if ((type == TYPE_FUNC_GLOBAL) ||
				(type == TYPE_FUNC_STATIC)) {
			struct func_node *fn = NULL;
			struct function *func;
			func = DECL_STRUCT_FUNCTION((tree)obj_addr);
			if (func && func->gimple_body) {
				fn = func_node_new((void *)obj_addr);
				fn->name = sn_new->name;
				sn_new->data = (char *)fn;
				sn_new->datalen = sizeof(*fn);
			}
		}

		BUG_ON(sinode__sinode_insert(sn_new, behavior));
next_loop:
		mutex_unlock(&_sinode_lock);
	}
}

static struct type_node *find_type_node(tree type)
{
	struct sinode *sn;
	struct type_node *tn;
	get_type_xnode(type, &sn, &tn);
	if (sn) {
		tn = (struct type_node *)sn->data;
	}
	BUG_ON(!tn);

	return tn;
}

/*
 * XXX: if this is the upper call, base MUST NOT be NULL, otherwise, MUST be NULL
 * if new field_list should be inserted into a list, then head is not NULL,
 * if new field_list should be pointed by next, then next is not NULL
 */
static void __get_type_detail(struct type_node **base, struct list_head *head,
				struct type_node **next, tree node)
{
	int upper_call = 0;
	if (base)
		upper_call = 1;
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
		SI_LOG("miss %s\n", tree_code_name[code]);
		BUG();
		break;
	case REFERENCE_TYPE:
	{
		new_type = find_type_node(node);
		if (new_type->is_set)
			break;
		new_type->is_set = 1;
		__get_type_detail(NULL, NULL, &new_type->next, TREE_TYPE(node));
		break;
	}
	case POINTER_BOUNDS_TYPE:
	case BOOLEAN_TYPE:
	case VOID_TYPE:
	{
		new_type = find_type_node(node);
		if (new_type->is_set)
			break;
		new_type->is_set = 1;
		break;
	}
	case ENUMERAL_TYPE:
	{
		new_type = find_type_node(node);
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
			struct var_node_list *_new_var;
			_new_var = var_node_list_new((void *)enum_list);
			_new_var->var.name =
					(char *)src__src_buf_get(strlen(name)+1);
			memcpy(_new_var->var.name, name, strlen(name)+1);
			long value = (long)TREE_INT_CST_LOW(TREE_VALUE(enum_list));
			struct possible_value_list *pv = NULL;
			if (!possible_value_list_find(&_new_var->var.possible_values,
						VALUE_IS_INT_CST, value)) {
				pv = possible_value_list_new();
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
		if (new_type->is_set)
			break;

		new_type->is_set = 1;
		if (TYPE_DOMAIN(node) && TYPE_MAX_VALUE(TYPE_DOMAIN(node)))
			new_type->baselen = TREE_INT_CST_LOW(TYPE_MAX_VALUE(
							TYPE_DOMAIN(node))) + 1;
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

			struct var_node_list *newf0;
			newf0 = var_node_list_new((void *)fields);
			struct var_node *newf1 = &newf0->var;
			__get_type_detail(&newf1->type, NULL, NULL,
						TREE_TYPE(fields));
			if (DECL_NAME(fields)) {
				memset(name, 0, NAME_MAX);
				get_node_name(DECL_NAME(fields), name);
				newf1->name = (char *)src__src_buf_get(
								strlen(name)+1);
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
				struct type_node *_new = type_node_new((void *)args,
									VOID_TYPE);
				_new->type_name = (char *)src__src_buf_get(
								strlen(name)+1);
				memcpy(_new->type_name, name, strlen(name)+1);
				list_add_tail(&_new->sibling, &new_type->children);
			} else {
				__get_type_detail(NULL, &new_type->children,
							NULL, TREE_VALUE(args));
			}

			args = TREE_CHAIN(args);
		}
		break;
	}
	}

	if (upper_call)
		*base = new_type;
	if (head && new_type)
		list_add_tail(&new_type->sibling, head);
	if (next)
		*next = new_type;
}

/* XXX, check gcc/c/c-decl.c c-typeck.c for more information */
static void get_type_detail(struct type_node *tn)
{
	tree node = (tree)tn->node;
	struct type_node *new_type = NULL;

	/* TODO: reduce the information we get here. value and attributes */
	__get_type_detail(&new_type, NULL, NULL, node);
	if ((long)tn != (long)new_type)
		BUG();
	return;
}

static void get_var_detail(struct sinode *sn)
{
	resfile__resfile_load(sn->buf);
	tree node = (tree)(long)sn->obj->real_addr;
	struct var_node *new_var;
	new_var = (struct var_node *)sn->data;

	__get_type_detail(&new_var->type, NULL, NULL, TREE_TYPE(node));
	if (unlikely(!new_var->type))
		BUG();

	/* TODO, value(DECL_INITIAL) and attributes */
	return;
}

static void get_function_detail(struct sinode *sn)
{
	resfile__resfile_load(sn->buf);
	tree node = (tree)(long)sn->obj->real_addr;
	/* XXX: the function body is in node->f->gimple_body */
	struct function *func = DECL_STRUCT_FUNCTION(node);
	if ((!func) || (!func->gimple_body))
		return;
	gimple_seq body = func->gimple_body;
	gimple_seq next = body;
	char name[NAME_MAX];
	struct func_node *new_func;
	new_func = (struct func_node *)sn->data;

	/* the return type */
	__get_type_detail(&new_func->ret_type, NULL, NULL,
				TREE_TYPE(TREE_TYPE(node)));
	if (unlikely(!new_func->ret_type))
		BUG();

	/* the argument list */
	tree args = DECL_ARGUMENTS(node);
	while (args) {
		struct var_node_list *new_arg;
		new_arg = var_node_list_new((void *)args);
		struct var_node *new_vn = &new_arg->var;
		__get_type_detail(&new_vn->type, NULL, NULL, TREE_TYPE(args));
		if (unlikely(!new_vn->type))
			BUG();

		memset(name, 0, NAME_MAX);
		get_node_name(DECL_NAME(args), name);
		BUG_ON(!name[0]);
		new_vn->name = (char *)src__src_buf_get(strlen(name)+1);
		memcpy(new_vn->name, name, strlen(name)+1);

		list_add_tail(&new_arg->sibling, &new_func->args);

		args = TREE_CHAIN(args);
	}

	/*
	 * time to deal with the function body, which now is low gimple
	 * for each label, we alloc a code_path for it;
	 */
	gimple_seq labels[LABEL_MAX] = { 0 };
	struct code_path *cps[LABEL_MAX] = { 0 };
	unsigned long brs[LABEL_MAX] = { 0 };
	int linked[LABEL_MAX] = { 0 };
	int label_idx = 0;
	unsigned long total_gimples = 0;
	unsigned long label_gimples = 0;
	next = body;
	if (body->code != GIMPLE_LABEL) {
		labels[0] = body;
		linked[label_idx++] = 1;
	}
	while (next) {
		enum gimple_code gc = next->code;
		if (gc == GIMPLE_LABEL) {
			labels[label_idx] = next;
			label_idx++;
			label_gimples++;
			if (unlikely(label_idx >= LABEL_MAX))
				BUG();
		} else if (gc == GIMPLE_SWITCH) {
			BUG_ON((next->next) && (next->next->code != GIMPLE_LABEL));
			int branches = gimple_num_ops(next)-1;
			if (branches <= 2)
				branches = 0;
			else
				branches = branches - 2;
			brs[label_idx-1] += branches;
		} else if (gc == GIMPLE_ASM) {
			struct gasm *ga;
			ga = (struct gasm *)next;
			int branches = ga->nl;
			if (!branches)
				goto next_loop0;
			else if (branches <= 2)
				branches = 0;
			else
				branches = branches - 2;

			if (next->next && (next->next->code != GIMPLE_LABEL)) {
				total_gimples++;
				next = next->next;
				labels[label_idx] = next;
				label_idx++;
				branches++;
			}
			brs[label_idx-1] += branches;
		}
next_loop0:
		total_gimples++;
		next = next->next;
	}
	gimple_seq reachable[total_gimples] = { 0 };
	unsigned long reachable_idx = 0;
	for (int i = 0; i < label_idx; i++) {
		cps[i] = code_path_new(new_func, brs[i]);
	}
	for (int i = 0; i < label_idx; i++) {
		if ((!i) && (body->code != GIMPLE_LABEL))
			next = labels[i];
		else
			next = labels[i]->next;
		while (next) {
			enum gimple_code gc = next->code;
			if (gc == GIMPLE_SWITCH) {
				cps[i]->cond_head = (void *)next;
				reachable[reachable_idx++] = next;
				tree *ops = gimple_ops(next);
				for (int k = 1; k < gimple_num_ops(next); k++) {
					BUG_ON(TREE_CODE(ops[k]) != CASE_LABEL_EXPR);
					tree label = CASE_LABEL(ops[k]);
					int j = 0;
					if (body->code != GIMPLE_LABEL)
						j = 1;
					while (j < label_idx) {
						struct glabel *gl =
							(struct glabel *)labels[j];
						if (label == gl->op[0])
							break;
						j++;
					}
					BUG_ON(j == label_idx);
					cps[i]->next[k-1] = cps[j];
					linked[j] = 1;
				}
				break;
			} else if (gc == GIMPLE_COND) {
				cps[i]->cond_head = (void *)next;
				reachable[reachable_idx++] = next;
				struct gcond *gs = (struct gcond *)next;
				tree true_label = gs->op[2];
				tree false_label = gs->op[3];
				int j = 0;
				if (body->code != GIMPLE_LABEL)
					j = 1;
				while (j < label_idx) {
					struct glabel *gl=(struct glabel *)labels[j];
					if (true_label == gl->op[0])
						break;
					j++;
				}
				BUG_ON(j == label_idx);
				cps[i]->next[1] = cps[j];
				linked[j] = 1;

				j = 0;
				if (body->code != GIMPLE_LABEL)
					j = 1;
				while (j < label_idx) {
					struct glabel *gl=(struct glabel *)labels[j];
					if (false_label == gl->op[0])
						break;
					j++;
				}
				BUG_ON(j == label_idx);
				cps[i]->next[0] = cps[j];
				linked[j] = 1;
				break;
			} else if (gc == GIMPLE_GOTO) {
				reachable[reachable_idx++] = next;
				struct ggoto *gs = (struct ggoto *)next;
				tree label = gs->op[0];
				int j = 0;
				if (body->code != GIMPLE_LABEL)
					j = 1;
				while (j < label_idx) {
					struct glabel *gl=(struct glabel *)labels[j];
					if (label == gl->op[0])
						break;
					j++;
				}
				BUG_ON(j == label_idx);
				cps[i]->next[1] = cps[j];
				linked[j] = 1;
				break;
			} else if (gc == GIMPLE_LABEL) {
				struct glabel *gl0 = (struct glabel *)next;
				tree label = gl0->op[0];
				int j = 0;
				if (body->code != GIMPLE_LABEL)
					j = 1;
				while (j < label_idx) {
					struct glabel *gl1 =
							(struct glabel *)labels[j];
					if (label == gl1->op[0])
						break;
					j++;
				}
				BUG_ON(j == label_idx);
				cps[i]->next[1] = cps[j];
				linked[j] = 1;
				break;
			} else if (gc == GIMPLE_ASM) {
				struct gasm *ga = (struct gasm *)next;
				int asm_labels = ga->nl;
				if (!asm_labels)
					goto regular_stmt;
				int one_more = 0;
				if (next->next &&
					(next->next->code != GIMPLE_LABEL)) {
					cps[i]->next[0] = cps[i+1];
					one_more = 1;
					linked[i+1] = 1;
				}
				reachable[reachable_idx++] = next;
				tree *tls = &ga->op[ga->ni + ga->nc];
				for (int il = 0; il < asm_labels; il++) {
					BUG_ON(TREE_CODE(tls[il]) != TREE_LIST);
					struct tree_list *tl;
					tl = (struct tree_list *)tls[il];
					tree tlbl = tl->value;
					int j = 0;
					if (body->code != GIMPLE_LABEL)
						j = 1;
					while (j < label_idx) {
						struct glabel *gl1;
						gl1 = (struct glabel *)labels[j];
						if (tlbl == gl1->op[0])
							break;
						j++;
					}
					BUG_ON(j == label_idx);
					cps[i]->next[il+one_more] = cps[j];
					linked[j] = 1;
				}
				break;
			}
regular_stmt:
			struct code_sentence *newcs;
			newcs = (struct code_sentence *)src__src_buf_get(
						sizeof(*newcs));
			memset(newcs, 0, sizeof(*newcs));
			newcs->head = (void *)next;
			reachable[reachable_idx++] = next;
			list_add_tail(&newcs->sibling, &cps[i]->sentences);

			next = next->next;
		}
	}

	if ((reachable_idx + label_gimples) == total_gimples) {
		for (int i = 1; i < label_idx; i++) {
			if (unlikely(!linked[i]))
				BUG();
		}
		goto do_insert;
	}

	for (next = body; next; next = next->next) {
		/*
		 * TODO: some labels are not used, should handle that too
		 */
		int i = 0;
		struct unr_stmt *_unr;
		expanded_location *xloc;
		if (next->code == GIMPLE_LABEL)
			continue;

		for (i = 0; i < reachable_idx; i++) {
			if (next == reachable[i])
				break;
		}
		if (i < reachable_idx)
			continue;

		xloc = get_gimple_loc(sn->buf->payload, &next->location);
		BUG_ON(!xloc);
		_unr = unr_stmt_find(&new_func->unreachable_stmts, xloc->line,
					xloc->column);
		if (_unr)
			continue;

		_unr = unr_stmt_new();
		_unr->unr_gimple_stmt = (void *)next;
		_unr->line = xloc->line;
		_unr->col = xloc->column;
		list_add_tail(&_unr->sibling, &new_func->unreachable_stmts);
	}

do_insert:
	new_func->codes = cps[0];

	/* TODO, value and attributes */
	return;
}

/*
 * The gimple body is the key. we should get vars/funcs cross references info,
 * however we need do global_vars with type first.
 * For each type, we could know every global_var with this type.
 * For each global var, we could know where it is used.
 * For each codepath/func, we could know what it need and what it does.
 */
static void do_get_detail(struct sibuf *b)
{
	char name[NAME_MAX];
	expanded_location *xloc;
	enum sinode_type type;
	resfile__resfile_load(b);

	for (obj_idx = 0; obj_idx < obj_cnt; obj_idx++) {
		if (objs[obj_idx].is_dropped || objs[obj_idx].is_replaced)
			continue;

		void *obj_addr = (void *)(long)(objs[obj_idx].real_addr);
		if (!objs[obj_idx].is_type) {
			continue;
		}

		struct sinode *sn;
		struct type_node *tn;
		get_type_xnode((tree)obj_addr, &sn, &tn);
		if (sn) {
			resfile__resfile_load(sn->buf);
			tn = (struct type_node *)sn->data;
		}
		BUG_ON(!tn);

		if (tn->is_set)
			continue;
		get_type_detail(tn);
	}

	for (obj_idx = 0; obj_idx < obj_cnt; obj_idx++) {
		if (objs[obj_idx].is_dropped || objs[obj_idx].is_replaced)
			continue;

		void *obj_addr = (void *)(long)(objs[obj_idx].real_addr);
		if (!objs[obj_idx].is_global_var) {
			continue;
		}

		struct sinode *n;
		get_var_sinode((tree)obj_addr, &n, 0);
		if (!n)
			continue;

		get_var_detail(n);
	}

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

		get_function_detail(n);
	}

	return;
}

static unsigned long get_field_offset(tree field)
{
	unsigned long ret = 0;
	if (DECL_FIELD_OFFSET(field)) {
		ret += TREE_INT_CST_LOW(DECL_FIELD_OFFSET(field)) * 8;
	}
	if (DECL_FIELD_BIT_OFFSET(field)) {
		ret += TREE_INT_CST_LOW(DECL_FIELD_BIT_OFFSET(field));
	}
	return ret;
}

static void do_struct_init(struct type_node *tn, tree init_tree)
{
	BUG_ON(tn->type_code != RECORD_TYPE);

	vec<constructor_elt, va_gc> *init_elts =
			(vec<constructor_elt, va_gc> *)(CONSTRUCTOR_ELTS(init_tree));

	/* XXX, some structures may init the element NULL */
	if (!init_elts)
		return;

	unsigned long length = init_elts->vecpfx.m_num;
	struct constructor_elt *addr = &init_elts->vecdata[0];

	for (unsigned long i = 0; i < length; i++) {
		BUG_ON(TREE_CODE(addr[i].index) != FIELD_DECL);
		struct var_node_list *tmp;
		list_for_each_entry(tmp, &tn->children, sibling) {
			if (get_field_offset((tree)tmp->var.node) !=
					get_field_offset(addr[i].index))
				continue;
			do_init_value(&tmp->var, addr[i].value);
		}
	}
}

static void do_init_value(struct var_node *vn, tree init_tree)
{
	/* XXX, data_node could be var_node or type_node(for structure) */
	if (!init_tree)
		return;

	enum tree_code init_tc = TREE_CODE(init_tree);
	switch (init_tc) {
	case CONSTRUCTOR:
	{
		vec<constructor_elt, va_gc> *init_elts =
			(vec<constructor_elt, va_gc> *)(CONSTRUCTOR_ELTS(init_tree));

		unsigned long length = init_elts->vecpfx.m_num;
		struct constructor_elt *addr = &init_elts->vecdata[0];

		if (vn->type->type_code == RECORD_TYPE) {
			do_struct_init(vn->type, init_tree);
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
		struct possible_value_list *pv = NULL;
		if (!possible_value_list_find(&vn->possible_values,
						VALUE_IS_INT_CST, value)) {
			pv = possible_value_list_new();
			pv->value_flag = VALUE_IS_INT_CST;
			pv->value = value;
			list_add_tail(&pv->sibling, &vn->possible_values);
		}
		break;
	}
	case STRING_CST:
	{
		long value = (long)((struct tree_string *)init_tree)->str;
		struct possible_value_list *pv = NULL;
		if (!possible_value_list_find(&vn->possible_values,
						VALUE_IS_STR_CST, value)) {
			pv = possible_value_list_new();
			pv->value_flag = VALUE_IS_STR_CST;
			pv->value = value;
			list_add_tail(&pv->sibling, &vn->possible_values);
		}
		break;
	}
	case ADDR_EXPR:
	{
		tree addr = ((struct tree_exp *)init_tree)->operands[0];
		if (TREE_CODE(addr) == FUNCTION_DECL) {
			struct sinode *fsn;
			get_func_sinode(addr, &fsn, 1);
			if (!fsn) {
				/* TODO, this function not found yet */
				long value = (long)addr;
				struct possible_value_list *pv = NULL;
				if (!possible_value_list_find(&vn->possible_values,
								VALUE_IS_TREE,
								value)) {
					pv = possible_value_list_new();
					pv->value_flag = VALUE_IS_TREE;
					pv->value = value;
					list_add_tail(&pv->sibling,
							&vn->possible_values);
				}
			} else {
				long value = fsn->node_id.id.id1;
				struct possible_value_list *pv = NULL;
				if (!possible_value_list_find(&vn->possible_values,
								VALUE_IS_FUNC,
								value)) {
					pv = possible_value_list_new();
					pv->value_flag = VALUE_IS_FUNC;
					pv->value = value;
					list_add_tail(&pv->sibling,
							&vn->possible_values);
				}
			}
		} else if (TREE_CODE(addr) == VAR_DECL) {
			long value = (long)init_tree;
			struct possible_value_list *pv = NULL;
			if (!possible_value_list_find(&vn->possible_values,
							VALUE_IS_VAR_ADDR,
							value)) {
				pv = possible_value_list_new();
				pv->value_flag = VALUE_IS_VAR_ADDR;
				pv->value = value;
				list_add_tail(&pv->sibling, &vn->possible_values);
			}
		} else if (TREE_CODE(addr) == STRING_CST) {
			do_init_value(vn, addr);
		} else {
			SI_LOG("miss %s, %s\n",
					tree_code_name[TREE_CODE(addr)],
					vn->name);
			/* TODO, COMPONENT_REF */
			BUG();
		}
		break;
	}
	case NOP_EXPR:
	{
		tree value = ((struct tree_exp *)init_tree)->operands[0];
		do_init_value(vn, value);
		break;
	}
	default:
	{
		SI_LOG("miss %s\n", tree_code_name[init_tc]);
		BUG();
	}
	}
}

static void get_gvar_xref(struct sinode *sn)
{
	resfile__resfile_load(sn->buf);
	tree node = (tree)(long)sn->obj->real_addr;
	struct var_node *new_var = (struct var_node *)sn->data;

	/* add this var to TYPE_TYPE.global_vars */
	struct type_node *realtype_node = new_var->type;
	struct id_list *ref = NULL;
	if (unlikely(!new_var->type))
		BUG();
	if (!id_list_find(&realtype_node->global_vars, sn->node_id.id.id1, 0)) {
		ref = id_list_new();
		ref->value = sn->node_id.id.id1;
		ref->value = 0;
		list_add_tail(&ref->sibling, &realtype_node->global_vars);
	}

	do_init_value(new_var, DECL_INITIAL(node));

	return;
}

static void get_xrefs(struct sinode *func_sn);
static void do_xrefs(struct sibuf *b)
{
	for (obj_idx = 0; obj_idx < obj_cnt; obj_idx++) {
		if (objs[obj_idx].is_dropped || objs[obj_idx].is_replaced)
			continue;

		void *obj_addr = (void *)(long)(objs[obj_idx].real_addr);
		if (!objs[obj_idx].is_global_var) {
			continue;
		}

		struct sinode *n;
		get_var_sinode((tree)obj_addr, &n, 0);
		if (!n)
			continue;

		get_gvar_xref(n);
	}

	/* getting cross references information is too complicated, do it outside */
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

		get_xrefs(n);
	}

	return;
}

static void handle_marked_func(struct sinode *n);
static void do_indcfg1(struct sibuf *b)
{
	/* handle the marked func */
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

		handle_marked_func(n);
	}

	return;
}

static void get_indirect_cfg(struct sinode *func_sn);
static void do_indcfg2(struct sibuf *buf)
{
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

		get_indirect_cfg(n);
	}
	return;
}

static __thread long show_progress_arg[0x10];
static int show_progress_timeout = 3;
static void c_show_progress(int signo, siginfo_t *si, void *arg)
{
	long *args = (long *)arg;
	char *status = (char *)args[0];
	char *parsing_file = (char *)args[1];
	long *cur = (long *)args[2];
	long *goal = (long *)args[3];
	pthread_t id = (pthread_t)args[4];
	mt_print(id, "%s %s... %.3f%%\n", status, parsing_file,
			(double)(*cur) * 100 / (double)(*goal));
}

static int c_callback(struct sibuf *buf, int parse_mode)
{
	struct file_context *fc = (struct file_context *)buf->load_addr;
	mode = parse_mode;
	cur_sibuf = buf;
	addr_base = buf->payload;
	obj_cnt = buf->obj_cnt;
	objs = buf->objs;
	obj_idx = 0;
	obj_adjusted = 0;
	if (!obj_cnt)
		return 0;

	switch (mode) {
	case MODE_ADJUST:
	{
		if (unlikely(fc->status != FC_STATUS_NONE))
			break;

		show_progress_arg[0] = (long)"MODE_ADJUST";
		show_progress_arg[1] = (long)fc->path;
		show_progress_arg[2] = (long)&obj_adjusted;
		show_progress_arg[3] = (long)&obj_cnt;
		show_progress_arg[4] = (long)pthread_self();
		mt_print_add();
		mt_add_timer(show_progress_timeout, c_show_progress,
				show_progress_arg);

		void *raddrs[obj_cnt];
		void *faddrs[obj_cnt];
		for (int i = 0; i < obj_cnt; i++) {
			objs[i].is_adjusted = 0;
			raddrs[i] = (void *)(unsigned long)objs[i].real_addr;
			faddrs[i] = (void *)(unsigned long)objs[i].fake_addr;
		}
		real_addrs = raddrs;
		fake_addrs = faddrs;

		/*
		 * we collect the lower gimple before cfg pass, all functions have
		 * been chained
		 */
		do_function_decl(
				(tree)(unsigned long)objs[0].real_addr,
				1);

		BUG_ON(obj_adjusted != obj_cnt);
		fc->status = FC_STATUS_ADJUSTED;
		mt_del_timer();
		mt_print_del();
		break;
	}
	case MODE_GETBASE:
	{
		if (unlikely(fc->status != FC_STATUS_ADJUSTED))
			break;

		show_progress_arg[0] = (long)"MODE_GETBASE";
		show_progress_arg[1] = (long)fc->path;
		show_progress_arg[2] = (long)&obj_idx;
		show_progress_arg[3] = (long)&obj_cnt;
		show_progress_arg[4] = (long)pthread_self();
		mt_print_add();
		mt_add_timer(show_progress_timeout, c_show_progress,
				show_progress_arg);

		do_get_base(buf);

		fc->status = FC_STATUS_GETBASE;
		mt_del_timer();
		mt_print_del();
		break;
	}
	case MODE_GETDETAIL:
	{
		if (unlikely(fc->status != FC_STATUS_GETBASE))
			break;

		show_progress_arg[0] = (long)"MODE_GETDETAIL";
		show_progress_arg[1] = (long)fc->path;
		show_progress_arg[2] = (long)&obj_idx;
		show_progress_arg[3] = (long)&obj_cnt;
		show_progress_arg[4] = (long)pthread_self();
		mt_print_add();
		mt_add_timer(show_progress_timeout, c_show_progress,
				show_progress_arg);

		/* XXX: get details till codepath */
		do_get_detail(buf);

		fc->status = FC_STATUS_GETDETAIL;
		mt_del_timer();
		mt_print_del();
		break;
	}
	case MODE_GETXREFS:
	{
		if (unlikely(fc->status != FC_STATUS_GETDETAIL))
			break;

		show_progress_arg[0] = (long)"MODE_GETXREFS";
		show_progress_arg[1] = (long)fc->path;
		show_progress_arg[2] = (long)&obj_idx;
		show_progress_arg[3] = (long)&obj_cnt;
		show_progress_arg[4] = (long)pthread_self();
		mt_print_add();
		mt_add_timer(show_progress_timeout, c_show_progress,
				show_progress_arg);

		void *xaddrs[obj_cnt];
		xrefs_obj_checked = xaddrs;
		/* XXX, get func_node's callees/callers/global/local_vars... */
		do_xrefs(buf);

		fc->status = FC_STATUS_GETXREFS;
		mt_del_timer();
		mt_print_del();
		break;
	}
	case MODE_GETINDCFG1:
	{
		if (unlikely(fc->status != FC_STATUS_GETXREFS))
			break;

		show_progress_arg[0] = (long)"MODE_GETINDCFG1";
		show_progress_arg[1] = (long)fc->path;
		show_progress_arg[2] = (long)&obj_idx;
		show_progress_arg[3] = (long)&obj_cnt;
		show_progress_arg[4] = (long)pthread_self();
		mt_print_add();
		mt_add_timer(show_progress_timeout, c_show_progress,
				show_progress_arg);

		do_indcfg1(buf);

		fc->status = FC_STATUS_GETINDCFG1;
		mt_del_timer();
		mt_print_del();
		break;
	}
	case MODE_GETINDCFG2:
	{
		if (unlikely(fc->status != FC_STATUS_GETINDCFG1))
			break;

		show_progress_arg[0] = (long)"MODE_GETINDCFG2";
		show_progress_arg[1] = (long)fc->path;
		show_progress_arg[2] = (long)&obj_idx;
		show_progress_arg[3] = (long)&obj_cnt;
		show_progress_arg[4] = (long)pthread_self();
		mt_print_add();
		mt_add_timer(show_progress_timeout, c_show_progress,
				show_progress_arg);

		do_indcfg2(buf);

		fc->status = FC_STATUS_GETINDCFG2;
		mt_del_timer();
		mt_print_del();
		break;
	}
	default:
		BUG();
	}

	return 0;
}

/*
 * ************************************************************************
 * the following functions are for cross references for functions/types/vars
 * handle gimple statements
 * ************************************************************************
 */
static struct type_node *do_mem_ref(struct tree_exp *op)
{
	struct tree_exp *exp = (struct tree_exp *)op;
	tree t0 = exp->operands[0];
	tree t1 = exp->operands[1];
	BUG_ON(((struct tree_int_cst *)t1)->val[0]);
	BUG_ON((TREE_CODE(t0) != PARM_DECL) && (TREE_CODE(t0) != VAR_DECL));
	struct type_node *tn0 = find_type_node(TREE_TYPE(t0));
	struct type_node *tn1 = find_type_node(TREE_TYPE(op));
	struct type_node *tn = tn0;
	if (tn0 != tn1)
		tn = tn1;
	BUG_ON(!tn);
	return tn;
}

/*
 * this is a RECORD/UNION, we just get the field's var_node_list, and add use_at
 */
static struct var_node_list *component_ref_get_vnl(struct tree_exp *op)
{
	tree t0 = op->operands[0];
	tree t1 = op->operands[1];
	tree t2 = op->operands[2];
	BUG_ON(t2);
	enum tree_code tc = TREE_CODE(t0);

	struct type_node *tn = NULL;
	switch (tc) {
	case COMPONENT_REF:
	{
		tn = find_type_node(TREE_TYPE(t0));
		break;
	}
	case MEM_REF:
	{
		tn = do_mem_ref((struct tree_exp *)t0);
		break;
	}
	case VAR_DECL:
	{
		tn = find_type_node(TREE_TYPE(t0));
		break;
	}
	default:
	{
		enum gimple_code gc = gimple_code(cur_gimple);
		expanded_location *xloc;
		xloc = get_gimple_loc(cur_fn->buf->payload, &cur_gimple->location);
		SI_LOG("%s in %s, loc: %s %d %d\n",
				tree_code_name[tc], gimple_code_name[gc],
				xloc->file, xloc->line, xloc->column);
		/* TODO, ARRAY_REF */
		BUG();
	}
	}

	BUG_ON(!tn);
	/* now we found the union/struct which contain the t1 field */
	struct var_node_list *tmp;
	struct var_node_list *target_vn = NULL;
	if (tn->type_code == UNION_TYPE) {
		list_for_each_entry(tmp, &tn->children, sibling) {
			/* if the field has the same name or type name */
			tree ft = (tree)tmp->var.node;
			if (ft == t1) {
				target_vn = tmp;
				break;
			}
			char name[NAME_MAX];
			char name1[NAME_MAX];
			memset(name, 0, NAME_MAX);
			memset(name1, 0, NAME_MAX);
			if (DECL_NAME(t1) && DECL_NAME(ft)) {
				get_node_name(DECL_NAME(t1), name);
				get_node_name(DECL_NAME(ft), name1);
				if (!strcmp(name, name1)) {
					target_vn = tmp;
					break;
				}
			} else if ((!DECL_NAME(t1)) ||
					(!DECL_NAME(ft))) {
				continue;
			} else {
				get_type_name(TREE_TYPE(t1), name);
				get_type_name(TREE_TYPE(ft), name1);
				if (!strcmp(name, name1)) {
					target_vn = tmp;
					break;
				}
			}
		}
	} else {
		list_for_each_entry(tmp, &tn->children, sibling) {
			unsigned long off0 = get_field_offset(t1);
			unsigned long off1 = get_field_offset(
							(tree)tmp->var.node);
			if (off0 == off1) {
				target_vn = tmp;
				break;
			}
		}
	}

	BUG_ON(!target_vn);
	return target_vn;
}

static void do_component_ref(struct tree_exp *op)
{
	struct var_node_list *target_vn = component_ref_get_vnl(op);
	struct use_at_list *newua_type = NULL, *newua_var = NULL;
	newua_type = use_at_list_find(&target_vn->var.type->used_at, cur_gimple,
					cur_gimple_op_idx);
	newua_var = use_at_list_find(&target_vn->var.used_at, cur_gimple,
					cur_gimple_op_idx);
	if (!newua_type) {
		newua_type = use_at_list_new();
		newua_type->func_id = cur_fn->node_id.id;
		newua_type->gimple_stmt = (void *)cur_gimple;
		newua_type->op_idx = cur_gimple_op_idx;
		list_add_tail(&newua_type->sibling, &target_vn->var.type->used_at);
	}
	if (!newua_var) {
		newua_var = use_at_list_new();
		newua_var->func_id = cur_fn->node_id.id;
		newua_var->gimple_stmt = (void *)cur_gimple;
		newua_var->op_idx = cur_gimple_op_idx;
		list_add_tail(&newua_var->sibling, &target_vn->var.used_at);
	}
	return;
}

static void do_bit_field_ref(struct tree_exp *op)
{
	tree t0 = op->operands[0];
	tree t1 = op->operands[1];
	tree t2 = op->operands[2];
	struct type_node *tn = NULL;
	switch (TREE_CODE(t0)) {
	case MEM_REF:
	{
		tn = do_mem_ref((struct tree_exp *)t0);
		break;
	}
	case VAR_DECL:
	{
		tn = find_type_node(TREE_TYPE(t0));
		break;
	}
	default:
	{
		enum gimple_code gc = gimple_code(cur_gimple);
		expanded_location *xloc;
		xloc = get_gimple_loc(cur_fn->buf->payload, &cur_gimple->location);
		SI_LOG("%s %s in %s, loc: %s %d %d\n",
				tree_code_name[TREE_CODE(t0)],
				tree_code_name[TREE_CODE(t0)], gimple_code_name[gc],
				xloc->file, xloc->line, xloc->column);
		BUG();
	}
	}

	BUG_ON(!tn);
	struct var_node_list *tmp;
	struct var_node_list *target_vn = NULL;
	list_for_each_entry(tmp, &tn->children, sibling) {
		unsigned long off0 = TREE_INT_CST_LOW(t2);
		unsigned long off1 = get_field_offset((tree)tmp->var.node);
		if (off0 == off1) {
			target_vn = tmp;
			break;
		}
	}
	if (!target_vn)
		BUG();
	struct use_at_list *newua_type = NULL, *newua_var = NULL;
	newua_type = use_at_list_find(&target_vn->var.type->used_at, cur_gimple,
					cur_gimple_op_idx);
	newua_var = use_at_list_find(&target_vn->var.used_at, cur_gimple,
					cur_gimple_op_idx);
	if (!newua_type) {
		newua_type = use_at_list_new();
		newua_type->func_id = cur_fn->node_id.id;
		newua_type->gimple_stmt = (void *)cur_gimple;
		newua_type->op_idx = cur_gimple_op_idx;
		list_add_tail(&newua_type->sibling, &target_vn->var.type->used_at);
	}
	if (!newua_var) {
		newua_var = use_at_list_new();
		newua_var->func_id = cur_fn->node_id.id;
		newua_var->gimple_stmt = (void *)cur_gimple;
		newua_var->op_idx = cur_gimple_op_idx;
		list_add_tail(&newua_var->sibling, &target_vn->var.used_at);
	}
	return;
}

static void do_gimple_op_xref(tree op)
{
	struct sinode *fsn = cur_fn;
	gimple_seq gs = cur_gimple;
	enum gimple_code gc = gimple_code(gs);
	enum tree_code tc = TREE_CODE(op);
	enum tree_code_class tcc = TREE_CODE_CLASS(tc);
	if (tcc == tcc_constant)
		return;

	switch (tc) {
	case LABEL_DECL:
	{
		/* nothing to do here */
		break;
	}
	case CASE_LABEL_EXPR:
	{
		break;
	}
	case COMPONENT_REF:
	{
		struct tree_exp *exp = (struct tree_exp *)op;
		do_component_ref(exp);
		break;
	}
	case BIT_FIELD_REF:
	{
		struct tree_exp *exp = (struct tree_exp *)op;
		do_bit_field_ref(exp);
		break;
	}
	case ADDR_EXPR:
	{
		struct tree_exp *exp = (struct tree_exp *)op;
		tree op0 = exp->operands[0];
		if ((TREE_CODE(op0) == VAR_DECL) ||
			(TREE_CODE(op0) == PARM_DECL) ||
			(TREE_CODE(op0) == COMPONENT_REF) ||
			(TREE_CODE(op0) == ARRAY_REF))
			do_gimple_op_xref(op0);
		else if ((TREE_CODE(op0) == FUNCTION_DECL) ||
				(TREE_CODE(op0) == STRING_CST))
			return;
		else {
			expanded_location *xloc;
			xloc = get_gimple_loc(fsn->buf->payload, &gs->location);
			SI_LOG("%s %s in %s, loc: %s %d %d\n",
					tree_code_name[TREE_CODE(op0)],
					tree_code_name[tc], gimple_code_name[gc],
					xloc->file, xloc->line, xloc->column);
			BUG();
		}
		break;
	}
	case MEM_REF:
	{
		/* we have done PARM_DECL/VAR_DECL in do_tree */
		struct tree_exp *exp = (struct tree_exp *)op;
		tree t0 = exp->operands[0];
		tree t1 = exp->operands[1];
		BUG_ON(((struct tree_int_cst *)t1)->val[0]);
		BUG_ON((TREE_CODE(t0) != PARM_DECL) && (TREE_CODE(t0) != VAR_DECL));

		break;
	}
	case ARRAY_REF:
	{
		struct tree_exp *exp = (struct tree_exp *)op;
		tree op0 = exp->operands[0];
		tree op1 = exp->operands[1];
		tree op2 = exp->operands[2];
		tree op3 = exp->operands[3];

		tree type = TREE_TYPE(op0);
		BUG_ON(TREE_CODE(type) != ARRAY_TYPE);
		struct type_node *tn = find_type_node(TREE_TYPE(type));
		struct use_at_list *ua = NULL;
		if (!use_at_list_find(&tn->used_at, cur_gimple,
					cur_gimple_op_idx)) {
			ua = use_at_list_new();
			ua->func_id = cur_fn->node_id.id;
			ua->gimple_stmt = (void *)cur_gimple;
			ua->op_idx = cur_gimple_op_idx;
			list_add_tail(&ua->sibling, &tn->used_at);
		}
		break;
	}
	case VAR_DECL:
	case PARM_DECL:
	{
		break;
	}
	case CONSTRUCTOR:
	{
		struct tree_constructor *exp = (struct tree_constructor *)op;
		if (exp->elts)
			BUG();
		break;
	}
	case TREE_LIST:
	{
		break;
	}
	default:
	{
		expanded_location *xloc;
		xloc = get_gimple_loc(fsn->buf->payload, &gs->location);
		SI_LOG("%s in %s, loc: %s %d %d\n",
				tree_code_name[tc], gimple_code_name[gc],
				xloc->file, xloc->line, xloc->column);
		BUG();
	}
	}
}

static void get_var_func_marked(struct sinode *func_sn)
{
	struct func_node *fn = (struct func_node *)func_sn->data;

	cur_func = (tree)(long)func_sn->obj->real_addr;
	cur_fn = func_sn;
	cur_func_node = fn;

	struct code_path *next;
	utils__get_func_code_pathes_start(fn->codes);
	while ((next = utils__get_func_next_code_path())) {
		struct code_sentence *cs;
		list_for_each_entry(cs, &next->sentences, sibling) {
			gimple_seq gs = (gimple_seq)cs->head;
			cur_gimple = gs;
			enum gimple_code gc = gimple_code(gs);
			BUG_ON(gc == GIMPLE_LABEL);
			tree *ops = gimple_ops(gs);
			for (int i = 0; i < gimple_num_ops(gs); i++) {
				if ((gc == GIMPLE_CALL) && (i == 1))
					continue;
				cur_gimple_op_idx = i;
				if (ops[i]) {
					xrefs_obj_idx = 0;
					for (size_t i = 0; i < obj_cnt; i++) {
						xrefs_obj_checked[i] = NULL;
					}
					do_tree(ops[i]);
					do_gimple_op_xref(ops[i]);
				}
			}
		}
		if (next->cond_head) {
			gimple_seq gs = (gimple_seq)next->cond_head;
			cur_gimple = gs;
			enum gimple_code gc = gimple_code(gs);
			BUG_ON(gc == GIMPLE_LABEL);
			tree *ops = gimple_ops(gs);
			for (int i = 0; i < gimple_num_ops(gs); i++) {
				if ((gc == GIMPLE_CALL) && (i == 1))
					continue;
				cur_gimple_op_idx = i;
				if (ops[i]) {
					xrefs_obj_idx = 0;
					for (size_t i = 0; i < obj_cnt; i++) {
						xrefs_obj_checked[i] = NULL;
					}
					do_tree(ops[i]);
					do_gimple_op_xref(ops[i]);
				}
			}
		}
	}
}

static void add_caller(struct sinode *callee, struct sinode *caller)
{
	struct func_node *callee_func_node = (struct func_node *)callee->data;
	BUG_ON(!callee_func_node);
	if (call_func_list_find(&callee_func_node->callers,
				caller->node_id.id.id1, 0))
		return;
	struct call_func_list *_newc;
	_newc = call_func_list_new();
	_newc->value = caller->node_id.id.id1;
	_newc->value_flag = 0;
	/* XXX, no need to add gimple here */

	list_add_tail(&_newc->sibling, &callee_func_node->callers);
}

/*
 * just get the obviously one function it calls.
 * The PARM_DECL/VAR_DECL are traced later
 */
static void get_direct_callee_caller(struct sinode *func_sn)
{
	tree node = (tree)(long)func_sn->obj->real_addr;
	struct func_node *fn = (struct func_node *)func_sn->data;
	struct code_path *next_cp;
	char name[NAME_MAX];

	utils__get_func_code_pathes_start(fn->codes);
	while ((next_cp = utils__get_func_next_code_path())) {
		struct code_sentence *cs;
		list_for_each_entry(cs, &next_cp->sentences, sibling) {
			gimple_seq next_gs = (gimple_seq)cs->head;
			enum gimple_code gc = gimple_code(next_gs);
			tree *ops = gimple_ops(next_gs);
			struct gcall *g = NULL;
			tree call_op = NULL;
			if (gc != GIMPLE_CALL)
				continue;

			g = (struct gcall *)next_gs;
			if (next_gs->subcode & GF_CALL_INTERNAL) {
				struct call_func_list *_newc;
				_newc = call_func_list_find(&fn->callees,
							(long)g->u.internal_fn, 0);
				if (!_newc) {
					_newc = call_func_list_new();
					_newc->value =
						(unsigned long)g->u.internal_fn;
					_newc->value_flag = 0;
					_newc->body_missing = 1;
					list_add_tail(&_newc->sibling, &fn->callees);
				}
				call_func_gimple_stmt_list_add(&_newc->gimple_stmts,
								(void *)next_gs);
				cs->handled = 1;

				continue;
			}

			call_op = ops[1];
			BUG_ON(!call_op);
			switch (TREE_CODE(call_op)) {
			case ADDR_EXPR:
			{
				struct tree_exp *cfn = NULL;
				struct sinode *call_fn_sn = NULL;
				cfn = (struct tree_exp *)call_op;
				if (!cfn->operands[0])
					break;
				BUG_ON(TREE_CODE(cfn->operands[0]) != FUNCTION_DECL);
				get_func_sinode(cfn->operands[0], &call_fn_sn, 1);
				long value;
				long val_flag;
				if (!call_fn_sn) {
					value = (long)cfn->operands[0];
					val_flag = 1;
				} else {
					value = call_fn_sn->node_id.id.id1;
					val_flag = 0;
				}

				struct call_func_list *_newc;
				_newc = call_func_list_find(&fn->callees,
								value, val_flag);
				if (!_newc) {
					_newc = call_func_list_new();
					_newc->value = value;
					_newc->value_flag = val_flag;
					if (val_flag)
						_newc->body_missing = 1;
					list_add_tail(&_newc->sibling, &fn->callees);
				}
				call_func_gimple_stmt_list_add(&_newc->gimple_stmts,
								(void *)next_gs);
				cs->handled = 1;

				/* FIXME, what if call_fn_sn has no data? */
				if (!val_flag)
					add_caller(call_fn_sn, func_sn);

				break;
			}
			case PARM_DECL:
			case VAR_DECL:
			{
				break;
			}
			default:
				BUG();
			}
		}
	}
}

static void get_xrefs(struct sinode *func_sn)
{
	resfile__resfile_load(func_sn->buf);
	get_var_func_marked(func_sn);
	get_direct_callee_caller(func_sn);
	return;
}

/* NULL: the target var is extern, not defined in this source files */
static struct var_node *get_target_var_node(struct sinode *fsn, tree node)
{
	BUG_ON(TREE_CODE(node) != VAR_DECL);
	struct func_node *fn = (struct func_node *)fsn->data;

	struct var_node_list *tmp;
	list_for_each_entry(tmp, &fn->local_vars, sibling) {
		if (tmp->var.node == (void *)node)
			return &tmp->var;
	}

	struct id_list *t;
	list_for_each_entry(t, &fn->global_vars, sibling) {
		struct sinode *vsn0;
		struct var_node *vn;
		if (t->value_flag)
			continue;
		union siid *tid = (union siid *)&t->value;
		vsn0 = sinode__sinode_search(siid_get_type(tid), SEARCH_BY_ID, tid);
		if (unlikely(!vsn0))
			BUG();

		vn = (struct var_node *)vsn0->data;
		if (vn->node == (void *)node)
			return vn;
	}

	return NULL;
}

static void handle_marked_func(struct sinode *n)
{
	struct func_node *fn = (struct func_node *)n->data;
	struct use_at_list *tmp_ua;
	list_for_each_entry(tmp_ua, &fn->used_at, sibling) {
		struct sinode *fsn;
		fsn = sinode__sinode_search(siid_get_type(&tmp_ua->func_id),
						SEARCH_BY_ID, &tmp_ua->func_id);
		BUG_ON(!fsn);
		resfile__resfile_load(fsn->buf);

		gimple_seq gs = (gimple_seq)tmp_ua->gimple_stmt;
		enum gimple_code gc = gimple_code(gs);
		if (gc == GIMPLE_ASSIGN) {
			BUG_ON(tmp_ua->op_idx == 0);
			BUG_ON(gimple_num_ops(gs) != 2);
			tree *ops = gimple_ops(gs);
			tree lhs = ops[0];
			enum tree_code tc = TREE_CODE(lhs);
			if (tc == VAR_DECL) {
				struct var_node *vn;
				vn = get_target_var_node(fsn, lhs);
				if (!vn)
					continue;
				struct possible_value_list *pv;
				if (possible_value_list_find(
							&vn->possible_values,
							VALUE_IS_FUNC,
							n->node_id.id.id1)) {
					pv = possible_value_list_new();
					pv->value_flag = VALUE_IS_FUNC;
					pv->value = n->node_id.id.id1;
					list_add_tail(&pv->sibling,
							&vn->possible_values);
				}
			} else if (tc == COMPONENT_REF) {
				struct tree_exp *exp = (struct tree_exp *)lhs;
				struct var_node_list *vnl =
						component_ref_get_vnl(exp);
				struct possible_value_list *pv;
				if (possible_value_list_find(
							&vnl->var.possible_values,
							VALUE_IS_FUNC,
							n->node_id.id.id1)) {
					pv = possible_value_list_new();
					pv->value_flag = VALUE_IS_FUNC;
					pv->value = n->node_id.id.id1;
					list_add_tail(&pv->sibling,
							&vnl->var.possible_values);
				}
			} else {
				SI_LOG("miss %s\n", tree_code_name[tc]);
				BUG();
			}
		} else if (gc == GIMPLE_CALL) {
			BUG_ON(tmp_ua->op_idx <= 1);
		} else {
			SI_LOG("miss %s\n", gimple_code_name[gc]);
			BUG();
		}
	}
}

static void add_callee(struct sinode *caller_fsn, struct code_sentence *cs,
			struct sinode *called_fsn)
{
	struct func_node *caller_fn = (struct func_node *)caller_fsn->data;
	struct func_node *called_fn = (struct func_node *)called_fsn->data;

	struct call_func_list *_newc;
	_newc = call_func_list_find(&caller_fn->callees,
					called_fsn->node_id.id.id1, 0);
	if (!_newc) {
		_newc = call_func_list_new();
		_newc->value = called_fsn->node_id.id.id1;
		_newc->value_flag = 0;
		if (!called_fn)
			_newc->body_missing = 1;
		list_add_tail(&_newc->sibling, &caller_fn->callees);
	}
	call_func_gimple_stmt_list_add(&_newc->gimple_stmts, cs->head);
	if (called_fn)
		add_caller(called_fsn, caller_fsn);
	cs->handled = 1;
}

static void add_possible_callee(struct sinode *caller_fsn, struct code_sentence *cs,
				struct list_head *head)
{
	struct possible_value_list *tmp_pv;
	list_for_each_entry(tmp_pv, head, sibling) {
		if (tmp_pv->value_flag != VALUE_IS_FUNC)
			continue;
		union siid *id = (union siid *)&tmp_pv->value;
		struct sinode *called_fsn = sinode__sinode_search(TYPE_FUNC_GLOBAL,
								SEARCH_BY_ID,
								id);
		BUG_ON(!called_fsn);
		add_callee(caller_fsn, cs, called_fsn);
	}
}

static void do_parm_call(struct sinode *fsn, tree call_node,
				struct code_sentence *cs)
{
	SI_LOG("parm call happened in %s\n", fsn->name);
	return;
}

static void do_var_call(struct sinode *fsn, tree call_node, struct code_sentence *cs)
{
	struct var_node *vn = get_target_var_node(fsn, call_node);

	add_possible_callee(fsn, cs, &vn->possible_values);

	struct use_at_list *tmp_ua;
	list_for_each_entry(tmp_ua, &vn->used_at, sibling) {
		gimple_seq gs = (gimple_seq)tmp_ua->gimple_stmt;
		enum gimple_code gc = gimple_code(gs);
		if ((gc != GIMPLE_ASSIGN) || (tmp_ua->op_idx))
			continue;
		if (gimple_num_ops(gs) != 2)
			continue;
		tree *ops = gimple_ops(gs);
		tree rhs = ops[1];
		enum tree_code tc = TREE_CODE(rhs);
		if (tc == COMPONENT_REF) {
			struct tree_exp *exp = (struct tree_exp *)rhs;
			struct var_node_list *vnl = component_ref_get_vnl(exp);
			add_possible_callee(fsn, cs, &vnl->var.possible_values);
		} else if (tc == PARM_DECL) {
			do_parm_call(fsn, rhs, cs);
		} else if (tc == VAR_DECL) {
			SI_LOG("miss %s in %s\n",
					tree_code_name[tc],
					fsn->name);
		} else {
			SI_LOG("miss %s in %s\n",
					tree_code_name[tc],
					fsn->name);
			BUG();
		}
	}
}

static void get_indirect_cfg(struct sinode *func_sn)
{
	resfile__resfile_load(func_sn->buf);
	tree node = (tree)(long)func_sn->obj->real_addr;
	struct func_node *fn = (struct func_node *)func_sn->data;
	struct code_path *next_cp;
	char name[NAME_MAX];

	utils__get_func_code_pathes_start(fn->codes);
	while ((next_cp = utils__get_func_next_code_path())) {
		struct code_sentence *cs;
		list_for_each_entry(cs, &next_cp->sentences, sibling) {
			gimple_seq next = (gimple_seq)cs->head;
			enum gimple_code gcode = gimple_code(next);
			tree *ops = gimple_ops(next);
			struct gcall *g = NULL;
			tree call_op = NULL;
			if (gcode != GIMPLE_CALL)
				continue;

			g = (struct gcall *)next;
			if (next->subcode & GF_CALL_INTERNAL) {
				continue;
			}

			call_op = ops[1];
			BUG_ON(!call_op);
			switch (TREE_CODE(call_op)) {
			case PARM_DECL:
			{
				do_parm_call(func_sn, call_op, cs);
				break;
			}
			case VAR_DECL:
			{
				do_var_call(func_sn, call_op, cs);
				break;
			}
			case ADDR_EXPR:
			{
				break;
			}
			default:
			{
				SI_LOG("miss %s\n",
						tree_code_name[TREE_CODE(call_op)]);
				BUG();
			}
			}
		}
	}

	return;
}
