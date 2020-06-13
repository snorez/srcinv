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

#ifndef _GNU_SOURCE
#define	_GNU_SOURCE
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
#ifndef CONFIG_ID_VALUE_BITS
#define	ID_VALUE_BITS		28
#else
#define	ID_VALUE_BITS		(CONFIG_ID_VALUE_BITS)
#endif

#ifndef CONFIG_ID_TYPE_BITS
#define	ID_TYPE_BITS		4
#else
#define	ID_TYPE_BITS		(CONFIG_ID_TYPE_BITS)
#endif

union siid {
	struct {
		uint32_t	id_value: ID_VALUE_BITS;
		uint32_t	id_type: ID_TYPE_BITS;
	} id0;
	uint32_t		id1;
};

#define	SI_TYPE_BOTH	3
enum si_type_bin {
	SI_TYPE_SRC = 1,
	SI_TYPE_BIN,
};
enum si_type_kern {
	SI_TYPE_KERN = 1,
	SI_TYPE_USER,
};
enum si_type_os {
	SI_TYPE_OS_LINUX = 1,
	SI_TYPE_OS_OSX,
	SI_TYPE_OS_WIN,
	SI_TYPE_OS_IOS,
	SI_TYPE_OS_ANDROID,
	SI_TYPE_OS_ANY = 15,	/* 01111 */
};
enum si_type_more {
	SI_TYPE_MORE_GCC_C = 1,
	SI_TYPE_MORE_GCC_ASM,
	SI_TYPE_MORE_X86,
	SI_TYPE_MORE_X8664,
	SI_TYPE_MORE_ARM,
	SI_TYPE_MORE_ARM64,
	SI_TYPE_MORE_ANY = 0xff,
};
struct si_type {
	uint8_t		binary: 2;/* 0: invalid 1: binary 2: src 3: both */
	uint8_t		kernel: 2;/* 0: invalid 1: kernel 2: usr 3: both */
	uint8_t 	os_type:4;
	uint8_t 	type_more: 8;
};

/* default mode, not used */
#define	MODE_TEST	0
/* PHASE1, adjust the pointers */
#define	MODE_ADJUST	1
/* PHASE2, get type/var/func info */
#define	MODE_GETBASE	2
/* PHASE3, get type/var/func detail */
#define	MODE_GETDETAIL	3
/* PHASE4, get global vars initialising value */
#define	MODE_GETSTEP4	4
/* PHASE5, find where global vars and type field get called, func get assigned */
#define	MODE_GETINDCFG1	5
/* PHASE6, check if every GIMPLE_CALL stmt are handled */
#define	MODE_GETINDCFG2	6
#define	MODE_MAX	7

#define FC_STATUS_NONE		0
#define	FC_STATUS_ADJUSTED	MODE_ADJUST
#define	FC_STATUS_GETBASE	MODE_GETBASE
#define	FC_STATUS_GETDETAIL	MODE_GETDETAIL
#define FC_STATUS_GETSTEP4	MODE_GETSTEP4
#define	FC_STATUS_GETINDCFG1	MODE_GETINDCFG1
#define	FC_STATUS_GETINDCFG2	MODE_GETINDCFG2
#define	FC_STATUS_MAX		MODE_MAX

struct file_content {
	/* must be first field */
	uint32_t		total_size;
	uint32_t		objs_offs;
	uint32_t		objs_cnt;

	uint8_t			gcc_ver_major;
	uint8_t			gcc_ver_minor;
	uint16_t		path_len;

	/*
	 * track the compiling cmds for rebuilding the file and recompiling
	 * NOTE, cmd and path are terminated with null byte
	 */
	uint16_t		cmd_len;
	uint16_t		srcroot_len;

	struct si_type		type;

	/* must be the last field */
	char			path[PATH_MAX];
} __attribute__((packed));

/*
 * this is for copy information from one process(comiling) to another
 * process(srcinv-analysis)
 */
struct file_obj {
	uint64_t		fake_addr : 48;
	uint64_t		real_addr : 48;
	uint32_t		size;
	/* offs of the first obj(payload), must be uint32_t */
	uint32_t		offs;

	/* set in collect/ modules */
	uint8_t			is_function	: 1;
	uint8_t			is_global_var	: 1;
	uint8_t			is_type		: 1;

