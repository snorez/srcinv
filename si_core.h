/*
 * header files for this framework
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
#ifndef PARSE_AST_HEADER_H_GVBZRQCH
#define PARSE_AST_HEADER_H_GVBZRQCH

#define	_FILE_OFFSET_BITS 64
#define	xmalloc	malloc

#ifdef __cplusplus

/* for g++ compilation */
#include <gcc-plugin.h>
#include <plugin-version.h>
#include <c-tree.h>
#include <context.h>
#include <function.h>
#include <internal-fn.h>
#include <is-a.h>
#include <predict.h>
#include <basic-block.h>
#include <tree.h>
#include <tree-ssa-alias.h>
#include <gimple-expr.h>
#include <gimple.h>
#include <gimple-ssa.h>
#include <tree-pretty-print.h>
#include <tree-pass.h>
#include <tree-ssa-operands.h>
#include <tree-phinodes.h>
#include <gimple-pretty-print.h>
#include <gimple-iterator.h>
#include <gimple-walk.h>
#include <tree-core.h>
#include <dumpfile.h>
#include <print-tree.h>
#include <rtl.h>
#include <cgraph.h>
#include <cfg.h>
#include <cfgloop.h>
#include <except.h>
#include <real.h>
#include <function.h>
#include <input.h>
#include <typed-splay-tree.h>
#include <tree-pass.h>
#include <tree-phinodes.h>
#include <tree-cfg.h>
#include <cpplib.h>

#ifndef HAS_TREE_CODE_NAME
#define	HAS_TREE_CODE_NAME
#define DEFTREECODE(SYM, NAME, TYPE, LEN) NAME,
#define END_OF_BASE_TREE_CODES "@dummy",
static const char *const tree_code_name[] = {
#include "all-tree.def"
};
#undef DEFTREECODE
#undef END_OF_BASE_TREE_CODES
#endif

#else

#include "./treecodes.h"

#endif

#include <clib.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

DECL_BEGIN

/*
 * ************************************************************************
 * structures and macros and variables declare
 * ************************************************************************
 */
#define	ID_VALUE_BITS		28
#define	ID_TYPE_BITS		4
union siid {
	struct {
		unsigned int	id_value: ID_VALUE_BITS;
		unsigned int	id_type: ID_TYPE_BITS;
	} id0;
	unsigned int		id1;
};

#define	MAX_OBJS_PER_FILE (unsigned long)0x800000
#define	MAX_SIZE_PER_FILE (unsigned long)0x8000000
enum si_lang_type {
	/* gnu c */
	SI_LANG_TYPE_C,
	/* gnu c++ */
	SI_LANG_TYPE_CPP,
};

/* default mode, not used */
#define	MODE_TEST	0
/* PHASE1, adjust the pointers for every compiled file */
#define	MODE_ADJUST	1
/* PHASE2, get base info */
#define	MODE_GETBASE	2
/* PHASE3, setup every type var function data */
#define	MODE_GETDETAIL	3
/* PHASE4, get global vars initialising value */
#define	MODE_GETXREFS	4
/* PHASE5, find where global vars and type field get called, func get assigned */
#define	MODE_GETINDCFG1	5
/* PHASE6, check if every GIMPLE_CALL sentence are handled */
#define	MODE_GETINDCFG2	6
#define	MODE_MAX	7

#define	STEP1			MODE_ADJUST
#define	STEP2			MODE_GETBASE
#define	STEP3			MODE_GETDETAIL
#define	STEP4			MODE_GETXREFS
#define	STEP5			MODE_GETINDCFG1
#define	STEP6			MODE_GETINDCFG2
#define	STEPMAX			MODE_MAX

#define FC_STATUS_NONE		0
#define	FC_STATUS_ADJUSTED	MODE_ADJUST
#define	FC_STATUS_GETBASE	MODE_GETBASE
#define	FC_STATUS_GETDETAIL	MODE_GETDETAIL
#define FC_STATUS_GETXREFS	MODE_GETXREFS
#define	FC_STATUS_GETINDCFG1	MODE_GETINDCFG1
#define	FC_STATUS_GETINDCFG2	MODE_GETINDCFG2
#define	FC_STATUS_MAX		MODE_MAX

struct sibuf;
struct lang_ops {
	struct list_head	sibling;
	unsigned char		type;
	int			(*callback)(struct sibuf *buf, int parse_mode);
};

#define	METHOD_FLG_UNK		0
#define	METHOD_FLG_UNINIT	1
struct staticchk_method {
	struct list_head	sibling;
	unsigned long		method_flg;
	void			(*callback)(void);
};

#define	FC_FORMAT_BEFORE_CFG	0
#define	FC_FORMAT_AFTER_CFG	1
#define	FC_FORMAT_AFTER_CGRAPH	2
struct file_context {
	/* must be first field */
	unsigned int		total_size;

