/*
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

#ifndef SI_HELPER_H_67EHZSGV
#define SI_HELPER_H_67EHZSGV

#include "si_core.h"

DECL_BEGIN

C_SYM struct src *si;

C_SYM void *src_buf_get(size_t len);
C_SYM void si_module_init(struct si_module *p);
C_SYM int8_t si_module_str_to_category(char *string);
C_SYM int si_module_str_to_type(struct si_type *type, char *string);
C_SYM int si_module_get_abs_path(char *buf, size_t len, int category,
					char *path);
C_SYM struct slist_head *si_module_get_head(int category);
C_SYM struct si_module *si_module_find_by_name(char *name,
						struct slist_head *head);
C_SYM int si_module_add(struct si_module *p);
C_SYM int si_module_load_all(struct slist_head *head);
C_SYM int si_module_unload_all(struct slist_head *head);
C_SYM void si_module_cleanup(void);

#define	SI_MOD_SUBSHELL_CMDS() \
static void ____help_usage(void)\
{\
	fprintf(stdout, "\tShow this help message\n");\
}\
static long ____help_cb(int argc, char *argv[])\
{\
	clib_cmd_usages();\
	return 0;\
}\
static int exit_flag = 0;\
static void ____exit_usage(void)\
{\
	fprintf(stdout, "\tReturn to the previous shell\n");\
}\
static long ____exit_cb(int argc, char *argv[])\
{\
	exit_flag = 1;\
	return 0;\
}\
static int __maybe_unused ____v

#define	SI_MOD_SUBENV_INIT_NAME		__subenv_init
#define	SI_MOD_SUBENV_DEINIT_NAME	__subenv_deinit
#define	SI_MOD_SUBENV_EARLY_INIT_NAME	__subenv_early_init
#define	SI_MOD_SUBENV_EARLY_DEINIT_NAME	__subenv_early_deinit

#define	SI_MOD_SUBENV_INIT() \
static long SI_MOD_SUBENV_INIT_NAME(void)
#define	SI_MOD_SUBENV_DEINIT() \
static void SI_MOD_SUBENV_DEINIT_NAME(void)

#define	SI_MOD_SUBENV_EARLY_INIT() \
static long SI_MOD_SUBENV_EARLY_INIT_NAME(void)
#define	SI_MOD_SUBENV_EARLY_DEINIT() \
static void SI_MOD_SUBENV_EARLY_DEINIT_NAME(void)

#define	SI_MOD_SUBENV_SETUP(modname, n, ...) \
SI_MOD_SUBSHELL_CMDS();\
CLIB_MODULE_NAME(modname);\
CLIB_MODULE_NEEDEDx(n, ##__VA_ARGS__);\
static char *cmdname = #modname;\
static void modname##_usage(void)\
{\
	fprintf(stdout, "\tEnter %s subshell\n", #modname);\
}\
SI_MOD_SUBENV_INIT();\
SI_MOD_SUBENV_DEINIT();\
static long modname##_cb(int argc, char *argv[])\
{\
	int err = 0;\
	err = clib_ui_begin();\
	if (err) {\
		err_dbg(0, "clib_ui_begin err");\
		return -1;\
	}\
	err = clib_cmd_ac_add("help", ____help_cb, ____help_usage);\
	if (err) {\
		err_dbg(0, "clib_cmd_ac_add err");\
		clib_ui_end();\
		return -1;\
	}\
	err = clib_cmd_ac_add("exit", ____exit_cb, ____exit_usage);\
	if (err) {\
		err_dbg(0, "clib_cmd_ac_add err");\
		clib_cmd_ac_cleanup();\
		clib_ui_end();\
		return -1;\
	}\
	err = clib_cmd_ac_add("quit", ____exit_cb, ____exit_usage);\
	if (err) {\
		err_dbg(0, "clib_cmd_ac_add err");\
		clib_cmd_ac_cleanup();\
		clib_ui_end();\
		return -1;\
	}\
	err = SI_MOD_SUBENV_INIT_NAME();\
	if (err) {\
		err_dbg(0, "%s_subenv_setup err", #modname);\
		clib_cmd_ac_cleanup();\
		clib_ui_end();\
		return -1;\
	}\
	char *ibuf = NULL;\
	while (!exit_flag) {\
		ibuf = clib_readline(#modname"> ");\
		if (!ibuf) {\
			err_dbg(0, "readline get EOF and empty line, redo");\
			continue;\
		}\
		int cmd_argc;\
		int cmd_arg_max = 16;\
		char *cmd_argv[cmd_arg_max];\
		memset(cmd_argv, 0, cmd_arg_max*sizeof(char *));\
		err = clib_cmd_getarg(ibuf,strlen(ibuf)+1,&cmd_argc,cmd_argv,\
								cmd_arg_max);\
		if (err) {\
			err_dbg(0, "clib_cmd_getarg err, redo");\
			free(ibuf);\
			continue;\
		}\
		err = clib_cmd_exec(cmd_argv[0], cmd_argc, cmd_argv);\
		if (err) {\
			err_dbg(0, "clib_cmd_exec err");\
			free(ibuf);\
			continue;\
		}\
		free(ibuf);\
	}\
	exit_flag = 0;\
	SI_MOD_SUBENV_DEINIT_NAME();\
	clib_cmd_ac_cleanup();\
	clib_ui_end();\
	return 0;\
}\
SI_MOD_SUBENV_EARLY_INIT();\
SI_MOD_SUBENV_EARLY_DEINIT();\
CLIB_MODULE_INIT()\
{\
	int err;\
	err = clib_cmd_ac_add(cmdname, modname##_cb, modname##_usage);\
	if (err) {\
		err_dbg(0, "clib_cmd_ac_add err");\
		return -1;\
	}\
	err = SI_MOD_SUBENV_EARLY_INIT_NAME();\
	if (err) {\
		err_dbg(0, "SI_MOD_SUBENV_EARLY_INIT err");\
		clib_cmd_ac_del(cmdname);\
		return -1;\
	}\
	return 0;\
}\
CLIB_MODULE_EXIT()\
{\
	clib_cmd_ac_del(cmdname);\
	SI_MOD_SUBENV_EARLY_DEINIT_NAME();\
}\
static int __maybe_unused modname##____v

static inline void si_lock_r(void)
{
	read_lock(&si->lock);
}

static inline void si_unlock_r(void)
{
	read_unlock(&si->lock);
}

static inline void si_lock_w(void)
{
	write_lock(&si->lock);
}

static inline void si_unlock_w(void)
{
	write_unlock(&si->lock);
}

static inline void sibuf_lock_r(struct sibuf *b)
{
	read_lock(&b->lock);
}

static inline void sibuf_unlock_r(struct sibuf *b)
{
	read_unlock(&b->lock);
}

static inline void sibuf_lock_w(struct sibuf *b)
{
	write_lock(&b->lock);
}

static inline void sibuf_unlock_w(struct sibuf *b)
{
	write_unlock(&b->lock);
}

static inline struct sibuf_user *sibuf_user_alloc(void)
{
	struct sibuf_user *_new;
	_new = (struct sibuf_user *)xmalloc(sizeof(*_new));
	memset(_new, 0, sizeof(*_new));
	return _new;
}

static inline void sibuf_user_free(struct sibuf_user *buf_user)
{
	free(buf_user);
}

static inline struct sibuf_user *__sibuf_find_user(struct sibuf *b, pthread_t user)
{
	struct sibuf_user *tmp;
	slist_for_each_entry(tmp, &b->users, sibling) {
		if (pthread_equal(tmp->thread_id, user))
			return tmp;
	}
	return NULL;
}

static inline void __sibuf_add_user(struct sibuf *b, pthread_t user)
{
	struct sibuf_user *tmp;
	tmp = sibuf_user_alloc();
	tmp->thread_id = user;
	slist_add_tail(&tmp->sibling, &b->users);
}

static inline void __sibuf_del_user(struct sibuf *b, pthread_t user)
{
	struct sibuf_user *tmp;
	tmp = __sibuf_find_user(b, user);
	if (tmp) {
		slist_del(&tmp->sibling, &b->users);
		sibuf_user_free(tmp);
	}
}

static inline int sibuf_add_user(struct sibuf *b, pthread_t user)
{
	int ret = 0;

	sibuf_lock_w(b);

	if (!__sibuf_find_user(b, user))
		__sibuf_add_user(b, user);
	else
		ret = 1;

	sibuf_unlock_w(b);
	return ret;
}

static inline void sibuf_del_user(struct sibuf *b, pthread_t user)
{
	sibuf_lock_w(b);

	__sibuf_del_user(b, user);

	sibuf_unlock_w(b);
}

static inline void sibuf_cleanup_user(struct sibuf *b)
{
	struct sibuf_user *tmp, *next;
	slist_for_each_entry_safe(tmp, next, &b->users, sibling) {
		slist_del(&tmp->sibling, &b->users);
		sibuf_user_free(tmp);
	}
}

#define	node_lock_r(node) \
	do {\
		typeof(node) ____node = (node);\
		read_lock(&____node->lock);\
	} while (0)

#define	node_unlock_r(node) \
	do {\
		typeof(node) ____node = (node);\
		read_unlock(&____node->lock);\
	} while (0)

#define	node_lock_w(node) \
	do {\
		typeof(node) ____node = (node);\
		write_lock(&____node->lock);\
	} while (0)

#define	node_unlock_w(node) \
	do {\
		typeof(node) ____node = (node);\
		write_unlock(&____node->lock);\
	} while (0)

static inline int si_type_match(struct si_type *wider, struct si_type *smaller)
{
	if (!(wider->binary & smaller->binary))
		return 0;
	if (!(wider->kernel & smaller->kernel))
		return 0;
	if ((wider->os_type != SI_TYPE_OS_ANY) &&
		(wider->os_type != smaller->os_type))
		return 0;
	if ((wider->data_fmt != SI_TYPE_DF_ANY) &&
		(wider->data_fmt != smaller->data_fmt))
		return 0;
	return 1;
}

static inline struct sibuf *find_target_sibuf(void *addr)
{
	struct sibuf *tmp = NULL, *ret = NULL;
	si_lock_r();
	slist_for_each_entry(tmp, &si->sibuf_head, sibling) {
		if (((unsigned long)addr >= tmp->load_addr) &&
		    ((unsigned long)addr < (tmp->load_addr + tmp->total_len))) {
			ret = tmp;
			break;
		}
	}

	si_unlock_r();
	return ret;
}

static inline struct resfile *get_builtin_resfile(void)
{
	struct resfile *ret = NULL;
	si_lock_r();
	ret = slist_first_entry_or_null(&si->resfile_head, struct resfile,
					sibling);
	if (ret && (!ret->built_in))
		ret = NULL;
	si_unlock_r();
	return ret;
}

static inline struct name_list *name_list_new(char *tname, size_t namelen)
{
	struct name_list *_new;
	_new = (struct name_list *)src_buf_get(sizeof(*_new) + namelen);
	memset(_new, 0, sizeof(*_new));
	memcpy(_new->name, tname, namelen);
	return _new;
}

static inline
struct name_list *__name_list_find(char *tname, size_t namelen,
				   struct rb_root **root_pos,
				   struct rb_node ***newtmp_pos,
				   struct rb_node **parent_pos)
{
	struct rb_root *root = &si->names_root;
	struct rb_node **newtmp = &(root->rb_node), *parent = NULL;
	struct name_list *ret = NULL;

	while (*newtmp) {
		struct name_list *data;
		data = container_of(*newtmp, struct name_list, node);

		size_t thislen = strlen(data->name) + 1;
		parent = *newtmp;
		if (namelen < thislen) {
			newtmp = &((*newtmp)->rb_left);
		} else if (namelen > thislen) {
			newtmp = &((*newtmp)->rb_right);
		} else {
			int res = strncmp(data->name, tname, namelen);
			if (res > 0) {
				newtmp = &((*newtmp)->rb_left);
			} else if (res < 0) {
				newtmp = &((*newtmp)->rb_right);
			} else {
				ret = data;
				break;
			}
		}
	}

	if (!ret) {
		*root_pos = root;
		*newtmp_pos = newtmp;
		*parent_pos = parent;
	}

	return ret;
}

static inline void __name_list_insert(struct name_list *nl, size_t namelen)
{
	struct rb_root *root = &si->names_root;
	struct rb_node **newtmp = &(root->rb_node), *parent = NULL;

	while (*newtmp) {
		struct name_list *data;
		data = container_of(*newtmp, struct name_list, node);

		size_t thislen = strlen(data->name) + 1;
		parent = *newtmp;
		if (namelen < thislen) {
			newtmp = &((*newtmp)->rb_left);
		} else if (namelen > thislen) {
			newtmp = &((*newtmp)->rb_right);
		} else {
			int res = strncmp(data->name, nl->name, namelen);
			if (res > 0) {
				newtmp = &((*newtmp)->rb_left);
			} else if (res < 0) {
				newtmp = &((*newtmp)->rb_right);
			} else {
				err_dbg(0, "same name_list insert");
				return;
			}
		}
	}

	rb_link_node(&nl->node, parent, newtmp);
	rb_insert_color(&nl->node, root);
	return;
}

static inline void __name_list_insert_pos(struct name_list *nl, size_t namelen,
					  struct rb_root *root,
					  struct rb_node **newtmp,
					  struct rb_node *parent)
{
	rb_link_node(&nl->node, parent, newtmp);
	rb_insert_color(&nl->node, root);
	return;
}

static inline struct name_list *__name_list_add(char *tname, size_t namelen)
{
	struct name_list *nl;
	struct rb_root *root __maybe_unused;
	struct rb_node **newtmp __maybe_unused, *parent __maybe_unused;
	nl = __name_list_find(tname, namelen, &root, &newtmp, &parent);
	if (!nl) {
		nl = name_list_new(tname, namelen);
		__name_list_insert_pos(nl, namelen, root, newtmp, parent);
	}

	return nl;
}

static inline struct name_list *_name_list_add(char *tname, size_t namelen)
{
	si_lock_w();
	struct name_list *nl;
	nl = __name_list_add(tname, namelen);
	si_unlock_w();
	return nl;
}

static inline char *name_list_add(char *tname, size_t namelen)
{
	struct name_list *nl;
	nl = _name_list_add(tname, namelen);
	return nl->name;
}

static inline struct attr_list *attr_list_new(void)
{
	struct attr_list *_new;
	_new = (struct attr_list *)src_buf_get(sizeof(*_new));
	memset(_new, 0, sizeof(*_new));
	INIT_SLIST_HEAD(&_new->values);
	return _new;
}

static inline struct attr_list *__attr_list_add(char *name)
{
	struct attr_list *ret = NULL;
	struct name_list *nl;
	nl = __name_list_add(name, strlen(name) + 1);

	ret = attr_list_new();
	ret->attr_name = nl->name;

	return ret;
}

static inline struct attr_list *attr_list_add(char *name)
{
	struct attr_list *ret = NULL;
	si_lock_w();
	ret = __attr_list_add(name);
	si_unlock_w();
	return ret;
}

static inline struct attrval_list *attrval_list_new(void)
{
	struct attrval_list *_new;
	_new = (struct attrval_list *)src_buf_get(sizeof(*_new));
	memset(_new, 0, sizeof(*_new));
	return _new;
}

/* Use sibuf_hold/sibuf_drop before/after call these two functions. */
static inline void *fc_cmdptr(void *start)
{
	struct file_content *tmp = (struct file_content *)start;
	return ((char *)start + sizeof(struct file_content) - PATH_MAX +
			tmp->path_len);
}