	uint8_t			is_adjusted	: 1;
	uint8_t			is_dropped	: 1;
	uint8_t			is_replaced	: 1;
	uint16_t		reserved	: 10;

	uint16_t		gcc_global_varidx;	/* 2*8 = 16 bits */
} __attribute__((packed));

/* this buf is not expandable, maxium size and start address not changeable */
#ifndef CONFIG_SRC_BUF_START
#define	SRC_BUF_START		((uint64_t)0x100000000)
#else
#define	SRC_BUF_START		((uint64_t)(CONFIG_SRC_BUF_START))
#endif

#ifndef CONFIG_SRC_BUF_BLKSZ
#define	SRC_BUF_BLKSZ		((uint64_t)0x10000000)
#else
#define	SRC_BUF_BLKSZ		((uint64_t)(CONFIG_SRC_BUF_BLKSZ))
#endif

/* src index information size up to 8G, maxium could be 64G */
#ifndef CONFIG_SRC_BUF_END
#define	SRC_BUF_END		((uint64_t)0x300000000)
#else
#define	SRC_BUF_END		((uint64_t)(CONFIG_SRC_BUF_END))
#endif

/* this buf is expandable, the maxium size used is always RESFILE_BUF_SIZE */
#ifndef CONFIG_RESFILE_BUF_START
#define	RESFILE_BUF_START	((uint64_t)0x1000000000)
#else
#define	RESFILE_BUF_START	((uint64_t)(CONFIG_RESFILE_BUF_START))
#endif

#ifndef CONFIG_RESFILE_BUF_SIZE
#define	RESFILE_BUF_SIZE	((uint64_t)0x80000000)
#else
#define	RESFILE_BUF_SIZE	((uint64_t)(CONFIG_RESFILE_BUF_SIZE))
#endif

#ifndef CONFIG_SIBUF_LOADED_MAX
#define	SIBUF_LOADED_MAX	((uint64_t)0x200000000)
#else
#define	SIBUF_LOADED_MAX	((uint64_t)(CONFIG_SIBUF_LOADED_MAX))
#endif

BUILD_BUG_ON(SRC_BUF_START > SRC_BUF_END, "build arg check err");
BUILD_BUG_ON(SRC_BUF_END > RESFILE_BUF_START, "build arg check err");

#define	TYPE_DEFINED	0
#define	TYPE_UNDEFINED	1
#define	TYPE_CANONICALED 2
#define	VAR_IS_EXTERN	0
#define	VAR_IS_STATIC	1
#define	VAR_IS_GLOBAL	2
#define	VAR_IS_LOCAL	3
#define	FUNC_IS_EXTERN	0
#if 0
#define	FUNC_IS_NONE	1	/* for fndecl has saved_tree */
#endif
#define	FUNC_IS_STATIC	2
#define	FUNC_IS_GLOBAL	3
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

#ifndef CONFIG_SI_PATH_MAX
#define	SI_PATH_MAX		128
#else
#define	SI_PATH_MAX		(CONFIG_SI_PATH_MAX)
#endif

#ifndef CONFIG_SRC_ID_LEN
#define	SRC_ID_LEN		4
#else
#define	SRC_ID_LEN		(CONFIG_SRC_ID_LEN)
#endif

/* for a whole project, all information unpacked here */
struct src {
	struct list_head	resfile_head;
	/* sibuf, for one compiled file */
	struct list_head	sibuf_head;
	struct list_head	attr_name_head;

	atomic_t		sibuf_mem_usage;
	union siid		id_idx[TYPE_MAX];

	struct rb_root		sinodes[TYPE_MAX][RB_NODE_BY_MAX];

	/* next area to mmap */
	uint64_t		next_mmap_area;

	char			src_id[SRC_ID_LEN];

	/* 
	 * uint32_t		is_kernel: 1;
	 * uint32_t		padding: 31;
	 *
	 * add type for src, actually, we only concern the KERN & OS
	 */
	struct si_type		type;

	rwlock_t		lock;
};

enum si_module_category {
	SI_PLUGIN_CATEGORY_MIN = 0,
	SI_PLUGIN_CATEGORY_CORE,

