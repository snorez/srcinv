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

static int parse_resfile(char *path, int built_in, int step);
static void cmd0_usage(void)
{
	fprintf(stdout, "\t(res_path) (is_builtin) (linux_kernel?) (step)\n"
			"\t\tGet information of resfile, for step\n"
			"\t\t\t0 Get all information\n"
			"\t\t\t1 Get information adjusted\n"
			"\t\t\t2 Get base information\n"
			"\t\t\t3 Get detail information\n"
			"\t\t\t4 Get xrefs information\n"
			"\t\t\t5 Get indirect call information\n"
			"\t\t\t6 Check if all GIMPLE_CALL are set\n");
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

static int _do_phase(struct sibuf *buf, int step)
{
	int err = 0;
	if (step > STEP1)
		resfile__resfile_load(buf);

	struct file_context *fc = (struct file_context *)buf->load_addr;
	struct lang_ops *ops = lang_ops_find(fc->type);
	if (!ops) {
		err_dbg(0, err_fmt("lang_ops TYPE: %d not found"), fc->type);
		return -1;
	}

	err = ops->callback(buf, step);
	if (err) {
		err_dbg(0, err_fmt("fc->type callback err"));
		return -1;
	}

	return 0;
}

/* XXX: use multiple threads to parse the file, core*3 */
#define	THREAD_CNT	0x18
#define	THREAD_STACKSZ	(1024*1024*0x10)
static atomic_t *parsed_files;
struct mt_parse {
	struct sibuf	*buf;
	int		ret;
	int		step;
	/* 0, not use; 1, in use, not in thread; 2, in thread */
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

static void *do_phase(void *arg)
{
	struct mt_parse *t = (struct mt_parse *)arg;
	atomic_set(&t->in_use, 2);
	int step = t->step;

	int err = _do_phase(t->buf, step);
	if (err) {
		err_dbg(0, err_fmt("_do_phase err"));
		t->ret = 1;
		atomic_set(&t->in_use, 0);
		return (void *)-1;
	}

	t->buf->status = step;
	atomic_inc(&parsed_files[step]);

	t->buf = NULL;
	atomic_set(&t->in_use, 0);
	return (void *)0;
}

static void wait_for_all_threads(void)
{
	/* FIXME: may be called after SIGQUIT */
	for (int i = 0; i < THREAD_CNT; i++) {
		if (bufs[i].tid)
			pthread_join(bufs[i].tid, NULL);
		long in_use = atomic_read(&bufs[i].in_use);
		if (!in_use)
			continue;

		if (in_use == 1) {
			usleep(1000*10);
			in_use = atomic_read(&bufs[i].in_use);
			if (in_use == 1) {
				atomic_set(&bufs[i].in_use, 0);
				i--;
				continue;
			}
		}

		if (in_use == 2) {
			i = -1;
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
static void show_file_progress(int signo, siginfo_t *si, void *arg, int last)
{
	long *args = (long *)arg;
	pthread_t id = (pthread_t)args[0];
	unsigned long cur = atomic_read((atomic_t *)args[1]);
	unsigned long total = *(unsigned long *)args[2];
	char *status = (char *)args[3];

#if 0
	char *buf;
	buf = clib_ap_start("%s: processed(%ld) total(%ld) %.3f%%\n",
				status, cur, total, percent);
	mt_print0(id, buf);
	clib_ap_end(buf);
#else
	mt_print1(id, "%s processed(%ld) total(%ld)\n", status, cur, total);
#endif
}

static int parse_resfile(char *path, int built_in, int step)
{
	int err = 0;
	int flag = 0;
	struct resfile *newrf;

	if (step == 0) {
		flag = 1;
		step = STEP1;
	}

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

	pthread_attr_t attr;
	err = pthread_attr_init(&attr);
	if (err) {
		err_dbg(0, err_fmt("pthread_attr_init err"));
		return -1;
	}
	err = pthread_attr_setstacksize(&attr, THREAD_STACKSZ);
	if (err) {
		err_dbg(0, err_fmt("pthread_attr_setstacksize err"));
		return -1;
	}

	time_acct_start();

	while (step < STEPMAX) {
		switch (step) {
		case STEP1:
			status_str = "PHASE1";
			break;
		case STEP2:
			status_str = "PHASE2";
			break;
		case STEP3:
			status_str = "PHASE3";
			break;
		case STEP4:
			status_str = "PHASE4";
			break;
		case STEP5:
			status_str = "PHASE5";
			break;
		case STEP6:
			status_str = "PHASE6";
			break;
		default:
			BUG();
			break;
		}

		memset(&bufs, 0, sizeof(bufs));
		sigjmp_lbl = step;
		file_progress_arg[0] = (long)pthread_self();
		file_progress_arg[1] = (long)&newrf->parsed_files[step];
		file_progress_arg[2] = (long)&newrf->total_files;
		file_progress_arg[3] = (long)status_str;
#ifdef USE_NCURSES
		mt_print_init();
#endif
		mt_print_add();
		mt_add_timer(1, show_file_progress, file_progress_arg);

		switch (step) {
		case STEP1:
		{
			int force = 0;
			while (1) {
				if (unlikely(!parse_sig_set)) {
					parse_sig_set = 1;
					signal(SIGQUIT, sigquit_hdl);
				}
				if (sigsetjmp(parse_jmp_env, sigjmp_lbl)) {
					/* jump out this loop, wait for all threads */
					err_dbg(0, err_fmt("receive SIGQUIT signal,"
								" quit parsing"));
					break;
				}

				struct mt_parse *t = mt_parse_find0();
				if (!t) {
					sleep(1);
					continue;
				}
				if (t->ret) {
					err_dbg(0, err_fmt("do_phase ret err"));
					err = -1;
					break;
				}

				err = resfile__resfile_read(newrf, t->buf, force);
				if (force)
					force = 0;
				if (err == -1) {
					err_dbg(0,
						err_fmt("resfile__resfile_read err"));
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
				t->step = step;

				if (t->tid)
					pthread_join(t->tid, NULL);
redo1:
				err = pthread_create(&t->tid, &attr, do_phase, t);
				if (err) {
					err_dbg(0, err_fmt("pthread_create err"));
					sleep(1);
					goto redo1;
				}
				sibuf__sibuf_insert(t->buf);
			}

			resfile__resfile_unload_all();

			break;
		}
		case STEP2:
		case STEP3:
		case STEP4:
		case STEP5:
		case STEP6:
		{
			struct sibuf *tmp;
			list_for_each_entry_reverse(tmp, &si->sibuf_head, sibling) {
				if (unlikely(!parse_sig_set)) {
					parse_sig_set = 1;
					signal(SIGQUIT, sigquit_hdl);
				}
				if (sigsetjmp(parse_jmp_env, sigjmp_lbl)) {
					/* jump out this loop, wait for all threads */
					err_dbg(0, err_fmt("receive SIGQUIT signal,"
								" quit parsing"));
					break;
				}

				if (tmp->status >= step)
					continue;

				struct mt_parse *t;
				while (1) {
					t = mt_parse_find1();
					if (t)
						break;
					sleep(1);
				}
				if (t->ret) {
					err_dbg(0, err_fmt("do_phase ret err"));
					err = -1;
					break;
				}

				t->buf = tmp;
				atomic_set(&t->in_use, 1);
				t->step = step;

				if (t->tid)
					pthread_join(t->tid, NULL);
redo2:
				err = pthread_create(&t->tid, &attr, do_phase, t);
				if (err) {
					err_dbg(0, err_fmt("pthread_create err"));
					sleep(1);
					goto redo2;
				}
			}

			break;
		}
		default:
		{
			BUG();
		}
		}

		wait_for_all_threads();

		mt_del_timer();
		mt_print_del();
#ifdef USE_NCURSES
		mt_print_fini();
#endif

		if (!flag)
			break;
		else
			step++;
	}

	time_acct_end();

	pthread_attr_destroy(&attr);

	return err;
}
