/*
 * TODO
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

CLIB_PLUGIN_NAME(utils);
CLIB_PLUGIN_NEEDED1(getinfo);

CLIB_PLUGIN_INIT()
{
	return 0;
}

CLIB_PLUGIN_EXIT()
{
	return;
}

/*
 * ************************************************************************
 * get_func_code_pathes_start get_func_next_code_path
 * get code_pathes in a function
 * ************************************************************************
 */
static __thread struct code_path *code_path_labels[LABEL_MAX];
static __thread size_t code_path_label_idx = 0;
static __thread size_t code_path_idx = 0;
static int code_path_label_checked(struct code_path *codes)
{
	for (int i = 0; i < code_path_label_idx; i++) {
		if (code_path_labels[i] == codes)
			return 1;
	}
	return 0;
}

static void code_path_label_insert(struct code_path *codes)
{
	code_path_labels[code_path_label_idx++] = codes;
}

static void get_code_path_labels(struct code_path *codes)
{
	if (!codes)
		return;
	if (!code_path_label_checked(codes))
		code_path_label_insert(codes);

	for (int i = 0; i < codes->branches; i++) {
		if (!code_path_label_checked(codes->next[i]))
			get_code_path_labels(codes->next[i]);
	}
}

void get_func_code_pathes_start(struct code_path *codes)
{
	for (int i = 0; i < code_path_label_idx; i++) {
		code_path_labels[i] = NULL;
	}
	code_path_label_idx = 0;
	code_path_idx = 0;

	get_code_path_labels(codes);
}

struct code_path *get_func_next_code_path(void)
{
	if (code_path_idx == code_path_label_idx)
		return NULL;
	struct code_path *ret = code_path_labels[code_path_idx];
	code_path_idx++;
	return ret;
}

/*
 * ************************************************************************
 * trace_var
 * ************************************************************************
 */
/* -1: error, 0: found, 1: var is from user */
int trace_var(struct sinode *fsn, void *var_parm,
		struct sinode **target_fsn, void **target_node)
{
#if 0
	int ret = 0;
	tree node = (tree)var_parm;
	struct func_node *fn = (struct func_node *)fsn->data;

	enum tree_code tc = TREE_CODE(node);
	switch (tc) {
	case VAR_DECL:
	{
		/* TODO */
		break;
	}
	case PARM_DECL:
	{
		int parm_idx = 0;
		int found = 0;
		struct var_node_list *tmp;
		list_for_each_entry(tmp, &fn->args, sibling) {
			parm_idx++;
			if (tmp->var.node == node) {
				found = 1;
				break;
			}
		}
		BUG_ON(!found);

		struct call_func_list *caller;
		list_for_each_entry(caller, &fn->callers, sibling) {
			if (caller->call_id.id1 == fsn->node_id.id.id1)
				continue;

			BUG_ON(caller->value_flag);
			BUG_ON(caller->body_missing);
			BUG_ON(siid_get_type(&caller->call_id) == TYPE_FILE);

			struct sinode *caller_sn;
			union siid *tid = (union siid *)&caller->value;
			caller_sn = sinode__sinode_search(siid_get_type(tid),
								SEARCH_BY_ID, tid);
			BUG_ON(!caller_sn);

			resfile__resfile_load(caller_sn->buf);
			struct func_node *caller_fn =
						(struct func_node *)caller_sn->data;
			struct call_func_list *caller_target_cfl;
			caller_target_cfl = call_func_list_find(&caller_fn->callees,
							fsn->node_id.id.id1);
			BUG_ON(!caller_target_cfl);

			struct call_func_gimple_stmt_list *call_gs_list;
			list_for_each_entry(call_gs_list,
						&caller_target_cfl->gimple_stmts,
						sibling) {
				gimple_seq gs = (gimple_seq)call_gs_list->gimple_stmt;
				tree *ops = gimple_ops(gs);
				BUG_ON((parm_idx+1) >= gimple_num_ops(gs));
				tree parm_op = ops[parm_idx+1];

				*target_fsn = caller_sn;
				*target_node = parm_op;
			}
		}
		break;
	}
	default:
	{
		fprintf(stderr, "miss %s\n", tree_code_name[tc]);
		BUG();
	}
	}
#endif
	return 0;
}

/*
 * ************************************************************************
 * generate functions' call level
 * if this is kernel, sys_* is level 0
 * if this is user program, main is level 0, thread routine is level 0
 * if this is user library, all global functions are level 0
 * ************************************************************************
 */

/*
 * ************************************************************************
 * generate pathes from `fsn_from` to `fsn_to`
 * gen_func_pathes: for functions, not code_pathes
 * gen_code_pathes: for code pathes
 * ************************************************************************
 */
static void *path_funcs[CALL_LEVEL_DEEP];
static size_t path_func_deep;
static void push_func_path(struct sinode *fsn)
{
	path_funcs[path_func_deep] = (void *)fsn;
	path_func_deep++;
}

static void pop_func_path(void)
{
	path_func_deep--;
}

static int check_func_path(struct sinode *fsn)
{
	for (int i = 0; i < path_func_deep; i++) {
		if (path_funcs[i] == (void *)fsn)
			return 1;
	}
	return 0;
}