	unsigned int		objs_offs;
	unsigned int		objs_cnt;
	unsigned char		type;
	unsigned char		status	: 4;	/* not used */
	unsigned char		format	: 4;
	unsigned short		path_len;

	/*
	 * track the compiling cmds for rebuilding the source file and recompiling
	 * NOTE, cmd and path are terminated with null byte
	 */
	unsigned short		cmd_len;

	/* must be the last field */
	char			path[PATH_MAX];
} __attribute__((packed));

struct file_obj {
	unsigned long		fake_addr : 48;
	unsigned long		real_addr : 48;
	unsigned int		size;
	/* offs of the first obj(payload), must be unsigned int */
	unsigned int		offs;

	unsigned char		is_adjusted	: 1;
	unsigned char		is_dropped	: 1;
	unsigned char		is_replaced	: 1;
	unsigned int		reserved	: 26;

	/* set in collect/ modules */
	unsigned char		is_function	: 1;
	unsigned char		is_global_var	: 1;
	unsigned char		is_type		: 1;
} __attribute__((packed));

/* this buf is not expandable, the maxium size and start address is not changeable */
#define	SRC_BUF_START		(unsigned long)0x100000000
#define	SRC_BUF_BLKSZ		(unsigned long)0x10000000
/* src index information size up to 8G, maxium could be 64G */
#define	SRC_BUF_END		(unsigned long)0x300000000

/* this buf is expandable, the maxium size used is always RESFILE_BUF_SIZE */
#define	RESFILE_BUF_START	(unsigned long)0x1000000000
#define	RESFILE_BUF_SIZE	(unsigned long)0x80000000

#define	SIBUF_LOADED_MAX	(unsigned long)0x100000000

#define	TYPE_DEFINED	0
#define	TYPE_UNDEFINED	1
#define	TYPE_CANONICALED 2
#define	VAR_IS_EXTERN	0
#define	VAR_IS_STATIC	1
#define	VAR_IS_GLOBAL	2
#define	FUNC_IS_EXTERN	0
#define	FUNC_IS_STATIC	1
#define	FUNC_IS_GLOBAL	2
/* maxium 256 */
enum sinode_type {
	/* for all source files, headers */
	TYPE_FILE,

	/* types, has name and location */
	TYPE_TYPE,

	/* all functions, TODO, nested functions? */
	TYPE_FUNC_GLOBAL,
	TYPE_FUNC_STATIC,

	/* non-local vars */
	TYPE_VAR_GLOBAL,
	TYPE_VAR_STATIC,

	/* all codepath, id only, not used */
	TYPE_CODEP,

	TYPE_MAX,
};
#define	TYPE_NONE	((enum sinode_type)-1)

#define	RB_NODE_BY_ID		0
#define	RB_NODE_BY_SPEC		1
#define	RB_NODE_BY_MAX		2
/* for a whole project, all information unpacked here */
struct src {
	struct list_head	resfile_head;
	/* sibuf, for one compiled file */
	struct list_head	sibuf_head;
	struct list_head	attr_name_head;

	atomic_t		sibuf_mem_usage;
	union siid		id_idx[TYPE_MAX];

	struct rb_root		sinodes[TYPE_MAX][RB_NODE_BY_MAX];
#if 0
	struct rb_root		global_func_nodes;
	struct rb_root		static_func_nodes;
	struct rb_root		global_var_nodes;
	struct rb_root		static_var_nodes;

	/*
	 * if a type has a "location", search the loc_nodes. otherwise,
	 * if type has name, put it into type_name_nodes,
	 * type without loc, without name, put into type_code_nodes
	 */
	struct rb_root		type_loc_nodes;
	struct rb_root		type_name_nodes;
	struct rb_root		type_code_nodes;
#endif

	/* next area to mmap */
	unsigned long		next_mmap_area;

	/* what kind of this project */
	unsigned int		is_kernel: 1;
	unsigned int		is_setuid: 1;

	lock_t			lock;
};
C_SYM struct src *si;

/* for just one resfile, a project could have many resfiles */
struct resfile {
	struct list_head	sibling;

	int			fd;
	/* resfile length */
	size_t			filelen;
	/* read file_offs >= buf_offs */
	loff_t			file_offs;

	/* current mmap addr */
	unsigned long		buf_start;
	unsigned long		buf_size;
	/* current mmap offs */
	loff_t			buf_offs;

	unsigned long		total_files;
	atomic_t		parsed_files[FC_STATUS_MAX];
	/* true for the main routine, for kernel, this is the vmlinux */
	unsigned int		built_in: 1;
	/* the collect/x.so arg-x-output file */
	char			path[0];
};

