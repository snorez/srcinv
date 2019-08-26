/*
 * TODO
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
#ifndef SI_DECLS_H_HEGJI43K
#define SI_DECLS_H_HEGJI43K

#include "si_core.h"

DECL_BEGIN

C_SYM struct src *si;

C_SYM void *src_buf_get(size_t len);
C_SYM void si_module_init(struct si_module *p);
C_SYM int si_module_str_to_category(int *category, char *string);
C_SYM int si_module_str_to_type(struct si_type *type, char *string);
C_SYM int si_module_get_abs_path(char *buf, size_t len, int category, char *path);
C_SYM struct list_head *si_module_get_head(int category);
C_SYM struct si_module *si_module_find_by_name(char *name, struct list_head *head);
C_SYM struct si_module **si_module_find_by_type(struct si_type *type,
						struct list_head *head);
C_SYM int si_module_add(struct si_module *p);
C_SYM int si_module_load_all(struct list_head *head);
C_SYM int si_module_unload_all(struct list_head *head);
C_SYM void si_module_cleanup(void);

#undef PLUGIN_SYMBOL_CONFLICT
CLIB_MODULE_CALL_FUNC(analysis, sinode_new, struct sinode *,
		(enum sinode_type type, char *name, size_t namelen,
		 char *data, size_t datalen),
		5, type, name, namelen, data, datalen);
CLIB_MODULE_CALL_FUNC(analysis, sinode_insert, int,
		(struct sinode *node, int behavior),
		2, node, behavior);
CLIB_MODULE_CALL_FUNC(analysis, sinode_search, struct sinode *,
		(enum sinode_type type, int flag, void *arg),
		3, type, flag, arg);
CLIB_MODULE_CALL_FUNC(analysis, sinode_iter, void,
		(struct rb_node *node, void (*cb)(struct rb_node *n)),
		2, node, cb);
CLIB_MODULE_CALL_FUNC0(analysis, sibuf_new, struct sibuf *);
CLIB_MODULE_CALL_FUNC(analysis, sibuf_insert, void,
		(struct sibuf *b),
		1, b);
CLIB_MODULE_CALL_FUNC(analysis, sibuf_remove, void,
		(struct sibuf *b),
		1, b);
CLIB_MODULE_CALL_FUNC(analysis, sibuf_type_node_insert, int,
		(struct sibuf *b, struct sibuf_type_node *stn),
		2, b, stn);
CLIB_MODULE_CALL_FUNC(analysis, sibuf_type_node_search, struct type_node *,
		(struct sibuf *b, int tc, void *addr),
		3, b, tc, addr);
CLIB_MODULE_CALL_FUNC(analysis, resfile_new, struct resfile *,
		(char *path, int built_in),
		2, path, built_in);
CLIB_MODULE_CALL_FUNC(analysis, resfile_add, void,
		(struct resfile *rf),
		1, rf);
CLIB_MODULE_CALL_FUNC(analysis, resfile_read, int,
		(struct resfile *rf, struct sibuf *buf, int force),
		3, rf, buf, force);
CLIB_MODULE_CALL_FUNC(analysis, resfile_load, void,
		(struct sibuf *buf),
		1, buf);
CLIB_MODULE_CALL_FUNC(analysis, resfile_unload, void,
		(struct sibuf *buf),
		1, buf);
CLIB_MODULE_CALL_FUNC0(analysis, resfile_gc, int);
CLIB_MODULE_CALL_FUNC0(analysis, resfile_unload_all, void);
CLIB_MODULE_CALL_FUNC(analysis, resfile_get_filecnt, int,
		(struct resfile *rf, int *is_new),
		2, rf, is_new);
CLIB_MODULE_CALL_FUNC(analysis, get_func_code_paths_start, void,
		(struct code_path *codes),
		1, codes);
CLIB_MODULE_CALL_FUNC0(analysis, get_func_next_code_path, struct code_path *);
CLIB_MODULE_CALL_FUNC(analysis, trace_var, int,
		(struct sinode *fsn, void *var_parm,
		 struct sinode **target_fsn, void **target_vn),
		4, fsn, var_parm, target_fsn, target_vn);
CLIB_MODULE_CALL_FUNC(analysis, gen_func_paths, void,
		(struct sinode *from, struct sinode *to,
		 struct list_head *head, int idx),
		4, from, to, head, idx);
CLIB_MODULE_CALL_FUNC(analysis, drop_func_paths, void,
		(struct list_head *head),
		1, head);
/* XXX, for now, we only handle max to FUNC_CP_MAX paths */
#define FUNC_CP_MAX	0x100000
CLIB_MODULE_CALL_FUNC(analysis, gen_code_paths, void,
		(void *arg, struct clib_rw_pool *pool),
		2, arg, pool);
