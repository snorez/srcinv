/*
 * for maping resfile into memory, while mmap return ENOMEM, do the GC
 * GC:
 *	iter sibuf, check the last access_at, sort, unload the olddest sibuf, return
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

static struct resfile *resfile_find(char *path, struct list_head *head)
{
	struct resfile *tmp;
	list_for_each_entry(tmp, head, sibling) {
		if (strcmp(tmp->path, path) == 0)
			return tmp;
	}
	return NULL;
}

struct resfile *resfile_new(char *path, int built_in)
{
	struct resfile *ret;
	ret = resfile_find(path, &si->resfile_head);
	if (ret)
		return ret;

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
	if (resfile_find(rf->path, &si->resfile_head))
		return;

	if (rf->built_in)
		list_add(&rf->sibling, &si->resfile_head);
	else
		list_add_tail(&rf->sibling, &si->resfile_head);
}

static void prepare_unmap(void *addr, size_t len)
{
	struct sibuf *tmp;
	list_for_each_entry(tmp, &si->sibuf_head, sibling) {
		if ((tmp->load_addr >= (unsigned long)addr) &&
			(tmp->load_addr < (unsigned long)(addr+len))) {
			tmp->need_unload = 0;
		}
	}
}

/*
 * fill up a sibuf structure
 * return val:
 *	0: resfile read finished
 *	1: resfile read done, reread again
 *	-1: err
 */
int resfile_read(struct resfile *rf, struct sibuf *buf, int force)
{
	int err = 0;
	int fd = rf->fd;
	if (unlikely(fd == -1)) {
		fd = open(rf->path, O_RDWR);
		if (fd == -1) {
			err_dbg(1, "open err");
			return -1;
		}
		rf->fd = fd;
	}

	/*
	 * three situations:
	 *	resfile not mmap-ed
	 *	resfile mmap some
	 *	resfile mmap all
	 */
	if (unlikely(!rf->buf_start)) {
		struct stat st;
		void *addr;
		err = fstat(fd, &st);
		if (err == -1) {
			err_dbg(1, "fstat err");
			return -1;
		}
		rf->filelen = st.st_size;

		size_t mmaplen = rf->filelen;
		if (mmaplen > RESFILE_BUF_SIZE)
			mmaplen = RESFILE_BUF_SIZE;

		si->next_mmap_area = clib_round_up(si->next_mmap_area,
							PAGE_SIZE);
		mmaplen = clib_round_up(mmaplen, PAGE_SIZE);
mmap_again0:
		addr = mmap((void *)si->next_mmap_area, mmaplen,
					PROT_READ | PROT_WRITE,
					MAP_FIXED | MAP_SHARED, fd, 0);
		if (addr == MAP_FAILED) {
			if (errno == ENOMEM) {
				resfile_gc();
				goto mmap_again0;
			}
			err_dbg(1, "mmap err");
			return -1;
		}

		rf->buf_start = (unsigned long)addr;
		rf->buf_size = mmaplen;
		si->next_mmap_area += mmaplen;
	}

	if (rf->file_offs == rf->filelen)
		return 0;

	/* TODO, what if start is just at rf->buf_start + rf->buf_size */
	char *start = ((char *)rf->buf_start) + rf->buf_offs;
	unsigned int *this_total = (unsigned int *)start;
	if ((*this_total + rf->file_offs) > rf->filelen)
		BUG();

	if ((start + *this_total) > (char *)(rf->buf_start + rf->buf_size)) {
		if (!force)
			return -EAGAIN;

		/*
		 * XXX, need remap
		 * we try to keep the file is continous in memory
		 */
		void *addr;
		unsigned long unmap_addr = rf->buf_start;
		unsigned long unmap_end =
			clib_round_down(rf->buf_start + rf->buf_offs, PAGE_SIZE);
		prepare_unmap((void *)unmap_addr, unmap_end - unmap_addr);
		err = munmap((void *)unmap_addr, unmap_end - unmap_addr);
		if (err == -1) {
			err_dbg(1, "munmap err");
			return -1;
		}

		unsigned long mmap_fileoffset = rf->file_offs +
						(rf->buf_size-rf->buf_offs);
		size_t mmaplen = rf->filelen - mmap_fileoffset;
		if (mmaplen > RESFILE_BUF_SIZE)
			mmaplen = RESFILE_BUF_SIZE;
		si->next_mmap_area = clib_round_up(si->next_mmap_area,
							PAGE_SIZE);
		mmaplen = clib_round_up(mmaplen, PAGE_SIZE);
mmap_again1:
		addr = mmap((void *)si->next_mmap_area, mmaplen,
				PROT_READ | PROT_WRITE,
				MAP_FIXED | MAP_SHARED, fd, mmap_fileoffset);
		if (addr == MAP_FAILED) {
			if (errno == ENOMEM) {
				resfile_gc();
				goto mmap_again1;
			}
			err_dbg(1, "mmap err");
			return -1;
		}

		rf->buf_start = (unsigned long)unmap_end;
#if 0
		rf->buf_size = mmaplen + (unmap_end - unmap_addr);
		rf->buf_offs = rf->buf_offs-(unmap_end-unmap_addr);
#endif
		rf->buf_size = mmaplen + (rf->buf_size - (unmap_end - unmap_addr));
		rf->buf_offs = 0;
		si->next_mmap_area += mmaplen;

		start = ((char *)rf->buf_start) + rf->buf_offs;
		this_total = (unsigned int *)start;
	}

	memset(buf, 0, sizeof(*buf));
	struct file_context *fc = (struct file_context *)start;
	void *zero = file_context_payload_position(start);

	buf->rf = rf;
	buf->load_addr = (unsigned long)start;
	buf->total_len = *this_total;
	buf->offs_of_resfile = rf->file_offs;
	buf->payload = zero;
	buf->pldlen = (start + fc->objs_offs) - (char *)zero;
	buf->objs = (struct file_obj *)(start + fc->objs_offs);
	buf->obj_cnt = fc->objs_cnt;
	BUG_ON(gettimeofday(&buf->access_at, NULL));
	struct file_obj *objs = buf->objs;
	for (size_t i = 0; i < buf->obj_cnt; i++) {
		if (objs[i].real_addr)
			continue;
		if (!i)
			objs[i].offs = 0;
		else
			objs[i].offs = objs[i-1].offs + objs[i-1].size;
		objs[i].real_addr = (unsigned long)buf->payload + objs[i].offs;
	}

	rf->buf_offs += *this_total;
	rf->file_offs += *this_total;

	return 1;
}

