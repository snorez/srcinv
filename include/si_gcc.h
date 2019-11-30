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
#include <cpplib.h>

#ifndef HAS_TREE_CODE_NAME
#define	HAS_TREE_CODE_NAME
#define DEFTREECODE(SYM, NAME, TYPE, LEN) NAME,
#define END_OF_BASE_TREE_CODES "@dummy",
static const char *const tree_code_name[] = {
#include "all-tree.def"
};
#undef DEFTREECODE
#undef END_OF_BASE_TREE_CODES
#endif

#ifndef TYPE_MODE_RAW
#define	TYPE_MODE_RAW(NODE) (TYPE_CHECK(NODE)->type_common.mode)
#endif

#else

#include "gcc_treecodes.h"

#endif

#include "si_core.h"

DECL_BEGIN

#ifdef __cplusplus

#if __GNUC__ == 8
struct GTY(()) sorted_fields_type {
	int len;
	tree GTY((length("%h.len"))) elts[1];
};
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
			if (unlikely(!TREE_STATIC(node)))
				BUG();
			return VAR_IS_STATIC;
		}
	} else {
		BUG_ON(!DECL_NAME(node));
		return VAR_IS_EXTERN;
	}
}

static inline int check_file_func(tree node)
{
	BUG_ON(TREE_CODE(node) != FUNCTION_DECL);
	BUG_ON(!DECL_NAME(node));
	if ((!DECL_EXTERNAL(node)) || DECL_SAVED_TREE(node) ||
		(DECL_STRUCT_FUNCTION(node) &&
		 (DECL_STRUCT_FUNCTION(node)->gimple_body))) {
		/* we collect data before cfg */
		BUG_ON(DECL_SAVED_TREE(node));
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
static inline expanded_location *get_location(int flag, char *payload, tree node)
{
	if ((flag == GET_LOC_VAR) || (flag == GET_LOC_FUNC)) {
		return (expanded_location *)(payload + DECL_SOURCE_LOCATION(node));
	}

	/* for TYPE_TYPE */
	if (TYPE_NAME(node) && (TREE_CODE(TYPE_NAME(node)) == TYPE_DECL)) {
		return (expanded_location *)(payload +
					DECL_SOURCE_LOCATION(TYPE_NAME(node)));
	}

	/* try best to get a location of the type */
	if (((TREE_CODE(node) == RECORD_TYPE) || (TREE_CODE(node) == UNION_TYPE)) &&
		TYPE_FIELDS(node)) {
		return (expanded_location *)(payload +
				DECL_SOURCE_LOCATION(TYPE_FIELDS(node)));
	}

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

#if 0
static inline void gen_type_name(void *addr, char *ret, expanded_location *xloc)
{
	if (xloc && xloc->file) {
		snprintf(ret, NAME_MAX, "_%08lx%d%d", (long)xloc->file,
					xloc->line, xloc->column);
		return;
	} else {
		snprintf(ret, NAME_MAX, "_%08lx", (long)addr);
		return;
	}
}
#endif

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
	BUG_ON(!b);
	analysis__resfile_load(b);

	memset(name, 0, NAME_MAX);
	get_type_name((void *)node, name);
	xloc = get_location(GET_LOC_TYPE, b->payload, node);

	int flag;
	flag = check_file_type(node);
	if (flag == TYPE_CANONICALED) {
		node = TYPE_CANONICAL(node);

		b = find_target_sibuf((void *)node);
		BUG_ON(!b);
		analysis__resfile_load(b);

		memset(name, 0, NAME_MAX);
		get_type_name((void *)node, name);
		xloc = get_location(GET_LOC_TYPE, b->payload, node);
	} else if (flag == TYPE_UNDEFINED) {
#if 0
		BUG_ON(!name[0]);
		sn = analysis__sinode_search(TYPE_TYPE, SEARCH_BY_TYPE_NAME, name);
		*sn_ret = sn;
#endif
		return;
	}

	if (name[0] && xloc && xloc->file)
		type = TYPE_TYPE;
	else
		type = TYPE_NONE;

	if (type == TYPE_TYPE) {
		long args[4];
		struct sinode *loc_file;
		loc_file = analysis__sinode_search(TYPE_FILE, SEARCH_BY_SPEC,
							(void *)xloc->file);
		BUG_ON(!loc_file);
		args[0] = (long)loc_file;
		args[1] = xloc->line;
		args[2] = xloc->column;
		args[3] = (long)name;
		sn = analysis__sinode_search(type, SEARCH_BY_SPEC, (void *)args);
	} else {
		tn = analysis__sibuf_type_node_search(b, TREE_CODE(node), node);
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
		BUG();
	}

	*sn_ret = sn;
}

static inline void show_gimple(gimple_seq gs)
{
	tree *ops = gimple_ops(gs);
	enum gimple_code gc = gimple_code(gs);
	si_log(NULL, "Statement %s\n", gimple_code_name[gc]);
	for (unsigned int i = 0; i < gimple_num_ops(gs); i++) {
		if (ops[i]) {
			enum tree_code tc = TREE_CODE(ops[i]);
			si_log(NULL, "\tOp: %s %p\n", tree_code_name[tc], ops[i]);
		} else {
			si_log(NULL, "\tOp: null\n");
		}
	}
}

static inline void show_func_gimples(struct sinode *fsn)
{
	analysis__resfile_load(fsn->buf);
	tree node = (tree)(long)fsn->obj->real_addr;
	gimple_seq body = DECL_STRUCT_FUNCTION(node)->gimple_body;
	gimple_seq next;

	next = body;
	si_log(NULL, "gimples for function %s\n", fsn->name);

	while (next) {
		show_gimple(next);
		next = next->next;
	}
}

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
			struct attr_value_list *newavl;
			newavl = attr_value_list_new();
			newavl->node = (void *)valnode;

			list_add_tail(&newavl->sibling, &newal->values);

			tl2 = TREE_CHAIN(tl2);
		}

		list_add_tail(&newal->sibling, head);

		tl = TREE_CHAIN(tl);
	}
}

#endif

DECL_END

#endif /* end of include guard: SI_GCC_H_LETQ5PZR */
