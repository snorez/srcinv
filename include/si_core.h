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
#include <sys/sysinfo.h>

DECL_BEGIN

/*
 * ************************************************************************
 * structures and macros and variables declare
 * ************************************************************************
 */
#ifndef BITS_PER_UNIT
#ifndef CONFIG_BITS_PER_UNIT
#define	BITS_PER_UNIT		8
#else
#define	BITS_PER_UNIT		(CONFIG_BITS_PER_UNIT)
#endif
#endif

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
enum si_type_data_fmt {
	SI_TYPE_DF_NONE = 0,
	SI_TYPE_DF_GIMPLE = 1,	/* data is from gcc, format GIMPLE */
	SI_TYPE_DF_ASM,		/* data is from asm, format binary */
	SI_TYPE_DF_ANY = 0xff,
};
struct si_type {
	uint8_t		binary: 2;/* 0: invalid 1: binary 2: src 3: both */
	uint8_t		kernel: 2;/* 0: invalid 1: kernel 2: usr 3: both */
	uint8_t 	os_type:4;
	uint8_t 	data_fmt: 8;
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
};

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

	uint16_t		gcc_global_varidx;	/* 2*8 = 16 bits */

	/* set in collect/ modules */
	uint8_t			is_function	: 1;
	uint8_t			is_global_var	: 1;
	uint8_t			is_type		: 1;

	uint8_t			is_adjusted	: 1;
	uint8_t			is_dropped	: 1;
	uint8_t			is_replaced	: 1;
	uint16_t		reserved	: 10;
};

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

C_SYM size_t sibuf_loaded_max;

/* XXX: use multiple threads to parse the file, threads*1 */
#ifndef CONFIG_ANALYSIS_THREAD
#define	THREAD_CNT		0x8
#else
#define THREAD_CNT		(CONFIG_ANALYSIS_THREAD)
#endif

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

	/* all functions, FIXME, nested functions? */
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
	struct slist_head	resfile_head;
	/* sibuf, for one compiled file */
	struct slist_head	sibuf_head;

	/* global data states */
	struct slist_head	global_data_states;
	struct slist_head	global_data_rw_states;

	/* saved sample sets */
	struct slist_head	sample_set_head;

	atomic_t		sibuf_mem_usage;
	union siid		id_idx[TYPE_MAX];

	struct rb_root		sinodes[TYPE_MAX][RB_NODE_BY_MAX];
	struct rb_root		names_root;

	rwlock_t		lock;

	u64			sample_set_curid;
	char			src_id[SRC_ID_LEN];

	/* 
	 * uint32_t		is_kernel: 1;
	 * uint32_t		reserved: 31;
	 *
	 * add type for src, actually, we only concern the KERN & OS
	 */
	struct si_type		type;
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
	struct slist_head	sibling;
	char			*name;
	char			*path;
	char			*comment;

	int8_t			category: 4;
	int8_t			autoload: 1;
	int8_t			loaded: 1;
	int8_t			reserved: 2;
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

BUILD_BUG_ON(SRC_BUF_START > SRC_BUF_END, "build arg check err");
BUILD_BUG_ON(SRC_BUF_END > RESFILE_BUF_START, "build arg check err");

/* for just one resfile, a project could have many resfiles */
struct resfile {
	struct slist_head	sibling;

	/* resfile length */
	size_t			filelen;
#if 0
	/* read file_offs >= buf_offs */
	loff_t			file_offs;

	/* current mmap addr */
	uint64_t		buf_start;
	uint64_t		buf_size;
	/* current mmap offs */
	loff_t			buf_offs;
#endif

	uint64_t		total_files;
	atomic_t		parsed_files[FC_STATUS_MAX];

	int			fd;

	/* for kernel, this is the vmlinux */
	uint8_t			built_in: 1;
	uint8_t			reserved: 7;

	/* the collect/x.so arg-x-output file */
	char			path[0];
};

struct sibuf_user {
	struct slist_head	sibling;
	pthread_t		thread_id;
};

/* presentation of file_content in memory */
struct sibuf {
	struct slist_head	sibling;
	/* for type(no location or no name) */
	struct rb_root		file_types;
	struct resfile		*rf;

	rwlock_t		lock;

	/* aligned PAGE_SIZE */
	uint64_t		load_addr;
	loff_t			offs_of_resfile;

	size_t			pldlen;
	char			*payload;

	struct file_obj		*objs;
	size_t			obj_cnt;

	/* Threads need this sibuf */
	struct slist_head	users;

