/*
 * TODO: this file is just for demo purpose. It turns out the gcc compiler
 *	ignore the address_space attribute.
 *
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
#include "si_gcc.h"
#include "./hacking.h"

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

CLIB_MODULE_NAME(kern_copyuser);
CLIB_MODULE_NEEDED0();
static void kern_copyuser_cb(void);
static struct hacking_module kern_copyuser;

CLIB_MODULE_INIT()
{
	kern_copyuser.flag = HACKING_FLAG_STATIC;
	kern_copyuser.callback = kern_copyuser_cb;
	kern_copyuser.name = this_module_name;
	register_hacking_module(&kern_copyuser);
	return 0;
}

CLIB_MODULE_EXIT()
{
	unregister_hacking_module(&kern_copyuser);
}

/*
 * the __user is __attribute__((noderef, address_space(1)))
 * return: 0 kernel, 1 user
 */
static int check_address_space(struct list_head *head)
{
	struct attr_list *tmp0;
	list_for_each_entry(tmp0, head, sibling) {
		if (strcmp(tmp0->attr_name, "address_space"))
			continue;
		struct attr_value_list *tmp1;
		list_for_each_entry(tmp1, &tmp0->values, sibling) {
			tree attr_val = (tree)tmp1->node;
			/* this tree should be a INTEGER_CST */
			enum tree_code tc = TREE_CODE(attr_val);
			if (tc != INTEGER_CST) {
				si_log2("missing tc: %s\n", tree_code_name[tc]);
				break;
			}

			long value = TREE_INT_CST_LOW(attr_val);
			if (value)
				return 1;
			else
				return 0;
		}
	}
	return 0;
}

static struct sinode *cur_sn;
static void do_check(struct call_func_list *cf, int flag)
{
	struct call_func_gimple_stmt_list *tmp;
	list_for_each_entry(tmp, &cf->gimple_stmts, sibling) {
		gimple_seq gs = (gimple_seq)tmp->gimple_stmt;
		tree *ops = gimple_ops(gs);
		long op_cnt = gimple_num_ops(gs);
		/* flag: 0 copy_to_user, 1 copy_from_user */
		if (op_cnt != 6) {
			si_log2("copy_to/from_user GIMPLE_CALL not 5 ops\n");
			continue;
		}
		/* TODO, what does ops[2] do? */
		tree arg_to = ops[3];
		tree arg_from = ops[4];
		struct list_head head0, head1;
		INIT_LIST_HEAD(&head0);
		INIT_LIST_HEAD(&head1);

		/* ADDR_EXPR PARM_DECL VAR_DECL */
		enum tree_code tc0 = TREE_CODE(arg_to);
		enum tree_code tc1 = TREE_CODE(arg_from);
		if ((tc0 != ADDR_EXPR) && (tc0 != PARM_DECL) && (tc0 != VAR_DECL) &&
		    (tc1 != ADDR_EXPR) && (tc1 != PARM_DECL) && (tc1 != VAR_DECL)) {
			si_log2("miss %s %s\n", tree_code_name[tc0],
					tree_code_name[tc1]);
			continue;
		}

		/* TODO */
		tree type0 = TREE_TYPE(arg_to);
		tree type1 = TREE_TYPE(arg_from);
		if (TYPE_ATTRIBUTES(type0) || TYPE_ATTRIBUTES(type1))
			si_log2("has attributes\n");

		tree addr0 = NULL, addr1 = NULL;
		if (tc0 == ADDR_EXPR) {
			addr0 = ((struct tree_exp *)arg_to)->operands[0];
			tc0 = TREE_CODE(addr0);
			if ((tc0 != VAR_DECL)) {
				si_log2("miss %s\n", tree_code_name[tc0]);
				continue;
			}
			arg_to = addr0;
		}
		if (tc1 == ADDR_EXPR) {
			addr1 = ((struct tree_exp *)arg_from)->operands[0];
			tc1 = TREE_CODE(addr1);
			if ((tc1 == COMPONENT_REF) || (tc1 == STRING_CST)) {
				/* TODO */
				arg_from = NULL;
			} else if ((tc1 != PARM_DECL) && (tc1 != VAR_DECL)) {
				si_log2("miss %s\n", tree_code_name[tc1]);
				continue;
			} else {
				arg_from = addr1;
			}
		}

		/* Okay, now check the arguments */
		/* TODO, the attributes are NULL */
		if (!flag) {
			/* copy_to_user(void __user *to, void *from, ...) */
			get_attributes(&head0, DECL_ATTRIBUTES(arg_to));
			if (arg_from)
				get_attributes(&head1, DECL_ATTRIBUTES(arg_from));
			if ((!check_address_space(&head0)) ||
					(check_address_space(&head1))) {
				expanded_location *xloc;
				xloc = get_gimple_loc(cur_sn->buf->payload,
							&gs->location);
				si_log2("kern_copyuser BUG: %s %d %d\n", xloc->file,
						xloc->line, xloc->column);
			}
		} else {
			/* copy_from_user(void *to, void __user *from, ...) */
			get_attributes(&head0, DECL_ATTRIBUTES(arg_to));
			if (arg_from)
				get_attributes(&head1, DECL_ATTRIBUTES(arg_from));
			if ((check_address_space(&head0)) ||
					(!check_address_space(&head1))) {
				expanded_location *xloc;
				xloc = get_gimple_loc(cur_sn->buf->payload,
							&gs->location);
				si_log2("kern_copyuser BUG: %s %d %d\n", xloc->file,
						xloc->line, xloc->column);
			}
		}
	}
}

