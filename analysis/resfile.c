/*
 * for maping resfile into memory, while mmap return ENOMEM, do the GC
 *
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

static struct resfile *resfile_find(char *path, struct slist_head *head)
{
	struct resfile *tmp;
	slist_for_each_entry(tmp, head, sibling) {
		if (strcmp(tmp->path, path) == 0)
			return tmp;
	}
	return NULL;
}

struct resfile *resfile_new(char *path, int built_in, int *exist)
{
	struct resfile *ret;
	*exist = 0;
	ret = resfile_find(path, &si->resfile_head);
	if (ret) {
		*exist = 1;
		return ret;
	}

	size_t pathlen = strlen(path) + 1;
	ret = (struct resfile *)src_buf_get(sizeof(struct resfile) + pathlen);
	if (!ret) {
		err_dbg(0, "src_buf_get err");
		return NULL;
	}
	memset(ret, 0, sizeof(*ret));
	memcpy(ret->path, path, pathlen);

	ret->built_in = built_in;
	ret->fd = -1;
	return ret;
}

void resfile_add(struct resfile *rf)
{
	if (rf->built_in)
		slist_add(&rf->sibling, &si->resfile_head);
	else
		slist_add_tail(&rf->sibling, &si->resfile_head);
}

static void resfile_unload(struct sibuf *buf)
{
	if (!buf->need_unload)
		return;

	unsigned long load_addr = buf->load_addr;
	size_t filelen = buf->total_len;
	unsigned long mmap_addr = clib_round_down(load_addr, PAGE_SIZE);
	size_t mmap_size;
	mmap_size = clib_round_up(load_addr+filelen, PAGE_SIZE) - mmap_addr;
	BUG_ON(munmap((void *)mmap_addr, mmap_size) == -1);
	buf->need_unload = 0;
	sibuf_cleanup_user(buf);
	atomic_sub(buf->total_len, &si->sibuf_mem_usage);
}

static lock_t gc_lock;
static unsigned long usleep_usec = 1000*10;
static int resfile_gc(void)
{
	int err = 0;
	struct sibuf *gc_target = NULL;

	mutex_lock(&gc_lock);
	struct sibuf *tmp;
	slist_for_each_entry(tmp, &si->sibuf_head, sibling) {
		if (!tmp->need_unload)
			continue;

		sibuf_lock_r(tmp);
		if (slist_empty(&tmp->users)) {
			gc_target = tmp;
			resfile_unload(gc_target);
		}
		sibuf_unlock_r(tmp);
	}

	if (!gc_target)
		err = -1;	/* no memory could be released */
	mutex_unlock(&gc_lock);
	return err;
}

static int resfile_load(struct sibuf *buf)
{
	int exists = 0;

	mutex_lock(&gc_lock);
	struct resfile *rf = buf->rf;
	if (unlikely(rf->fd == -1)) {
		char workdir[PATH_MAX];
		if (unlikely(si_current_workdir(workdir, PATH_MAX))) {
			mutex_unlock(&gc_lock);
			return -1;
		}
		if (unlikely(strncmp(rf->path, workdir, strlen(workdir)))) {
			err_dbg(0, "Warning: resfile(%s) not in current "
					"workdir(%s)\n", rf->path, workdir);
			mutex_unlock(&gc_lock);
			return -1;
		}

		rf->fd = open(rf->path, O_RDWR);
		if (rf->fd == -1)
			BUG();
	}

	unsigned long load_addr = buf->load_addr;
	loff_t offs_of_resfile = buf->offs_of_resfile;
	unsigned int filelen = buf->total_len;
	unsigned long mmap_addr = clib_round_down(load_addr, PAGE_SIZE);
	size_t mmap_size;
	char *addr;
	mmap_size = clib_round_up(load_addr+filelen, PAGE_SIZE) - mmap_addr;

	if (buf->need_unload)
		goto fill_buf;

mmap_again0:
	while ((atomic_read(&si->sibuf_mem_usage) + mmap_size) >
			sibuf_loaded_max) {
		mutex_unlock(&gc_lock);
		if (resfile_gc()) {
			err_dbg(0, "resfile_gc() err, "
					"sibuf_mem_usage: 0x%lx, "
					"max: 0x%lx, "
					"needed: 0x%lx\n",
					si->sibuf_mem_usage,
					sibuf_loaded_max,
					mmap_size);
			usleep(usleep_usec);
		}
		mutex_lock(&gc_lock);
	}

	addr = mmap((void *)mmap_addr, mmap_size, PROT_READ | PROT_WRITE,
			MAP_FIXED | MAP_SHARED, rf->fd,
			offs_of_resfile-(load_addr-mmap_addr));
	if (addr == MAP_FAILED) {
		if (errno == ENOMEM) {
			mutex_unlock(&gc_lock);
			if (resfile_gc()) {
				err_dbg(0, "resfile_gc() err, "
						"sibuf_mem_usage: 0x%lx, "
						"max: 0x%lx, "
						"needed: 0x%lx\n",
						si->sibuf_mem_usage,
						sibuf_loaded_max,
						mmap_size);
				usleep(usleep_usec);
			}
			mutex_lock(&gc_lock);
			goto mmap_again0;
		}
		err_dbg(1, "mmap err");
		BUG();
	}
	atomic_add(buf->total_len, &si->sibuf_mem_usage);

fill_buf:
	buf->need_unload = 1;
	exists = sibuf_add_user(buf, pthread_self());
	mutex_unlock(&gc_lock);

	return exists;
}

int sibuf_hold(struct sibuf *buf)
{
	return resfile_load(buf);
}

void sibuf_drop(struct sibuf *buf)
{
	sibuf_del_user(buf, pthread_self());
}

