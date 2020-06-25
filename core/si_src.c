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
#include "si_core.h"

#ifndef CONFIG_SAVED_SRC
#define	SAVED_SRC	"src.saved"
#else
#define	SAVED_SRC	CONFIG_SAVED_SRC
#endif

struct src *si;
static lock_t src_buf_lock;

static void *buf_start = (void *)SRC_BUF_START;
static void *buf_cur = 0;
static void *buf_end = (void *)SRC_BUF_END;
static size_t buf_total_size = 0;
static void buf_restore(void);
static int buf_expanded(size_t expand_len);
void *src_buf_get(size_t len);
static void do_flush_outfile(void);
static int src_modified = 1;

static char cmd0[] = "load_srcfile";
static char cmd1[] = "flush_srcfile";
static char cmd_readmode_name[] = "readmode";
static void cmd0_usage(void);
static long cmd0_cb(int argc, char *argv[]);
static void cmd1_usage(void);
static long cmd1_cb(int argc, char *argv[]);
static void cmd_readmode_usage(void);
static long cmd_readmode_cb(int argc, char *argv[]);

int si_src_setup(void)
{
	int err;
	err = clib_cmd_ac_add(cmd0, cmd0_cb, cmd0_usage);
	if (err == -1) {
		err_msg("clib_cmd_ac_add err");
		return -1;
	}

	err = clib_cmd_ac_add(cmd1, cmd1_cb, cmd1_usage);
	if (err == -1) {
		err_msg("clib_cmd_ac_add err");
		goto out_del0;
	}

	err = clib_cmd_ac_add(cmd_readmode_name, cmd_readmode_cb,
				cmd_readmode_usage);
	if (err == -1) {
		err_msg("clib_cmd_ac_add err");
		goto out_del1;
	}

	if (unlikely(si)) {
		err_msg("si has been assigned");
		goto out_del2;
	}

	buf_cur = buf_start;
	if (buf_expanded(SRC_BUF_BLKSZ)) {
		err_msg("buf_expanded err");
		goto out_del2;
	}

	si = src_buf_get(sizeof(*si));
	memset(si, 0, sizeof(*si));
	INIT_LIST_HEAD(&si->resfile_head);
	INIT_LIST_HEAD(&si->sibuf_head);
	INIT_LIST_HEAD(&si->global_data_states);
	si->next_mmap_area = RESFILE_BUF_START;

	/* TODO, check if the src_id already exists */
	while (!si->src_id[0]) {
		char *tmpid = random_str_nr_en(sizeof(si->src_id)-1);
		if (!tmpid) {
			err_msg("random_str_nr_en err");
			goto err2;
		}

		memcpy(si->src_id, tmpid, sizeof(si->src_id)-1);
		free(tmpid);

		char buf[PATH_MAX];
		snprintf(buf, PATH_MAX, "%s/%s", DEF_TMPDIR, si->src_id);
		err = mkdir(buf, S_IRUSR | S_IWUSR | S_IXUSR);
		if (err == -1) {
			if (errno == EEXIST) {
				si->src_id[0] = 0;
				continue;
			}
			err_sys("mkdir err");
			goto err2;
		}
	}

	err = atexit(do_flush_outfile);
	if (err) {
		err_msg("atexit err");
		goto err2;
	}

	return 0;

err2:
	buf_restore();
	si = NULL;
out_del2:
	clib_cmd_ac_del(cmd_readmode_name);
out_del1:
	clib_cmd_ac_del(cmd1);
out_del0:
	clib_cmd_ac_del(cmd0);
	return -1;
}