	SI_PLUGIN_CATEGORY_COLLECT,
	SI_PLUGIN_CATEGORY_ANALYSIS,
	SI_PLUGIN_CATEGORY_HACKING,

	SI_PLUGIN_CATEGORY_MAX,
};
struct si_module {
	struct list_head	sibling;
	char			*name;
	char			*path;
	char			*comment;

	int8_t			category: 4;
	int8_t			autoload: 1;
	int8_t			loaded: 1;
	int8_t			reserved: 2;

	struct si_type		type;
};

#ifndef CONFIG_MAX_OBJS_PER_FILE
#define	MAX_OBJS_PER_FILE (uint64_t)0x800000
#else
#define	MAX_OBJS_PER_FILE ((uint64_t)(CONFIG_MAX_OBJS_PER_FILE))
#endif

#ifndef CONFIG_MAX_SIZE_PER_FILE
#define	MAX_SIZE_PER_FILE (uint64_t)0x8000000
#else
#define	MAX_SIZE_PER_FILE ((uint64_t)(CONFIG_MAX_SIZE_PER_FILE))
#endif

/* for just one resfile, a project could have many resfiles */
struct resfile {
	struct list_head	sibling;

	int			fd;
	/* resfile length */
	size_t			filelen;
	/* read file_offs >= buf_offs */
	loff_t			file_offs;

	/* current mmap addr */
	uint64_t		buf_start;
	uint64_t		buf_size;
	/* current mmap offs */
	loff_t			buf_offs;

	uint64_t		total_files;
	atomic_t		parsed_files[FC_STATUS_MAX];
	/* true for the core, for kernel, this is the vmlinux */
	uint8_t			built_in: 1;
	uint8_t			reserved: 7;
	/* the collect/x.so arg-x-output file */
	char			path[0];
};

/* presentation of file_content in memory */
struct sibuf {
	struct list_head	sibling;
	/* for type(no location or no name) */
	struct rb_root		file_types;
	struct resfile		*rf;

	rwlock_t		lock;
	uint32_t		total_len;

	/* aligned PAGE_SIZE */
	uint64_t		load_addr;
	loff_t			offs_of_resfile;

	size_t			pldlen;
	char			*payload;

	struct file_obj		*objs;
	size_t			obj_cnt;

	struct timeval		access_at;

	/* unload if true, reduce memory usage */
	uint8_t			need_unload: 1;
	uint8_t			status: 4;
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

enum sinode_fmt {
	SINODE_FMT_GCC = 1,	/* data is from gcc, format GIMPLE */
	SINODE_FMT_ASM,		/* data is from asm, format binary */
};

struct sinode {
	/* must be the first field */
	struct rb_node_id	node_id;

	struct sibuf		*buf;
	struct file_obj		*obj;

	/* location of the node in source */
	struct sinode		*loc_file;
	int			loc_line;
	int			loc_col;

	struct list_head	attributes;

	char			*name;
	size_t			namelen;

	char			*data;
	size_t			datalen;
	/* we need to know which module this sinode is from */
	int8_t			data_fmt;
	int8_t			weak_flag: 1;
	int8_t			reserved: 7;
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

struct attrval_list {
	struct list_head	sibling;
	void			*node;
};

#define	ARG_VA_LIST_NAME	"__VA_ARGS__"
struct type_node {
	rwlock_t		lock;
	uint16_t		type_code;

	uint16_t		is_unsigned: 1;
	uint16_t		is_variant: 1;
	uint16_t		is_set: 1;
	uint16_t		reserved: 13;

	void			*node;
	long			baselen;
	/* sizeof value, in bits */
	long			ofsize;

	char			*type_name;
	char			*fake_name;

	/* var_list for UNION_TYPE/RECORD_TYPE/ENUMERAL_TYPE/... */
	struct list_head	children;
	struct list_head	sibling;
	/* POINTER_TYPE/ARRAY_TYPE/... */
	struct type_node	*next;

	struct list_head	used_at;
} __attribute__((packed));

struct var_node {
	rwlock_t		lock;
	struct type_node	*type;
	/* in most case, this is the var tree node */
	void			*node;

	char			*name;
	char			*fake_name;

	struct list_head	used_at;
	struct list_head	possible_values;

	uint32_t		detailed: 1;
	uint32_t		padding: 31;