CLIB_MODULE_CALL_FUNC(analysis, drop_code_path, void,
		(struct path_list_head *head),
		1, head);
CLIB_MODULE_CALL_FUNC(hacking, debuild_type, void,
		(struct type_node *type),
		1, type);

/*
 * ************************************************************************
 * inline functions
 * ************************************************************************
 */
static inline int si_type_match(struct si_type *t0, struct si_type *t1)
{
	if (!(t0->binary && t1->binary))
		return 0;
	if (!(t0->kernel && t1->kernel))
		return 0;
	if (!(t0->os_type && t1->os_type))
		return 0;
	if (!(t0->type_more && t1->type_more))
		return 0;
	return 1;
}

static inline struct sibuf *find_target_sibuf(void *addr)
{
	struct sibuf *tmp = NULL, *ret = NULL;
	read_lock(&si->lock);
	list_for_each_entry(tmp, &si->sibuf_head, sibling) {
		if (((unsigned long)addr >= tmp->load_addr) &&
			((unsigned long)addr < (tmp->load_addr + tmp->total_len))) {
			ret = tmp;
			break;
		}
	}

	read_unlock(&si->lock);
	return ret;
}

static inline struct resfile *get_builtin_resfile(void)
{
	struct resfile *ret = NULL;
	read_lock(&si->lock);
	ret = list_first_entry_or_null(&si->resfile_head, struct resfile, sibling);
	if (ret && (!ret->built_in))
		ret = NULL;
	read_unlock(&si->lock);
	return ret;
}

static inline char *__attr_name_find(char *name)
{
	struct name_list *ret = NULL;
	struct name_list *tmp;
	list_for_each_entry(tmp, &si->attr_name_head, sibling) {
		if (!strcmp(tmp->name, name)) {
			ret = tmp;
			break;
		}
	}

	if (ret)
		return ret->name;
	return NULL;
}

static inline char *attr_name_find(char *name)
{
	char *ret = NULL;
	read_lock(&si->lock);
	ret = __attr_name_find(name);
	read_unlock(&si->lock);
	return ret;
}

static inline char *attr_name_new(char *name)
{
	char *ret = NULL;
	write_lock(&si->lock);
	if ((ret = __attr_name_find(name))) {
		write_unlock(&si->lock);
		return ret;
	}

	struct name_list *_new;
	_new = (struct name_list *)src_buf_get(sizeof(*_new));
	memset(_new, 0, sizeof(*_new));
	_new->name = (char *)src_buf_get(strlen(name)+1);
	memcpy(_new->name, name, strlen(name)+1);
	list_add_tail(&_new->sibling, &si->attr_name_head);
	ret = _new->name;

	write_unlock(&si->lock);
	return ret;
}

static inline struct attr_list *attr_list_new(char *attr_name)
{
	char *oldname = attr_name_new(attr_name);
	BUG_ON(!oldname);

	struct attr_list *_new;
	_new = (struct attr_list *)src_buf_get(sizeof(*_new));
	memset(_new, 0, sizeof(*_new));
	INIT_LIST_HEAD(&_new->values);
	_new->attr_name = oldname;

	return _new;
}

