/*
 * this is for struct src
 * when process exit, make sure all buf write to output folder
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

struct src *si;
char src_outfile[NAME_MAX] = "srcoutput";
static lock_t src_buf_lock;

static void *buf_start = (void *)SRC_BUF_START;
static void *buf_cur = 0;
static void *buf_end = (void *)SRC_BUF_END;
static size_t buf_total_size = 0;
static int buf_expanded(size_t expand_len);
void *src_buf_get(size_t len);
static void do_flush_outfile(void);
static __maybe_unused void src_eh_do_flush(int signo, siginfo_t *si, void *arg);

static char cmd0[] = "load_srcfile";
static char cmd1[] = "set_srcfile";
static char cmd2[] = "flush_srcfile";
static void cmd0_usage(void);
static long cmd0_cb(int argc, char *argv[]);
static void cmd1_usage(void);
static long cmd1_cb(int argc, char *argv[]);
static void cmd2_usage(void);
static long cmd2_cb(int argc, char *argv[]);
static struct clib_cmd _cmd0 = {
	.cmd = cmd0,
	.cb = cmd0_cb,
	.usage = cmd0_usage,
};
static struct clib_cmd _cmd1 = {
	.cmd = cmd1,
	.cb = cmd1_cb,
	.usage = cmd1_usage,
};
static struct clib_cmd _cmd2 = {
	.cmd = cmd2,
	.cb = cmd2_cb,
	.usage = cmd2_usage,
};

CLIB_PLUGIN_NAME(src);
CLIB_PLUGIN_NEEDED0();
CLIB_PLUGIN_INIT()
{
#if 0
	struct eh_list *new_eh;
	new_eh = eh_list_new(src_eh_do_flush);
	set_eh(new_eh);
#endif

	int logfd = open(DEFAULT_LOG_FILE, O_RDWR | O_CREAT | O_TRUNC,
				S_IRUSR | S_IWUSR);
	if (logfd == -1) {
		err_dbg(0, err_fmt("open logfile err"));
		return -1;
	}
	close(logfd);

	buf_cur = buf_start;
	if (buf_expanded(SRC_BUF_BLKSZ)) {
		err_dbg(0, err_fmt("buf_expanded err"));
		return -1;
	}

	si = src_buf_get(sizeof(*si));
	memset(si, 0, sizeof(*si));
	INIT_LIST_HEAD(&si->resfile_head);
	INIT_LIST_HEAD(&si->sibuf_head);
	INIT_LIST_HEAD(&si->attr_name_head);
	si->next_mmap_area = RESFILE_BUF_START;

	int err = atexit(do_flush_outfile);
	if (err) {
		err_dbg(1, err_fmt("atexit err"));
		return -1;
	}

	err = clib_cmd_add(&_cmd0);
	if (err == -1) {
		err_dbg(0, err_fmt("clib_cmd_add err"));
		return -1;
	}

	err = clib_cmd_add(&_cmd1);
	if (err == -1) {
		clib_cmd_del(_cmd0.cmd);
		err_dbg(0, err_fmt("clib_cmd_add err"));
		return -1;
	}

	err = clib_cmd_add(&_cmd2);
	if (err == -1) {
		clib_cmd_del(_cmd0.cmd);
		clib_cmd_del(_cmd1.cmd);
		err_dbg(0, err_fmt("clib_cmd_add err"));
		return -1;
	}

	return 0;
}

CLIB_PLUGIN_EXIT()
{
	clib_cmd_del(_cmd0.cmd);
	clib_cmd_del(_cmd1.cmd);
	clib_cmd_del(_cmd2.cmd);
	BUG();				/* this should never be unloaded */
	return;
}