static inline void *fc_pldptr(void *start)
{
	struct file_content *tmp = (struct file_content *)start;
	return ((char *)start + sizeof(struct file_content) - PATH_MAX +
			tmp->path_len + tmp->cmd_len);
}

static inline void sinode_id(enum sinode_type type, union siid *id)
{
	si_lock_w();
	union siid *type_id = &si->id_idx[type];

	unsigned long cur_val = type_id->id0.id_value;
	if (unlikely(cur_val == (unsigned long)-1)) {
		err_dbg(0, "id exceed, type %d", type);
		si_unlock_w();
		BUG();
	}
	if (unlikely(!cur_val))
		type_id->id0.id_type = type;

	id->id1 = type_id->id1;
	type_id->id0.id_value++;
	si_unlock_w();
}

static inline enum sinode_type siid_type(union siid *id)
{
	return (enum sinode_type)id->id0.id_type;
}

static inline unsigned long siid_value(union siid *id)
{
	return (unsigned long)id->id0.id_value;
}

static inline unsigned long siid_all(union siid *id)
{
	return (unsigned long)id->id1;
}

static inline enum sinode_type sinode_idtype(struct sinode *sn)
{
	return siid_type(&sn->node_id.id);
}

static inline unsigned long sinode_idvalue(struct sinode *sn)
{
	return siid_value(&sn->node_id.id);
}

