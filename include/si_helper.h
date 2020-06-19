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
C_SYM struct list_head *si_module_get_head(int category);
C_SYM struct si_module *si_module_find_by_name(char *name,
						struct list_head *head);
C_SYM struct si_module **si_module_find_by_type(struct si_type *type,
						struct list_head *head);
C_SYM int si_module_add(struct si_module *p);
C_SYM int si_module_load_all(struct list_head *head);
C_SYM int si_module_unload_all(struct list_head *head);
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

#define	SI_MOD_SUBENV_INIT_NAME	__subenv_init
#define	SI_MOD_SUBENV_DEINIT_NAME __subenv_deinit
#define	SI_MOD_SUBENV_INIT() \
static long SI_MOD_SUBENV_INIT_NAME(void)
#define	SI_MOD_SUBENV_DEINIT() \
static void SI_MOD_SUBENV_DEINIT_NAME(void)

#define	SI_MOD_SUBENV_SETUP(modname) \
SI_MOD_SUBSHELL_CMDS();\
CLIB_MODULE_NAME(modname);\
CLIB_MODULE_NEEDED0();\
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
CLIB_MODULE_INIT()\
{\
	int err;\
	err = clib_cmd_ac_add(cmdname, modname##_cb, modname##_usage);\
	if (err) {\
		err_dbg(0, "clib_cmd_ac_add err");\
		return -1;\
	}\
	return 0;\
}\
CLIB_MODULE_EXIT()\
{\
	clib_cmd_ac_del(cmdname);\
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
	if ((wider->type_more != SI_TYPE_MORE_ANY) &&
		(wider->type_more != smaller->type_more))
		return 0;
	return 1;
}

static inline struct sibuf *find_target_sibuf(void *addr)
{
	struct sibuf *tmp = NULL, *ret = NULL;
	si_lock_r();
	list_for_each_entry(tmp, &si->sibuf_head, sibling) {
		if (((unsigned long)addr >= tmp->load_addr) &&
			((unsigned long)addr < (tmp->load_addr +
						tmp->total_len))) {
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
	ret = list_first_entry_or_null(&si->resfile_head, struct resfile,
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

static inline struct name_list *__name_list_find(char *tname, size_t namelen)
{
	struct rb_root *root = &si->names_root;
	struct rb_node **newtmp = &(root->rb_node);
	struct name_list *ret = NULL;

	while (*newtmp) {
		struct name_list *data = container_of(*newtmp, struct name_list,
							node);
		size_t thislen = strlen(data->name) + 1;
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

static inline struct name_list *__name_list_add(char *tname, size_t namelen)
{
	struct name_list *nl;
	nl = __name_list_find(tname, namelen);
	if (!nl) {
		nl = name_list_new(tname, namelen);
		__name_list_insert(nl, namelen);
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
	INIT_LIST_HEAD(&_new->values);
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

static inline struct var_list *var_list_new(void *node)
{
	struct var_list *_new;
	_new = (struct var_list *)src_buf_get(sizeof(*_new));
	memset(_new, 0, sizeof(*_new));
	var_node_init(&_new->var, node);
	return _new;
}

static inline struct var_list *var_list_find(struct list_head *head,
							void *node)
{
	struct var_list *tmp;
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

	INIT_LIST_HEAD(&_new->used_at);
	return _new;
}

static inline int func_caller_internal(union siid *id)
{
	return siid_type(id) == TYPE_FILE;
}

static inline void code_path_init(struct code_path *cp)
{
	return;
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

static inline struct callf_list *callf_list_new(void)
{
	struct callf_list *_new;
	_new = (struct callf_list *)src_buf_get(sizeof(*_new));
	memset(_new, 0, sizeof(*_new));
	INIT_LIST_HEAD(&_new->stmts);
	return _new;
}

static inline struct callf_list *callf_list_find(struct list_head *head,
							unsigned long value,
							unsigned long flag)
{
	struct callf_list *tmp;
	list_for_each_entry(tmp, head, sibling) {
		if ((tmp->value == value) && (tmp->value_flag == flag))
			return tmp;
	}
	return NULL;
}

static inline struct callf_list *__add_call(struct list_head *head,
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
		list_add_tail(&newc->sibling, head);
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

static inline int callf_stmt_list_exist(struct list_head *head, int type,
					void *where)
{
	struct callf_stmt_list *tmp;
	list_for_each_entry(tmp, head, sibling) {
		if ((tmp->type == type) && (tmp->where == where))
			return 1;
	}
	return 0;
}

static inline void callf_stmt_list_add(struct list_head *head, int type,
					void *where)
{
	if (callf_stmt_list_exist(head, type, where))
		return;

	struct callf_stmt_list *_new;
	_new = callf_stmt_list_new();
	_new->where = where;
	_new->type = type;
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
							int type,
							void *where,
							unsigned long extra_info)
{
	struct use_at_list *tmp;
	list_for_each_entry(tmp, head, sibling) {
		if ((tmp->type == type) &&
			(tmp->where == where) &&
			(tmp->extra_info == extra_info))
			return tmp;
	}
	return NULL;
}

static inline struct use_at_list *__add_use_at(struct list_head *head,
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
		list_add_tail(&newua->sibling, head);
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

static inline struct possible_list *possible_list_find(struct list_head *head,
							unsigned long val_flag,
							unsigned long value)
{
	struct possible_list *tmp;
	list_for_each_entry(tmp, head, sibling) {
		if (tmp->value == value)
			return tmp;
	}
	return NULL;
}

static inline struct possible_list *__add_possible(struct list_head *head,
							unsigned long val_flag,
							unsigned long value)
{
	struct possible_list *pv;
	pv = possible_list_find(head, val_flag, value);
	if (!pv) {
		pv = possible_list_new();
		pv->value_flag = val_flag;
		pv->value = value;
		list_add_tail(&pv->sibling, head);
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

static inline struct funcp_list *funcp_list_new(void)
{
	struct funcp_list *_new;
	_new = (struct funcp_list *)xmalloc(sizeof(*_new));
	memset(_new, 0, sizeof(*_new));
	return _new;
}

static inline struct path_list *path_list_new(void)
{
	struct path_list *_new;
	_new = (struct path_list *)xmalloc(sizeof(*_new));
	memset(_new, 0, sizeof(*_new));
	INIT_LIST_HEAD(&_new->path_head);
	return _new;
}

static inline struct cp_list *cp_list_new(void)
{
	struct cp_list *_new;
	_new = (struct cp_list *)xmalloc(sizeof(*_new));
	memset(_new, 0, sizeof(*_new));
	return _new;
}

static inline struct sample_state *sample_state_new(void)
{
	struct sample_state *_new;
	_new = (struct sample_state *)xmalloc(sizeof(*_new));
	memset(_new, 0, sizeof(*_new));
	INIT_LIST_HEAD(&_new->cp_list_head);
	return _new;
}

static inline void sample_state_free(struct sample_state *state)
{
	free(state);
}

static inline void sample_state_cleanup(struct sample_state *state)
{
	struct cp_list *tmp, *next;
	list_for_each_entry_safe(tmp, next, &state->cp_list_head, sibling) {
		list_del(&tmp->sibling);
		free(tmp);
	}
}

static inline struct data_state_ref *data_state_ref_new(void)
{
	struct data_state_ref *_new;
	_new = (struct data_state_ref *)src_buf_get(sizeof(*_new));
	memset(_new, 0, sizeof(*_new));
	return _new;
}

static inline struct data_state_val *data_state_val_new(void)
{
	struct data_state_val *_new;
	_new = (struct data_state_val *)src_buf_get(sizeof(*_new));
	memset(_new, 0, sizeof(*_new));
	return _new;
}

static inline struct data_state *data_state_new(void)
{
	struct data_state *_new;
	_new = (struct data_state *)src_buf_get(sizeof(*_new));
	memset(_new, 0, sizeof(*_new));
	_new->ref = data_state_ref_new();
	_new->val = data_state_val_new();
	return _new;
}

static inline struct data_state *__data_state_find(struct cp_state *state,
				void *addr, u64 offset, u64 bits, u8 fmt)
{
	struct data_state *tmp;
	list_for_each_entry(tmp, &state->data_state_list, sibling) {
		if ((tmp->ref->addr == addr) &&
			(tmp->ref->offset == offset) &&
			(tmp->ref->bits == bits) &&
			(tmp->ref->data_fmt == fmt))
			return tmp;
	}

	return NULL;
}

static inline struct data_state *data_state_add(struct cp_state *state,
				  void *addr, u64 offset, u64 bits, u8 fmt)
{
	struct data_state *ret;

	ret = __data_state_find(state, addr, offset, bits, fmt);
	if (ret)
		return ret;

	ret = data_state_new();
	ret->ref->addr = addr;
	ret->ref->offset = offset;
	ret->ref->bits = bits;
	ret->ref->data_fmt = fmt;
	list_add_tail(&ret->sibling, &state->data_state_list);

	return ret;
}

static inline struct cp_state *cp_state_new(void)
{
	struct cp_state *_new;
	_new = (struct cp_state *)src_buf_get(sizeof(*_new));
	memset(_new, 0, sizeof(*_new));
	INIT_LIST_HEAD(&_new->data_state_list);
	return _new;
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

#define	si_log(fmt, ...) \
	SI_LOG_NOLINE("INFO", "0", fmt, ##__VA_ARGS__)

#define	si_log1(fmt, ...) \
	SI_LOG("INFO", "1", fmt, ##__VA_ARGS__)
#define	si_log1_todo(fmt, ...) \
	SI_LOG("TODO", "1", fmt, ##__VA_ARGS__)
#define	si_log1_emer(fmt, ...) \
	SI_LOG("EMER", "1", fmt, ##__VA_ARGS__)

#define	si_log2(fmt, ...) \
	SI_LOG("INFO", "2", fmt, ##__VA_ARGS__)
#define	si_log2_todo(fmt, ...) \
	SI_LOG("TODO", "2", fmt, ##__VA_ARGS__)
#define	si_log2_emer(fmt, ...) \
	SI_LOG("EMER", "2", fmt, ##__VA_ARGS__)

/* for CLIB_MODULE_CALL_FUNC */
#undef PLUGIN_SYMBOL_CONFLICT

DECL_END

#endif /* end of include guard: SI_HELPER_H_67EHZSGV */