/* presentation of file_context in memory */
struct sibuf {
	struct list_head	sibling;
	/* for type(no location or no name) */
	struct rb_root		file_types;
	lock_t			lock;
	struct resfile		*rf;
	/* aligned PAGE_SIZE */
	unsigned long		load_addr;
	loff_t			offs_of_resfile;
	unsigned int		total_len;

	size_t			pldlen;
	char			*payload;

	struct file_obj		*objs;
	size_t			obj_cnt;

	struct timeval		access_at;

	/* unload if true, reduce memory usage */
	unsigned int		need_unload: 1;
	unsigned int		status: 4;
} __attribute__((packed));

#define	SEARCH_BY_ID		0
#define	SEARCH_BY_SPEC		1
#define	SEARCH_BY_TYPE_NAME	2

/* the return value of check_tree_name_conflict, control how to insert */
#define	TREE_NAME_CONFLICT_FAILED	-1
#define	TREE_NAME_CONFLICT_DROP		0
#define	TREE_NAME_CONFLICT_REPLACE	1
#define	TREE_NAME_CONFLICT_SOFT		2

#define	SINODE_INSERT_BH_NONE		0
#define	SINODE_INSERT_BH_REPLACE	1
#define	SINODE_INSERT_BH_SOFT		2

struct rb_node_id {
	struct rb_node		node[RB_NODE_BY_MAX];
	union siid		id;
};
#define	SINODE_BY_ID_NODE	node_id.node[RB_NODE_BY_ID]
#define	SINODE_BY_SPEC_NODE	node_id.node[RB_NODE_BY_SPEC]

struct sinode {
	/* must be the first field */
	struct rb_node_id	node_id;

	lock_t			lock;

	char			*name;
	size_t			namelen;
	char			*data;
	size_t			datalen;

	struct sibuf		*buf;
	struct file_obj		*obj;

	/* location of the node in source */
	struct sinode		*loc_file;
	int			loc_line;
	int			loc_col;

	struct list_head	attributes;
} __attribute__((packed));

struct name_list {
	struct list_head	sibling;
	char			*name;
};

struct attr_list {
	struct list_head	sibling;
	struct list_head	values;
	char			*attr_name;
};

struct attr_value_list {
	struct list_head	sibling;
	void			*node;
};

#define	ARG_VA_LIST_NAME	"__VA_ARGS__"
struct type_node {
	unsigned short		type_code;

	unsigned int		is_unsigned: 1;
	unsigned int		is_variant: 1;
	unsigned int		is_set: 1;
	unsigned int		reserved: 13;

	void			*node;
	long			baselen;
	/* sizeof value, in bits */
	long			ofsize;

	char			*type_name;
	char			*fake_name;

	/* var_node_list for UNION_TYPE/RECORD_TYPE/ENUMERAL_TYPE/... */
	struct list_head	children;
	struct list_head	sibling;
	/* POINTER_TYPE/ARRAY_TYPE/... */
	struct type_node	*next;

	struct list_head	used_at;
} __attribute__((packed));

struct var_node {
	struct type_node	*type;
	/* in most case, this is the var tree node */
	void			*node;

	char			*name;
	char			*fake_name;

	struct list_head	used_at;
	struct list_head	possible_values;
} __attribute__((packed));

/* presentation for each instruction, sentence, etc. */
struct code_sentence {
	struct list_head	sibling;
	/* binary tree head, could be a gimple stmt */
	void			*head;
	/* most for indirect calls */
	unsigned long		handled: 1;
} __attribute__((packed));

struct code_path {
	struct func_node	*func;

#if 0
	/* for checking dead locks, and what kind of value the lock protect */
	struct list_head	locks_in;
	struct list_head	locks_out;
#endif

	/* alloc? mem read/write? free? insert into list/...? */
	unsigned long		action_flag;
	unsigned long		chk_flag0;

	struct list_head	sentences;
	/* condition sentence binary tree, could be a gimple stmt */
	void			*cond_head;
	unsigned long		branches;
	/* MUST be last field */
	struct code_path	*next[2];	/* label jump to */
} __attribute__((packed));

/* for TYPE_FUNC */
#define	LABEL_MAX		(2048+1024)
struct func_node {
	struct code_path	*codes;
	void			*node;
	char			*name;
	char			*fake_name;

	struct type_node	*ret_type;
	/* var_node_list */
	struct list_head	args;
	struct list_head	callees;
	struct list_head	callers;
	/* id_list */
	struct list_head	global_vars;
	/* var_node_list */
	struct list_head	local_vars;

	struct list_head	unreachable_stmts;

	/* this is this function used in other function exclude GIMPLE_CALL op[1] */
	struct list_head	used_at;