static inline struct attr_value_list *attr_value_list_new(void)
{
	struct attr_value_list *_new;
	_new = (struct attr_value_list *)src_buf_get(sizeof(*_new));
	memset(_new, 0, sizeof(*_new));
	return _new;
}

static inline void *file_context_cmd_position(void *start)
{
	struct file_context *tmp = (struct file_context *)start;
	return ((char *)start + sizeof(struct file_context) - PATH_MAX +
			tmp->path_len);
}

static inline void *file_context_payload_position(void *start)
{
	struct file_context *tmp = (struct file_context *)start;
	return ((char *)start + sizeof(struct file_context) - PATH_MAX +
			tmp->path_len + tmp->cmd_len);
}

static inline void sinode_id(enum sinode_type type, union siid *id)
{
	write_lock(&si->lock);
	union siid *type_id = &si->id_idx[type];

	unsigned long cur_val = type_id->id0.id_value;
	if (unlikely(cur_val == (unsigned long)-1)) {
		err_dbg(0, "id exceed, type %d", type);
		write_unlock(&si->lock);
		BUG();
	}
	if (unlikely(!cur_val))
		type_id->id0.id_type = type;

	id->id1 = type_id->id1;
	type_id->id0.id_value++;
	write_unlock(&si->lock);
}

static inline enum sinode_type siid_get_type(union siid *id)
{
	return (enum sinode_type)id->id0.id_type;
}

static inline unsigned long siid_get_value(union siid *id)
{
	return (unsigned long)id->id0.id_value;
}

static inline unsigned long siid_get_whole(union siid *id)
{
	return (unsigned long)id->id1;
}

static inline enum sinode_type sinode_get_id_type(struct sinode *sn)
{
	return siid_get_type(&sn->node_id.id);
}

static inline unsigned long sinode_get_id_value(struct sinode *sn)
{
	return siid_get_value(&sn->node_id.id);
}

static inline unsigned long sinode_get_id_whole(struct sinode *sn)
{
	return siid_get_whole(&sn->node_id.id);
}

static inline void type_node_init(struct type_node *tn, void *node, int type_code)
{
	struct type_node *_new = tn;
	_new->node = node;
	_new->type_code = type_code;
	INIT_LIST_HEAD(&_new->sibling);
	INIT_LIST_HEAD(&_new->children);

	INIT_LIST_HEAD(&_new->used_at);
	return;
}

static inline struct type_node *type_node_new(void *node, int type_code)
{
	struct type_node *_new;
	size_t size_need = 0;
	size_need = sizeof(*_new);
	_new = (struct type_node *)src_buf_get(size_need);
	memset(_new, 0, size_need);
	type_node_init(_new, node, type_code);
	return _new;
}

static inline void var_node_init(struct var_node *n, void *node)
{
	n->node = node;
	INIT_LIST_HEAD(&n->used_at);
	INIT_LIST_HEAD(&n->possible_values);
}

static inline struct var_node *var_node_new(void *node)
{
	struct var_node *_new;
	_new = (struct var_node *)src_buf_get(sizeof(*_new));
	memset(_new, 0, sizeof(*_new));
	var_node_init(_new, node);
	return _new;
}

static inline struct var_node_list *var_node_list_new(void *node)
{
	struct var_node_list *_new;
	_new = (struct var_node_list *)src_buf_get(sizeof(*_new));
	memset(_new, 0, sizeof(*_new));
	var_node_init(&_new->var, node);
	return _new;
}

static inline struct var_node_list *var_node_list_find(struct list_head *head,
							void *node)
{
	struct var_node_list *tmp;
	list_for_each_entry(tmp, head, sibling) {
		if (tmp->var.node == node)
			return tmp;
	}
	return NULL;
}

static inline struct func_node *func_node_new(void *node)
{
	struct func_node *_new;
	_new = (struct func_node *)src_buf_get(sizeof(*_new));
	memset(_new, 0, sizeof(*_new));
	_new->node = node;
	INIT_LIST_HEAD(&_new->args);
	INIT_LIST_HEAD(&_new->callers);
	INIT_LIST_HEAD(&_new->callees);
	INIT_LIST_HEAD(&_new->global_vars);
	INIT_LIST_HEAD(&_new->local_vars);

	INIT_LIST_HEAD(&_new->unreachable_stmts);
	INIT_LIST_HEAD(&_new->used_at);
	return _new;
}

