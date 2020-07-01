/*
 * TODO
 *
 * Copyright (C) 2020 zerons
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
#include "si_core.h"
#include "./analysis.h"

LIST_HEAD(analysis_lang_ops_head);
lock_t getbase_lock;

static int parse_sibuf(struct sibuf *buf, int step, int force)
{
	int err = 0;
	if (step > STEP1)
		analysis__resfile_load(buf);

	struct file_content *fc;
	fc = (struct file_content *)buf->load_addr;
	struct lang_ops *ops;
	ops = lang_ops_find(&analysis_lang_ops_head, &fc->type);
	if (!ops) {
		err_dbg(0, "lang_ops TYPE: %d not found", fc->type);
		return -1;
	}

	if (unlikely((!force) && (buf->status != (step - 1))))
		return 0;

	si_lock_w();
	if (unlikely(si->type.kernel != fc->type.kernel)) {
		si->type.kernel = fc->type.kernel;
		si->type.os_type = fc->type.os_type;
	}
	si_unlock_w();

	int dbgmode = get_dbg_mode();
	if (dbgmode)
		disable_dbg_mode();
	err = ops->parse(buf, step);
	if (dbgmode)
		enable_dbg_mode();
	if (unlikely(clib_dbg_func_check())) {
		si_log1("CLIB_DBG_FUNC_ENTER/CLIB_DBG_FUNC_EXIT not paired\n");
	}
	if (err) {
		err_dbg(0, "fc->type parse err");
		return -1;
	}

	return 0;
}

/* XXX: use multiple threads to parse the file, core*3 */
#ifndef CONFIG_ANALYSIS_THREAD
#define	THREAD_CNT	0x18
#else
#define THREAD_CNT	(CONFIG_ANALYSIS_THREAD)
#endif

#ifndef CONFIG_THREAD_STACKSZ
#define	THREAD_STACKSZ	(1024*1024*0x10)
#else
#define	THREAD_STACKSZ	(CONFIG_THREAD_STACKSZ)
#endif

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

	int err = parse_sibuf(t->buf, step, 0);
	if (err) {
		err_dbg(0, "parse_sibuf err");
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
		if (bufs[i].tid) {
			pthread_join(bufs[i].tid, NULL);
			bufs[i].tid = 0;
		}
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

int parse_resfile(char *path, int built_in, int step, int autoy)
{
	int err = 0;
	int flag = 0;
	struct resfile *newrf;

	if (step == 0) {
		flag = 1;
		step = STEP1;
	}

	newrf = analysis__resfile_new(path, built_in);
	if (!newrf) {
		err_dbg(0, "resfile_new err");
		return -1;
	}
	analysis__resfile_add(newrf);

	fprintf(stdout, "[....] take a preview of the resfile\r");
	fflush(stdout);
	err = analysis__resfile_get_filecnt(newrf);
	if (err == -1) {
		err_dbg(0, "analysis__resfile_get_filecnt err");
		return -1;
	}
	fprintf(stdout, "[done]\n");
	fflush(stdout);

	if (step == STEP1) {
		/*
		 * we only need to do the backup for STEP1, because the future
		 * step will not modify the resfile.
		 */
		char b[PATH_MAX];
		struct stat st;
		snprintf(b, PATH_MAX, "%s.0", path);
		err = stat(b, &st);
		if (err == -1) {
			if (errno == ENOENT) {
				char *line;
				int ch;
				if (!autoy) {
					line = readline("resfile is new, "
							"backup?(Y/N)");
					ch = *line;
				} else {
					ch = 'Y';
				}
				if ((ch != 'N') && (ch != 'n'))
					clib_copy_file(path, b, 0);
			} else {
				err_dbg(1, "stat err");
			}
		}
	}

	parsed_files = newrf->parsed_files;

	pthread_attr_t attr;
	err = pthread_attr_init(&attr);
	if (err) {
		err_dbg(0, "pthread_attr_init err");
		return -1;
	}
	err = pthread_attr_setstacksize(&attr, THREAD_STACKSZ);
	if (err) {
		err_dbg(0, "pthread_attr_setstacksize err");
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
			break;
		}

		memset(&bufs, 0, sizeof(bufs));
		sigjmp_lbl = step;
		file_progress_arg[0] = (long)pthread_self();
		file_progress_arg[1] = (long)&newrf->parsed_files[step];
		file_progress_arg[2] = (long)&newrf->total_files;
		file_progress_arg[3] = (long)status_str;
#ifdef USE_NCURSES
		mt_print_init_ncurse();
#endif
		mt_print_add();
		mt_add_timer(1, show_file_progress, file_progress_arg, 0, 1);

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
					/*
					 * jump out this loop
					 * wait for all threads
					 */
					err_dbg(0, "receive SIGQUIT signal, "
							"quit parsing");
					break;
				}

				struct mt_parse *t = mt_parse_find0();
				if (!t) {
					sleep(1);
					continue;
				}
				if (t->ret) {
					err_dbg(0, "do_phase ret err");
					err = -1;
					break;
				}

				err = analysis__resfile_read(newrf,
								t->buf, force);
				if (force)
					force = 0;
				if (err == -1) {
					err_dbg(0,
						  "analysis__resfile_read err");
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

				if (t->tid) {
					pthread_join(t->tid, NULL);
					t->tid = 0;
				}
redo1:
				err = pthread_create(&t->tid, &attr,
							do_phase, t);
				if (err) {
					err_dbg(0, "pthread_create err");
					sleep(1);
					goto redo1;
				}
				analysis__sibuf_insert(t->buf);
			}

			analysis__resfile_unload_all();

			break;
		}
		case STEP2:
		case STEP3:
		case STEP4:
		case STEP5:
		case STEP6:
		{
			struct sibuf *tmp;
			list_for_each_entry_reverse(tmp, &si->sibuf_head,
							sibling) {
				if (tmp->rf != newrf)
					continue;

				if (unlikely(!parse_sig_set)) {
					parse_sig_set = 1;
					signal(SIGQUIT, sigquit_hdl);
				}
				if (sigsetjmp(parse_jmp_env, sigjmp_lbl)) {
					/*
					 * jump out this loop
					 * wait for all threads
					 */
					err_dbg(0, "receive SIGQUIT signal, "
							"quit parsing");
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
					err_dbg(0, "do_phase ret err");
					err = -1;
					break;
				}

				t->buf = tmp;
				atomic_set(&t->in_use, 1);
				t->step = step;

				if (t->tid) {
					pthread_join(t->tid, NULL);
					t->tid = 0;
				}
redo2:
				err = pthread_create(&t->tid, &attr,
							do_phase, t);
				if (err) {
					err_dbg(0, "pthread_create err");
					sleep(1);
					goto redo2;
				}
			}

			break;
		}
		default:
		{
			break;
		}
		}

		wait_for_all_threads();

		mt_del_timer(0);
		mt_print_del();
#ifdef USE_NCURSES
		mt_print_fini_ncurse();
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

int parse_sibuf_bypath(char *srcpath, int step, int force)
{
	struct sibuf *tmp;
	list_for_each_entry_reverse(tmp, &si->sibuf_head, sibling) {
		analysis__resfile_load(tmp);

		struct file_content *fc;
		fc = (struct file_content *)tmp->load_addr;
		if (!is_same_path(fc->path, srcpath))
			continue;

		return parse_sibuf(tmp, step, force);
	}

	return 0;
}
