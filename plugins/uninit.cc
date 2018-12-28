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
#include "../si_core.h"

CLIB_PLUGIN_NAME(uninit);
CLIB_PLUGIN_NEEDED1(staticchk);
static void uninit_cb(void);
static struct staticchk_method uninit_method;

CLIB_PLUGIN_INIT()
{
	uninit_method.method_flg = METHOD_FLG_UNINIT;
	uninit_method.callback = uninit_cb;
	register_staticchk_method(&uninit_method);
	return 0;
}

CLIB_PLUGIN_EXIT()
{
	return;
}

static struct sinode *cur_fsn;
static expanded_location *get_code_path_loc(struct code_path *cp)
{
	struct code_sentence *first_cs;
	gimple_seq gs;

	first_cs = list_first_entry_or_null(&cp->sentences,
						struct code_sentence, sibling);
	if (!first_cs)
		gs = (gimple_seq)cp->cond_head;
	else
		gs = (gimple_seq)first_cs->head;

	if (!gs)
		return NULL;

	expanded_location *xloc = get_gimple_loc(cur_fsn->buf->payload,
							&gs->location);
	return xloc;
}

static struct use_at_list *first_use_in_cp(struct var_node *vn, struct list_head *cp)
{
	struct code_path_list *tmp0;
	list_for_each_entry(tmp0, cp, sibling) {
		struct code_sentence *tmp1;
		list_for_each_entry(tmp1, &tmp0->cp->sentences, sibling) {
			struct use_at_list *tmp2;
			list_for_each_entry(tmp2, &vn->used_at, sibling) {
				if (tmp2->gimple_stmt == tmp1->head)
					return tmp2;
			}
		}
		if (tmp0->cp->cond_head) {
			struct use_at_list *tmp2;
			list_for_each_entry(tmp2, &vn->used_at, sibling) {
				if (tmp2->gimple_stmt == tmp0->cp->cond_head)
					return tmp2;
			}
		}
	}
	return NULL;
}

static void log_uninit_use(struct list_head *cp, expanded_location *xloc)
{
	struct code_path_list *cp_tmp;

	si_log("========trigger location========\n");
	si_log("maybe uninitialized var used at (%s %d %d)\n",
			xloc->file, xloc->line, xloc->column);

#if 0
	si_log("========trigger code pathes========\n");
	list_for_each_entry(cp_tmp, cp, sibling) {
		expanded_location *xloc = get_code_path_loc(cp_tmp->cp);
		if (!xloc)
			continue;
		si_log("\t(%s %d %d)\n", xloc->file, xloc->line, xloc->column);
	}
#endif
}

static void first_use_as_read_check_in_cp(struct var_node *vn, struct list_head *cp,
						int *res)
{
	struct use_at_list *first_ua;

	first_ua = first_use_in_cp(vn, cp);
	if (!first_ua)
		return;

	gimple_seq gs = (gimple_seq)first_ua->gimple_stmt;
	expanded_location *xloc = get_gimple_loc(cur_fsn->buf->payload,&gs->location);
	enum gimple_code gc = gimple_code(gs);
	switch (gc) {
	case GIMPLE_ASSIGN:
	{
		tree *ops = gimple_ops(gs);
		for (int idx = 1; idx < gimple_num_ops(gs); idx++) {
			if ((void *)ops[idx] == vn->node) {
				log_uninit_use(cp, xloc);
				*res = 1;
				break;
			}
		}
		/* TODO, what if op_idx not zero, and is ADDR_EXPR */
		break;
	}
	case GIMPLE_COND:
	{
		/* TODO, mostly, is a read purpose here */
		log_uninit_use(cp, xloc);
		*res = 1;
		break;
	}
	case GIMPLE_CALL:
	{
		if (first_ua->op_idx == 0)
			break;
		/* TODO, follow this value into callee */
		break;
	}
	case GIMPLE_ASM:
	{
		/* TODO, check if this value is in ni list */
		break;
	}
	default:
	{
		si_log("miss %s. %s %d %d\n", gimple_code_name[gc],
				xloc->file, xloc->line, xloc->column);
		break;
	}
	}
}

