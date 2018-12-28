/*
 * this is for parsing the collected tree nodes, thus we MUST NOT use any function
 * that manipulate pointer in tree_node structure before we adjust the pointer
 * this routine is compiled as a single program, not a dynamic library
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

/*
 * ************************************************************************
 * variables
 * ************************************************************************
 */
LIST_HEAD(lang_ops_head);

#define	STEPMIN	0
#define	STEP1	1
#define	STEP2	2
#define	STEP3	3
#define	STEP4	4
#define	STEP5	5
#define	STEPMAX	6

static int parse_resfile(char *path, int built_in, int step);
static void cmd0_usage(void)
{
	fprintf(stdout, "\t(res_path) (is_builtin) (linux_kernel?) (step)\n"
			"\t\tGet information of resfile, for step\n"
			"\t\t\t0 Get all information\n"
			"\t\t\t1 Get base information\n"
			"\t\t\t2 Get detail information\n"
			"\t\t\t3 Get xrefs information\n"
			"\t\t\t4 Get indirect call information\n"
			"\t\t\t5 Check if all GIMPLE_CALL are set\n");
}
static long cmd0_cb(int argc, char *argv[])
{
	int err;
	if (argc != 5) {
		err_dbg(0, err_fmt("invalid arguments"));
		return -1;
	}

	int built_in = atoi(argv[2]);
	int is_linux_kernel = atoi(argv[3]);
	int step = atoi(argv[4]);
	si->is_kernel = is_linux_kernel;
	err = parse_resfile(argv[1], !!built_in, step);
	if (err) {
		err_dbg(0, err_fmt("parse_resfile err"));
		return -1;
	}

	return 0;
}

static char cmd0[] = "getinfo";
static struct clib_cmd _cmd0 = {
	.cmd = cmd0,
	.cb = cmd0_cb,
	.usage = cmd0_usage,
};

CLIB_PLUGIN_NAME(getinfo);
CLIB_PLUGIN_NEEDED3(src, sinode, resfile);
CLIB_PLUGIN_INIT()
{
	INIT_LIST_HEAD(&lang_ops_head);

	int err;
	err = clib_cmd_add(&_cmd0);
	if (err) {
		err_dbg(0, err_fmt("clib_cmd_add err"));
		return -1;
	}

	return 0;
}
CLIB_PLUGIN_EXIT()
{
	clib_cmd_del(_cmd0.cmd);
}

static int _do_phase1(struct sibuf *buf)
{
	int err = 0;

	struct file_context *fc = (struct file_context *)buf->load_addr;
	struct lang_ops *ops = lang_ops_find(fc->type);
	if (!ops) {
		err_dbg(0, err_fmt("lang_ops TYPE: %d not found"), fc->type);
		return -1;
	}

	err = ops->callback(buf, MODE_ADJUST);
	if (err) {
		err_dbg(0, err_fmt("MODE_ADJUST err"));
		return -1;
	}

	err = ops->callback(buf, MODE_GETBASE);
	if (err) {
		err_dbg(0, err_fmt("MODE_GETBASE err"));
		return -1;
	}

#if 0
	/* MODE_GETDETAIL should be after all files MODE_GETBASE done */
	err = ops->callback(buf, MODE_GETDETAIL);
	if (err) {
		err_dbg(0, err_fmt("MODE_GETDETAIL err"));
		return -1;
	}
#endif

	return 0;
}

static int _do_phase2(struct sibuf *buf)
{
	int err = 0;
	resfile__resfile_load(buf);
	struct file_context *fc = (struct file_context *)buf->load_addr;
	struct lang_ops *ops = lang_ops_find(fc->type);
	if (!ops) {
		err_dbg(0, err_fmt("lang_ops TYPE: %d not found"), fc->type);
		return -1;
	}

	err = ops->callback(buf, MODE_GETDETAIL);
	if (err) {
		err_dbg(0, err_fmt("MODE_GETDETAIL err"));
		return -1;
	}

	return 0;
}

static int _do_phase3(struct sibuf *buf)
{
	int err = 0;
	resfile__resfile_load(buf);
	struct file_context *fc = (struct file_context *)buf->load_addr;
	struct lang_ops *ops = lang_ops_find(fc->type);
	if (!ops) {
		err_dbg(0, err_fmt("lang_ops TYPE: %d not found"), fc->type);
		return -1;
	}

	err = ops->callback(buf, MODE_GETXREFS);
	if (err) {
		err_dbg(0, err_fmt("MODE_GETXREFS err"));
		return -1;
	}

	return 0;
}