void resfile_unload_all(void)
{
	mutex_lock(&gc_lock);
	struct sibuf *tmp;
	slist_for_each_entry(tmp, &si->sibuf_head, sibling) {
		resfile_unload(tmp);
	}
	atomic_set(&si->sibuf_mem_usage, 0);
	mutex_unlock(&gc_lock);
}

int resfile_preview(struct resfile *rf)
{
	int fd, err;
	fd = rf->fd;
	if (fd == -1) {
		fd = open(rf->path, O_RDWR);
		if (fd == -1) {
			err_dbg(1, "open err");
			return -1;
		}
		rf->fd = fd;
	}

	struct stat st;
	err = fstat(fd, &st);
	if (err == -1) {
		err_dbg(1, "fstat err");
		return -1;
	}

	unsigned long file_offs = 0;
	unsigned long file_cnt = 0;
	unsigned long addr_base = RESFILE_BUF_START, addr_tmp;
	struct resfile *tmp;
	slist_for_each_entry(tmp, &si->resfile_head, sibling) {
		addr_base += clib_round_up(tmp->filelen, PAGE_SIZE);
	}
	addr_tmp = addr_base;

	while (1) {
		void *addr;
		addr = mmap(NULL, MAX_SIZE_PER_FILE, PROT_READ | PROT_WRITE,
				MAP_SHARED, fd, file_offs);
		if (addr == MAP_FAILED) {
			err_dbg(1, "mmap err");
			return -1;
		}

		unsigned int this_total = *(unsigned int *)addr;
		struct file_content *fc = (struct file_content *)addr;
		void *zero = fc_pldptr(addr);

		struct sibuf *b;
		b = sibuf_new();
		b->rf = rf;
		b->load_addr = addr_tmp;
		b->total_len = this_total;
		b->offs_of_resfile = file_offs;
		b->payload = (char *)(zero - addr + addr_tmp);
		b->pldlen = addr + fc->objs_offs - zero;
		b->objs = (struct file_obj *)(addr_tmp + fc->objs_offs);
		b->obj_cnt = fc->objs_cnt;

		struct file_obj *objs;
		objs = (struct file_obj *)(addr + fc->objs_offs);
		for (size_t i = 0; i < b->obj_cnt; i++) {
			if (objs[i].real_addr)
				continue;
			if (!i)
				objs[i].offs = 0;
			else
				objs[i].offs = objs[i-1].offs + objs[i-1].size;
			objs[i].real_addr = (unsigned long)b->payload +
						objs[i].offs;
		}
		sibuf_insert(b);

		err = munmap(addr, MAX_SIZE_PER_FILE);
		if (err == -1) {
			err_dbg(1, "munmap err");
			return -1;
		}

		file_cnt++;
		file_offs += this_total;
		addr_tmp += this_total;
		if (file_offs == st.st_size)
			break;
		if (file_offs > st.st_size) {
			err_dbg(0, "resfile file size not right.");
			return -1;
		}
	}

	rf->filelen = st.st_size;
	rf->total_files = file_cnt;

	return 0;
}

static char tmp_read_buf[MAX_SIZE_PER_FILE];
int resfile_get_offset(char *path, unsigned long filecnt, unsigned long *offs)
{
	int fd, err = 0;
	unsigned long i = 0;
	unsigned long _offs = 0;
	*offs = _offs;

	if (!filecnt)
		return 0;

	fd = open(path, O_RDONLY);
	if (fd == -1) {
		err_dbg(1, "open err");
		return -1;
	}

	while (filecnt) {
		unsigned int *this_total = (unsigned int *)tmp_read_buf;
		err = read(fd, this_total, sizeof(unsigned int));
		if (err == -1) {
			err_dbg(1, "read err");
			close(fd);
			return -1;
		} else if (!err)
			break;

		err = read(fd, tmp_read_buf+sizeof(unsigned int),
				*this_total-sizeof(unsigned int));
		if (err == -1) {
			err_dbg(1, "read err");
			close(fd);
			return -1;
		} else if (!err) {
			err_dbg(0, "resfile format err");
			close(fd);
			return -1;
		} else if (err != (*this_total-sizeof(unsigned int))) {
			err_dbg(0, "resfile format err");
			close(fd);
			return -1;
		}
		_offs += *this_total;
		filecnt--;
		i++;
	}

	if (filecnt)
		err_dbg(0, "given filecnt too large, total is %d", i);
	*offs = _offs;
	close(fd);
	return 0;
}

struct file_content *resfile_get_fc(char *path, char *targetfile, int *idx)
{
	int fd;
	int err = 0;
	fd = open(path, O_RDONLY);
	if (fd == -1) {
		err_dbg(1, "open err");
		return NULL;
	}

	*idx = 0;
	while (1) {
		unsigned int *this_total = (unsigned int *)tmp_read_buf;
		err = read(fd, this_total, sizeof(unsigned int));
		if (err == -1) {
			err_dbg(1, "read err");
			close(fd);
			return NULL;
		} else if (!err)
			break;

		err = read(fd, tmp_read_buf+sizeof(unsigned int),
				*this_total-sizeof(unsigned int));
		if (err == -1) {
			err_dbg(1, "read err");
			close(fd);
			return NULL;
		} else if (!err) {
			err_dbg(0, "resfile format err");
			close(fd);
			return NULL;
		} else if (err != (*this_total-sizeof(unsigned int))) {
			err_dbg(0, "resfile format err");
			close(fd);
			return NULL;
		}

		struct file_content *fc;
		fc = (struct file_content *)tmp_read_buf;
		if (is_same_path(fc->path, targetfile)) {
			close(fd);
			return fc;
		}
		*idx = *idx + 1;
	}

	close(fd);
	return NULL;
}