static inline unsigned long sinode_id_all(struct sinode *sn)
{
	return siid_all(&sn->node_id.id);
}

static inline void type_node_init(struct type_node *tn,
					void *node, int type_code)
{
	struct type_node *_new = tn;
	_new->node = node;
	_new->type_code = type_code;
	INIT_SLIST_HEAD(&_new->sibling);
	INIT_SLIST_HEAD(&_new->children);

	INIT_SLIST_HEAD(&_new->used_at);
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
	INIT_SLIST_HEAD(&n->used_at);
	INIT_SLIST_HEAD(&n->possible_values);
}

static inline struct var_node *var_node_new(void *node)
{
	struct var_node *_new;
	_new = (struct var_node *)src_buf_get(sizeof(*_new));
	memset(_new, 0, sizeof(*_new));
	var_node_init(_new, node);
	return _new;
}

static inline struct var_list *var_list_new(void *node)
{
	struct var_list *_new;
	_new = (struct var_list *)src_buf_get(sizeof(*_new));
	memset(_new, 0, sizeof(*_new));
	var_node_init(&_new->var, node);
	return _new;
}

static inline struct var_list *var_list_find(struct slist_head *head,
							void *node)
{
	struct var_list *tmp;
	slist_for_each_entry(tmp, head, sibling) {
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
	INIT_SLIST_HEAD(&_new->args);
	INIT_SLIST_HEAD(&_new->callers);
	INIT_SLIST_HEAD(&_new->callees);
	INIT_SLIST_HEAD(&_new->global_vars);
	INIT_SLIST_HEAD(&_new->local_vars);
	INIT_SLIST_HEAD(&_new->data_state_list);

	INIT_SLIST_HEAD(&_new->used_at);
	return _new;
}

static inline int func_caller_internal(union siid *id)
{
	return siid_type(id) == TYPE_FILE;
}

static inline struct code_path *code_path_new(struct func_node *func,
						unsigned long extra_branches)
{
	struct code_path *_new;
	size_t size_need = sizeof(*_new) + extra_branches * sizeof(_new);
	_new = (struct code_path *)src_buf_get(size_need);
	memset(_new, 0, sizeof(*_new));
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

static inline struct id_list *id_list_find(struct slist_head *head,
						unsigned long value,
						unsigned long flag)
{
	struct id_list *tmp;
	slist_for_each_entry(tmp, head, sibling) {
		if ((tmp->value == value) && (tmp->value_flag == flag))
			return tmp;
	}
	return NULL;
}

static inline struct callf_list *callf_list_new(void)
{
	struct callf_list *_new;
	_new = (struct callf_list *)src_buf_get(sizeof(*_new));
	memset(_new, 0, sizeof(*_new));
	INIT_SLIST_HEAD(&_new->stmts);
	return _new;
}

static inline struct callf_list *callf_list_find(struct slist_head *head,
							unsigned long value,
							unsigned long flag)
{
	struct callf_list *tmp;
	slist_for_each_entry(tmp, head, sibling) {
		if ((tmp->value == value) && (tmp->value_flag == flag))
			return tmp;
	}
	return NULL;
}

static inline struct callf_list *__add_call(struct slist_head *head,
					unsigned long value,
					unsigned long value_flag,
					unsigned long body_missing)
{
	struct callf_list *newc;
	newc = callf_list_find(head, value, value_flag);
	if (!newc) {
		newc = callf_list_new();
		newc->value = value;
		newc->value_flag = value_flag;
		newc->body_missing = body_missing;
		slist_add_tail(&newc->sibling, head);
	}

	return newc;
}

static inline struct callf_stmt_list *callf_stmt_list_new(void)
{
	struct callf_stmt_list *_new;
	_new = (struct callf_stmt_list *)src_buf_get(sizeof(*_new));
	memset(_new, 0, sizeof(*_new));
	return _new;
}

static inline int callf_stmt_list_exist(struct slist_head *head, int type,
					void *where)
{
	struct callf_stmt_list *tmp;
	slist_for_each_entry(tmp, head, sibling) {
		if ((tmp->type == type) && (tmp->where == where))
			return 1;
	}
	return 0;
}

static inline void callf_stmt_list_add(struct slist_head *head, int type,
					void *where)
{
	if (callf_stmt_list_exist(head, type, where))
		return;

	struct callf_stmt_list *_new;
	_new = callf_stmt_list_new();
	_new->where = where;
	_new->type = type;
	slist_add_tail(&_new->sibling, head);
}

static inline struct use_at_list *use_at_list_new(void)
{
	struct use_at_list *_new;
	_new = (struct use_at_list *)src_buf_get(sizeof(*_new));
	memset(_new, 0, sizeof(*_new));
	return _new;
}

static inline struct use_at_list *use_at_list_find(struct slist_head *head,
						   int type, void *where,
						   unsigned long extra_info)
{
	struct use_at_list *tmp;
	slist_for_each_entry(tmp, head, sibling) {
		if ((tmp->type == type) &&
			(tmp->where == where) &&
			(tmp->extra_info == extra_info))
			return tmp;
	}
	return NULL;
}

static inline struct use_at_list *__add_use_at(struct slist_head *head,
						union siid id, int type,
						void *where,
						unsigned long extra_info)
{
	struct use_at_list *newua;
	newua = use_at_list_find(head, type, where, extra_info);
	if (!newua) {
		newua = use_at_list_new();
		newua->func_id = id;
		newua->type = type;
		newua->where = where;
		newua->extra_info = extra_info;
		slist_add_tail(&newua->sibling, head);
	}

	return newua;
}

static inline struct possible_list *possible_list_new(void)
{
	struct possible_list *_new;
	_new = (struct possible_list *)src_buf_get(sizeof(*_new));
	memset(_new, 0, sizeof(*_new));
	return _new;
}

static inline struct possible_list *possible_list_find(struct slist_head *head,
							unsigned long val_flag,
							unsigned long value)
{
	struct possible_list *tmp;
	slist_for_each_entry(tmp, head, sibling) {
		if (tmp->value == value)
			return tmp;
	}
	return NULL;
}

static inline struct possible_list *__add_possible(struct slist_head *head,
						   u8 val_flag,
						   u64 value,
						   u32 extra)
{
	struct possible_list *pv;
	pv = possible_list_find(head, val_flag, value);
	if (!pv) {
		pv = possible_list_new();
		pv->value_flag = val_flag;
		pv->value = value;
		pv->extra_param = extra;
		slist_add_tail(&pv->sibling, head);
	}

	return pv;
}

static inline struct sibuf_typenode *sibuf_typenode_new(void)
{
	struct sibuf_typenode *_new;
	_new = (struct sibuf_typenode *)src_buf_get(sizeof(*_new));
	memset(_new, 0, sizeof(*_new));
	return _new;
}

static inline struct cp_list *cp_list_new(int dyn, struct code_path *cp)
{
	struct cp_list *_new;
	if (dyn)
		_new = (struct cp_list *)xmalloc(sizeof(*_new));
	else
		_new = (struct cp_list *)src_buf_get(sizeof(*_new));
	memset(_new, 0, sizeof(*_new));
	_new->cp = cp;
	return _new;
}

static inline void cp_list_free(struct cp_list *cp)
{
	free(cp);
}

static inline int ds_get_section(u8 val_type)
{
	if (val_type == DSVT_UNK) {
		return 0;
	} else if ((val_type >= DSVT_INT_CST) &&
			(val_type <= DSVT_REAL_CST)) {
		return 1;
	} else if ((val_type >= DSVT_ADDR) &&
			(val_type <= DSVT_REF)) {
		return 2;
	} else if (val_type >= DSVT_CONSTRUCTOR) {
		return 3;
	} else {
		BUG();
	}
}

static inline void data_state_init_base(struct data_state_base *base, 
				        u64 addr, u8 type)
{
	base->ref_base = addr;
	base->ref_type = type;
}

static inline struct data_state_base *data_state_base_new(u64 addr, u8 type)
{
	struct data_state_base *_new;
	_new = (struct data_state_base *)src_buf_get(sizeof(*_new));
	memset(_new, 0, sizeof(*_new));
	data_state_init_base(_new, addr, type);
	return _new;
}

static inline void dsv_set_raw(struct data_state_val *dsv, void *raw)
{
	dsv->raw = (u64)raw;
}

static inline struct data_state_rw *data_state_rw_new(u64 addr, u8 type,
							void *raw)
{
	struct data_state_rw *_new;
	_new = (struct data_state_rw *)xmalloc(sizeof(*_new));
	memset(_new, 0, sizeof(*_new));
	data_state_init_base(&_new->base, addr, type);
	dsv_set_raw(&_new->val, raw);
	atomic_set(&_new->refcnt, 1);
	return _new;
}

static inline void data_state_drop(struct data_state_rw *ds);
static inline void dsv_free_data(struct data_state_val *dsv)
{
	switch (ds_get_section(DSV_TYPE(dsv))) {
	case 0:
	{
		break;
	}
	case 1:
	{
		free(DSV_SEC1_VAL(dsv));
		DSV_SEC1_VAL(dsv) = NULL;
		break;
	}
	case 2:
	{
		struct data_state_val_ref *dsvr;
		dsvr = DSV_SEC2_VAL(dsv);
		data_state_drop(dsvr->ds);

		free(DSV_SEC2_VAL(dsv));
		DSV_SEC2_VAL(dsv) = NULL;
		break;
	}
	case 3:
	{
		struct data_state_val1 *tmp, *next;
		slist_for_each_entry_safe(tmp, next, DSV_SEC3_VAL(dsv),
					  sibling) {
			slist_del(&tmp->sibling, DSV_SEC3_VAL(dsv));
			dsv_free_data(&tmp->val);
			free(tmp);
		}
		break;
	}
	default:
	{
		BUG();
	}
	}
	DSV_TYPE(dsv) = DSVT_UNK;
	return;
}

static inline void dsv_alloc_data(struct data_state_val *dsv, u8 val_type,
				  u32 bytes)
{
	if ((ds_get_section(val_type) == 1) && (DSV_TYPE(dsv) == val_type) &&
			(dsv->info.v1_info.bytes == bytes)) {
		memset(DSV_SEC1_VAL(dsv), 0, dsv->info.v1_info.bytes);
		return;
	}

	dsv_free_data(dsv);

	switch (ds_get_section(val_type)) {
	case 0:
	{
		break;
	}
	case 1:
	{
		BUG_ON(!bytes);
		DSV_SEC1_VAL(dsv) = xmalloc(bytes);
		memset(DSV_SEC1_VAL(dsv), 0, bytes);
		dsv->info.v1_info.bytes = bytes;
		break;
	}
	case 2:
	{
		bytes = sizeof(struct data_state_val_ref);
		DSV_SEC2_VAL(dsv) = (struct data_state_val_ref *)xmalloc(bytes);
		memset((void *)DSV_SEC2_VAL(dsv), 0, bytes);
		break;
	}
	case 3:
	{
		break;
	}
	default:
	{
		BUG();
	}
	}
	DSV_TYPE(dsv) = val_type;
	return;
}

static inline struct data_state_val1 *data_state_val1_alloc(void *raw)
{
	struct data_state_val1 *_new;
	_new = (struct data_state_val1 *)xmalloc(sizeof(*_new));
	memset(_new, 0, sizeof(*_new));
	dsv_set_raw(&_new->val, raw);
	return _new;
}

static inline void __dsv_str_data_add_byte(struct data_state_val *dsv, void *raw,
					   char c, u32 idx)
{
	struct data_state_val1 *tmp;
	u32 this_bytes = sizeof(c);

	tmp = data_state_val1_alloc(raw);
	dsv_alloc_data(&tmp->val, DSVT_INT_CST, this_bytes);
	clib_memcpy_bits(DSV_SEC1_VAL(&tmp->val), this_bytes * BITS_PER_UNIT,
			 &c, this_bytes * BITS_PER_UNIT);
	tmp->bits = this_bytes * BITS_PER_UNIT;
	tmp->offset = idx * BITS_PER_UNIT;
	slist_add_tail(&tmp->sibling, DSV_SEC3_VAL(dsv));
}

static inline void dsv_fill_str_data(struct data_state_val *dsv, void *raw, 
					char *str, u32 bytes)
{
	for (u32 i = 0; i < bytes; i++) {
		__dsv_str_data_add_byte(dsv, raw, str[i], i);
	}

	if (bytes && str[bytes-1])
		__dsv_str_data_add_byte(dsv, raw, '\0', bytes);
}

static inline void data_state_destroy(struct data_state_rw *ds)
{
	if (!slist_empty(&ds->base.sibling)) {
		WARN();
	}

	dsv_free_data(&ds->val);
	free(ds);
}

static inline struct data_state_rw *data_state_hold(struct data_state_rw *ds)
{
	if (!ds)
		return NULL;

	atomic_inc(&ds->refcnt);
	return ds;
}

static inline void data_state_drop(struct data_state_rw *ds)
{
	if (!ds)
		return;

	if (atomic_dec_and_test(&ds->refcnt))
		data_state_destroy(ds);

	return;
}

static inline
void dsv_copy_data(struct data_state_val *dst, struct data_state_val *src)
{
	if (dst == src)
		return;

	dsv_free_data(dst);

	switch (ds_get_section(DSV_TYPE(src))) {
	case 0:
	{
		break;
	}
	case 1:
	{
		size_t sz = src->info.v1_info.bytes;

		dsv_alloc_data(dst, DSV_TYPE(src), sz);
		clib_memcpy_bits(DSV_SEC1_VAL(dst), sz * BITS_PER_UNIT,
				 DSV_SEC1_VAL(src),
				 src->info.v1_info.bytes * BITS_PER_UNIT);
		break;
	}
	case 2:
	{
		/* FIXME: increase the target ds refcount if DSVT_ADDR */
		dsv_alloc_data(dst, DSV_TYPE(src), 0);
		DSV_SEC2_VAL(dst)->ds = DSV_SEC2_VAL(src)->ds;
		data_state_hold(DSV_SEC2_VAL(dst)->ds);
		DSV_SEC2_VAL(dst)->offset = DSV_SEC2_VAL(src)->offset;
		DSV_SEC2_VAL(dst)->bits = DSV_SEC2_VAL(src)->bits;
		break;
	}
	case 3:
	{
		struct data_state_val1 *tmp;
		dsv_alloc_data(dst, DSV_TYPE(src), 0);
		slist_for_each_entry(tmp, DSV_SEC3_VAL(src), sibling) {
			struct data_state_val1 *_tmp;
			void *raw = (void *)(long)tmp->val.raw;
			_tmp = data_state_val1_alloc(raw);
			_tmp->bits = tmp->bits;
			_tmp->offset = tmp->offset;
			dsv_copy_data(&_tmp->val, &tmp->val);
			slist_add_tail(&_tmp->sibling, DSV_SEC3_VAL(dst));
		}
		break;
	}
	default:
	{
		BUG();
	}
	}

	dsv_set_raw(dst, (void *)(long)src->raw);
	/* TODO: trace_id_head */
	memcpy(&dst->info, &src->info, sizeof(src->info));

	return;
}

static inline void __ds_vref_setv(struct data_state_val *t,
				struct data_state_rw *ds, s32 offset, u32 bits)
{
	struct data_state_val_ref *dsvr;
	dsvr = DSV_SEC2_VAL(t);
	dsvr->ds = ds;
	dsvr->offset = offset;
	dsvr->bits = bits;
}

static inline void ds_vref_setv(struct data_state_val *t,
				struct data_state_rw *ds, s32 offset, u32 bits)
{
	switch (DS_VTYPE(ds)) {
	case DSVT_REF:
	{
		struct data_state_val_ref *dsvr;
		dsvr = DS_SEC2_VAL(ds);
		data_state_hold(dsvr->ds);
		__ds_vref_setv(t, dsvr->ds, offset + dsvr->offset, bits);
		break;
	}
	default:
	{
		data_state_hold(ds);
		__ds_vref_setv(t, ds, offset, bits);
		break;
	}
	}
}

static inline
struct data_state_rw *data_state_dup_base(struct data_state_base *base)
{
	struct data_state_rw *_new;
	void *raw = NULL;
	switch (base->ref_type) {
	case DSRT_VN:
	{
		struct var_node *vn;
		vn = (struct var_node *)(long)base->ref_base;
		raw = vn->node;
		break;
	}
	case DSRT_FN:
	{
		struct func_node *fn;
		fn = (struct func_node *)(long)base->ref_base;
		raw = fn->node;
		break;
	}
	case DSRT_RAW:
	{
		raw = (void *)(long)base->ref_base;
		break;
	}
	default:
	{
		/* should not happen here */
		err_dbg(0, "miss %d\n", base->ref_type);
		BUG();
	}
	}
	_new = data_state_rw_new(base->ref_base, base->ref_type, raw);
	return _new;
}

static inline
struct data_state_base *__data_state_find(struct slist_head *head,
					  u64 addr, u8 type)
{
	struct data_state_base *tmp;
	slist_for_each_entry(tmp, head, sibling) {
		if ((tmp->ref_base == addr) && (tmp->ref_type == type))
			return tmp;
	}

	return NULL;
}

static inline
struct data_state_base *fn_data_state_find(struct func_node *fn,
					   u64 addr, u8 type)
{
	return __data_state_find(&fn->data_state_list, addr, type);
}

static inline
struct data_state_base *fn_data_state_add(struct func_node *fn,
					  u64 addr, u8 type)
{
	struct data_state_base *ret;

	ret = fn_data_state_find(fn, addr, type);
	if (ret)
		return ret;

	/* insert into func_node, use src_buf_get to alloc */
	ret = data_state_base_new(addr, type);
	slist_add_tail(&ret->sibling, &fn->data_state_list);

	return ret;
}

static inline
struct data_state_rw *fnl_data_state_find(struct fn_list *fnl,
					  u64 addr, u8 type)
{
	struct data_state_base *base;
	struct data_state_rw *ret;
	base = __data_state_find(&fnl->data_state_list, addr, type);
	if (!base)
		return NULL;

	ret = container_of(base, struct data_state_rw, base);
	data_state_hold(ret);
	return ret;
}

static inline
struct data_state_rw *fnl_data_state_add(struct fn_list *fnl,
					 u64 addr, u8 type, void *raw)
{
	struct data_state_rw *ret;

	ret = fnl_data_state_find(fnl, addr, type);
	if (ret)
		return ret;

	ret = data_state_rw_new(addr, type, raw);
	if (slist_add_tail_check(&ret->base.sibling, &fnl->data_state_list)) {
		data_state_drop(ret);
		return NULL;
	} else {
		data_state_hold(ret);
	}

	return ret;
}

static inline
struct data_state_base *__global_data_state_base_find(u64 addr, u8 type)
{
	return __data_state_find(&si->global_data_states, addr, type);
}

static inline
struct data_state_base *global_data_state_base_find(u64 addr, u8 type)
{
	struct data_state_base *ret;
	si_lock_r();
	ret = __global_data_state_base_find(addr, type);
	si_unlock_r();
	return ret;
}

static inline
struct data_state_base *global_data_state_base_add(u64 addr, u8 type)
{
	struct data_state_base *ret;

	si_lock_w();
	ret = __global_data_state_base_find(addr, type);
	if (!ret) {
		/* likewise */
		ret = data_state_base_new(addr, type);
		slist_add_tail(&ret->sibling, &si->global_data_states);
	}
	si_unlock_w();

	return ret;
}

static inline void init_global_data_rw_states(void)
{
	struct data_state_base *base;
	slist_for_each_entry(base, &si->global_data_states, sibling) {
		struct data_state_rw *tmp;
		tmp = data_state_dup_base(base);
		slist_add_tail(&tmp->base.sibling, &si->global_data_rw_states);
	}
}

static inline
struct data_state_rw *__global_data_state_rw_find(u64 addr, u8 type)
{
	struct data_state_base *base;
	struct data_state_rw *ret;
	base = __data_state_find(&si->global_data_rw_states, addr, type);
	if (!base)
		return NULL;

	ret = container_of(base, struct data_state_rw, base);
	data_state_hold(ret);
	return ret;
}

static inline
struct data_state_rw *global_data_state_rw_find(u64 addr, u8 type)
{
	struct data_state_rw *ret;
	si_lock_w();
	if (unlikely(slist_empty(&si->global_data_rw_states)))
		init_global_data_rw_states();
	ret = __global_data_state_rw_find(addr, type);
	si_unlock_w();

	return ret;
}

static inline
struct data_state_rw *global_data_state_rw_add(u64 addr, u8 type, void *raw)
{
	struct data_state_rw *ret;

	si_lock_w();
	if (unlikely(slist_empty(&si->global_data_rw_states)))
		init_global_data_rw_states();
	ret = __global_data_state_rw_find(addr, type);
	if (!ret) {
		ret = data_state_rw_new(addr, type, raw);
		if (slist_add_tail_check(&ret->base.sibling,
					 &si->global_data_rw_states)) {
			data_state_drop(ret);
			ret = NULL;
		} else {
			data_state_hold(ret);
		}
	}
	si_unlock_w();

	return ret;
}

/*
 * @data_state_find: search for data_state_*
 * 1st, search in the global data_state_*
 * 2nd, Optional, search in the sample set allocated_data_states
 * 3rd, search in the local func_node
 */
static inline
struct data_state_base *data_state_base_find(struct sample_set *sset, int idx,
					     struct func_node *fn,
					     u64 addr, u8 type)
{
	struct data_state_base *ret;

	si_lock_r();
	ret = __global_data_state_base_find(addr, type);
	si_unlock_r();

	if (ret)
		return ret;

	if (fn)
		ret = fn_data_state_find(fn, addr, type);

	return ret;
}

static inline
struct data_state_rw *data_state_rw_find(struct sample_set *sset, int idx,
					 struct fn_list *fnl,
					 u64 addr, u8 type)
{
	struct data_state_rw *ret;

	ret = global_data_state_rw_find(addr, type);
	if (ret)
		return ret;

	if (sset) {
		struct data_state_base *base;
		base = __data_state_find(&sset->allocated_data_states,
					 addr, type);
		if (base) {
			ret = container_of(base, struct data_state_rw, base);
			data_state_hold(ret);
			return ret;
		}
	}

	if (fnl)
		ret = fnl_data_state_find(fnl, addr, type);

	return ret;
}

static inline struct fn_list *fn_list_new(struct func_node *fn)
{
	struct fn_list *_new;
	_new = (struct fn_list *)xmalloc(sizeof(*_new));
	memset(_new, 0, sizeof(*_new));
	INIT_SLIST_HEAD(&_new->data_state_list);
	INIT_SLIST_HEAD(&_new->cp_list);

	struct data_state_base *tmp0;
	struct data_state_rw *tmp1;
	slist_for_each_entry(tmp0, &fn->data_state_list, sibling) {
		tmp1 = data_state_dup_base(tmp0);
		slist_add_tail(&tmp1->base.sibling, &_new->data_state_list);
	}

	_new->fn = fn;
	return _new;
}

static inline void fn_list_free(struct fn_list *fnl)
{
	struct data_state_rw *tmp_ds, *next_ds;
	slist_for_each_entry_safe(tmp_ds, next_ds, &fnl->data_state_list,
				base.sibling) {
		slist_del(&tmp_ds->base.sibling, &fnl->data_state_list);
		data_state_drop(tmp_ds);
	}

	struct cp_list *tmp_cpl, *next_cpl;
	slist_for_each_entry_safe(tmp_cpl, next_cpl, &fnl->cp_list, sibling) {
		slist_del(&tmp_cpl->sibling, &fnl->cp_list);
		cp_list_free(tmp_cpl);
	}

	free(fnl);
}

static inline void sample_state_init_loopinfo(struct sample_state *sstate)
{
	memset(&sstate->loop_info.lhs_val, 0, sizeof(sstate->loop_info.lhs_val));
	memset(&sstate->loop_info.rhs_val, 0, sizeof(sstate->loop_info.rhs_val));
	sstate->loop_info.start = -1;
	sstate->loop_info.end = -1;
	sstate->loop_info.head = -1;
	sstate->loop_info.tail = -1;
}

static inline void sample_state_cleanup_loopinfo(struct sample_state *sstate)
{
	if (sstate->loop_info.lhs_val.value.v1)
		dsv_free_data(&sstate->loop_info.lhs_val);
	if (sstate->loop_info.rhs_val.value.v1)
		dsv_free_data(&sstate->loop_info.rhs_val);
	sample_state_init_loopinfo(sstate);
}

static inline struct sample_state *sample_state_alloc(int dyn)
{
	struct sample_state *_new;
	if (dyn)
		_new = (struct sample_state *)xmalloc(sizeof(*_new));
	else
		_new = (struct sample_state *)src_buf_get(sizeof(*_new));
	memset(_new, 0, sizeof(*_new));
	INIT_SLIST_HEAD(&_new->fn_list_head);
	INIT_SLIST_HEAD(&_new->cp_list_head);
	INIT_SLIST_HEAD(&_new->arg_head);

	sample_state_init_loopinfo(_new);

	_new->entry_curidx = -1;
	return _new;
}

static inline struct sinode **sample_state_entry_alloc(u32 count, int use_malloc)
{
	struct sinode **_ret;
	size_t total_len = count * sizeof(*_ret);

	if (use_malloc) {
		_ret = (struct sinode **)xmalloc(total_len);
	} else {
		_ret = (struct sinode **)src_buf_get(total_len);
	}
	memset(_ret, 0, total_len);

	return _ret;
}

static inline void sample_state_entry_free(struct sample_state *sstate)
{
	free(sstate->entries);
	sstate->entry_count = 0;
}

static inline void sample_empty_arg_head(struct sample_state *sample)
{
	struct data_state_rw *tmp, *next;
	slist_for_each_entry_safe(tmp, next, &sample->arg_head, base.sibling) {
		slist_del(&tmp->base.sibling, &sample->arg_head);
		data_state_drop(tmp);
	}
}

static inline void sample_state_free(struct sample_state *state)
{
	struct fn_list *tmp, *next;
	slist_for_each_entry_safe(tmp, next, &state->fn_list_head, sibling) {
		slist_del(&tmp->sibling, &state->fn_list_head);
		fn_list_free(tmp);
	}

	struct cp_list *tmp_cpl, *next_cpl;
	slist_for_each_entry_safe(tmp_cpl, next_cpl,
				&state->cp_list_head, sibling) {
		slist_del(&tmp_cpl->sibling, &state->cp_list_head);
		cp_list_free(tmp_cpl);
	}

	sample_empty_arg_head(state);

	if (state->retval && (state->retval != VOID_RETVAL))
		data_state_drop(state->retval);

	sample_state_cleanup_loopinfo(state);

	sample_state_entry_free(state);

	free(state);
}

static inline
struct fn_list *sample_add_new_fn(struct sample_state *sample,
					struct func_node *fn)
{
	struct fn_list *fnl;
	fnl = fn_list_new(fn);
	slist_add_tail(&fnl->sibling, &sample->fn_list_head);
	return fnl;
}

static inline
struct cp_list *sample_add_new_cp(struct sample_state *sample,
					struct code_path *cp)
{
	struct cp_list *cpl;
	cpl = cp_list_new(1, cp);
	slist_add_tail(&cpl->sibling, &sample->cp_list_head);
	return cpl;
}

static inline
struct fn_list *sample_state_last_fnl(struct sample_state *sstate)
{
	struct fn_list *fnl = NULL;
	if (!slist_empty(&sstate->fn_list_head))
		fnl = slist_last_entry(&sstate->fn_list_head,
					struct fn_list, sibling);
	return fnl;
}

static inline
struct sinode *sample_state_cur_entry(struct sample_state *sstate)
{
	if (sstate->entry_curidx == -1) {
		sstate->entry_curidx = 0;
		return sstate->entries[0];
	}

	if (!sample_state_last_fnl(sstate))
		sstate->entry_curidx++;

	if (sstate->entry_curidx == sstate->entry_count)
		return NULL;
	else
		return sstate->entries[sstate->entry_curidx];
}

static inline
struct cp_list *fnl_add_new_cp(struct fn_list *fnl, struct code_path *cp)
{
	struct cp_list *cpl;
	cpl = cp_list_new(1, cp);
	slist_add_tail(&cpl->sibling, &fnl->cp_list);
	return cpl;
}

static inline struct cp_list *fnl_last_cpl(struct fn_list *fnl)
{
	struct cp_list *cpl = NULL;
	if (!slist_empty(&fnl->cp_list))
		cpl = slist_last_entry(&fnl->cp_list, struct cp_list, sibling);
	return cpl;
}

static inline 
enum sample_set_flag check_dsv_flag(struct data_state_val *dsv, int action)
{
	enum sample_set_flag ret = SAMPLE_SF_OK;

	switch (action) {
	case DS_F_ACT_READ:
	{
		if (!dsv->flag.init) {
			ret = SAMPLE_SF_UNINIT;
			break;
		}

		if (dsv->flag.freed) {
			ret = SAMPLE_SF_UAF;
			break;
		}

		break;
	}
	case DS_F_ACT_WRITE:
	{
		if (dsv->flag.freed) {
			ret = SAMPLE_SF_UAF;
			break;
		}

		break;
	}
	case DS_F_ACT_ALLOCATED:
	{
		break;
	}
	case DS_F_ACT_FREED:
	{
		if (dsv->flag.freed) {
			/* double free */
			ret = SAMPLE_SF_UAF;
			break;
		}

		break;
	}
	case DS_F_ACT_INCREF:
	{
		if (dsv->flag.freed) {
			ret = SAMPLE_SF_UAF;
			break;
		}

		break;
	}
	case DS_F_ACT_DECREF:
	{
		if (atomic_read(&dsv->flag.refcount) == 0) {
			ret = SAMPLE_SF_DECERR;
			break;
		}

		if ((dsv->flag.allocated) && (!dsv->flag.freed) &&
			(atomic_read(&dsv->flag.refcount) == 1)) {
			ret = SAMPLE_SF_MEMLK;
			break;
		}

		break;
	}
	default:
	{
		BUG();
	}
	}

	return ret;
}

static inline void set_dsv_flag(struct data_state_val *dsv, int action)
{
	switch (action) {
	case DS_F_ACT_READ:
	{
		dsv->flag.used = 1;
		break;
	}
	case DS_F_ACT_WRITE:
	{
		dsv->flag.init = 1;
		dsv->flag.used = 1;
		break;
	}
	case DS_F_ACT_ALLOCATED:
	{
		dsv->flag.allocated = 1;
		break;
	}
	case DS_F_ACT_FREED:
	{
		dsv->flag.freed = 1;
		break;
	}
	case DS_F_ACT_INCREF:
	{
		atomic_inc(&dsv->flag.refcount);
		break;
	}
	case DS_F_ACT_DECREF:
	{
		atomic_dec(&dsv->flag.refcount);
		break;
	}
	default:
	{
		BUG();
	}
	}
}

static inline
enum sample_set_flag check_and_set_dsv_flag(struct data_state_val *dsv,
					    int action)
{
	enum sample_set_flag ret;

	ret = check_dsv_flag(dsv, action);
	if (ret == SAMPLE_SF_OK)
		set_dsv_flag(dsv, action);

	return ret;
}

static inline struct sample_set *sample_set_alloc(int dyn, u64 count)
{
	struct sample_set *_new;
	u64 total_len = sizeof(*_new) + count * sizeof(_new->samples[0]);
	if (dyn)
		_new = (struct sample_set *)xmalloc(total_len);
	else
		_new = (struct sample_set *)src_buf_get(total_len);

	memset(_new, 0, total_len);
	INIT_SLIST_HEAD(&_new->allocated_data_states);
	_new->count = count;
	for (u64 i = 0; i < count; i++)
		_new->samples[i] = sample_state_alloc(dyn);
	return _new;
}

static inline void sample_set_free(struct sample_set *sset)
{
	struct data_state_rw *tmp, *next;
	slist_for_each_entry_safe(tmp, next,
				 &sset->allocated_data_states, base.sibling) {
		slist_del(&tmp->base.sibling, &sset->allocated_data_states);
		data_state_drop(tmp);
	}

	for (u64 i = 0; i < sset->count; i++) {
		struct sample_state *sample;
		sample = sset->samples[i];
		sample_state_free(sample);
		sset->samples[i] = NULL;
	}
	free(sset);
}

static inline void save_sample_state(struct sample_state *dst,
					struct sample_state *src)
{
	struct cp_list *tmp;
	slist_for_each_entry(tmp, &src->cp_list_head, sibling) {
		struct cp_list *new_cpl;
		new_cpl = cp_list_new(0, tmp->cp);
		slist_add_tail(&new_cpl->sibling, &dst->cp_list_head);
	}

	if (src->entry_count) {
		dst->entries = sample_state_entry_alloc(src->entry_count, 0);
		dst->entry_count = src->entry_count;
		memcpy(dst->entries, src->entries,
				dst->entry_count * sizeof(*dst->entries));
	}
}

static inline void save_sample_set(struct sample_set *set)
{
	struct sample_set *new_set;
	new_set = sample_set_alloc(0, set->count);
	for (u64 i = 0; i < set->count; i++) {
		struct sample_state *new_sample;
		new_sample = sample_state_alloc(0);
		save_sample_state(new_sample, set->samples[i]);
		new_set->samples[i] = new_sample;
	}

	si_lock_w();
	slist_add_tail(&new_set->sibling, &si->sample_set_head);
	si_unlock_w();
}

static inline int sample_set_zeroflag(struct sample_set *sset)
{
	return (sset->flag == 0);
}

static inline int sample_set_done(struct sample_set *sset)
{
	for (u64 i = 0; i < sset->count; i++) {
		struct sample_state *sstate = sset->samples[i];
		struct sinode *sn;
		sn = sample_state_cur_entry(sstate);
		if (sn)
			return 0;
	}

	return 1;
}

static inline int sample_set_chk_flag(struct sample_set *sset, int nr)
{
	return test_bit(nr, (long *)&sset->flag);
}

static inline void sample_set_set_flag(struct sample_set *sset, int nr)
{
	if (nr >= SAMPLE_SF_MAX)
		return;
	test_and_set_bit(nr, (long *)&sset->flag);
}

static inline void sample_set_clear_flag(struct sample_set *sset, int nr)
{
	if (nr >= SAMPLE_SF_MAX)
		return;
	test_and_clear_bit(nr, (long *)&sset->flag);
}

static inline int sample_set_check_nullptr(void **pptr)
{
	void *ptr = *(pptr);
	size_t pagesize = PAGE_SIZE;
	if (ptr < (void *)pagesize)
		return 1;
	return 0;
}

static inline int setup_sibuf_loaded_max(void)
{
	struct sysinfo sinfo;
	if (sysinfo(&sinfo) == -1) {
		err_dbg(1, "sysinfo err");
		return -1;
	}

	sibuf_loaded_max = sinfo.totalram * 3 / 4;
	return 0;
}

static inline int src_init(int empty)
{
	if (setup_sibuf_loaded_max() == -1)
		return -1;

	if (empty) {
		memset(si, 0, sizeof(*si));
		INIT_SLIST_HEAD(&si->resfile_head);
		INIT_SLIST_HEAD(&si->sibuf_head);
		INIT_SLIST_HEAD(&si->global_data_states);
		INIT_SLIST_HEAD(&si->sample_set_head);
	}

	INIT_SLIST_HEAD(&si->global_data_rw_states);
	init_global_data_rw_states();

	return 0;
}

static inline u64 src_get_sset_curid(void)
{
	u64 ret = 0;
	si_lock_r();
	ret = si->sample_set_curid;
	si_unlock_r();
	return ret;
}

static inline void src_set_sset_curid(u64 id)
{
	si_lock_w();
	si->sample_set_curid = id;
	si_unlock_w();
}

static inline int src_is_linux_kernel(void)
{
	if ((si->type.os_type == SI_TYPE_OS_LINUX) &&
		(si->type.kernel == SI_TYPE_KERN))
		return 1;
	return 0;
}

#include "defdefine.h"

static inline int si_current_workdir(char *p, size_t len)
{
	if (unlikely(!si))
		return -1;

	snprintf(p, len, "%s/%s/", DEF_TMPDIR, si->src_id);
	return 0;
}

static inline int si_current_resfile(char *p, size_t len, char *name)
{
	if (unlikely(!si))
		return -1;

	memset(p, 0, len);
	if (!strchr(name, '/'))
		snprintf(p, len, "%s/%s/%s", DEF_TMPDIR, si->src_id, name);
	else
		snprintf(p, len, "%s", name);
	return 0;
}

#define	si_get_logfile() ({\
		char ____path[PATH_MAX];\
		snprintf(____path, PATH_MAX, "%s/%s/log", DEF_TMPDIR,\
				si->src_id);\
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

#define	SI_LOG_FMT(fmt)	"[%02d/%02d %02d:%02d][%s %s:%d][%s] " fmt
#define	SI_LOG_TMARGS \
	____tm.tm_mon+1, ____tm.tm_mday, \
	____tm.tm_hour, ____tm.tm_min
#define	SI_LOG_LINEINFO \
	__FILE__, __LINE__

#define	SI_LOG_GETTM() ({\
	time_t ____t = time(NULL);\
	struct tm ____tm = *localtime(&____t);\
	____tm;\
	})