	/* For some special global variables. CNT is handled by mod itself */
	void			*globals;

	uint32_t		total_len;

	/* This sibuf is loaded or not */
	u8			need_unload: 1;
	u8			status: 4;
};

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

	struct sibuf		*buf;
	struct file_obj		*obj;

	/* location of the node in source */
	struct sinode		*loc_file;
	int			loc_line;
	int			loc_col;

	struct slist_head	attributes;

	char			*name;
	size_t			namelen;

	char			*data;
	size_t			datalen;

	/* we need to know which module this sinode is from */
	int8_t			weak_flag: 1;
	int8_t			reserved: 7;
};

struct name_list {
	struct rb_node		node;
	char			name[0];
};

struct attr_list {
	struct slist_head	sibling;
	struct slist_head	values;
	char			*attr_name;
};

struct attrval_list {
	struct slist_head	sibling;
	void			*node;
};

#define	ARG_VA_LIST_NAME	"__VA_ARGS__"
struct type_node {
	rwlock_t		lock;
	void			*node;
	long			baselen;
	/* sizeof value, in bits */
	long			ofsize;

	char			*type_name;
	char			*fake_name;

	/* var_list for UNION_TYPE/RECORD_TYPE/ENUMERAL_TYPE/... */
	struct slist_head	children;
	struct slist_head	sibling;
	/* POINTER_TYPE/ARRAY_TYPE/... */
	struct type_node	*next;

	struct slist_head	used_at;

	uint16_t		type_code;
	uint16_t		is_unsigned: 1;
	uint16_t		is_variant: 1;
	uint16_t		is_set: 1;
	uint16_t		reserved: 13;
};

struct varnode_lft;
struct var_node {
	rwlock_t		lock;
	struct type_node	*type;
	/* in most case, this is the var tree node */
	void			*node;

	char			*name;
	char			*fake_name;

	struct slist_head	used_at;
	struct slist_head	possible_values;

	struct varnode_lft	*vn_lft;

	/* the memory size of this var */
	uint32_t		size;

	uint32_t		detailed: 1;
	uint32_t		reserved: 31;
};

struct code_path {
	struct func_node	*func;

	/*
	 * for gcc, cp is basic_block
	 * for asm, this is the first insn addr
	 */
	void			*cp;

	/*
	 * for gcc, this point to a GIMPLE_COND stmt
	 * for asm, this point to the Jxx insn
	 */
	void			*cond_head;

	uint64_t		branches;
	/* MUST be last field. [0] false path; [1] true path. */
	struct code_path	*next[2];	/* label jump to */
};

/* for TYPE_FUNC */
#define	LABEL_MAX		(2048+1024)
#define CALL_DEPTH_BITS		(10)
struct func_node {
	rwlock_t		lock;
	struct code_path	**cps;
	void			*node;
	char			*name;
	char			*fake_name;

	struct type_node	*ret_type;
	/* var_list */
	struct slist_head	args;
	struct slist_head	local_vars;
	/* id_list */
	struct slist_head	global_vars;
	struct slist_head	callees;
	struct slist_head	callers;

	/* this function used in other function exclude GIMPLE_CALL op[1] */
	struct slist_head	used_at;

	/* data state */
	struct slist_head	data_state_list;

	/* the memory size of this function, useful in asm mode */
	u32			size;

	u16			cp_cnt;
	u16			detailed: 1;
	u16			call_depth: CALL_DEPTH_BITS;	/* 0: not set */
	u16			reserved: 5;
};

struct sibuf_typenode {
	struct rb_node		tc_node;
	union {
		struct rb_root	same_tc_root;
		struct rb_node	same_tc_node;
	} same_tc_rbtree;
	struct type_node	type;
};
#define	same_tc_root	same_tc_rbtree.same_tc_root
#define	same_tc_node	same_tc_rbtree.same_tc_node

/* most use for global vars used in functions */
struct id_list {
	struct slist_head	sibling;
	uint64_t		value;
	/* 0 for siid, 1 for tree node */
	uint64_t		value_flag;
};

struct var_list {
	struct slist_head	sibling;
	struct var_node		var;
};

struct callf_list {
	struct slist_head	sibling;
	/* if id_type is TYPE_FILE, this is an internal_fn call */
	uint64_t		value;
	/* 0: siid, 1: tree */
	uint64_t		value_flag;
	/* for callee, check whether the function has a body or not */
	struct slist_head	stmts;
	uint64_t		body_missing: 1;
};

struct callf_stmt_list {
	struct slist_head	sibling;
	void			*where;
	int8_t			type;
};

