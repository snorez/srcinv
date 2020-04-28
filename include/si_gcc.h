/*
 * TODO
 * Copyright (C) 2019  zerons
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
#ifndef SI_GCC_H_LETQ5PZR
#define SI_GCC_H_LETQ5PZR

#ifndef _FILE_OFFSET_BITS
#define	_FILE_OFFSET_BITS 64
#endif

#ifndef xmalloc
#define	xmalloc malloc
#endif

/* for gcc */
#ifdef __cplusplus

/* for g++ compilation */
#include <gcc-plugin.h>
//#include <plugin-version.h>
#include <c-tree.h>
#include <context.h>
#include <function.h>
#include <internal-fn.h>
#include <is-a.h>
#include <predict.h>
#include <basic-block.h>
#include <tree.h>
#include <tree-ssa-alias.h>
#include <gimple-expr.h>
#include <gimple.h>
#include <gimple-ssa.h>
#include <tree-pretty-print.h>
#include <tree-pass.h>
#include <tree-ssa-operands.h>
#include <tree-phinodes.h>
#include <gimple-pretty-print.h>
#include <gimple-iterator.h>
#include <gimple-walk.h>
#include <tree-core.h>
#include <dumpfile.h>
#include <print-tree.h>
#include <rtl.h>
#include <cgraph.h>
#include <cfg.h>
#include <cfgloop.h>
#include <except.h>
#include <real.h>
#include <function.h>
#include <input.h>
#include <typed-splay-tree.h>
#include <tree-pass.h>
#include <tree-phinodes.h>
#include <tree-cfg.h>
#include <value-prof.h>
#include <stringpool.h>
#include <tree-vrp.h>
#include <tree-ssanames.h>
#include <cpplib.h>

#include "g++_treecodes.h"

#ifndef TYPE_MODE_RAW
#define	TYPE_MODE_RAW(NODE) (TYPE_CHECK(NODE)->type_common.mode)
#endif

#else /* __cplusplus */

#include "gcc_treecodes.h"

#endif

#include "si_core.h"

#ifdef __cplusplus

DECL_BEGIN

#if __GNUC__ >= 8
struct GTY(()) sorted_fields_type {
	int len;
	tree GTY((length("%h.len"))) elts[1];
};
#endif

#if __GNUC__ < 8
void symtab_node::dump_table(FILE *f)
{
}
#endif

static inline int check_file_type(tree node)
{
	enum tree_code tc = TREE_CODE(node);

	if (!TYPE_CANONICAL(node)) {
		return TYPE_DEFINED;
	} else if (TYPE_CANONICAL(node) != node) {
		return TYPE_CANONICALED;
	} else if (((tc == RECORD_TYPE) || (tc == UNION_TYPE)) &&
		(TYPE_CANONICAL(node) == node) &&
		(!TYPE_VALUES(node))) {
		return TYPE_UNDEFINED;
	} else {
		return TYPE_DEFINED;
	}
}

static inline int check_file_var(tree node)
{
	BUG_ON(TREE_CODE(node) != VAR_DECL);
	if (!DECL_EXTERNAL(node)) {
		if (TREE_PUBLIC(node)) {
			BUG_ON(!DECL_NAME(node));
			return VAR_IS_GLOBAL;
		} else {
			if (unlikely(!TREE_STATIC(node))) {
				return VAR_IS_LOCAL;
			}
			return VAR_IS_STATIC;
		}
	} else {
		BUG_ON(!DECL_NAME(node));
		return VAR_IS_EXTERN;
	}
}

/*
 * Now, we collect data at IPA_ALL_PASSES_END, which is after cfg,
 * the gimple_body is cleared, and the cfg is set.
 * FIXME: is this right?
 */
static inline int check_file_func(tree node)
{
	BUG_ON(TREE_CODE(node) != FUNCTION_DECL);
	BUG_ON(!DECL_NAME(node));

	/*
	 * update: we ignore DECL_SAVED_TREE check here, just
	 * check functions with f->cfg
	 * in the meanwhile, we should handle the caller
	 */
	if ((!DECL_EXTERNAL(node)) ||
		(DECL_STRUCT_FUNCTION(node) &&
		 (DECL_STRUCT_FUNCTION(node)->cfg))) {
		if (TREE_PUBLIC(node)) {
			return FUNC_IS_GLOBAL;
		} else {
			BUG_ON(!TREE_STATIC(node));
			return FUNC_IS_STATIC;
		}
	} else {
		return FUNC_IS_EXTERN;
	}
}

