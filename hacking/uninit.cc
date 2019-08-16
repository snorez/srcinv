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
#include "si_gcc.h"
#include "./hacking.h"

CLIB_MODULE_NAME(uninit);
CLIB_MODULE_NEEDED0();
static void uninit_cb(void);
static struct hacking_module uninit;

CLIB_MODULE_INIT()
{
	uninit.flag = HACKING_FLAG_STATIC;
	uninit.callback = uninit_cb;
	uninit.name = this_module_name;
	register_hacking_module(&uninit);
	return 0;
}

CLIB_MODULE_EXIT()
{
	unregister_hacking_module(&uninit);
	return;
}

static expanded_location __maybe_unused *get_code_path_loc(struct sinode *fsn, struct code_path *cp)
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

	expanded_location *xloc = get_gimple_loc(fsn->buf->payload,
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

static lock_t log_lock;
static void log_uninit_use(struct list_head *cp, expanded_location *xloc)
{
	mutex_lock(&log_lock);
	struct code_path_list __maybe_unused *cp_tmp;

	si_log2("========trigger location========\n");
	si_log2("maybe uninitialized var used at (%s %d %d)\n",
			xloc->file, xloc->line, xloc->column);

#if 0
	si_log2("========trigger code paths========\n");
	list_for_each_entry(cp_tmp, cp, sibling) {
		expanded_location *xloc = get_code_path_loc(cp_tmp->cp);
		if (!xloc)
			continue;
		si_log2("\t(%s %d %d)\n", xloc->file, xloc->line, xloc->column);
	}
#endif
	mutex_unlock(&log_lock);
}

static void first_use_as_read_check(struct sinode *fsn, struct var_node *vn,
					struct list_head *cp, int *res)
{
	struct use_at_list *first_ua;

	first_ua = first_use_in_cp(vn, cp);
	if (!first_ua)
		return;

	gimple_seq gs = (gimple_seq)first_ua->gimple_stmt;
	expanded_location *xloc = get_gimple_loc(fsn->buf->payload,&gs->location);
	enum gimple_code gc = gimple_code(gs);
	switch (gc) {
	case GIMPLE_ASSIGN:
	{
		tree *ops = gimple_ops(gs);
		for (unsigned int idx = 1; idx < gimple_num_ops(gs); idx++) {
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
		si_log2("miss %s. %s %d %d\n", gimple_code_name[gc],
				xloc->file, xloc->line, xloc->column);
		break;
	}
	}
}

static char *status_str = NULL;
static unsigned int processed = 0;
static unsigned int total = 0;
static atomic_t processed_cp;
static unsigned long print_args[5];

static void *__do_uninit_check_func(void *arg)
{
	long *args = (long *)arg;
	struct sinode *fsn;
	struct clib_rw_pool __maybe_unused *rw_pool;
	struct path_list_head *cp;
	struct clib_mt_pool *mt_pool;
	atomic_t *paths_read;
	fsn = (struct sinode *)args[0];
	rw_pool = (struct clib_rw_pool *)args[1];
	cp = (struct path_list_head *)args[2];
	mt_pool = (struct clib_mt_pool *)args[3];
	paths_read = (atomic_t *)args[4];
	struct func_node *fn = (struct func_node *)fsn->data;

	struct var_node_list *vnl;
	list_for_each_entry(vnl, &fn->local_vars, sibling) {
		tree node = (tree)vnl->var.node;
		if (TREE_STATIC(node))
			continue;

		int res = 0;
		first_use_as_read_check(fsn, &vnl->var, &cp->path_head, &res);
		if (res) {
			mt_pool->ret = res;
			break;
		}
	}

	analysis__drop_code_path(cp);
	atomic_inc(paths_read);
	atomic_inc(&processed_cp);
	clib_mt_pool_put(mt_pool);

	return (void *)0;
}

static void do_uninit_check_func(void *arg, struct clib_rw_pool *pool)
{
	long *args = (long *)arg;
	struct sinode *fsn;
	atomic_t *paths_read;
	fsn = (struct sinode *)args[0];
	paths_read = (atomic_t *)args[1];
	struct func_node *fn = (struct func_node *)fsn->data;
	struct clib_mt_pool *mt_pool;
	int thread_cnt = 0x18;
	int found = 0;

	mt_pool = clib_mt_pool_new(thread_cnt);

	struct path_list_head *single_cp;
	while ((single_cp = (struct path_list_head *)clib_rw_pool_pop(pool))) {
		if (found) {
			analysis__drop_code_path(single_cp);
			atomic_inc(paths_read);
			continue;
		}

		if (unlikely(!mt_pool)) {
			struct var_node_list *vnl;
			list_for_each_entry(vnl, &fn->local_vars, sibling) {
				tree node = (tree)vnl->var.node;
				if (TREE_STATIC(node))
					continue;

				int res = 0;
				first_use_as_read_check(fsn, &vnl->var,
							&single_cp->path_head, &res);
				if (res) {
					analysis__drop_code_path(single_cp);
					return;
				}
			}
			analysis__drop_code_path(single_cp);
			atomic_inc(&processed_cp);
		} else {
			struct clib_mt_pool *pool_avail;
			pool_avail = clib_mt_pool_get(mt_pool, thread_cnt);
			if (pool_avail->ret) {
				found = 1;
			}

			if (pool_avail->tid)
				pthread_join(pool_avail->tid, NULL);

			int err;
			pool_avail->arg[0] = (long)fsn;
			pool_avail->arg[1] = (long)pool;
			pool_avail->arg[2] = (long)single_cp;
			pool_avail->arg[3] = (long)pool_avail;
			pool_avail->arg[4] = (long)paths_read;
			err = pthread_create(&pool_avail->tid, NULL,
						__do_uninit_check_func,
						(void *)pool_avail->arg);
			if (err) {
				err_dbg(1, "pthread_create err");
				BUG();
			}
		}
	}

	if (mt_pool) {
		clib_mt_pool_wait_all(mt_pool, thread_cnt);
		clib_mt_pool_free(mt_pool);
	}
}

/*
 * ************************************************************************
 * output GIMPLE_* count
 * ************************************************************************
 */
size_t count_gimple_stmt(struct sinode *fsn, int code)
{
	if (!fsn->data)
		return 0;

	size_t ret = 0;
	analysis__resfile_load(fsn->buf);
	tree node = (tree)(long)fsn->obj->real_addr;
	gimple_seq body = DECL_STRUCT_FUNCTION(node)->gimple_body;
	gimple_seq next;

	next = body;
	while (next) {
		enum gimple_code gc_tmp;
		gc_tmp = gimple_code(next);
		if (gc_tmp == code)
			ret++;
		next = next->next;
	}

	return ret;
}

static void uninit_check_func(struct sinode *fsn)
{
	atomic_set(&processed_cp, 0);
	analysis__resfile_load(fsn->buf);

#if 0
	size_t count = 0;
	count = analysis__count_gimple_stmt(fsn, GIMPLE_COND);
	if (count > 48) {
		si_log2("%s not checked\n", fsn->name);
		analysis__resfile_unload(fsn->buf);
		return;
	}
#endif

	int err;
	struct clib_rw_pool_job *new_job;
	long write_arg[5];
	long read_arg[2];
	atomic_t paths_write;
	atomic_t paths_read;
	atomic_set(&paths_write, 0);
	atomic_set(&paths_read, 0);

	write_arg[0] = (long)fsn;
	write_arg[1] = 0;
	write_arg[2] = (long)fsn;
	write_arg[3] = (long)-1;
	write_arg[4] = (long)&paths_write;

	read_arg[0] = (long)fsn;
	read_arg[1] = (long)&paths_read;

	new_job = clib_rw_pool_job_new(OBJPOOL_DEF,
					analysis__gen_code_paths, write_arg,
					do_uninit_check_func, read_arg);
	if (!new_job) {
		err_dbg(0, "clib_rw_pool_job_new err");
		goto out;
	}

	err = clib_rw_pool_job_run(new_job);
	if (err == -1) {
		err_dbg(0, "clib_rw_pool_job_run err");
		goto out;
	}

	if (atomic_read(&paths_write) >= FUNC_CP_MAX) {
		si_log2("%s not completely checked\n", fsn->name);
	}

	if (atomic_read(&paths_write) != atomic_read(&paths_read)) {
		si_log2("%s MAY MEMORY LEAK: paths_write(%ld) not"
				" equal paths_read(%ld)\n",
				fsn->name, atomic_read(&paths_write),
				atomic_read(&paths_read));
	}

out:
	clib_rw_pool_job_free(new_job);
	analysis__resfile_unload(fsn->buf);
}

static void show_progress(int signo, siginfo_t *si, void *arg, int last)
{
	long *args = (long *)arg;
	pthread_t __maybe_unused id = (pthread_t)args[0];
	char *status = (char *)args[1];
	unsigned int p = *(unsigned int *)args[2];
	unsigned int t = *(unsigned int *)args[3];
	unsigned long pcp = atomic_read((atomic_t *)args[4]);

	if (!last) {
		fprintf(stdout, "%s: processed(%08x) total(%08x)"
				" processed_cp(%08lx)\r",
			status, p, t, pcp);
		fflush(stdout);
	} else {
		fprintf(stdout, "%s: processed(%08x) total(%08x)"
				" processed_cp(%08lx)\n",
			status, p, t, pcp);
		fflush(stdout);
	}
}

static void uninit_cb(void)
{
	si_log2("checking uninitialized variables\n");

	unsigned long func_id = 0;
	union siid *tid = (union siid *)&func_id;

	status_str = (char *)"UNINIT_GLOBAL_FUNCS";
	processed = 0;
	total = si->id_idx[TYPE_FUNC_GLOBAL].id0.id_value;
	print_args[0] = (long)pthread_self();
	print_args[1] = (long)status_str;
	print_args[2] = (long)&processed;
	print_args[3] = (long)&total;
	print_args[4] = (long)&processed_cp;

	mt_add_timer(1, show_progress, print_args, 0, 1);

	tid->id0.id_type = TYPE_FUNC_GLOBAL;
	for (; func_id < si->id_idx[TYPE_FUNC_GLOBAL].id1; func_id++) {
		struct sinode *fsn;
		fsn = analysis__sinode_search(siid_get_type(tid), SEARCH_BY_ID, tid);
		processed++;
		if (!fsn)
			continue;
		uninit_check_func(fsn);
	}

	mt_del_timer(0);

	status_str = (char *)"UNINIT_STATIC_FUNCS";
	processed = 0;
	total = si->id_idx[TYPE_FUNC_STATIC].id0.id_value;
	print_args[0] = (long)pthread_self();
	print_args[1] = (long)status_str;
	print_args[2] = (long)&processed;
	print_args[3] = (long)&total;
	print_args[4] = (long)&processed_cp;

	mt_add_timer(1, show_progress, print_args, 0, 1);

	func_id = 0;
	tid->id0.id_type = TYPE_FUNC_STATIC;
	for (; func_id < si->id_idx[TYPE_FUNC_STATIC].id1; func_id++) {
		struct sinode *fsn;
		fsn = analysis__sinode_search(siid_get_type(tid), SEARCH_BY_ID, tid);
		processed++;
		if (!fsn)
			continue;
		uninit_check_func(fsn);
	}

	mt_del_timer(0);

	si_log2("checking uninitialized variables done\n");
	return;
}