/*
 * the function might not have a body yet, we should traverse all sinode, check
 * call_func_list, and do the check
 */
static void kern_copyuser_cb(void)
{
	unsigned long func_id = 0;
	union siid *tid = (union siid *)&func_id;
#define func_cnt	0x10
	char *func0_names[func_cnt] = {
		(char *)"copy_to_user",
		(char *)"_copy_to_user",
		(char *)"__copy_to_user",
		(char *)"__copy_to_user_inatomic",
		NULL,
	};
	char *func1_names[func_cnt] = {
		(char *)"copy_from_user",
		(char *)"_copy_from_user",
		(char *)"__copy_from_user",
		(char *)"__copy_from_user_inatomic",
		NULL,
	};

	tid->id0.id_type = TYPE_FUNC_GLOBAL;
	for (; func_id < si->id_idx[TYPE_FUNC_GLOBAL].id1; func_id++) {
		struct sinode *fsn;
		fsn = analysis__sinode_search(siid_get_type(tid), SEARCH_BY_ID, tid);
		if (!fsn)
			continue;

		struct func_node *fn = (struct func_node *)fsn->data;
		if (!fn)
			continue;

		analysis__resfile_load(fsn->buf);
		cur_sn = fsn;
		struct call_func_list *tmp;
		list_for_each_entry(tmp, &fn->callees, sibling) {
			if (!tmp->value_flag) {
				struct sinode *sn;
				union siid *id_tmp = (union siid *)&tmp->value;
				if (siid_get_type(id_tmp) == TYPE_FILE)
					continue;
				sn = analysis__sinode_search(siid_get_type(id_tmp),
								SEARCH_BY_ID,
								id_tmp);
				if (!sn)
					continue;

				for (int i = 0; i < func_cnt; i++) {
					if (!func0_names[i])
						break;
					if (!strcmp(func0_names[i], sn->name)) {
						do_check(tmp, 0);
					}
				}
				for (int i = 0; i < func_cnt; i++) {
					if (!func1_names[i])
						break;
					if (!strcmp(func1_names[i], sn->name)) {
						do_check(tmp, 1);
					}
				}
			} else {
				tree node = (tree)tmp->value;
				char name[NAME_MAX];
				get_function_name((void *)node, name);
				/* TODO */
			}
		}
	}

	cur_sn = NULL;
}