struct use_at_list {
	struct slist_head	sibling;
	union siid		func_id;
	void			*where;
	uint64_t		extra_info;
	int8_t			type;		/* may not need this field */
};

struct cp_list {
	struct slist_head	sibling;
	struct code_path	*cp;
};

struct fn_list {
	struct slist_head	sibling;
	struct slist_head	data_state_list;
	struct slist_head	cp_list;
	struct func_node	*fn;
	void			*curpos;
};

enum possible_list_val {
	VALUE_IS_UNSPEC = 0,
	VALUE_IS_INT_CST,
	VALUE_IS_STR_CST,
	VALUE_IS_FUNC,
	VALUE_IS_VAR_ADDR,
	VALUE_IS_TREE,
	VALUE_IS_EXPR,	
	VALUE_IS_REAL_CST
};
struct possible_list {
	struct slist_head	sibling;
	u64			value;

	/* str length(exclude the nul byte) for VALUE_IS_STR_CST */
	u32			extra_param;
	u8			value_flag;
};

#ifdef CONFIG_GUESS_DSV_RANDOM
#define GUESS_DSV_RANDOM
#endif

enum data_state_ref_reg {
	DSRT_REG_INVALID,

	/* RAX */
	DSRT_REG_0,
	/* RBX */
	DSRT_REG_1,
	/* RCX */
	DSRT_REG_2,
	/* RDX */
	DSRT_REG_3,
	/* RSI */
	DSRT_REG_4,
	/* RDI */
	DSRT_REG_5,
	/* RBP */
	DSRT_REG_6,
	/* RSP */
	DSRT_REG_7,
	/* R8 */
	DSRT_REG_8,
	/* R9 */
	DSRT_REG_9,
	/* R10 */
	DSRT_REG_10,
	/* R11 */
	DSRT_REG_11,
	/* R12 */
	DSRT_REG_12,
	/* R13 */
	DSRT_REG_13,
	/* R14 */
	DSRT_REG_14,
	/* R15 */
	DSRT_REG_15,

	/* Reserved */
	DSRT_REG_16,
	DSRT_REG_17,
	DSRT_REG_18,
	DSRT_REG_19,
	DSRT_REG_20,
	DSRT_REG_21,
	DSRT_REG_22,
	DSRT_REG_23,
	DSRT_REG_24,
	DSRT_REG_25,
	DSRT_REG_26,
	DSRT_REG_27,
	DSRT_REG_28,
	DSRT_REG_29,
	DSRT_REG_30,
	DSRT_REG_31,
	DSRT_REG_32,
};

enum data_state_ref_type {
	DSRT_UNK,
	DSRT_VN,
	DSRT_FN,
	DSRT_RAW,	/* for gcc, this is tree node */
	DSRT_REG,
};

/*
 * In list:
 *	si->global_data_states
 *	func_node->data_state_list
 */
struct data_state_base {
	struct slist_head	sibling;
	/* data_state_ref */
	u64			ref_base: 48;
	u64			ref_type: 8;
	u64			ref_padding: 8;
};

enum data_state_val_type {
	/* section 0 */
	DSVT_UNK,

	/* section 1 */
	DSVT_INT_CST,	/* sec1 first */
	DSVT_REAL_CST,	/* sec1 last */

	/* section 2 */
	DSVT_ADDR,	/* address of some other data_state, sec2 first */
	DSVT_REF,	/* value stored in some other data_state, sec2 last */

	/* section 3: data_state_val1 */
	/* DSVT_ARRAY, DSVT_COMPONENT, */
	DSVT_CONSTRUCTOR,
};

enum data_state_val_compare_flag {
	DSV_COMP_F_UNK,
	DSV_COMP_F_EQ,
	DSV_COMP_F_NE,
	DSV_COMP_F_GE,
	DSV_COMP_F_GT,
	DSV_COMP_F_LE,
	DSV_COMP_F_LT,
};

enum dsv_flag_action {
	DS_F_ACT_READ,
	DS_F_ACT_WRITE,
	DS_F_ACT_ALLOCATED,
	DS_F_ACT_FREED,
	DS_F_ACT_INCREF,
	DS_F_ACT_DECREF,
};

struct dsv_flag {
	atomic_t		refcount;

	u8			allocated: 1;
	u8			freed: 1;

	/* first r/w on this data. write: set both, read: set used only. */
	u8			init: 1;
	u8			used: 1;
};