static int _do_phase4(struct sibuf *buf)
{
	int err = 0;
	resfile__resfile_load(buf);
	struct file_context *fc = (struct file_context *)buf->load_addr;
	struct lang_ops *ops = lang_ops_find(fc->type);
	if (!ops) {
		err_dbg(0, err_fmt("lang_ops TYPE: %d not found"), fc->type);
		return -1;
	}

	err = ops->callback(buf, MODE_GETINDCFG1);
	if (err) {
		err_dbg(0, err_fmt("MODE_GETINDCFG1 err"));
		return -1;
	}

	return 0;
}

static int _do_phase5(struct sibuf *buf)
{
	int err = 0;
	resfile__resfile_load(buf);
	struct file_context *fc = (struct file_context *)buf->load_addr;
	struct lang_ops *ops = lang_ops_find(fc->type);
	if (!ops) {
		err_dbg(0, err_fmt("lang_ops TYPE: %d not found"), fc->type);
		return -1;
	}

	err = ops->callback(buf, MODE_GETINDCFG2);
	if (err) {
		err_dbg(0, err_fmt("MODE_GETINDCFG2 err"));
		return -1;
	}

	return 0;
}

/* XXX: use multiple threads to parse the file */
#define	THREAD_CNT	0x10
static atomic_t *parsed_files;
struct mt_parse {
	struct sibuf	*buf;
	int		ret;
	atomic_t	in_use;
	pthread_t	tid;
};
static struct mt_parse bufs[THREAD_CNT];
static struct mt_parse *mt_parse_find0(void)
{
	for (int i = 0; i < THREAD_CNT; i++) {
		if (!atomic_read(&bufs[i].in_use)) {
			if (!bufs[i].buf) {
				bufs[i].buf = sibuf_new();
				BUG_ON(!bufs[i].buf);
			}
			return &bufs[i];
		}
	}
	return NULL;
}
static struct mt_parse *mt_parse_find1(void)
{
	for (int i = 0; i < THREAD_CNT; i++) {
		if (!atomic_read(&bufs[i].in_use)) {
			return &bufs[i];
		}
	}
	return NULL;
}

static void *do_phase1(void *arg)
{
	struct mt_parse *t = (struct mt_parse *)arg;
	int err = _do_phase1(t->buf);
	if (err) {
		err_dbg(0, err_fmt("_do_phase1 err"));
		t->ret = 1;
		atomic_set(&t->in_use, 0);
		return (void *)-1;
	}

	if (t->buf->status < FC_STATUS_GETBASE)
		atomic_inc(&parsed_files[FC_STATUS_GETBASE]);
	t->buf->status = FC_STATUS_GETBASE;
	sibuf_insert(t->buf);
	t->buf = NULL;
	atomic_set(&t->in_use, 0);
	return (void *)0;
}

static void *do_phase2(void *arg)
{
	struct mt_parse *t = (struct mt_parse *)arg;
	int err = _do_phase2(t->buf);
	if (err) {
		err_dbg(0, err_fmt("_do_phase2 err"));
		t->ret = 1;
		atomic_set(&t->in_use, 0);
		return (void *)-1;
	}

	if (t->buf->status < FC_STATUS_GETDETAIL)
		atomic_inc(&parsed_files[FC_STATUS_GETDETAIL]);
	t->buf->status = FC_STATUS_GETDETAIL;
	t->buf = NULL;
	atomic_set(&t->in_use, 0);
	return (void *)0;
}

static void *do_phase3(void *arg)
{
	struct mt_parse *t = (struct mt_parse *)arg;
	int err = _do_phase3(t->buf);
	if (err) {
		err_dbg(0, err_fmt("_do_phase3 err"));
		t->ret = 1;
		atomic_set(&t->in_use, 0);
		return (void *)-1;
	}

	if (t->buf->status < FC_STATUS_GETXREFS)
		atomic_inc(&parsed_files[FC_STATUS_GETXREFS]);
	t->buf->status = FC_STATUS_GETXREFS;
	t->buf = NULL;
	atomic_set(&t->in_use, 0);
	return (void *)0;
}