/* get location of TYPE_TYPE TYPE_VAR TYPE_FUNC */
#define	GET_LOC_TYPE	0
#define	GET_LOC_VAR	1
#define	GET_LOC_FUNC	2
static inline expanded_location *get_location(int flag, char *payload,
						tree node)
{
	if ((flag == GET_LOC_VAR) || (flag == GET_LOC_FUNC)) {
		return (expanded_location *)(payload +
						DECL_SOURCE_LOCATION(node));
	}

	/* for TYPE_TYPE */
	if (TYPE_NAME(node) && (TREE_CODE(TYPE_NAME(node)) == TYPE_DECL)) {
		return (expanded_location *)(payload +
					DECL_SOURCE_LOCATION(TYPE_NAME(node)));
	}

	/* try best to get a location of the type */
	if (((TREE_CODE(node) == RECORD_TYPE) ||
		(TREE_CODE(node) == UNION_TYPE)) &&
		TYPE_FIELDS(node)) {
		return (expanded_location *)(payload +
				DECL_SOURCE_LOCATION(TYPE_FIELDS(node)));
	}

	return NULL;
}

/* EXPR_LOCATION */
static inline expanded_location *si_expr_location(tree expr)
{
	struct sibuf *b;
	b = find_target_sibuf((void *)expr);
	BUG_ON(!b);
	analysis__resfile_load(b);

	if (CAN_HAVE_LOCATION_P(expr))
		return (expanded_location *)(b->payload + (expr)->exp.locus);
	else
		return NULL;
}

static inline expanded_location *get_gimple_loc(char *payload, location_t *loc)
{
	return (expanded_location *)(payload + *loc);
}

/* get tree_identifier node name, the IDENTIFIER_NODE must not be NULL */
static inline void get_node_name(tree name, char *ret)
{
	BUG_ON(!name);
	BUG_ON(TREE_CODE(name) != IDENTIFIER_NODE);

	struct tree_identifier *node = (struct tree_identifier *)name;
	BUG_ON(!node->id.len);
	BUG_ON(node->id.len >= NAME_MAX);
	memcpy(ret, node->id.str, node->id.len);
}

static inline void get_function_name(void *addr, char *ret)
{
	tree node = (tree)addr;
	get_node_name(DECL_NAME(node), ret);
}

static inline void get_var_name(void *addr, char *ret)
{
	tree node = (tree)addr;
	if (DECL_NAME(node)) {
		get_node_name(DECL_NAME(node), ret);
	} else {
		snprintf(ret, NAME_MAX, "_%08lx", (long)addr);
		return;
	}
}

/* for type, if it doesn't have a name, use _%ld */
static inline void get_type_name(void *addr, char *ret)
{
	tree node = (tree)addr;
	if (!TYPE_NAME(node)) {
		return;
	}

	if (TREE_CODE(TYPE_NAME(node)) == IDENTIFIER_NODE) {
		get_node_name(TYPE_NAME(node), ret);
	} else {
		if (!DECL_NAME(TYPE_NAME(node))) {
			return;
		}
		get_node_name(DECL_NAME(TYPE_NAME(node)), ret);
	}
}

/*
 * FIXME: only generate name for UNION_TYPE RECORD_TYPE
 * we use tree_code, the number of the children, max to 10 names of the fields,
 * location to ident the type
 */
#define	FIELD_NAME_MAX	0x8
static inline void gen_type_name(char *name, size_t namelen, tree node,
				struct sinode *loc_file,
				expanded_location *xloc)
{
	enum tree_code tc = TREE_CODE(node);
	if ((tc != UNION_TYPE) && (tc != RECORD_TYPE))
		return;

	int copied_cnt = 0;
	int field_cnt = 0;
	char tmp_name[NAME_MAX];
	char *p = name;
	const char *sep = "_";

	tree fields = TYPE_FIELDS(node);
	while (fields) {
		field_cnt++;

		if ((copied_cnt < FIELD_NAME_MAX) && DECL_NAME(fields)) {
			memset(tmp_name, 0, NAME_MAX);
			get_node_name(DECL_NAME(fields), tmp_name);
			if (tmp_name[0]) {
				memcpy(p, sep, strlen(sep)+1);
				p += strlen(sep);
				memcpy(p, tmp_name, strlen(tmp_name)+1);
				p += strlen(tmp_name);
				copied_cnt++;
			}
		}
		fields = DECL_CHAIN(fields);
	}

	snprintf(tmp_name, NAME_MAX, "%s%d%s%d%s%ld%s%d%s%d",
			sep, (int)tc, sep, field_cnt,
			sep, (unsigned long)loc_file,
			sep, xloc->line, sep, xloc->column);
	memcpy(p, tmp_name, strlen(tmp_name)+1);

	return;
}