	/* how deep this function is */
	unsigned long		call_level;
} __attribute__((packed));

struct sibuf_type_node {
	struct rb_node		tc_node;
	union {
		struct rb_root	same_tc_root;
		struct rb_node	same_tc_node;
	} same_tc_rbtree;
	struct type_node	type;
} __attribute__((packed));
#define	same_tc_root	same_tc_rbtree.same_tc_root
#define	same_tc_node	same_tc_rbtree.same_tc_node

/* most use for global vars used in functions */
struct id_list {
	struct list_head	sibling;
	unsigned long		value;
	/* 0 for siid, 1 for tree node */
	unsigned long		value_flag;
} __attribute__((packed));

struct var_node_list {
	struct list_head	sibling;
	struct var_node		var;
} __attribute__((packed));

/* some functions may contain some statments which may never be triggered */
struct unr_stmt {
	struct list_head	sibling;
	/* low gimple statment */
	void			*unr_gimple_stmt;
	int			line;
	int			col;
} __attribute__((packed));

struct call_func_list {
	struct list_head	sibling;
	/* if id_type is TYPE_FILE, this is an internal_fn call */
	unsigned long		value;
	unsigned long		value_flag;
	/* for callee, check whether the function has a body or not */
	struct list_head	gimple_stmts;
	unsigned long		body_missing: 1;
} __attribute__((packed));

struct call_func_gimple_stmt_list {
	struct list_head	sibling;
	void			*gimple_stmt;
} __attribute__((packed));

struct use_at_list {
	struct list_head	sibling;
	union siid		func_id;
	void			*gimple_stmt;
	unsigned long		op_idx;
} __attribute__((packed));

#define	CALL_LEVEL_DEEP		64
struct func_path_list {
	struct list_head	sibling;
	struct sinode		*fsn;
};

struct code_path_list {
	struct list_head	sibling;
	struct code_path	*cp;
};

struct path_list_head {
	struct list_head	sibling;
	struct list_head	path_head;
};

#define	VALUE_IS_UNSPEC		0
#define	VALUE_IS_INT_CST	1
#define	VALUE_IS_STR_CST	2
#define	VALUE_IS_FUNC		3
#define	VALUE_IS_VAR_ADDR	4
#define	VALUE_IS_TREE		5
#define	VALUE_IS_EXPR		6
#define	VALUE_IS_REAL_CST	7
struct possible_value_list {
	struct list_head	sibling;
	unsigned long		value_flag;
	unsigned long		value;
};

/*
 * ************************************************************************
 * plugins' exported symbols
 * ************************************************************************
 */
#undef PLUGIN_SYMBOL_CONFLICT
CLIB_PLUGIN_CALL_FUNC(src, src_buf_get, void *,
		(size_t len),
		1, len);
CLIB_PLUGIN_CALL_FUNC(sinode, sinode_new, struct sinode *,
		(enum sinode_type type, char *name, size_t namelen,
		 char *data, size_t datalen),
		5, type, name, namelen, data, datalen);
CLIB_PLUGIN_CALL_FUNC(sinode, sinode_insert, int,
		(struct sinode *node, int behavior),
		2, node, behavior);
CLIB_PLUGIN_CALL_FUNC(sinode, sinode_search, struct sinode *,
		(enum sinode_type type, int flag, void *arg),
		3, type, flag, arg);
CLIB_PLUGIN_CALL_FUNC(sinode, sinode_iter, void,
		(struct rb_node *node, void (*cb)(struct rb_node *n)),
		2, node, cb);
CLIB_PLUGIN_CALL_FUNC0(sibuf, sibuf_new, struct sibuf *);
CLIB_PLUGIN_CALL_FUNC(sibuf, sibuf_insert, void,
		(struct sibuf *b),
		1, b);
CLIB_PLUGIN_CALL_FUNC(sibuf, sibuf_remove, void,
		(struct sibuf *b),
		1, b);
CLIB_PLUGIN_CALL_FUNC(sibuf, sibuf_type_node_insert, int,
		(struct sibuf *b, struct sibuf_type_node *stn),
		2, b, stn);
CLIB_PLUGIN_CALL_FUNC(sibuf, sibuf_type_node_search, struct type_node *,
		(struct sibuf *b, int tc, void *addr),
		3, b, tc, addr);
CLIB_PLUGIN_CALL_FUNC(resfile, resfile_new, struct resfile *,
		(char *path, int built_in),
		2, path, built_in);
CLIB_PLUGIN_CALL_FUNC(resfile, resfile_add, void,
		(struct resfile *rf),
		1, rf);
CLIB_PLUGIN_CALL_FUNC(resfile, resfile_read, int,
		(struct resfile *rf, struct sibuf *buf, int force),
		3, rf, buf, force);