static void *do_phase4(void *arg)
{
	struct mt_parse *t = (struct mt_parse *)arg;
	int err = _do_phase4(t->buf);
	if (err) {
		err_dbg(0, err_fmt("_do_phase4 err"));
		t->ret = 1;
		atomic_set(&t->in_use, 0);
		return (void *)-1;
	}

	if (t->buf->status < FC_STATUS_GETINDCFG1)
		atomic_inc(&parsed_files[FC_STATUS_GETINDCFG1]);
	t->buf->status = FC_STATUS_GETINDCFG1;
	t->buf = NULL;
	atomic_set(&t->in_use, 0);
	return (void *)0;
}

static void *do_phase5(void *arg)
{
	struct mt_parse *t = (struct mt_parse *)arg;
	int err = _do_phase5(t->buf);
	if (err) {
		err_dbg(0, err_fmt("_do_phase5 err"));
		t->ret = 1;
		atomic_set(&t->in_use, 0);
		return (void *)-1;
	}

	if (t->buf->status < FC_STATUS_GETINDCFG2)
		atomic_inc(&parsed_files[FC_STATUS_GETINDCFG2]);
	t->buf->status = FC_STATUS_GETINDCFG2;
	t->buf = NULL;
	atomic_set(&t->in_use, 0);
	return (void *)0;
}

static void wait_for_all_threads(void)
{
	for (int i = 0; i < THREAD_CNT; i++) {
		while (atomic_read(&bufs[i].in_use)) {
			i = 0;
			usleep(1000);
			continue;
		}
	}
}

static sigjmp_buf parse_jmp_env;
static int parse_sig_set = 0;
static int sigjmp_lbl;
static void sigquit_hdl(int signo)
{
	siglongjmp(parse_jmp_env, sigjmp_lbl);
}
static long file_progress_arg[4];
static char *status_str;
static void show_file_progress(int signo, siginfo_t *si, void *arg)
{
	long *args = (long *)arg;
	pthread_t id = (pthread_t)args[0];
	unsigned long cur = atomic_read((atomic_t *)args[1]);
	unsigned long total = *(unsigned long *)args[2];
	char *status = (char *)args[3];
	mt_print(id, "%s: processed: %ld, total: %ld, %.3f%%\n",
			status, cur, total, (double)cur * 100 / (double)total);
}

