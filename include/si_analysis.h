/*
 * XXX: put all exported symbols here
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

#ifndef SI_ANALYSIS_H_N57L1QPA
#define SI_ANALYSIS_H_N57L1QPA

#include "si_core.h"

DECL_BEGIN

#define	STEP1			MODE_ADJUST
#define	STEP2			MODE_GETBASE
#define	STEP3			MODE_GETDETAIL
#define	STEP4			MODE_GETSTEP4
#define	STEP5			MODE_GETINDCFG1
#define	STEP6			MODE_GETINDCFG2
#define	STEPMAX			MODE_MAX

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

C_SYM struct slist_head analysis_lang_ops_head;
C_SYM lock_t getbase_lock;

struct lang_ops {
	struct slist_head	sibling;
	struct si_type		type;
	int			(*parse)(struct sibuf *buf, int parse_mode);
	int			(*dec)(struct sample_set *, int idx,
					struct func_node *start_fn);
	void			*(*get_global)(struct sibuf *, const char *,
					int *);
};

static inline struct lang_ops *lang_ops_find(struct slist_head *h,
						struct si_type *type)
{
	struct lang_ops *tmp;
	slist_for_each_entry(tmp, h, sibling) {
		if (si_type_match(&tmp->type, type))
			return tmp;
	}
	return NULL;
}

static inline void register_lang_ops(struct lang_ops *ops)
{
	struct slist_head *h = &analysis_lang_ops_head;
	if (lang_ops_find(h, &ops->type))
		return;
	slist_add_tail(&ops->sibling, h);
}

static inline void unregister_lang_ops(struct lang_ops *ops)
{
	slist_del(&ops->sibling, &analysis_lang_ops_head);
}

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

CLIB_MODULE_CALL_FUNC(analysis, sinode_match, void,
		(const char *type, void (*match)(struct sinode *, void *),
		 void *match_arg),
		3, type, match, match_arg);

CLIB_MODULE_CALL_FUNC(analysis, func_add_use_at, void,
		(struct func_node *fn, union siid id, int type,
		 void *where, unsigned long extra_info),
		5, fn, id, where, extra_info);

CLIB_MODULE_CALL_FUNC(analysis, var_add_use_at, void,
		(struct var_node *vn, union siid id, int type,
		 void *where, unsigned long extra_info),
		5, vn, id, type, where, extra_info);

CLIB_MODULE_CALL_FUNC(analysis, type_add_use_at, void,
		(struct type_node *tn, union siid id, int type,
		 void *where, unsigned long extra_info),
		5, tn, id, type, where, extra_info);

typedef void (*add_caller_alias_f)(struct sinode *, struct sinode *);
CLIB_MODULE_CALL_FUNC(analysis, add_caller, void,
		(struct sinode *callee_fsn, struct sinode *caller_fsn,
		 add_caller_alias_f add_caller_alias),
		3, callee_fsn, caller_fsn, add_caller_alias);

CLIB_MODULE_CALL_FUNC(analysis, add_callee, void,
		(struct sinode *caller_fsn, struct sinode *callee_fsn,
		 void *where, int8_t type),
		4, caller_fsn, callee_fsn, where, type);
CLIB_MODULE_CALL_FUNC(analysis, add_possible, void,
		(struct var_node *vn, unsigned long value_flag,
		 unsigned long value),
		3, vn, value_flag, value);

CLIB_MODULE_CALL_FUNC0(analysis, sibuf_new, struct sibuf *);

CLIB_MODULE_CALL_FUNC(analysis, sibuf_insert, void,
		(struct sibuf *b),
		1, b);

CLIB_MODULE_CALL_FUNC(analysis, sibuf_remove, void,
		(struct sibuf *b),
		1, b);

CLIB_MODULE_CALL_FUNC(analysis, sibuf_typenode_insert, int,
		(struct sibuf *b, struct sibuf_typenode *stn),
		2, b, stn);

CLIB_MODULE_CALL_FUNC(analysis, sibuf_typenode_search, struct type_node *,
		(struct sibuf *b, int tc, void *addr),
		3, b, tc, addr);

CLIB_MODULE_CALL_FUNC(analysis, sibuf_get_global, void *,
		(struct sibuf *b, const char *string, int *maxlen),
		3, b, string, maxlen);

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
		(struct resfile *rf),
		1, rf);

CLIB_MODULE_CALL_FUNC(analysis, resfile_get_offset, int,
		(char *path,unsigned long filecnt,unsigned long *offs),
		3, path, filecnt, offs);

CLIB_MODULE_CALL_FUNC(analysis, resfile_get_fc, struct file_content *,
		(char *path, char *targetfile, int *idx),
		3, path, targetfile, idx);

CLIB_MODULE_CALL_FUNC0(analysis, mark_entry, int);

CLIB_MODULE_CALL_FUNC(analysis, dec_next, int,
		(struct sample_set *sset, int idx),
		2, sample, idx);

CLIB_MODULE_CALL_FUNC(analysis, dec_special_call, int,
		(struct sample_set *sset, int idx, struct fn_list *fnl,
		 struct func_node *call_fn),
		4, sset, idx, fnl, call_fn);

CLIB_MODULE_CALL_FUNC(analysis, sample_set_flag_str, const char *,
		(int nr),
		1, nr);

CLIB_MODULE_CALL_FUNC(analysis, sample_state_dump_cp, void,
		(struct sample_state *sstate, int ident),
		2, sstate, ident);

CLIB_MODULE_CALL_FUNC(analysis, sample_set_dump, void,
		(struct sample_set *sset),
		1, sset);

CLIB_MODULE_CALL_FUNC(analysis, build_sample_state_till, int,
		(struct sample_set *sset, int idx, struct code_path *cp_entry,
		 struct code_path *till),
		4, sset, idx, cp_entry, till);

CLIB_MODULE_CALL_FUNC(analysis, sample_can_run, int,
		(struct sample_set *sset, int idx),
		2, sset, idx);

CLIB_MODULE_CALL_FUNC(analysis, sample_set_exists, int,
		(struct sample_set *sset),
		1, sset);

CLIB_MODULE_CALL_FUNC(analysis, sample_set_stuck, int,
		(struct sample_set *sset),
		1, sset);

CLIB_MODULE_CALL_FUNC(analysis, sample_set_replay, int,
		(struct sample_set *sset),
		1, sset);

CLIB_MODULE_CALL_FUNC(analysis, sample_set_validate, int,
		(struct sample_set *sset),
		1, sset);

CLIB_MODULE_CALL_FUNC(analysis, sample_state_check_loop, int,
		(struct sample_set *sset, int idx,
		 struct data_state_val *lhs_dsv, struct data_state_val *rhs_dsv,
		 struct code_path *next_cp),
		5, sset, idx, lhs_dsv, rhs_dsv, next_cp);

CLIB_MODULE_CALL_FUNC(analysis, sample_set_select_entries, int,
		(struct sample_set *sset),
		1, sset);

CLIB_MODULE_CALL_FUNC0(analysis, sys_bootup, int);

CLIB_MODULE_CALL_FUNC(analysis, dsv_compute, int,
		(struct data_state_val *l, struct data_state_val *r, int flag,
		 int extra_flag, cur_max_signint *retval),
		5, l, r, flag, extra_flag, retval);

DECL_END

#endif /* end of include guard: SI_ANALYSIS_H_N57L1QPA */