static inline void get_type_xnode(tree node, struct sinode **sn_ret,
					struct type_node **tn_ret)
{
	char name[NAME_MAX];
	expanded_location *xloc;
	enum sinode_type type;
	struct sinode *sn = NULL;
	struct type_node *tn = NULL;
	*sn_ret = NULL;
	*tn_ret = NULL;

	struct sibuf *b = find_target_sibuf((void *)node);
	if (unlikely(!b)) {
		si_log("target_sibuf not found, %p\n", node);
		return;
	}
	analysis__resfile_load(b);

	memset(name, 0, NAME_MAX);
	get_type_name((void *)node, name);
	xloc = get_location(GET_LOC_TYPE, b->payload, node);

	int flag;
	flag = check_file_type(node);
	if (flag == TYPE_CANONICALED) {
		tree tmp_node = TYPE_CANONICAL(node);

		b = find_target_sibuf((void *)tmp_node);
		if (unlikely(!b)) {
			si_log("target_sibuf not found, %p\n", tmp_node);
			/* FIXME: should we fall through or just return? */
			return;
		} else {
			node = tmp_node;
			analysis__resfile_load(b);

			memset(name, 0, NAME_MAX);
			get_type_name((void *)node, name);
			xloc = get_location(GET_LOC_TYPE, b->payload, node);
		}
	} else if (flag == TYPE_UNDEFINED) {
#if 0
		BUG_ON(!name[0]);
		sn = analysis__sinode_search(TYPE_TYPE, SEARCH_BY_TYPE_NAME,
						name);
		*sn_ret = sn;
#endif
		return;
	}

	struct sinode *loc_file = NULL;
	if (xloc && xloc->file) {
		loc_file = analysis__sinode_search(TYPE_FILE, SEARCH_BY_SPEC,
							(void *)xloc->file);
		BUG_ON(!loc_file);
	}

	if (loc_file && (!name[0]))
		gen_type_name(name, NAME_MAX, node, loc_file, xloc);

	if (name[0] && loc_file)
		type = TYPE_TYPE;
	else
		type = TYPE_NONE;

	if (type == TYPE_TYPE) {
		long args[4];
		args[0] = (long)loc_file;
		args[1] = xloc->line;
		args[2] = xloc->column;
		args[3] = (long)name;
		sn = analysis__sinode_search(type, SEARCH_BY_SPEC,
						(void *)args);
	} else {
		tn = analysis__sibuf_typenode_search(b, TREE_CODE(node), node);
	}

	if (sn) {
		*sn_ret = sn;
	} else if (tn) {
		*tn_ret = tn;
	}
}

static inline void get_func_sinode(tree node, struct sinode **sn_ret, int flag)
{
	char name[NAME_MAX];
	expanded_location *xloc;
	struct sinode *sn = NULL;

	*sn_ret = NULL;
	struct sibuf *b = find_target_sibuf((void *)node);
	BUG_ON(!b);
	analysis__resfile_load(b);

	memset(name, 0, NAME_MAX);
	get_function_name((void *)node, name);

	int func_flag = check_file_func(node);
	if (func_flag == FUNC_IS_EXTERN) {
		if (!flag)
			return;
		long args[3];
		args[0] = (long)b->rf;
		args[1] = (long)get_builtin_resfile();
		args[2] = (long)name;
		sn = analysis__sinode_search(TYPE_FUNC_GLOBAL, SEARCH_BY_SPEC,
						(void *)args);
	} else if (func_flag == FUNC_IS_GLOBAL) {
		long args[3];
		args[0] = (long)b->rf;
		args[1] = (long)get_builtin_resfile();
		args[2] = (long)name;
		sn = analysis__sinode_search(TYPE_FUNC_GLOBAL, SEARCH_BY_SPEC,
						(void *)args);
		BUG_ON(!sn);
	} else if (func_flag == FUNC_IS_STATIC) {
		xloc = get_location(GET_LOC_FUNC, b->payload, node);
		struct sinode *loc_file;
		loc_file = analysis__sinode_search(TYPE_FILE, SEARCH_BY_SPEC,
							(void *)xloc->file);
		BUG_ON(!loc_file);
		long args[4];
		args[0] = (long)loc_file;
		args[1] = xloc->line;
		args[2] = xloc->column;
		args[3] = (long)name;
		sn = analysis__sinode_search(TYPE_FUNC_STATIC, SEARCH_BY_SPEC,
						(void *)args);
	} else {
		BUG();
	}

	*sn_ret = sn;
}