CLIB_PLUGIN_CALL_FUNC(resfile, resfile_load, void,
		(struct sibuf *buf),
		1, buf);
CLIB_PLUGIN_CALL_FUNC(resfile, resfile_unload, void,
		(struct sibuf *buf),
		1, buf);
CLIB_PLUGIN_CALL_FUNC0(resfile, resfile_gc, int);
CLIB_PLUGIN_CALL_FUNC0(resfile, resfile_unload_all, void);
CLIB_PLUGIN_CALL_FUNC(resfile, resfile_get_filecnt, int,
		(struct resfile *rf),
		1, rf);
CLIB_PLUGIN_CALL_FUNC(debuild, debuild_type, void,
		(struct type_node *type),
		1, type);
CLIB_PLUGIN_CALL_FUNC(utils, get_func_code_pathes_start, void,
		(struct code_path *codes),
		1, codes);
CLIB_PLUGIN_CALL_FUNC0(utils, get_func_next_code_path, struct code_path *);
CLIB_PLUGIN_CALL_FUNC(utils, trace_var, int,
		(struct sinode *fsn, void *var_parm,
		 struct sinode **target_fsn, struct var_node **target_vn),
		4, fsn, var_parm, target_fsn, target_vn);
CLIB_PLUGIN_CALL_FUNC(utils, gen_func_pathes, void,
		(struct sinode *from, struct sinode *to,
		 struct list_head *head, int idx),
		4, from, to, head, idx);
CLIB_PLUGIN_CALL_FUNC(utils, drop_func_pathes, void,
		(struct list_head *head),
		1, head);
CLIB_PLUGIN_CALL_FUNC(utils, gen_code_pathes, void,
		(void *arg, struct clib_rw_pool *pool),
		2, arg, pool);
CLIB_PLUGIN_CALL_FUNC(utils, drop_code_path, void,
		(struct path_list_head *head),
		1, head);

/*
 * ************************************************************************
 * inline functions
 * ************************************************************************
 */
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

#ifdef __cplusplus

void symtab_node::dump_table(FILE *f)
{
}

static inline int check_file_type(tree node)
{
	enum tree_code tc = TREE_CODE(node);

	if (!TYPE_CANONICAL(node)) {
		return TYPE_DEFINED;
	} else if (TYPE_CANONICAL(node) != node) {
		return TYPE_CANONICALED;
	} else if (((tc == RECORD_TYPE) || (tc == UNION_TYPE)) &&
		(TYPE_CANONICAL(node) == node) &&
		(!TYPE_VALUES(node))) {
		return TYPE_UNDEFINED;
	} else {
		return TYPE_DEFINED;
	}
}

static inline int check_file_var(tree node)
{
	BUG_ON(TREE_CODE(node) != VAR_DECL);
	if (!DECL_EXTERNAL(node)) {
		if (TREE_PUBLIC(node)) {
			BUG_ON(!DECL_NAME(node));
			return VAR_IS_GLOBAL;
		} else {
			if (unlikely(!TREE_STATIC(node)))
				BUG();
			return VAR_IS_STATIC;
		}
	} else {
		BUG_ON(!DECL_NAME(node));
		return VAR_IS_EXTERN;
	}
}

static inline int check_file_func(tree node)
{
	BUG_ON(TREE_CODE(node) != FUNCTION_DECL);
	BUG_ON(!DECL_NAME(node));
	if ((!DECL_EXTERNAL(node)) || DECL_SAVED_TREE(node) ||
		(DECL_STRUCT_FUNCTION(node) &&
		 (DECL_STRUCT_FUNCTION(node)->gimple_body))) {
		/* we collect data before cfg */
		BUG_ON(DECL_SAVED_TREE(node));
		if (TREE_PUBLIC(node)) {
			return FUNC_IS_GLOBAL;
		} else {
			BUG_ON(!TREE_STATIC(node));
			return FUNC_IS_STATIC;
		}
	} else {
		return FUNC_IS_EXTERN;
	}
}

/* get location of TYPE_TYPE TYPE_VAR TYPE_FUNC */
#define	GET_LOC_TYPE	0
#define	GET_LOC_VAR	1
#define	GET_LOC_FUNC	2
static inline expanded_location *get_location(int flag, char *payload, tree node)
{
	if ((flag == GET_LOC_VAR) || (flag == GET_LOC_FUNC)) {
		return (expanded_location *)(payload + DECL_SOURCE_LOCATION(node));
	}

	/* for TYPE_TYPE */
	if (TYPE_NAME(node) && (TREE_CODE(TYPE_NAME(node)) == TYPE_DECL)) {
		return (expanded_location *)(payload +
					DECL_SOURCE_LOCATION(TYPE_NAME(node)));
	}

	/* try best to get a location of the type */
	if (((TREE_CODE(node) == RECORD_TYPE) || (TREE_CODE(node) == UNION_TYPE)) &&
		TYPE_FIELDS(node)) {
		return (expanded_location *)(payload +
				DECL_SOURCE_LOCATION(TYPE_FIELDS(node)));
	}

	return NULL;
}