static lock_t gc_lock;
void resfile_load(struct sibuf *buf)
{
	mutex_lock(&gc_lock);
	struct resfile *rf = buf->rf;
	if (rf->fd == -1) {
		rf->fd = open(rf->path, O_RDWR);
		if (rf->fd == -1)
			BUG();
	}

	unsigned long load_addr = buf->load_addr;
	loff_t offs_of_resfile = buf->offs_of_resfile;
	unsigned int filelen = buf->total_len;
	unsigned long mmap_addr = clib_round_down(load_addr, PAGE_SIZE);
	size_t mmap_size = clib_round_up(load_addr+filelen, PAGE_SIZE) - mmap_addr;

	if (buf->need_unload)
		goto fill_buf;
	/* TODO, race condition */
	if ((mmap_addr >= si->next_mmap_area) &&
		((mmap_addr+mmap_size) <= (si->next_mmap_area + RESFILE_BUF_SIZE)))
		goto fill_buf;

	if ((mmap_addr < si->next_mmap_area) &&
			((mmap_addr+mmap_size) > (si->next_mmap_area)))
		BUG();
	if ((mmap_addr < (si->next_mmap_area + RESFILE_BUF_SIZE)) &&
		((mmap_addr+mmap_size) > (si->next_mmap_area+RESFILE_BUF_SIZE)))
		BUG();

	char *addr;
mmap_again0:
	while ((atomic_read(&si->sibuf_mem_usage) + mmap_size) >
			SIBUF_LOADED_MAX) {
		mutex_unlock(&gc_lock);
		resfile_gc();
		mutex_lock(&gc_lock);
	}

	addr = mmap((void *)mmap_addr, mmap_size, PROT_READ | PROT_WRITE,
			MAP_FIXED | MAP_SHARED, rf->fd,
			offs_of_resfile-(load_addr-mmap_addr));
	if (addr == MAP_FAILED) {
		if (errno == ENOMEM) {
			mutex_unlock(&gc_lock);
			resfile_gc();
			mutex_lock(&gc_lock);
			goto mmap_again0;
		}
		err_dbg(1, "mmap err");
		BUG();
	}
	atomic_add(buf->total_len, &si->sibuf_mem_usage);

fill_buf:
	buf->need_unload = 1;
	BUG_ON(gettimeofday(&buf->access_at, NULL));
	mutex_unlock(&gc_lock);
}