static inline void get_var_sinode(tree node, struct sinode **sn_ret, int flag)
{
	char name[NAME_MAX];
	expanded_location *xloc;
	struct sinode *sn = NULL;

	*sn_ret = NULL;
	struct sibuf *b = find_target_sibuf((void *)node);
	BUG_ON(!b);
	analysis__resfile_load(b);

	memset(name, 0, NAME_MAX);
	get_var_name((void *)node, name);

	int var_flag = check_file_var(node);
	if (var_flag == VAR_IS_EXTERN) {
		if (!flag)
			return;
		long args[3];
		args[0] = (long)b->rf;
		args[1] = (long)get_builtin_resfile();
		args[2] = (long)name;
		sn = analysis__sinode_search(TYPE_VAR_GLOBAL, SEARCH_BY_SPEC,
						(void *)args);
	} else if (var_flag == VAR_IS_GLOBAL) {
		long args[3];
		args[0] = (long)b->rf;
		args[1] = (long)get_builtin_resfile();
		args[2] = (long)name;
		sn = analysis__sinode_search(TYPE_VAR_GLOBAL, SEARCH_BY_SPEC,
						(void *)args);
		BUG_ON(!sn);
	} else if (var_flag == VAR_IS_STATIC) {
		xloc = get_location(GET_LOC_VAR, b->payload, node);
		struct sinode *loc_file;
		loc_file = analysis__sinode_search(TYPE_FILE, SEARCH_BY_SPEC,
							(void *)xloc->file);
		BUG_ON(!loc_file);
		long args[4];
		args[0] = (long)loc_file;
		args[1] = xloc->line;
		args[2] = xloc->column;
		args[3] = (long)name;
		sn = analysis__sinode_search(TYPE_VAR_STATIC, SEARCH_BY_SPEC,
						(void *)args);
	} else {
		/* VAR_IS_LOCAL */
		return;
	}

	*sn_ret = sn;
	return;
}

static inline void show_gimple(gimple_seq gs)
{
	tree *ops = gimple_ops(gs);
	enum gimple_code gc = gimple_code(gs);
	si_log("Statement %s\n", gimple_code_name[gc]);
	for (unsigned int i = 0; i < gimple_num_ops(gs); i++) {
		if (ops[i]) {
			enum tree_code tc = TREE_CODE(ops[i]);
			si_log("\tOp: %s %p\n", tree_code_name[tc], ops[i]);
		} else {
			si_log("\tOp: null\n");
		}
	}
}

/* TODO: gcc/tree-cfg.c dump_function_to_file() */
static inline void get_attributes(struct list_head *head, tree attr_node)
{
	if (!attr_node)
		return;

	BUG_ON(TREE_CODE(attr_node) != TREE_LIST);
	char name[NAME_MAX];

	tree tl = attr_node;
	while (tl) {
		tree purpose = TREE_PURPOSE(tl);
		BUG_ON(TREE_CODE(purpose) != IDENTIFIER_NODE);
		memset(name, 0, NAME_MAX);
		get_node_name(purpose, name);

		struct attr_list *newal;
		newal = attr_list_new(name);

		tree tl2;
		tl2 = TREE_VALUE(tl);
		while (tl2) {
			tree valnode = TREE_VALUE(tl2);
			struct attrval_list *newavl;
			newavl = attrval_list_new();
			newavl->node = (void *)valnode;

			list_add_tail(&newavl->sibling, &newal->values);

			tl2 = TREE_CHAIN(tl2);
		}

		list_add_tail(&newal->sibling, head);

		tl = TREE_CHAIN(tl);
	}
}

/*
 * some functions in gcc header files
 */