static inline expanded_location *get_gimple_loc(char *payload, location_t *loc)
{
	return (expanded_location *)(payload + *loc);
}

/* get tree_identifier node name, the IDENTIFIER_NODE must not be NULL */
static inline void get_node_name(tree name, char *ret)
{
	BUG_ON(!name);
	BUG_ON(TREE_CODE(name) != IDENTIFIER_NODE);

	struct tree_identifier *node = (struct tree_identifier *)name;
	BUG_ON(!node->id.len);
	BUG_ON(node->id.len >= NAME_MAX);
	memcpy(ret, node->id.str, node->id.len);
}

static inline void get_function_name(void *addr, char *ret)
{
	tree node = (tree)addr;
	get_node_name(DECL_NAME(node), ret);
}

static inline void get_var_name(void *addr, char *ret)
{
	tree node = (tree)addr;
	if (DECL_NAME(node)) {
		get_node_name(DECL_NAME(node), ret);
	} else {
		snprintf(ret, NAME_MAX, "_%08lx", (long)addr);
		return;
	}
}

/* for type, if it doesn't have a name, use _%ld */
static inline void get_type_name(void *addr, char *ret)
{
	tree node = (tree)addr;
	if (!TYPE_NAME(node)) {
		return;
	}

	if (TREE_CODE(TYPE_NAME(node)) == IDENTIFIER_NODE) {
		get_node_name(TYPE_NAME(node), ret);
	} else {
		if (!DECL_NAME(TYPE_NAME(node))) {
			return;
		}
		get_node_name(DECL_NAME(TYPE_NAME(node)), ret);
	}
}

#if 0
static inline void gen_type_name(void *addr, char *ret, expanded_location *xloc)
{
	if (xloc && xloc->file) {
		snprintf(ret, NAME_MAX, "_%08lx%d%d", (long)xloc->file,
					xloc->line, xloc->column);
		return;
	} else {
		snprintf(ret, NAME_MAX, "_%08lx", (long)addr);
		return;
	}
}
#endif

static inline void get_type_xnode(tree node, struct sinode **sn_ret,
					struct type_node **tn_ret)
{
	char name[NAME_MAX];
	expanded_location *xloc;
	enum sinode_type type;
	struct sinode *sn = NULL;
	struct type_node *tn = NULL;
	*sn_ret = NULL;
	*tn_ret = NULL;

	struct sibuf *b = find_target_sibuf((void *)node);
	BUG_ON(!b);
	resfile__resfile_load(b);

	memset(name, 0, NAME_MAX);
	get_type_name((void *)node, name);
	xloc = get_location(GET_LOC_TYPE, b->payload, node);

	int flag;
	flag = check_file_type(node);
	if (flag == TYPE_CANONICALED) {
		node = TYPE_CANONICAL(node);

		b = find_target_sibuf((void *)node);
		BUG_ON(!b);
		resfile__resfile_load(b);

		memset(name, 0, NAME_MAX);
		get_type_name((void *)node, name);
		xloc = get_location(GET_LOC_TYPE, b->payload, node);
	} else if (flag == TYPE_UNDEFINED) {
#if 0
		BUG_ON(!name[0]);
		sn = sinode__sinode_search(TYPE_TYPE, SEARCH_BY_TYPE_NAME, name);
		*sn_ret = sn;
#endif
		return;
	}

	if (name[0] && xloc && xloc->file)
		type = TYPE_TYPE;
	else
		type = TYPE_NONE;

	if (type == TYPE_TYPE) {
		long args[4];
		struct sinode *loc_file;
		loc_file = sinode__sinode_search(TYPE_FILE, SEARCH_BY_SPEC,
							(void *)xloc->file);
		BUG_ON(!loc_file);
		args[0] = (long)loc_file;
		args[1] = xloc->line;
		args[2] = xloc->column;
		args[3] = (long)name;
		sn = sinode__sinode_search(type, SEARCH_BY_SPEC, (void *)args);
	} else {
		tn = sibuf__sibuf_type_node_search(b, TREE_CODE(node), node);
	}

	if (sn) {
		*sn_ret = sn;
	} else if (tn) {
		*tn_ret = tn;
	}
}