static int parse_resfile(char *path, int built_in, int step)
{
	int err = 0;
	struct resfile *newrf;
	newrf = resfile__resfile_new(path, built_in);
	if (!newrf) {
		err_dbg(0, err_fmt("resfile_new err"));
		return -1;
	}
	resfile__resfile_add(newrf);

	fprintf(stdout, "[....] take a preview of the resfile\r");
	fflush(stdout);
	err = resfile__resfile_get_filecnt(newrf);
	if (err == -1) {
		err_dbg(0, err_fmt("resfile__resfile_get_filecnt err"));
		return -1;
	}
	fprintf(stdout, "[done]\n");
	fflush(stdout);
	parsed_files = newrf->parsed_files;

	time_acct_start();
	if (step == STEP1)
		goto step_1;
	else if (step == STEP2)
		goto step_2;
	else if (step == STEP3)
		goto step_3;
	else if (step == STEP4)
		goto step_4;
	else if (step == STEP5)
		goto step_5;

step_1:
	status_str = "PHASE1";
	memset(&bufs, 0, sizeof(bufs));
	sigjmp_lbl = 1;
	file_progress_arg[0] = (long)pthread_self();
	file_progress_arg[1] = (long)&newrf->parsed_files[FC_STATUS_GETBASE];
	file_progress_arg[2] = (long)&newrf->total_files;
	file_progress_arg[3] = (long)status_str;
#ifdef USE_NCURSES
	mt_print_init();
#endif
	mt_print_add();
	mt_add_timer(1, show_file_progress, file_progress_arg);

	/* phase 1, until get base */
	int force = 0;
	while (1) {
		if (unlikely(!parse_sig_set)) {
			parse_sig_set = 1;
			signal(SIGQUIT, sigquit_hdl);
		}
		if (sigsetjmp(parse_jmp_env, sigjmp_lbl)) {
			/* jump out this loop, and wait for all threads terminate */
			err_dbg(0, err_fmt("receive SIGQUIT signal, quit parsing"));
			break;
		}

		struct mt_parse *t = mt_parse_find0();
		if (!t) {
			sleep(1);
			continue;
		}
		if (t->ret) {
			err_dbg(0, err_fmt("do_phase1 ret err"));
			err = -1;
			break;
		}

		err = resfile__resfile_read(newrf, t->buf, force);
		if (force)
			force = 0;
		if (err == -1) {
			err_dbg(0, err_fmt("resfile__resfile_read err"));
			break;
		} else if (!err) {
			break;
		} else if (err == -EAGAIN) {
			wait_for_all_threads();
			force = 1;
			continue;
		}
		t->buf->rf = newrf;
		atomic_set(&t->in_use, 1);

redo_parse1:
		err = pthread_create(&t->tid, NULL, do_phase1, t);
		if (err) {
			err_dbg(0, err_fmt("pthread_create err"));
			sleep(1);
			goto redo_parse1;
		}
	}

	wait_for_all_threads();

	mt_del_timer();
	mt_print_del();
#ifdef USE_NCURSES
	mt_print_fini();
#endif
	if (step == STEP1)
		goto out;

step_2:
	status_str = "PHASE2";
	memset(&bufs, 0, sizeof(bufs));
	sigjmp_lbl = 2;
	file_progress_arg[0] = (long)pthread_self();
	file_progress_arg[1] = (long)&newrf->parsed_files[FC_STATUS_GETDETAIL];
	file_progress_arg[2] = (long)&newrf->total_files;
	file_progress_arg[3] = (long)status_str;
#ifdef USE_NCURSES
	mt_print_init();
#endif
	mt_print_add();
	mt_add_timer(1, show_file_progress, file_progress_arg);

	/* phase 2, get detail */
	struct sibuf *tmp;
	list_for_each_entry_reverse(tmp, &si->sibuf_head, sibling) {
		if (unlikely(!parse_sig_set)) {
			parse_sig_set = 1;
			signal(SIGQUIT, sigquit_hdl);
		}
		if (sigsetjmp(parse_jmp_env, sigjmp_lbl)) {
			/* jump out this loop, and wait for all threads terminate */
			err_dbg(0, err_fmt("receive SIGQUIT signal, quit parsing"));
			break;
		}

		struct mt_parse *t;
		while (1) {
			t = mt_parse_find1();
			if (t)
				break;
			sleep(1);
		}
		if (t->ret) {
			err_dbg(0, err_fmt("do_phase2 ret err"));
			err = -1;
			break;
		}

		t->buf = tmp;
		atomic_set(&t->in_use, 1);

redo_parse2:
		err = pthread_create(&t->tid, NULL, do_phase2, t);
		if (err) {
			err_dbg(0, err_fmt("pthread_create err"));
			sleep(1);
			goto redo_parse2;
		}
	}

	wait_for_all_threads();

	mt_del_timer();
	mt_print_del();
#ifdef USE_NCURSES
	mt_print_fini();
#endif
	if (step == STEP2)
		goto out;

step_3:
	status_str = "PHASE3";
	memset(&bufs, 0, sizeof(bufs));
	sigjmp_lbl = 3;
	file_progress_arg[0] = (long)pthread_self();
	file_progress_arg[1] = (long)&newrf->parsed_files[FC_STATUS_GETXREFS];
	file_progress_arg[2] = (long)&newrf->total_files;
	file_progress_arg[3] = (long)status_str;
#ifdef USE_NCURSES
	mt_print_init();
#endif
	mt_print_add();
	mt_add_timer(1, show_file_progress, file_progress_arg);

	/* phase 3, get xrefs except indirect calls */
	list_for_each_entry_reverse(tmp, &si->sibuf_head, sibling) {
		if (unlikely(!parse_sig_set)) {
			parse_sig_set = 1;
			signal(SIGQUIT, sigquit_hdl);
		}
		if (sigsetjmp(parse_jmp_env, sigjmp_lbl)) {
			/* jump out this loop, and wait for all threads terminate */
			err_dbg(0, err_fmt("receive SIGQUIT signal, quit parsing"));
			break;
		}

		struct mt_parse *t;
		while (1) {
			t = mt_parse_find1();
			if (t)
				break;
			sleep(1);
		}
		if (t->ret) {
			err_dbg(0, err_fmt("do_phase3 ret err"));
			err = -1;
			break;
		}

		t->buf = tmp;
		atomic_set(&t->in_use, 1);

redo_parse3:
		err = pthread_create(&t->tid, NULL, do_phase3, t);
		if (err) {
			err_dbg(0, err_fmt("pthread_create err"));
			sleep(1);
			goto redo_parse3;
		}
	}

	wait_for_all_threads();

	mt_del_timer();
	mt_print_del();
#ifdef USE_NCURSES
	mt_print_fini();
#endif
	if (step == STEP3)
		goto out;

step_4:
	status_str = "PHASE4";
	memset(&bufs, 0, sizeof(bufs));
	sigjmp_lbl = 4;
	file_progress_arg[0] = (long)pthread_self();
	file_progress_arg[1] = (long)&newrf->parsed_files[FC_STATUS_GETINDCFG1];
	file_progress_arg[2] = (long)&newrf->total_files;
	file_progress_arg[3] = (long)status_str;
#ifdef USE_NCURSES
	mt_print_init();
#endif
	mt_print_add();
	mt_add_timer(1, show_file_progress, file_progress_arg);

	/* phase 4, get indirect calls */
	list_for_each_entry_reverse(tmp, &si->sibuf_head, sibling) {
		if (unlikely(!parse_sig_set)) {
			parse_sig_set = 1;
			signal(SIGQUIT, sigquit_hdl);
		}
		if (sigsetjmp(parse_jmp_env, sigjmp_lbl)) {
			/* jump out this loop, and wait for all threads terminate */
			err_dbg(0, err_fmt("receive SIGQUIT signal, quit parsing"));
			break;
		}

		struct mt_parse *t;
		while (1) {
			t = mt_parse_find1();
			if (t)
				break;
			sleep(1);
		}
		if (t->ret) {
			err_dbg(0, err_fmt("do_phase4 ret err"));
			err = -1;
			break;
		}

		t->buf = tmp;
		atomic_set(&t->in_use, 1);

redo_parse4:
		err = pthread_create(&t->tid, NULL, do_phase4, t);
		if (err) {
			err_dbg(0, err_fmt("pthread_create err"));
			sleep(1);
			goto redo_parse4;
		}
	}

	wait_for_all_threads();

	mt_del_timer();
	mt_print_del();
#ifdef USE_NCURSES
	mt_print_fini();
#endif
	if (step == STEP4)
		goto out;

step_5:
	status_str = "PHASE5";
	memset(&bufs, 0, sizeof(bufs));
	sigjmp_lbl = 5;
	file_progress_arg[0] = (long)pthread_self();
	file_progress_arg[1] = (long)&newrf->parsed_files[FC_STATUS_GETINDCFG2];
	file_progress_arg[2] = (long)&newrf->total_files;
	file_progress_arg[3] = (long)status_str;
#ifdef USE_NCURSES
	mt_print_init();
#endif
	mt_print_add();
	mt_add_timer(1, show_file_progress, file_progress_arg);

	/* phase 4, get indirect calls */
	list_for_each_entry_reverse(tmp, &si->sibuf_head, sibling) {
		if (unlikely(!parse_sig_set)) {
			parse_sig_set = 1;
			signal(SIGQUIT, sigquit_hdl);
		}
		if (sigsetjmp(parse_jmp_env, sigjmp_lbl)) {
			/* jump out this loop, and wait for all threads terminate */
			err_dbg(0, err_fmt("receive SIGQUIT signal, quit parsing"));
			break;
		}

		struct mt_parse *t;
		while (1) {
			t = mt_parse_find1();
			if (t)
				break;
			sleep(1);
		}
		if (t->ret) {
			err_dbg(0, err_fmt("do_phase5 ret err"));
			err = -1;
			break;
		}

		t->buf = tmp;
		atomic_set(&t->in_use, 1);

redo_parse5:
		err = pthread_create(&t->tid, NULL, do_phase5, t);
		if (err) {
			err_dbg(0, err_fmt("pthread_create err"));
			sleep(1);
			goto redo_parse5;
		}
	}

	wait_for_all_threads();

	mt_del_timer();
	mt_print_del();
#ifdef USE_NCURSES
	mt_print_fini();
#endif
	if (step == STEP5)
		goto out;

out:
	time_acct_end();

	return err;
}