static inline int si_is_global_var(tree node, expanded_location *xloc)
{
	int ret = 0;
	tree ctx = DECL_CONTEXT(node);
	if (is_global_var(node) &&
		((!ctx) || (TREE_CODE(ctx) == TRANSLATION_UNIT_DECL)))
		ret = 1;

	if (xloc)
		ret = (ret && (xloc->file));

	return !!ret;
}

static inline int si_gimple_has_body_p(tree fndecl)
{
	struct function *f;
	f = DECL_STRUCT_FUNCTION(fndecl);
	return (gimple_body(fndecl) ||
		(f && f->cfg && !(f->curr_properties & PROP_rtl)));
}

static inline int si_function_lowered(tree node)
{
	struct function *f;
	f = DECL_STRUCT_FUNCTION(node);
	return (f && (f->decl == node) &&
			(f->curr_properties & PROP_gimple_lcf));
}

static inline int si_gimple_in_ssa_p(struct function *f)
{
	return (f && f->gimple_df && f->gimple_df->in_ssa_p);
}

static inline tree si_gimple_vop(struct function *f)
{
	BUG_ON((!f) || (!f->gimple_df));
	return f->gimple_df->vop;
}

static inline int si_function_bb(tree fndecl)
{
	struct function *f;
	f = DECL_STRUCT_FUNCTION(fndecl);

	return (f && (f->decl == fndecl) && f->cfg &&
			f->cfg->x_basic_block_info);
}

static inline int si_n_basic_blocks_for_fn(struct function *f)
{
	BUG_ON((!f) || (!f->cfg));
	return f->cfg->x_n_basic_blocks;
}

static inline gphi_iterator si_gsi_start_phis(basic_block bb)
{
	gimple_seq *pseq = phi_nodes_ptr(bb);

	gphi_iterator i;

	i.ptr = gimple_seq_first(*pseq);
	i.seq = pseq;
	i.bb = i.ptr ? gimple_bb(i.ptr) : NULL;

	return i;
}

static inline void si_extract_true_false_edges_from_block(basic_block bb,
							  edge *true_edge,
							  edge *false_edge)
{
	edge e = EDGE_SUCC(bb, 0);

	if (e->flags & EDGE_TRUE_VALUE) {
		*true_edge = e;
		*false_edge = EDGE_SUCC(bb, 1);
	} else {
		*false_edge = e;
		*true_edge = EDGE_SUCC(bb, 1);
	}
}

static inline gimple *si_last_stmt(basic_block bb)
{
	gimple_stmt_iterator i;
	gimple *stmt = NULL;

	i = gsi_last_bb(bb);
	while ((!gsi_end_p(i)) &&
		is_gimple_debug((stmt = gsi_stmt(i)))) {
		gsi_prev(&i);
		stmt = NULL;
	}

	return stmt;
}

/*
 * check tree_code gimple_code is not out-of-bound
 */
static inline int check_tree_code(tree n)
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

static inline int check_gimple_code(gimple_seq gs)
{
	if (!gs)
		return 0;

	enum gimple_code gc = gimple_code(gs);
	size_t len = sizeof(gimple_code_name) / sizeof(gimple_code_name[0]);
	if (gc >= len)
		return 1;
	else
		return 0;
}

static inline unsigned long get_field_offset(tree field)
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

static inline int si_integer_zerop(tree expr)
{
	switch (TREE_CODE(expr)) {
	case INTEGER_CST:
	{
		return wi::to_wide(expr) == 0;
	}
	case COMPLEX_CST:
	{
		return (si_integer_zerop(TREE_REALPART(expr)) &&
			si_integer_zerop(TREE_IMAGPART(expr)));
	}
	case VECTOR_CST:
	{
		return ((VECTOR_CST_NPATTERNS(expr) == 1) &&
			(VECTOR_CST_DUPLICATE_P(expr)) &&
			(si_integer_zerop(VECTOR_CST_ENCODED_ELT(expr, 0))));
	}
	default:
	{
		return false;
	}
	}
}

static inline int si_tree_int_cst_equal(const_tree t1, const_tree t2)
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

static inline tree si_get_containing_scope(const_tree t)
{
	return (TYPE_P(t) ? TYPE_CONTEXT(t) : DECL_CONTEXT(t));
}

C_SYM histogram_value si_gimple_histogram_value(struct function *, gimple *);

C_SYM tree si_build1(enum tree_code code, tree type, tree node);
static inline tree si_build1_loc(location_t loc, enum tree_code code,
				tree type, tree arg1)
{
	tree t = si_build1(code, type, arg1);
	if (CAN_HAVE_LOCATION_P(t))
		SET_EXPR_LOCATION(t, loc);
	return t;
}