static void create_func_pathes(struct list_head *head)
{
	struct path_list_head *new_head;
	new_head = path_list_head_new();
	for (int i = 0; i < path_func_deep; i++) {
		struct func_path_list *_new;
		_new = func_path_list_new();
		_new->fsn = (struct sinode *)path_funcs[i];
		list_add_tail(&_new->sibling, &new_head->path_head);
	}
	list_add_tail(&new_head->sibling, head);
}

void gen_func_pathes(struct sinode *fsn_from, struct sinode *fsn_to,
			struct list_head *head, int idx)
{
	int retval = 0;
	if (!idx) {
		path_func_deep = 0;
		INIT_LIST_HEAD(head);
	}

	push_func_path(fsn_from);
	if (fsn_from == fsn_to) {
		create_func_pathes(head);
		pop_func_path();
		return;
	}

	struct func_node *fn;
	fn = (struct func_node *)fsn_from->data;

	struct call_func_list *tmp;
	list_for_each_entry(tmp, &fn->callees, sibling) {
		if (tmp->value_flag)
			continue;
		if (tmp->body_missing)
			continue;
		union siid *tid = (union siid *)&tmp->value;
		struct sinode *next_fsn;
		next_fsn = sinode__sinode_search(siid_get_type(tid),
							SEARCH_BY_ID, tid);
		BUG_ON(!next_fsn);

		if (check_func_path(next_fsn))
			continue;

		gen_func_pathes(next_fsn, fsn_to, head, idx+1);
	}

	pop_func_path();
	return;
}

void drop_func_pathes(struct list_head *head)
{
	struct path_list_head *tmp0, *next0;
	list_for_each_entry_safe(tmp0, next0, head, sibling) {
		struct func_path_list *tmp1, *next1;
		list_for_each_entry_safe(tmp1, next1, &tmp0->path_head, sibling) {
			list_del(&tmp1->sibling);
			free(tmp1);
		}
		list_del(&tmp0->sibling);
		free(tmp0);
	}
}

/*
 * ************************************************************************
 * generate code pathes for function fsn
 * ************************************************************************
 */
static void *func_code_pathes[LABEL_MAX];
static size_t func_code_path_deep;
static void push_func_codepath(struct code_path *cp)
{
	func_code_pathes[func_code_path_deep] = (void *)cp;
	func_code_path_deep++;
}

static void pop_func_codepath(void)
{
	func_code_path_deep--;
}

static int check_func_code_path(struct code_path *cp)
{
	for (int i = 0; i < func_code_path_deep; i++) {
		if (func_code_pathes[i] == (void *)cp)
			return 1;
	}
	return 0;
}

static void create_func_code_pathes(struct list_head *head)
{
	struct path_list_head *new_head;
	new_head = path_list_head_new();
	for (int i = 0; i < func_code_path_deep; i++) {
		struct code_path_list *_new;
		_new = code_path_list_new();
		_new->cp = (struct code_path *)func_code_pathes[i];
		list_add_tail(&_new->sibling, &new_head->path_head);
	}
	list_add_tail(&new_head->sibling, head);
}

static int code_path_last(struct code_path *cp)
{
	for (int i = 0; i < cp->branches; i++) {
		if (cp->next[i])
			return 0;
	}
	return 1;
}

static void gen_func_codepathes(struct code_path *cp, struct list_head *head,
				int idx, int flag)
{
	int err = 0;
	if (!idx) {
		func_code_path_deep = 0;
		INIT_LIST_HEAD(head);
	}

	push_func_codepath(cp);
	if (code_path_last(cp)) {
		create_func_code_pathes(head);
		pop_func_codepath();
		return;
	}

	for (int i = 0; i < cp->branches; i++) {
		if (!cp->next[i])
			continue;

		int checked = check_func_code_path(cp->next[i]);
		if (checked && flag)
			continue;
		else if (checked)
			gen_func_codepathes(cp->next[i], head, idx+1, 1);
		else
			gen_func_codepathes(cp->next[i], head, idx+1, 0);

	}

	pop_func_codepath();
	return;
}

static void drop_func_codepathes(struct list_head *head)
{
	struct path_list_head *tmp0, *next0;
	list_for_each_entry_safe(tmp0, next0, head, sibling) {
		struct code_path_list *tmp1, *next1;
		list_for_each_entry_safe(tmp1, next1, &tmp0->path_head, sibling) {
			list_del(&tmp1->sibling);
			free(tmp1);
		}
		list_del(&tmp0->sibling);
		free(tmp0);
	}
}

/*
 * ************************************************************************
 * generate code pathes in functions,
 * if line0 is 0, from the beginning of fsn_from
 * if line1 is 0, till the beginning of fsn_to
 * if line1 is -1, till the end of fsn_to
 * ************************************************************************
 */
void gen_code_pathes(struct sinode *fsn_from, int line0,
			struct sinode *fsn_to, int line1,
			struct list_head *head)
{
	if ((fsn_from == fsn_to) && (line0 == 0) && (line1 == -1)) {
		struct func_node *fn = (struct func_node *)fsn_from->data;
		gen_func_codepathes(fn->codes, head, 0, 0);
		return;
	}

	/* TODO */
}

void drop_code_pathes(struct list_head *head)
{
	drop_func_codepathes(head);
}