static inline int func_caller_internal(union siid *id)
{
	return siid_get_type(id) == TYPE_FILE;
}

static inline void code_path_init(struct code_path *cp)
{
	INIT_LIST_HEAD(&cp->sentences);
}

static inline struct code_path *code_path_new(struct func_node *func,
						unsigned long extra_branches)
{
	struct code_path *_new;
	size_t size_need = sizeof(*_new) + extra_branches * sizeof(_new);
	_new = (struct code_path *)src_buf_get(size_need);
	memset(_new, 0, sizeof(*_new));
	code_path_init(_new);
	_new->func = func;
	_new->branches = 2+extra_branches;
	return _new;
}

static inline struct id_list *id_list_new(void)
{
	struct id_list *_new;
	_new = (struct id_list *)src_buf_get(sizeof(*_new));
	return _new;
}

static inline struct id_list *id_list_find(struct list_head *head,
						unsigned long value,
						unsigned long flag)
{
	struct id_list *tmp;
	list_for_each_entry(tmp, head, sibling) {
		if ((tmp->value == value) && (tmp->value_flag == flag))
			return tmp;
	}
	return NULL;
}

static inline struct unr_stmt *unr_stmt_find(struct list_head *head,
						int line, int col)
{
	struct unr_stmt *tmp;
	list_for_each_entry(tmp, head, sibling) {
		if ((tmp->line == line) && (tmp->col == col))
			return tmp;
	}

	return NULL;
}

static inline struct unr_stmt *unr_stmt_new(void)
{
	struct unr_stmt *_new;
	_new = (struct unr_stmt *)src_buf_get(sizeof(*_new));
	memset(_new, 0, sizeof(*_new));
	return _new;
}

static inline struct call_func_list *call_func_list_new(void)
{
	struct call_func_list *_new;
	_new = (struct call_func_list *)src_buf_get(sizeof(*_new));
	memset(_new, 0, sizeof(*_new));
	INIT_LIST_HEAD(&_new->gimple_stmts);
	return _new;
}

static inline struct call_func_list *call_func_list_find(struct list_head *head,
								unsigned long value,
								unsigned long flag)
{
	struct call_func_list *tmp;
	list_for_each_entry(tmp, head, sibling) {
		if ((tmp->value == value) && (tmp->value_flag == flag))
			return tmp;
	}
	return NULL;
}

static inline struct call_func_gimple_stmt_list *call_func_gimple_stmt_list_new(void)
{
	struct call_func_gimple_stmt_list *_new;
	_new = (struct call_func_gimple_stmt_list *)src_buf_get(sizeof(*_new));
	memset(_new, 0, sizeof(*_new));
	return _new;
}

static inline int call_func_gimple_stmt_list_exist(struct list_head *head,
							void *gimple)
{
	struct call_func_gimple_stmt_list *tmp;
	list_for_each_entry(tmp, head, sibling) {
		if (tmp->gimple_stmt == gimple)
			return 1;
	}
	return 0;
}

static inline void call_func_gimple_stmt_list_add(struct list_head *head,
							void *gimple)
{
	if (call_func_gimple_stmt_list_exist(head, gimple))
		return;

	struct call_func_gimple_stmt_list *_new;
	_new = call_func_gimple_stmt_list_new();
	_new->gimple_stmt = gimple;
	list_add_tail(&_new->sibling, head);
}

static inline struct use_at_list *use_at_list_new(void)
{
	struct use_at_list *_new;
	_new = (struct use_at_list *)src_buf_get(sizeof(*_new));
	memset(_new, 0, sizeof(*_new));
	return _new;
}