C_SYM tree si_size_int_kind(poly_int64 number, enum size_type_kind kind);

#define si_sizetype si_sizetype_tab[(int) stk_sizetype]
#define si_bitsizetype si_sizetype_tab[(int) stk_bitsizetype]
#define si_ssizetype si_sizetype_tab[(int) stk_ssizetype]
#define si_sbitsizetype si_sizetype_tab[(int) stk_sbitsizetype]
#define si_size_int(L) si_size_int_kind (L, stk_sizetype)
#define si_ssize_int(L) si_size_int_kind (L, stk_ssizetype)
#define si_bitsize_int(L) si_size_int_kind (L, stk_bitsizetype)
#define si_sbitsize_int(L) si_size_int_kind (L, stk_sbitsizetype)

#define si_error_mark_node		si_global_trees[TI_ERROR_MARK]
#define si_integer_zero_node		si_global_trees[TI_INTEGER_ZERO]

C_SYM tree si_build0(enum tree_code code, tree tt);
C_SYM tree si_ss_ph_in_expr(tree, tree);

#define	SI_SS_PH_IN_EXPR(EXP,OBJ) \
	((EXP) == 0 || TREE_CONSTANT(EXP) ? (EXP) : \
	 si_ss_ph_in_expr(EXP,OBJ))

C_SYM tree si_fold_build1_loc(location_t loc, enum tree_code code,
			tree type, tree op0);
C_SYM tree si_fold_build2_loc(location_t loc, enum tree_code code,
			tree type, tree op0, tree op1);
C_SYM tree si_build_fixed(tree type, FIXED_VALUE_TYPE f);
C_SYM tree si_build_poly_int_cst(tree type, const poly_wide_int_ref &values);
C_SYM REAL_VALUE_TYPE si_real_value_from_int_cst(const_tree type, const_tree i);
C_SYM tree si_build_real_from_int_cst(tree type, const_tree i);
C_SYM tree si_fold_ignored_result(tree t);
C_SYM tree si_build_vector_from_val(tree vectype, tree sc);
C_SYM tree si_decl_function_context(const_tree decl);
C_SYM bool si_decl_address_invariant_p(const_tree op);
C_SYM bool si_tree_invariant_p(tree t);
C_SYM tree si_skip_simple_arithmetic(tree expr);
C_SYM bool si_contains_placeholder_p(const_tree exp);
C_SYM tree si_save_expr(tree expr);
C_SYM tree si_fold_convert_loc(location_t loc, tree type, tree arg);
C_SYM tree si_component_ref_field_offset(tree exp);

DECL_END

template<typename T, typename A>
inline unsigned gcc_vec_safe_length(const vec<T, A, vl_embed> *v)
{
	if (!v)
		return 0;

	CLIB_DBG_FUNC_ENTER();
	struct vec_prefix *pfx;
	pfx = (struct vec_prefix *)&v->vecpfx;

	unsigned length = pfx->m_num;
	CLIB_DBG_FUNC_EXIT();
	return length;
}

template<typename T, typename A>
inline T *gcc_vec_safe_address(vec<T, A, vl_embed> *v)
{
	if (!v)
		return NULL;

	CLIB_DBG_FUNC_ENTER();
	T *addr = &v->vecdata[0];
	CLIB_DBG_FUNC_EXIT();
	return addr;
}

template<typename T, typename A>
inline int gcc_vec_safe_is_empty(const vec<T, A, vl_embed> *v)
{
	return gcc_vec_safe_length(v) == 0;
}

template<typename T, typename A>
inline void gcc_vec_length_address(vec<T, A, vl_embed> *v,
				unsigned *length, T **addr)
{
	CLIB_DBG_FUNC_ENTER();
	*length = gcc_vec_safe_length(v);
	*addr = gcc_vec_safe_address(v);
	CLIB_DBG_FUNC_EXIT();
}

static inline struct var_list *get_tn_field(struct type_node *tn, char *name)
{
	if (!tn)
		return NULL;

	struct var_list *tmp;
	list_for_each_entry(tmp, &tn->children, sibling) {
		if (!tmp->var.name)
			continue;

		if (!strcmp(tmp->var.name, name))
			return tmp;
	}

	return NULL;
}

#endif

#endif /* end of include guard: SI_GCC_H_LETQ5PZR */