static inline void get_func_sinode(tree node, struct sinode **sn_ret, int flag)
{
	char name[NAME_MAX];
	expanded_location *xloc;
	enum sinode_type type;
	struct sinode *sn = NULL;

	*sn_ret = NULL;
	struct sibuf *b = find_target_sibuf((void *)node);
	BUG_ON(!b);
	resfile__resfile_load(b);

	memset(name, 0, NAME_MAX);
	get_function_name((void *)node, name);

	int func_flag = check_file_func(node);
	if (func_flag == FUNC_IS_EXTERN) {
		if (!flag)
			return;
		long args[2];
		args[0] = (long)b->rf;
		args[1] = (long)name;
		sn = sinode__sinode_search(TYPE_FUNC_GLOBAL, SEARCH_BY_SPEC,
						(void *)args);
	} else if (func_flag == FUNC_IS_GLOBAL) {
		long args[2];
		args[0] = (long)b->rf;
		args[1] = (long)name;
		sn = sinode__sinode_search(TYPE_FUNC_GLOBAL, SEARCH_BY_SPEC,
						(void *)args);
		BUG_ON(!sn);
	} else if (func_flag == FUNC_IS_STATIC) {
		xloc = get_location(GET_LOC_FUNC, b->payload, node);
		struct sinode *loc_file;
		loc_file = sinode__sinode_search(TYPE_FILE, SEARCH_BY_SPEC,
							(void *)xloc->file);
		BUG_ON(!loc_file);
		long args[4];
		args[0] = (long)loc_file;
		args[1] = xloc->line;
		args[2] = xloc->column;
		args[3] = (long)name;
		sn = sinode__sinode_search(TYPE_FUNC_STATIC, SEARCH_BY_SPEC,
						(void *)args);
	} else {
		BUG();
	}

	*sn_ret = sn;
}

static inline void get_var_sinode(tree node, struct sinode **sn_ret, int flag)
{
	char name[NAME_MAX];
	expanded_location *xloc;
	enum sinode_type type;
	struct sinode *sn = NULL;

	*sn_ret = NULL;
	struct sibuf *b = find_target_sibuf((void *)node);
	BUG_ON(!b);
	resfile__resfile_load(b);

	memset(name, 0, NAME_MAX);
	get_var_name((void *)node, name);

	int var_flag = check_file_var(node);
	if (var_flag == VAR_IS_EXTERN) {
		if (!flag)
			return;
		long args[2];
		args[0] = (long)b->rf;
		args[1] = (long)name;
		sn = sinode__sinode_search(TYPE_VAR_GLOBAL, SEARCH_BY_SPEC,
						(void *)args);
	} else if (var_flag == VAR_IS_GLOBAL) {
		long args[2];
		args[0] = (long)b->rf;
		args[1] = (long)name;
		sn = sinode__sinode_search(TYPE_VAR_GLOBAL, SEARCH_BY_SPEC,
						(void *)args);
		BUG_ON(!sn);
	} else if (var_flag == VAR_IS_STATIC) {
		xloc = get_location(GET_LOC_VAR, b->payload, node);
		struct sinode *loc_file;
		loc_file = sinode__sinode_search(TYPE_FILE, SEARCH_BY_SPEC,
							(void *)xloc->file);
		BUG_ON(!loc_file);
		long args[4];
		args[0] = (long)loc_file;
		args[1] = xloc->line;
		args[2] = xloc->column;
		args[3] = (long)name;
		sn = sinode__sinode_search(TYPE_VAR_STATIC, SEARCH_BY_SPEC,
						(void *)args);
	} else {
		BUG();
	}

	*sn_ret = sn;
}

static inline void si_log(const char *fmt, ...);
static inline void show_gimple(gimple_seq gs)
{
	tree *ops = gimple_ops(gs);
	enum gimple_code gc = gimple_code(gs);
	si_log("Statement %s\n", gimple_code_name[gc]);
	for (int i = 0; i < gimple_num_ops(gs); i++) {
		if (ops[i]) {
			enum tree_code tc = TREE_CODE(ops[i]);
			si_log("\tOp: %s %p\n", tree_code_name[tc], ops[i]);
		} else {
			si_log("\tOp: null\n");
		}
	}
}

static inline void show_func_gimples(struct sinode *fsn)
{
	resfile__resfile_load(fsn->buf);
	tree node = (tree)(long)fsn->obj->real_addr;
	gimple_seq body = DECL_STRUCT_FUNCTION(node)->gimple_body;
	gimple_seq next;

	next = body;
	si_log("gimples for function %s\n", fsn->name);

	while (next) {
		show_gimple(next);
		next = next->next;
	}
}

#endif

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
	_new = (struct name_list *)src__src_buf_get(sizeof(*_new));
	memset(_new, 0, sizeof(*_new));
	_new->name = (char *)src__src_buf_get(strlen(name)+1);
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
	_new = (struct attr_list *)src__src_buf_get(sizeof(*_new));
	memset(_new, 0, sizeof(*_new));
	INIT_LIST_HEAD(&_new->values);
	_new->attr_name = oldname;

	return _new;
}