#define	SI_LOG_TM() \
	struct tm ____tm = SI_LOG_GETTM()

#define	SI_LOG(LEVEL, str, fmt, ...) ({\
	SI_LOG_TM();\
	__si_log(SI_LOG_FMT(fmt),SI_LOG_TMARGS,str,\
			SI_LOG_LINEINFO,LEVEL,##__VA_ARGS__);\
	})
#define	SI_LOG_NOLINE(LEVEL, str, fmt, ...) ({\
	SI_LOG_TM();\
	__si_log(SI_LOG_FMT(fmt),SI_LOG_TMARGS,str,\
			"NOFILE",0,LEVEL,##__VA_ARGS__);\
	})

#define	si_log(fmt, ...)	SI_LOG_NOLINE("INFO", "0", fmt, ##__VA_ARGS__)

#define	si_log1(fmt, ...)	SI_LOG("INFO", "1", fmt, ##__VA_ARGS__)
#define	si_log1_todo(fmt, ...)	SI_LOG("TODO", "1", fmt, ##__VA_ARGS__)
#define	si_log1_warn(fmt, ...)	SI_LOG("WARN", "1", fmt, ##__VA_ARGS__)
#define	si_log1_err(fmt, ...)	SI_LOG(" ERR", "1", fmt, ##__VA_ARGS__)

#define	si_log2(fmt, ...)	SI_LOG("INFO", "2", fmt, ##__VA_ARGS__)
#define	si_log2_todo(fmt, ...)	SI_LOG("TODO", "2", fmt, ##__VA_ARGS__)
#define	si_log2_warn(fmt, ...)	SI_LOG("WARN", "2", fmt, ##__VA_ARGS__)
#define	si_log2_err(fmt, ...)	SI_LOG(" ERR", "2", fmt, ##__VA_ARGS__)

/* for CLIB_MODULE_CALL_FUNC */
#undef PLUGIN_SYMBOL_CONFLICT

DECL_END

#endif /* end of include guard: SI_HELPER_H_67EHZSGV */