static long do_load_srcfile(char *file)
{
	char buf[strlen(DEFAULT_MIDOUT_DIR)+strlen(file)+3];
	memcpy(buf, DEFAULT_MIDOUT_DIR, strlen(DEFAULT_MIDOUT_DIR)+1);
	if (buf[strlen(buf)-1] != '/')
		memcpy(buf+strlen(buf), "/", 2);
	memcpy(buf+strlen(buf), file, strlen(file)+1);

	int fd = clib_open(buf, O_RDONLY);
	if (fd == -1) {
		err_dbg(1, err_fmt("clib_open err"));
		return -1;
	}

	struct stat st;
	long err = fstat(fd, &st);
	if (err == -1) {
		err_dbg(1, err_fmt("fstat err"));
		close(fd);
		return -1;
	}
	size_t readlen = st.st_size;
	if (unlikely(readlen >= (SRC_BUF_END-SRC_BUF_START)))
		BUG();

	BUG_ON(buf_expanded(readlen));
	err = clib_read(fd, buf_start, readlen);
	if (err == -1) {
		err_dbg(1, err_fmt("clib_read err"));
		close(fd);
		return -1;
	}

	buf_cur = buf_start+readlen;
	si->next_mmap_area = RESFILE_BUF_START;

	struct resfile *tmp0;
	list_for_each_entry(tmp0, &si->resfile_head, sibling) {
		tmp0->fd = -1;
		tmp0->file_offs = 0;
		tmp0->buf_start = 0;
		tmp0->buf_offs = 0;
	}

	struct sibuf *tmp1;
	list_for_each_entry(tmp1, &si->sibuf_head, sibling) {
		tmp1->need_unload = 0;
		si->next_mmap_area += tmp1->total_len;
	}
	atomic_set(&si->sibuf_mem_usage, 0);

	close(fd);
	return 0;
}
static void cmd0_usage(void)
{
	fprintf(stdout, "\t[srcfile]\n");
}
static long cmd0_cb(int argc, char *argv[])
{
	/* XXX, check current si status first */
	/* TODO, race condition here */
	if (!list_empty(&si->resfile_head)) {
		err_dbg(0, err_fmt("too late, si already modified"));
		return -1;
	}

	char *srcfile = src_outfile;
	if (argc == 2)
		srcfile = argv[1];

	long err = do_load_srcfile(srcfile);
	if (err == -1) {
		err_dbg(0, err_fmt("do_load_srcfile err"));
		return -1;
	}

	return 0;
}

static void cmd1_usage(void)
{
	fprintf(stdout, "\t(srcfile_name)\n");
}
static long cmd1_cb(int argc, char *argv[])
{
	if (argc != 2) {
		err_dbg(0, err_fmt("argc invalid"));
		return -1;
	}

	memset(src_outfile, 0, NAME_MAX);
	memcpy(src_outfile, argv[1], strlen(argv[1]));
	return 0;
}

static void do_flush_outfile(void)
{
	if (list_empty(&si->resfile_head))
		return;
	char buf[strlen(DEFAULT_MIDOUT_DIR)+strlen(src_outfile)+3];
	memcpy(buf, DEFAULT_MIDOUT_DIR, strlen(DEFAULT_MIDOUT_DIR)+1);
	if (buf[strlen(buf)-1] != '/')
		memcpy(buf+strlen(buf), "/", 2);
	memcpy(buf+strlen(buf), src_outfile, strlen(src_outfile)+1);

	int fd = clib_open(buf, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
	if (fd == -1) {
		err_dbg(1, err_fmt("open err"));
		return;
	}

	long err;
	err = clib_write(fd, buf_start, buf_cur - buf_start);
	if (err == -1) {
		err_dbg(1, err_fmt("clib_write err"));
		close(fd);
		return;
	}

	close(fd);
	return;
}

static void src_eh_do_flush(int signo, siginfo_t *si, void *arg)
{
	do_flush_outfile();
}

static void cmd2_usage(void)
{
	fprintf(stdout, "\tWrite current src info to srcfile\n");
}

static long cmd2_cb(int argc, char *argv[])
{
	if (argc != 1) {
		err_dbg(0, err_fmt("argc invalid"));
		return -1;
	}

	do_flush_outfile();
	return 0;
}

static int buf_expanded(size_t expand_len)
{
	expand_len = clib_round_up(expand_len, SRC_BUF_BLKSZ);
	BUG_ON((buf_start + buf_total_size + expand_len) > buf_end);
	void *addr = mmap(buf_start+buf_total_size,
			  expand_len, PROT_READ | PROT_WRITE,
			  MAP_FIXED | MAP_ANON | MAP_SHARED, -1, 0);
	if (addr == MAP_FAILED) {
		err_dbg(1, err_fmt("mmap err"));
		return -1;
	}

	buf_total_size += expand_len;
	return 0;
}

void *src_buf_get(size_t len)
{
	mutex_lock(&src_buf_lock);
	if ((len + buf_cur) > (buf_start + buf_total_size)) {
		BUG_ON(buf_expanded(SRC_BUF_BLKSZ));
	}
	BUG_ON((len + buf_cur) > (buf_start + buf_total_size));
	void *ret = (void *)(buf_cur);
	buf_cur += len;
	/* XXX: important, aligned 0x4 */
	buf_cur = (void *)clib_round_up((unsigned long)buf_cur, sizeof(long));
	mutex_unlock(&src_buf_lock);
	return ret;
}