	/* the memory size of this var */
	uint32_t		size;
} __attribute__((packed));

struct cp_state;
struct code_path {
	struct func_node	*func;
	struct cp_state		*state;

	/*
	 * for gcc, cp is basic_block
	 */
	void			*cp;

	/*
	 * for gcc, cond_head point to a GIMPLE_COND stmt
	 */
	void			*cond_head;

	uint64_t		branches;
	/* MUST be last field. [0] false path; [1] true path. */
	struct code_path	*next[2];	/* label jump to */
} __attribute__((packed));

/* for TYPE_FUNC */
#define	LABEL_MAX		(2048+1024)
struct func_node {
	rwlock_t		lock;
	struct code_path	*codes;
	void			*node;
	char			*name;
	char			*fake_name;

	struct type_node	*ret_type;
	/* var_list */
	struct list_head	args;
	struct list_head	callees;
	struct list_head	callers;
	/* id_list */
	struct list_head	global_vars;
	/* var_list */
	struct list_head	local_vars;

	/* this function used in other function exclude GIMPLE_CALL op[1] */
	struct list_head	used_at;

	/* how deep this function is */
	uint64_t		call_level;

	uint32_t		detailed: 1;
	uint32_t		padding: 31;

	/* the memory size of this function, useful in asm mode */
	uint32_t		size;
} __attribute__((packed));

struct sibuf_typenode {
	struct rb_node		tc_node;
	union {
		struct rb_root	same_tc_root;
		struct rb_node	same_tc_node;
	} same_tc_rbtree;
	struct type_node	type;
} __attribute__((aligned(4))); /* fix -Wpacked-not-aligned warning in gcc-8 */
#define	same_tc_root	same_tc_rbtree.same_tc_root
#define	same_tc_node	same_tc_rbtree.same_tc_node

/* most use for global vars used in functions */
struct id_list {
	struct list_head	sibling;
	uint64_t		value;
	/* 0 for siid, 1 for tree node */
	uint64_t		value_flag;
} __attribute__((packed));

struct var_list {
	struct list_head	sibling;
	struct var_node		var;
} __attribute__((packed));

struct callf_list {
	struct list_head	sibling;
	/* if id_type is TYPE_FILE, this is an internal_fn call */
	uint64_t		value;
	/* 0: siid, 1: tree */
	uint64_t		value_flag;
	/* for callee, check whether the function has a body or not */
	struct list_head	stmts;
	uint64_t		body_missing: 1;
} __attribute__((packed));

struct callf_stmt_list {
	struct list_head	sibling;
	void			*where;
	int8_t			type;
} __attribute__((packed));

struct use_at_list {
	struct list_head	sibling;
	union siid		func_id;
	void			*where;
	uint64_t		extra_info;
	int8_t			type;		/* may not need this field */
} __attribute__((packed));

#define	CALL_LEVEL_DEEP		64
struct funcp_list {
	struct list_head	sibling;
	struct sinode		*fsn;
};

struct path_list {
	struct list_head	sibling;
	struct list_head	path_head;
};

struct cp_list {
	struct list_head	sibling;
	struct code_path	*cp;
};

#define	VALUE_IS_UNSPEC		0
#define	VALUE_IS_INT_CST	1
#define	VALUE_IS_STR_CST	2
#define	VALUE_IS_FUNC		3
#define	VALUE_IS_VAR_ADDR	4
#define	VALUE_IS_TREE		5
#define	VALUE_IS_EXPR		6
#define	VALUE_IS_REAL_CST	7
struct possible_list {
	struct list_head	sibling;
	uint64_t		value_flag;
	uint64_t		value;
};

struct sample_state {
	struct list_head	cp_list_head;
};

struct cp_state {
	/* the prev could be the reason */
	struct list_head	sibling;

	struct list_head	data_state_list;

	void			*point;

	u8			data_fmt;
};

struct data_state_ref {
	void			*todo;
};

struct data_state_val {
	void			*todo;
};

struct data_state {
	struct list_head	sibling;

	struct data_state_ref	*ref;
	struct data_state_val	*val;
};

#include "si_helper.h"
#include "si_collect.h"
#include "si_analysis.h"
#include "si_hacking.h"

DECL_END

#endif /* end of include guard: PARSE_AST_HEADER_H_GVBZRQCH */