static inline struct attr_value_list *attr_value_list_new(void)
{
	struct attr_value_list *_new;
	_new = (struct attr_value_list *)src__src_buf_get(sizeof(*_new));
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

C_SYM struct list_head lang_ops_head;
static inline struct lang_ops *lang_ops_find(unsigned char type)
{
	struct lang_ops *tmp;
	list_for_each_entry(tmp, &lang_ops_head, sibling) {
		if (tmp->type == type)
			return tmp;
	}
	return NULL;
}

static inline void register_lang_ops(struct lang_ops *ops)
{
	if (lang_ops_find(ops->type))
		return;
	list_add_tail(&ops->sibling, &lang_ops_head);
}

C_SYM struct list_head staticchk_method_head;
static inline struct staticchk_method *staticchk_method_find(
			struct staticchk_method *m)
{
	struct staticchk_method *tmp;
	list_for_each_entry(tmp, &staticchk_method_head, sibling) {
		if (tmp->method_flg == m->method_flg)
			return tmp;
	}
	return NULL;
}

static inline void register_staticchk_method(struct staticchk_method *m)
{
	if (staticchk_method_find(m))
		return;
	list_add_tail(&m->sibling, &staticchk_method_head);
}

static inline void sinode_id(enum sinode_type type, union siid *id)
{
	write_lock(&si->lock);
	union siid *type_id = &si->id_idx[type];

	unsigned long cur_val = type_id->id0.id_value;
	if (unlikely(cur_val == (unsigned long)-1)) {
		err_dbg(0, err_fmt("id exceed, type %d"), type);
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
	_new = (struct type_node *)src__src_buf_get(size_need);
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
	_new = (struct var_node *)src__src_buf_get(sizeof(*_new));
	memset(_new, 0, sizeof(*_new));
	var_node_init(_new, node);
	return _new;
}

static inline struct var_node_list *var_node_list_new(void *node)
{
	struct var_node_list *_new;
	_new = (struct var_node_list *)src__src_buf_get(sizeof(*_new));
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
	_new = (struct func_node *)src__src_buf_get(sizeof(*_new));
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
	_new = (struct code_path *)src__src_buf_get(size_need);
	memset(_new, 0, sizeof(*_new));
	code_path_init(_new);
	_new->func = func;
	_new->branches = 2+extra_branches;
	return _new;
}

static inline struct id_list *id_list_new(void)
{
	struct id_list *_new;
	_new = (struct id_list *)src__src_buf_get(sizeof(*_new));
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
	_new = (struct unr_stmt *)src__src_buf_get(sizeof(*_new));
	memset(_new, 0, sizeof(*_new));
	return _new;
}

static inline struct call_func_list *call_func_list_new(void)
{
	struct call_func_list *_new;
	_new = (struct call_func_list *)src__src_buf_get(sizeof(*_new));
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
	_new = (struct call_func_gimple_stmt_list *)src__src_buf_get(sizeof(*_new));
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
	_new = (struct use_at_list *)src__src_buf_get(sizeof(*_new));
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
	_new = (struct possible_value_list *)src__src_buf_get(sizeof(*_new));
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
	_new = (struct sibuf_type_node *)src__src_buf_get(sizeof(*_new));
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

#define	DEFAULT_PLUGIN_DIR	"/media/zerons/workspace/srcinv/plugins"
#define	DEFAULT_MIDOUT_DIR	"/media/zerons/workspace/srcinv/output"
#define	DEFAULT_LOG_FILE	"/media/zerons/workspace/srcinv/output/log.txt"
static inline void si_log(const char *fmt, ...)
{
	char logbuf[MAXLINE];
	memset(logbuf, 0, MAXLINE);

	va_list ap;
	va_start(ap, fmt);
	vsnprintf(logbuf, MAXLINE, fmt, ap);
	va_end(ap);

	int logfd = open(DEFAULT_LOG_FILE, O_WRONLY | O_APPEND);
	if (logfd == -1) {
		fprintf(stderr, "%s", logbuf);
	} else {
		int err __maybe_unused = write(logfd, logbuf, strlen(logbuf));
		close(logfd);
	}
}

#define	SI_LOG_FMT(fmt)	"[%s %d] " fmt
#define	SI_LOG(fmt, ...) \
si_log(SI_LOG_FMT(fmt), __FILE__, __LINE__, ##__VA_ARGS__)

C_SYM char src_outfile[];

DECL_END

#endif /* end of include guard: PARSE_AST_HEADER_H_GVBZRQCH */