#define	DSV_SEC1_VAL(dsv)	((dsv)->value.v1)
#define	DSV_SEC2_VAL(dsv)	((dsv)->value.v2)
#define	DSV_SEC3_VAL(dsv)	(&((dsv)->value.v3))
#define	DSV_TYPE(dsv)		((dsv)->type)

#define	DS_SEC1_VAL(ds)		DSV_SEC1_VAL(&ds->val)
#define	DS_SEC2_VAL(ds)		DSV_SEC2_VAL(&ds->val)
#define	DS_SEC3_VAL(ds)		DSV_SEC3_VAL(&ds->val)
#define	DS_VTYPE(ds)		DSV_TYPE(&ds->val)

struct data_state_val {
	union {
		void				*v1;	/* section 1 */
		struct data_state_val_ref	*v2;	/* section 2 */
		struct slist_head		v3;	/* section 3 */
	} value;

	/*
	 * Make sure all data_state nodes have @raw set.
	 * @raw could be a type/decl/const node.
	 */
	u64					raw: 48;
	u64					type: 8;
	u64					reserved: 8;

	struct slist_head			trace_id_head;

	struct dsv_flag				flag;

	union {
		struct {
			u32			bytes;
			u32			sign: 1;
		} v1_info;

		struct {
			/* How many bytes this array/struct/union contains */
			u32			total_bytes;
		} v3_info;
	} info;
};

/*
 * This is the runtime data_state.
 * If it is in some slist, then should remove it first then free it.
 */
struct data_state_rw {
	/* MUST be first field */
	struct data_state_base	base;

	atomic_t		refcnt;

	struct data_state_val	val;
};

/* for DSVT_ADDR and DSVT_REF */
struct data_state_val_ref {
	struct data_state_rw	*ds;
	s32			offset;	/* in bits */
	u32			bits;
};

/* for DSVT_ARRAY and DSVT_COMPONENT */
struct data_state_val1 {
	struct slist_head	sibling;
	s32			offset;	/* offset(in bits) in data_state_rw */
	u32			bits;	/* efficient bits in val1 */
	struct data_state_val	val;
};

#define	VOID_RETVAL		((void *)-1)
struct sample_state {
	struct slist_head	fn_list_head;
	struct slist_head	cp_list_head;	/* saved into src */

	struct slist_head	arg_head;
	struct data_state_rw	*retval;

	struct {
		struct cp_list		*start;	/* the pos to search at */

		struct cp_list		*head;
		struct cp_list		*tail;
		struct data_state_val	lhs_val;
		struct data_state_val	rhs_val;
	} loop_info;

	struct sinode		**entries;
	u8			entry_count;
	s8			entry_curidx;
};

enum sample_set_flag {
	SAMPLE_SF_OK = -1,

	/* 0 - 3 */
	SAMPLE_SF_UAF = 0,
	SAMPLE_SF_NCHKRV,	/* ignore return value */
	SAMPLE_SF_VOIDRV,	/* not ignore void return value */
	SAMPLE_SF_UNINIT,	/* use uninitialized variables */

	/* 4 - 7 */
	SAMPLE_SF_OOBR,		/* out-of-bound read */
	SAMPLE_SF_OOBW,		/* out-of-bound write */
	SAMPLE_SF_INFOLK,	/* info leak */
	SAMPLE_SF_MEMLK,	/* memory leak */

	/* 8 - 11 */
	SAMPLE_SF_DEADLK,	/* dead lock */
	SAMPLE_SF_NULLREF,	/* NULL deref, maybe not inited well */
	SAMPLE_SF_INFLOOP,	/* infinite loop */

	SAMPLE_SF_DECERR = 31,	/* dec_*() mishandled the data states */
	SAMPLE_SF_MAX = 32,
};

/*
 * sample_set saved in struct src.
 * use the index in the list_head as the id of the sample set.
 */
#define	SAMPLE_SET_STATICCHK_MODE_FULL		0
#define	SAMPLE_SET_STATICCHK_MODE_QUICK		1
struct sample_set {
	struct slist_head	sibling;
	struct slist_head	allocated_data_states;

	u64			id;
	u32			flag;
	u8			count;
	u8			staticchk_mode: 1;	/* 0: full, 1: quick */
	u8			reserved0: 7;
	u8			reserved1;
	u8			reserved2;

	/* XXX: must be last field */
	struct sample_state	*samples[0];
};

struct varnode_lft {
	u64			todo;
};

#include "si_helper.h"
#include "si_collect.h"
#include "si_analysis.h"
#include "si_hacking.h"

DECL_END

#endif /* end of include guard: PARSE_AST_HEADER_H_GVBZRQCH */