static inline struct use_at_list *use_at_list_find(struct list_head *head,
							void *gimple_stmt,
							unsigned long op_idx)
{
	struct use_at_list *tmp;
	list_for_each_entry(tmp, head, sibling) {
		if ((tmp->gimple_stmt == gimple_stmt) &&
			(tmp->op_idx == op_idx))
			return tmp;
	}
	return NULL;
}

static inline struct possible_value_list *possible_value_list_new(void)
{
	struct possible_value_list *_new;
	_new = (struct possible_value_list *)src_buf_get(sizeof(*_new));
	memset(_new, 0, sizeof(*_new));
	return _new;
}

static inline struct possible_value_list *possible_value_list_find(
						struct list_head *head,
						unsigned long val_flag,
						unsigned long value)
{
	struct possible_value_list *tmp;
	list_for_each_entry(tmp, head, sibling) {
		if (tmp->value == value)
			return tmp;
	}
	return NULL;
}

static inline struct sibuf_type_node *sibuf_type_node_new(void)
{
	struct sibuf_type_node *_new;
	_new = (struct sibuf_type_node *)src_buf_get(sizeof(*_new));
	memset(_new, 0, sizeof(*_new));
	return _new;
}

static inline struct func_path_list *func_path_list_new(void)
{
	struct func_path_list *_new;
	_new = (struct func_path_list *)xmalloc(sizeof(*_new));
	memset(_new, 0, sizeof(*_new));
	return _new;
}

static inline struct code_path_list *code_path_list_new(void)
{
	struct code_path_list *_new;
	_new = (struct code_path_list *)xmalloc(sizeof(*_new));
	memset(_new, 0, sizeof(*_new));
	return _new;
}

static inline struct path_list_head *path_list_head_new(void)
{
	struct path_list_head *_new;
	_new = (struct path_list_head *)xmalloc(sizeof(*_new));
	memset(_new, 0, sizeof(*_new));
	INIT_LIST_HEAD(&_new->path_head);
	return _new;
}

#include "defdefine.h"

#define	si_get_logfile() ({\
		char ____path[PATH_MAX];\
		snprintf(____path, PATH_MAX, "%s/%s/log", DEF_TMPDIR, si->src_id);\
		____path;\
		})

static inline void __si_log(const char *fmt, ...)
{
	char logbuf[MAXLINE];
	memset(logbuf, 0, MAXLINE);

	va_list ap;
	va_start(ap, fmt);
	vsnprintf(logbuf, MAXLINE, fmt, ap);
	va_end(ap);

	char *logfile = si_get_logfile();
	int logfd = open(logfile, O_WRONLY | O_APPEND);
	if (unlikely(logfd == -1))
		logfd = open(logfile, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);

	if (logfd == -1) {
		fprintf(stderr, "%s", logbuf);
	} else {
		int err __maybe_unused = write(logfd, logbuf, strlen(logbuf));
		close(logfd);
	}
}

#define	SI_LOG_FMT(fmt)	"[%02d/%02d/%d %02d:%02d:%02d][%s %s:%d]\n\t" fmt
#define	SI_LOG_TMARGS \
	____tm.tm_mon+1, ____tm.tm_mday, ____tm.tm_year, \
	____tm.tm_hour, ____tm.tm_min, ____tm.tm_sec
#define	SI_LOG_LINEINFO \
	__FILE__, __LINE__

#define	SI_LOG_GETTM() ({\
	time_t ____t = time(NULL);\
	struct tm ____tm = *localtime(&____t);\
	____tm;\
	})
#define	SI_LOG_TM() \
	struct tm ____tm = SI_LOG_GETTM()

#define	si_log(str, fmt, ...) ({\
	SI_LOG_TM();\
	__si_log(SI_LOG_FMT(fmt),SI_LOG_TMARGS,str,SI_LOG_LINEINFO,##__VA_ARGS__);\
	})
#define	si_log1(fmt, ...) \
	si_log("analysis", fmt, ##__VA_ARGS__)
#define	si_log2(fmt, ...) \
	si_log("hacking", fmt, ##__VA_ARGS__)

DECL_END

#endif /* end of include guard: SI_DECLS_H_HEGJI43K */