static void first_use_as_read_check(struct var_node *vn, struct list_head *cps)
{
	struct path_list_head *tmp0;
	list_for_each_entry(tmp0, cps, sibling) {
		int res = 0;
		first_use_as_read_check_in_cp(vn, &tmp0->path_head, &res);
		if (res)
			return;
	}
}

static void uninit_check_func(struct sinode *fsn)
{
	cur_fsn = fsn;
	resfile__resfile_load(fsn->buf);
	struct func_node *fn = (struct func_node *)fsn->data;
	struct list_head head;
	utils__gen_code_pathes(fsn, 0, fsn, -1, &head);

#if 0
	unsigned long path_count = 0;
	struct path_list_head *tmp;
	list_for_each_entry(tmp, &head, sibling) {
		path_count++;
	}
	fprintf(stderr, "paths: %ld\n", path_count);
#endif
	show_func_gimples(fsn);

	struct var_node_list *vnl;
	list_for_each_entry(vnl, &fn->local_vars, sibling) {
		tree node = (tree)vnl->var.node;
		if (TREE_STATIC(node))
			continue;

		first_use_as_read_check(&vnl->var, &head);
	}

	utils__drop_code_pathes(&head);
	resfile__resfile_unload(fsn->buf);
}

static char *status_str = NULL;
static unsigned int processed = 0;
static unsigned int total = 0;
static unsigned long print_args[4];
static void show_progress(int signo, siginfo_t *si, void *arg)
{
	long *args = (long *)arg;
	pthread_t id = (pthread_t)args[0];
	char *status = (char *)args[1];
	unsigned int p = *(unsigned int *)args[2];
	unsigned int t = *(unsigned int *)args[3];

	mt_print1(id, "%s: processed(%d) total(%d)\n", status, p, t);
}

static void uninit_cb(void)
{
	si_log("checking uninitialized variables\n");

	unsigned long func_id = 0;
	union siid *tid = (union siid *)&func_id;

	status_str = (char *)"UNINIT_GLOBAL_FUNCS";
	processed = 0;
	total = si->id_idx[TYPE_FUNC_GLOBAL].id0.id_value;
	print_args[0] = (long)pthread_self();
	print_args[1] = (long)status_str;
	print_args[2] = (long)&processed;
	print_args[3] = (long)&total;

	mt_print_add();
	mt_add_timer(1, show_progress, print_args);

	tid->id0.id_type = TYPE_FUNC_GLOBAL;
	for (; func_id < si->id_idx[TYPE_FUNC_GLOBAL].id1; func_id++) {
		struct sinode *fsn;
		fsn = sinode__sinode_search(siid_get_type(tid), SEARCH_BY_ID, tid);
		processed++;
		if (!fsn)
			continue;
		uninit_check_func(fsn);
	}

	mt_del_timer();
	mt_print_del();

	status_str = (char *)"UNINIT_STATIC_FUNCS";
	processed = 0;
	total = si->id_idx[TYPE_FUNC_STATIC].id0.id_value;
	print_args[0] = (long)pthread_self();
	print_args[1] = (long)status_str;
	print_args[2] = (long)&processed;
	print_args[3] = (long)&total;

	mt_print_add();
	mt_add_timer(1, show_progress, print_args);

	func_id = 0;
	tid->id0.id_type = TYPE_FUNC_STATIC;
	for (; func_id < si->id_idx[TYPE_FUNC_STATIC].id1; func_id++) {
		struct sinode *fsn;
		fsn = sinode__sinode_search(siid_get_type(tid), SEARCH_BY_ID, tid);
		processed++;
		if (!fsn)
			continue;
		uninit_check_func(fsn);
	}

	mt_del_timer();
	mt_print_del();

	si_log("checking uninitialized variables done\n");
	return;
}