void resfile_unload(struct sibuf *buf)
{
	if (!buf->need_unload)
		return;

	unsigned long load_addr = buf->load_addr;
	size_t filelen = buf->total_len;
	unsigned long mmap_addr = clib_round_down(load_addr, PAGE_SIZE);
	size_t mmap_size = clib_round_up(load_addr+filelen, PAGE_SIZE) - mmap_addr;
	BUG_ON(munmap((void *)mmap_addr, mmap_size) == -1);
	buf->need_unload = 0;
	atomic_sub(buf->total_len, &si->sibuf_mem_usage);
}

void resfile_unload_all(void)
{
	mutex_lock(&gc_lock);
	struct sibuf *tmp;
	list_for_each_entry(tmp, &si->sibuf_head, sibling) {
		analysis__resfile_unload(tmp);
	}
	atomic_set(&si->sibuf_mem_usage, 0);
	mutex_unlock(&gc_lock);
}

/* XXX, this should be called very carefully, in case race condition happens */
int resfile_gc(void)
{
	int err = 0;
	struct timeval tv_old;
	struct sibuf *gc_target = NULL;
	memset(&tv_old, 0, sizeof(tv_old));

	mutex_lock(&gc_lock);
	struct sibuf *tmp;
	list_for_each_entry(tmp, &si->sibuf_head, sibling) {
		if (!tmp->need_unload)
			continue;
		if (!tv_old.tv_sec) {
			gc_target = tmp;
			tv_old = tmp->access_at;
		}
		if (tv_old.tv_sec < tmp->access_at.tv_sec) {
			continue;
		} else if ((tv_old.tv_sec > tmp->access_at.tv_sec) ||
				(tv_old.tv_usec > tmp->access_at.tv_usec)) {
			gc_target = tmp;
			tv_old = tmp->access_at;
		}
	}

	if (!gc_target)
		err = -1;	/* no memory could be collect */
	else
		analysis__resfile_unload(gc_target);
	mutex_unlock(&gc_lock);
	return err;
}

static char tmp_read_buf[MAX_SIZE_PER_FILE];
int resfile_get_filecnt(struct resfile *rf, int *is_new)
{
	*is_new = 1;
	int fd, err;
	fd = rf->fd;
	if (fd == -1) {
		fd = open(rf->path, O_RDONLY);
		if (fd == -1) {
			err_dbg(1, "open err");
			return -1;
		}
	}

	unsigned long *cnt = &rf->total_files;
	if (!*cnt) {
		while (1) {
			unsigned int *this_total = (unsigned int *)tmp_read_buf;
			err = read(fd, this_total, sizeof(unsigned int));
			if (err == -1) {
				err_dbg(1, "read err");
				close(fd);
				rf->fd = -1;
				return -1;
			} else if (!err)
				break;

			err = read(fd, tmp_read_buf+sizeof(unsigned int),
					*this_total-sizeof(unsigned int));
			if (err == -1) {
				err_dbg(1, "read err");
				close(fd);
				rf->fd = -1;
				return -1;
			} else if (!err) {
				err_dbg(0, "resfile format err");
				close(fd);
				rf->fd = -1;
				return -1;
			} else if (err != (*this_total-sizeof(unsigned int))) {
				err_dbg(0, "resfile format err");
				close(fd);
				rf->fd = -1;
				return -1;
			}

			struct file_context *fc;
			fc = (struct file_context *)tmp_read_buf;
			if (fc->status != FC_STATUS_NONE)
				*is_new = 0;

			*cnt += 1;
		}
	} else {
		*is_new = 0;
	}

	close(fd);
	rf->fd = -1;

	return 0;
}

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

struct file_context *resfile_get_filecontext(char *path, char *targetfile)
{
	int fd;
	int err = 0;
	fd = open(path, O_RDONLY);
	if (fd == -1) {
		err_dbg(1, "open err");
		return NULL;
	}

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

		struct file_context *fc;
		fc = (struct file_context *)tmp_read_buf;
		if (!strcmp(fc->path, targetfile)) {
			close(fd);
			return fc;
		}
	}

	close(fd);
	return NULL;
}