static long do_load_srcfile(char *id)
{
	do_flush_outfile();

	char buf[PATH_MAX];
	snprintf(buf, PATH_MAX, "%s/%s/%s", DEF_TMPDIR, id, SAVED_SRC);

	int fd = clib_open(buf, O_RDONLY);
	if (fd == -1) {
		if (errno == ENOENT) {
			err_msg("reset src_id from %s to %s", si->src_id, id);
			size_t l = strlen(id) + 1;
			if (l > (sizeof(si->src_id)))
				l = sizeof(si->src_id);
			memcpy(si->src_id, id, l);
			return 0;
		}
		err_sys("clib_open err");
		return -1;
	}

	struct stat st;
	long err = fstat(fd, &st);
	if (err == -1) {
		err_sys("fstat err");
		close(fd);
		return -1;
	}
	size_t readlen = st.st_size;
	if (unlikely(readlen >= (SRC_BUF_END-SRC_BUF_START)))
		BUG();

	mutex_lock(&src_buf_lock);
	buf_restore();
	BUG_ON(buf_expanded(readlen));

	err = clib_read(fd, buf_start, readlen);
	if (err == -1) {
		err_sys("clib_read err");
		close(fd);
		mutex_unlock(&src_buf_lock);
		return -1;
	}

	si = (struct src *)buf_start;
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
#if 0
	/* There are rwlocks in si, we can not do this now. */
	if (mprotect(buf_start, buf_total_size, PROT_READ)) {
		err_sys("mprotect err");
		src_modified = 1;
	} else {
		src_modified = 0;
	}
#endif
	mutex_unlock(&src_buf_lock);
	return 0;
}
static void cmd0_usage(void)
{
	fprintf(stdout, "\t(srcid)\n");
}
static long cmd0_cb(int argc, char *argv[])
{
	if (argc != 2) {
		err_msg("argc invalid");
		return -1;
	}

	if (!list_empty(&si->resfile_head)) {
		err_msg("si already set");
		return 0;
	}

	long err = do_load_srcfile(argv[1]);
	if (err == -1) {
		err_msg("do_load_srcfile err");
		return -1;
	}

	return 0;
}

static void do_flush_outfile(void)
{
	if (list_empty(&si->resfile_head)) {
		char buf[PATH_MAX];
		snprintf(buf, PATH_MAX, "%s/%s", DEF_TMPDIR, si->src_id);
		rmdir(buf);
		return;
	}

	if (!src_modified)
		return;

	char buf[PATH_MAX];
	snprintf(buf, PATH_MAX, "%s/%s/%s", DEF_TMPDIR, si->src_id, SAVED_SRC);

	int fd = clib_open(buf, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
	if (fd == -1) {
		err_sys("open err");
		return;
	}

	long err;
	err = clib_write(fd, buf_start, buf_cur - buf_start);
	if (err == -1) {
		err_sys("clib_write err");
		close(fd);
		return;
	}

	close(fd);
	return;
}

static void cmd1_usage(void)
{
	fprintf(stdout, "\tWrite current src info to srcfile\n");
}

static long cmd1_cb(int argc, char *argv[])
{
	if (argc != 1) {
		err_msg("argc invalid");
		return -1;
	}

	do_flush_outfile();
	return 0;
}

static void cmd_readmode_usage(void)
{
	fprintf(stdout, "\tManually set read mode\n");
}

static long cmd_readmode_cb(int argc, char *argv[])
{
	fprintf(stdout, "\tPrevious mode: %s\n",
			src_modified ? "Read/Write" : "Read ONLY");
	src_modified = !src_modified;
	fprintf(stdout, "\tCurrent mode: %s\n",
			src_modified ? "Read/Write" : "Read ONLY");
	return 0;
}

static void buf_restore(void)
{
	munmap(buf_start, buf_total_size);
	buf_cur = buf_start;
	buf_total_size = 0;
}

static int buf_expanded(size_t expand_len)
{
	expand_len = clib_round_up(expand_len, SRC_BUF_BLKSZ);
	BUG_ON((buf_start + buf_total_size + expand_len) > buf_end);
	void *addr = mmap(buf_start+buf_total_size,
			  expand_len, PROT_READ | PROT_WRITE,
			  MAP_FIXED | MAP_ANON | MAP_SHARED, -1, 0);
	if (addr == MAP_FAILED) {
		err_sys("mmap err");
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
	if (!src_modified)
		src_modified = 1;
	mutex_unlock(&src_buf_lock);
	return ret;
}

int src_buf_fix(void *fault_addr)
{
	int err = 0;

	mutex_lock(&src_buf_lock);
	if ((fault_addr >= buf_start) &&
		(fault_addr < (buf_start + buf_total_size))) {
		err = mprotect(buf_start, buf_total_size, PROT_READ | PROT_WRITE);
		if (err == -1)
			err_sys("mprotect err");
		else
			src_modified = 1;
	}
	mutex_unlock(&src_buf_lock);

	return err;
}
