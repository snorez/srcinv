/*
 * collect C program info compiled by GCC at ALL_IPA_PASSES_END
 *	some tree_node is not handled for now, just raise a BUG
 *
 * refs:
 *	https://gcc.gnu.org/onlinedocs/gcc-8.3.0/gccint/GTY-Options.html
 *
 * TODOs:
 *	why is there functions still contain saved_tree??
 *	decls_to_pointers in gimple_df
 *	default_defs in gimple_df
 *	tm_restart in gimple_df, this one not needed
 *
 * UPDATE:
 *	) in make_int_cst(), the total length is taken from ext_len, which
 *	  means that we should use TREE_INT_CST_EXT_NUNITS to calculate the
 *	  length. We use tree_size() to handle that.
 *
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

#define	GCC_COLLECT
#include "si_gcc.h"

#define DEFTREECODE(SYM, NAME, TYPE, LENGTH) TYPE,
#define END_OF_BASE_TREE_CODES tcc_exceptional,
const enum tree_code_class tree_code_type[] = {
#include "all-tree.def"
};
#undef DEFTREECODE
#undef END_OF_BASE_TREE_CODES

#define DEFTREECODE(SYM, NAME, TYPE, LENGTH) LENGTH,
#define END_OF_BASE_TREE_CODES 0,
const unsigned char tree_code_length[] = {
#include "all-tree.def"
};
#undef DEFTREECODE
#undef END_OF_BASE_TREE_CODES

#define DEFTREECODE(SYM, NAME, TYPE, LEN) NAME,
#define END_OF_BASE_TREE_CODES "@dummy",
static const char *const tree_code_name[] = {
#include "all-tree.def"
};
#undef DEFTREECODE
#undef END_OF_BASE_TREE_CODES

#define DEFGSSTRUCT(SYM, STRUCT, HAS_TREE_OP) \
	(HAS_TREE_OP ? sizeof (struct STRUCT) - sizeof (tree) : 0),
EXPORTED_CONST size_t gimple_ops_offset_[] = {
#include "gsstruct.def"
};
#undef DEFGSSTRUCT

#define DEFGSSTRUCT(SYM, STRUCT, HAS_TREE_OP) sizeof (struct STRUCT),
static const size_t gsstruct_code_size[] = {
#include "gsstruct.def"
};
#undef DEFGSSTRUCT

#define DEFGSCODE(SYM, NAME, GSSCODE)	NAME,
const char *const gimple_code_name[] = {
#include "gimple.def"
};
#undef DEFGSCODE

#define DEFGSCODE(SYM, NAME, GSSCODE)	GSSCODE,
EXPORTED_CONST enum gimple_statement_structure_enum gss_for_code_[] = {
#include "gimple.def"
};
#undef DEFGSCODE

#if __GNUC__ < 9
#define DEFTREECODE(SYM, STRING, TYPE, NARGS)   			    \
  (unsigned char)							    \
  ((TYPE) == tcc_unary ? GIMPLE_UNARY_RHS				    \
   : ((TYPE) == tcc_binary						    \
      || (TYPE) == tcc_comparison) ? GIMPLE_BINARY_RHS   		    \
   : ((TYPE) == tcc_constant						    \
      || (TYPE) == tcc_declaration					    \
      || (TYPE) == tcc_reference) ? GIMPLE_SINGLE_RHS			    \
   : ((SYM) == TRUTH_AND_EXPR						    \
      || (SYM) == TRUTH_OR_EXPR						    \
      || (SYM) == TRUTH_XOR_EXPR) ? GIMPLE_BINARY_RHS			    \
   : (SYM) == TRUTH_NOT_EXPR ? GIMPLE_UNARY_RHS				    \
   : ((SYM) == COND_EXPR						    \
      || (SYM) == WIDEN_MULT_PLUS_EXPR					    \
      || (SYM) == WIDEN_MULT_MINUS_EXPR					    \
      || (SYM) == DOT_PROD_EXPR						    \
      || (SYM) == SAD_EXPR						    \
      || (SYM) == REALIGN_LOAD_EXPR					    \
      || (SYM) == VEC_COND_EXPR						    \
      || (SYM) == VEC_PERM_EXPR                                             \
      || (SYM) == BIT_INSERT_EXPR					    \
      || (SYM) == FMA_EXPR) ? GIMPLE_TERNARY_RHS			    \
   : ((SYM) == CONSTRUCTOR						    \
      || (SYM) == OBJ_TYPE_REF						    \
      || (SYM) == ASSERT_EXPR						    \
      || (SYM) == ADDR_EXPR						    \
      || (SYM) == WITH_SIZE_EXPR					    \
      || (SYM) == SSA_NAME) ? GIMPLE_SINGLE_RHS				    \
   : GIMPLE_INVALID_RHS),
#else
#define DEFTREECODE(SYM, STRING, TYPE, NARGS)   			    \
  (unsigned char)							    \
  ((TYPE) == tcc_unary ? GIMPLE_UNARY_RHS				    \
   : ((TYPE) == tcc_binary						    \
      || (TYPE) == tcc_comparison) ? GIMPLE_BINARY_RHS   		    \
   : ((TYPE) == tcc_constant						    \
      || (TYPE) == tcc_declaration					    \
      || (TYPE) == tcc_reference) ? GIMPLE_SINGLE_RHS			    \
   : ((SYM) == TRUTH_AND_EXPR						    \
      || (SYM) == TRUTH_OR_EXPR						    \
      || (SYM) == TRUTH_XOR_EXPR) ? GIMPLE_BINARY_RHS			    \
   : (SYM) == TRUTH_NOT_EXPR ? GIMPLE_UNARY_RHS				    \
   : ((SYM) == COND_EXPR						    \
      || (SYM) == WIDEN_MULT_PLUS_EXPR					    \
      || (SYM) == WIDEN_MULT_MINUS_EXPR					    \
      || (SYM) == DOT_PROD_EXPR						    \
      || (SYM) == SAD_EXPR						    \
      || (SYM) == REALIGN_LOAD_EXPR					    \
      || (SYM) == VEC_COND_EXPR						    \
      || (SYM) == VEC_PERM_EXPR                                             \
      || (SYM) == BIT_INSERT_EXPR) ? GIMPLE_TERNARY_RHS			    \
   : ((SYM) == CONSTRUCTOR						    \
      || (SYM) == OBJ_TYPE_REF						    \
      || (SYM) == ASSERT_EXPR						    \
      || (SYM) == ADDR_EXPR						    \
      || (SYM) == WITH_SIZE_EXPR					    \
      || (SYM) == SSA_NAME) ? GIMPLE_SINGLE_RHS				    \
   : GIMPLE_INVALID_RHS),
#endif
#define END_OF_BASE_TREE_CODES (unsigned char) GIMPLE_INVALID_RHS,
const unsigned char gimple_rhs_class_table[] = {
#include "all-tree.def"
};
#undef DEFTREECODE
#undef END_OF_BASE_TREE_CODES

static struct plugin_info this_plugin_info = {
	.version = "0.1",
	.help = "collect function Abstract Syntax Tree at ALL_IPA_PASSES_END",
};

static int gcc_ver = __GNUC__;
static int gcc_ver_minor = __GNUC_MINOR__;
static char ver[16];
/* init in plugin_init */
struct plugin_gcc_version version_needed = {
	.basever = "",
};

int plugin_is_GPL_compatible;

static char nodes_mem[MAX_SIZE_PER_FILE];
static char *mem_ptr_start = NULL;
static char *mem_ptr = NULL;
static struct file_content *write_ctx;

static const char *outpath = "/tmp/c_ast";
static int is_kernel = 0;
static int outfd = -1;
static int prepare_outfile(void)
{
	outfd = open(outpath, O_WRONLY | O_APPEND | O_CREAT,// | O_TRUNC,
			S_IRUSR | S_IWUSR);
	if (outfd == -1)
		return -1;

	return 0;
}

static int get_compiling_args(void)
{
	/*
	 * UPDATE:
	 * for file_content, the kernel can not be SI_TYPE_BOTH
	 */
	write_ctx = (struct file_content *)nodes_mem;
	write_ctx->type.binary = SI_TYPE_SRC;
	if (is_kernel)
		write_ctx->type.kernel = SI_TYPE_KERN;
	else
		write_ctx->type.kernel = SI_TYPE_USER;
	write_ctx->type.os_type = SI_TYPE_OS_LINUX;
	write_ctx->type.data_fmt = SI_TYPE_DF_GIMPLE;
	write_ctx->gcc_ver_major = gcc_ver;
	write_ctx->gcc_ver_minor = gcc_ver_minor;

	char *ret = getcwd(write_ctx->path, PATH_MAX);
	if (!ret) {
		fprintf(stderr, "getcwd err\n");
		return -1;
	}

	char *filepath = write_ctx->path;
	size_t dir_len = strlen(filepath);
	size_t file_len = strlen(main_input_filename);

	if ((dir_len + file_len + 1) >= PATH_MAX) {
		fprintf(stderr, "path too long\n");
		return -1;
	}

	if (filepath[dir_len-1] != '/')
		filepath[dir_len] = '/';
	write_ctx->srcroot_len = strlen(filepath);

	/* XXX, the compiling file */
	memcpy(filepath+strlen(filepath), main_input_filename,
			strlen(main_input_filename)+1);
	write_ctx->path_len = strlen(filepath) + 1;

	/* XXX, the compiling args */
	char *ptr = getenv("COLLECT_GCC_OPTIONS");
	if (!ptr)
		write_ctx->cmd_len = 0;
	else {
		/* XXX, drop -fplugin args */
		char *cmd;
		cmd = (char *)fc_cmdptr((void *)nodes_mem);
		char *cur = ptr;
		char *end;
		while (1) {
			if (!*cur)
				break;

			cur = strchr(cur, '\'');
			if (!cur)
				break;
			if (!strncmp(cur+1, "-fplugin", 8)) {
				end = strchr(cur+1, '\'');
				cur = end+1;
				continue;
			}
			end = strchr(cur+1, '\'');
			memcpy(cmd, cur, end+1-cur);
			memcpy(cmd+(end+1-cur), " ", 1);

			cmd += end+1-cur+1;
			write_ctx->cmd_len += end+1-cur+1;
			cur = end + 1;
		}
		write_ctx->cmd_len += 1;	/* the last null byte */
	}
	mem_ptr_start = (char *)fc_pldptr((void *)nodes_mem);
	mem_ptr = mem_ptr_start;
	return 0;
}

static struct file_obj objs[MAX_OBJS_PER_FILE];
/* XXX: for is_obj_checked optimization */
static void *fake_addrs[MAX_OBJS_PER_FILE];
static int obj_idx = 0;
static inline int is_obj_checked(void *obj)
{
	if (unlikely(!obj))
		return 1;

	if (unlikely(obj_idx >= (int)MAX_OBJS_PER_FILE))
		BUG();

	int i = 0;
	int loop_limit = obj_idx;
	if (unlikely(loop_limit > (int)(MAX_OBJS_PER_FILE / 8))) {
		loop_limit = (int)MAX_OBJS_PER_FILE / 8;
		for (i = loop_limit; i < obj_idx; i++) {
			if (unlikely(fake_addrs[i] == obj))
				return 1;
		}
	}
	for (i = 0; i < loop_limit; i++) {
		if (unlikely(fake_addrs[i] == obj))
			return 1;
	}

	return 0;
}

static inline void insert_obj(void *obj, size_t size)
{
	objs[obj_idx].fake_addr = (unsigned long)obj;
	fake_addrs[obj_idx] = obj;
	objs[obj_idx].size = size;
	obj_idx++;
}

static void nodes_flush(int fd)
{
	if (mem_ptr == mem_ptr_start)
		return;

	int i = 0;
	for (i = 0; i < (int)MAX_OBJS_PER_FILE; i++) {
		if ((!objs[i].fake_addr) && (!objs[i].gcc_global_varidx))
			break;
	}
	BUG_ON(i != obj_idx);

	write_ctx->objs_offs = mem_ptr - nodes_mem;
	write_ctx->objs_cnt = i;
	write_ctx->total_size = write_ctx->objs_offs + i * sizeof(objs[0]);
	write_ctx->total_size = clib_round_up(write_ctx->total_size,PAGE_SIZE);
	if (unlikely((write_ctx->total_size < write_ctx->objs_offs) ||
			(write_ctx->total_size > MAX_SIZE_PER_FILE)))
		BUG();

	memcpy(mem_ptr, (char *)objs, i*sizeof(objs[0]));

	int err = write(fd, nodes_mem, write_ctx->total_size);
	if (unlikely(err == -1))
		BUG();
}

/* NOTE, use this function paired with is_obj_checked */
static void mem_write(void *addr, size_t size)
{
	if (unlikely(!addr))
		return;
	insert_obj(addr, size);
	memcpy(mem_ptr, addr, size);
	mem_ptr += size;
	if (unlikely((mem_ptr < nodes_mem) ||
		((unsigned long)(mem_ptr - nodes_mem) > MAX_SIZE_PER_FILE)))
		BUG();
}

/* NOTE, we need to keep a expanded_location tracked */
static unsigned long xloc_addr = (unsigned long)-1;
static void mem_write_loc(void *addr, size_t size)
{
	insert_obj((void *)xloc_addr, size);
	xloc_addr--;
	memcpy(mem_ptr, addr, size);
	mem_ptr += size;
	if (unlikely((mem_ptr < nodes_mem) ||
		((unsigned long)(mem_ptr - nodes_mem) > MAX_SIZE_PER_FILE)))
		BUG();
}

static void mem_write_last_char(char c)
{
	*(mem_ptr-1) = c;
}

static void mem_write_path(void *addr)
{
	char path[PATH_MAX];
	BUG_ON(!realpath((char *)addr, path));

	insert_obj((void *)addr, strlen(path)+1);
	memcpy(mem_ptr, path, strlen(path)+1);
	mem_ptr += strlen(path)+1;
	if (unlikely((mem_ptr < nodes_mem) ||
		((unsigned long)(mem_ptr - nodes_mem) > MAX_SIZE_PER_FILE)))
		BUG();
}

static void node_write(tree node)
{
	mem_write((void *)node, tree_size(node));
}


/*
 * ************************************************************************
 * some strucutures need to be dumped
 * ************************************************************************
 */
static void do_tree(tree node);
static void do_location(location_t loc)
{
	/* NOTE, all calls must put at the first place? */
	expanded_location xloc = expand_location(loc);
	mem_write_loc((void *)&xloc, sizeof(xloc));
	if (!is_obj_checked((void *)xloc.file)) {
		if (unlikely(!strncmp(xloc.file, "<built-in>", 10)))
			mem_write((void *)xloc.file, strlen(xloc.file)+1);
		else
			mem_write_path((void *)xloc.file);
	}
}

static void do_vec_tree(void *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;

	vec<tree, va_gc, vl_embed> *node0;
	node0 = (vec<tree, va_gc, vl_embed> *)node;

	unsigned long length = node0->length();
	tree *addr = node0->address();
	if (flag) {
		if (length)
			mem_write(node, sizeof(*node0)+
					(length-1)*sizeof(*addr));
		else
			mem_write(node, sizeof(*node0));
	}
	for (unsigned long i = 0; i < length; i++) {
		do_tree(addr[i]);
	}
}

static void do_vec_constructor(void *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;

	vec<constructor_elt, va_gc> *node0;
	node0 = (vec<constructor_elt, va_gc> *)node;

	unsigned long length = node0->length();
	struct constructor_elt *addr = node0->address();
	if (flag) {
		if (length)
			mem_write(node, sizeof(*node0)+
					(length-1)*sizeof(*addr));
		else
			mem_write(node, sizeof(*node0));
	}
	for (unsigned long i = 0; i < length; i++) {
		do_tree(addr[i].index);
		do_tree(addr[i].value);
	}
}

struct GTY((chain_next ("%h.outer"))) c_scope { /* gcc/c/c-decl.c */
	struct c_scope *outer;
	struct c_scope *outer_function;
	struct c_binding *bindings;
	tree blocks;
	tree blocks_last;
	unsigned int depth : 28;
	BOOL_BITFIELD parm_flag : 1;
	BOOL_BITFIELD had_vla_unspec : 1;
	BOOL_BITFIELD warned_forward_parm_decls : 1;
	BOOL_BITFIELD function_body : 1;
	BOOL_BITFIELD keep : 1;
	BOOL_BITFIELD float_const_decimal64 : 1;
	BOOL_BITFIELD has_label_bindings : 1;
	BOOL_BITFIELD has_jump_unsafe_decl : 1;
};
static void do_c_binding(struct c_binding *node, int flag);
static void do_c_scope(struct c_scope *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;
	if (flag)
		mem_write((void *)node, sizeof(*node));

	do_c_scope(node->outer, 1);
	do_c_scope(node->outer_function, 1);
	do_c_binding(node->bindings, 1);
	do_tree(node->blocks);
	do_tree(node->blocks_last);
}

struct GTY(()) c_spot_bindings { /* gcc/c/c-decl.c */
	struct c_scope *scope;
	struct c_binding *bindings_in_scope;
	int stmt_exprs;
	bool left_stmt_expr;
};
static void do_c_spot_bindings(struct c_spot_bindings *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;
	if (flag)
		mem_write((void *)node, sizeof(*node));

	do_c_scope(node->scope, 1);
	do_c_binding(node->bindings_in_scope, 1);
}

struct GTY(()) c_goto_bindings { /* gcc/c/c-decl.c */
	location_t loc;
	struct c_spot_bindings goto_bindings;
};
typedef struct c_goto_bindings *c_goto_bindings_p;
static void do_c_goto_bindings(struct c_goto_bindings *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;
	if (flag)
		mem_write((void *)node, sizeof(*node));
	do_location(node->loc);
	do_c_spot_bindings(&node->goto_bindings, 0);
}
static void do_vec_c_goto_bindings(void *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;

	vec<c_goto_bindings_p, va_gc> *node0;
	node0 = (vec<c_goto_bindings_p, va_gc> *)node;
	unsigned long len = node0->length();
	c_goto_bindings_p *addr = node0->address();
	if (flag) {
		if (len)
			mem_write(node, sizeof(*node0)+(len-1)*sizeof(*addr));
		else
			mem_write(node, sizeof(*node0));
	}
	for (unsigned long i = 0; i < len; i++) {
		do_c_goto_bindings(addr[i], 1);
	}
}

struct GTY(()) c_label_vars { /* gcc/c/c-decl.c */
	struct c_label_vars *shadowed;
	struct c_spot_bindings label_bindings;
	vec<tree, va_gc> *decls_in_scope;
	vec<c_goto_bindings_p, va_gc> *gotos;
};
static void do_c_label_vars(struct c_label_vars *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;
	if (flag)
		mem_write((void *)node, sizeof(*node));

	do_c_label_vars(node->shadowed, 1);
	do_c_spot_bindings(&node->label_bindings, 0);
	do_vec_tree((void *)node->decls_in_scope, 1);
	do_vec_c_goto_bindings((void *)node->gotos, 1);
}

struct GTY((chain_next ("%h.prev"))) c_binding { /* gcc/c/c-decl.c */
	union GTY(()) {
		tree GTY((tag ("0"))) type;
		struct c_label_vars * GTY((tag ("1"))) label;
	} GTY((desc ("TREE_CODE (%0.decl) == LABEL_DECL"))) u;
	tree decl;
	tree id;
	struct c_binding *prev;
	struct c_binding *shadowed;
	unsigned int depth : 28;
	BOOL_BITFIELD invisible : 1;
	BOOL_BITFIELD nested : 1;
	BOOL_BITFIELD inner_comp : 1;
	BOOL_BITFIELD in_struct : 1;
	location_t locus;
};
static void do_c_binding(struct c_binding *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;
	if (flag)
		mem_write(node, sizeof(*node));
	do_location(node->locus);
	do_tree(node->decl);
	if (TREE_CODE(node->decl) != LABEL_DECL)
		do_tree(node->u.type);
	else
		do_c_label_vars(node->u.label, 1);
	do_tree(node->id);
	do_c_binding(node->prev, 1);
	do_c_binding(node->shadowed, 1);
}

static void do_cpp_macro(struct cpp_macro *node, int flag);
static void do_ht_identifier(struct ht_identifier *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;

	struct ht_identifier *id = (struct ht_identifier *)node;
	if (flag)
		mem_write((void *)id, sizeof(*id));
	if (!is_obj_checked((void *)id->str))
		mem_write((void *)id->str, id->len);
}

static void do_answer(struct answer *node, int flag);
static void do_cpp_hashnode(struct cpp_hashnode *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;
	if (flag)
		mem_write((void *)node, sizeof(*node));
	do_ht_identifier(&node->ident, 0);
#ifdef COLLECT_MORE
	struct cpp_hashnode node0 = *node;
	switch (CPP_HASHNODE_VALUE_IDX(node0)) {
	case NTV_MACRO:
		do_cpp_macro(node->value.macro, 1);
		break;
	case NTV_ANSWER:
		do_answer(node->value.answers, 1);
		break;
	case NTV_BUILTIN:
	case NTV_ARGUMENT:
	case NTV_NONE:
		break;
	default:
		fprintf(stderr, "%d\n", CPP_HASHNODE_VALUE_IDX(node0));
		BUG();
	}
#endif
}

static void do_cpp_marco_arg(struct cpp_macro_arg *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;
	if (flag)
		mem_write((void *)node, sizeof(*node));
	do_cpp_hashnode(node->spelling, 1);
}

static void do_cpp_string(struct cpp_string *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;
	if (flag)
		mem_write((void *)node, sizeof(*node));
	if (!is_obj_checked((void *)node->text))
		mem_write((void *)node->text, node->len);
}

static void do_cpp_identifier(struct cpp_identifier *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;
	if (flag)
		mem_write((void *)node, sizeof(*node));
	do_cpp_hashnode(node->node, 1);
	do_cpp_hashnode(node->spelling, 1);
}

static void do_cpp_token(struct cpp_token *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;
	if (flag) {
		mem_write((void *)node, sizeof(*node)+1);
		mem_write_last_char((char)cpp_token_val_index(node));
	}
	do_location(node->src_loc);
	switch (cpp_token_val_index(node)) {
	case CPP_TOKEN_FLD_NODE:
		do_cpp_identifier(&node->val.node, 0);
		break;
	case CPP_TOKEN_FLD_SOURCE:
		do_cpp_token(node->val.source, 1);
		break;
	case CPP_TOKEN_FLD_STR:
		do_cpp_string(&node->val.str, 0);
		break;
	case CPP_TOKEN_FLD_ARG_NO:
		do_cpp_marco_arg(&node->val.macro_arg, 0);
		break;
	case CPP_TOKEN_FLD_TOKEN_NO:
	case CPP_TOKEN_FLD_PRAGMA:
		break;
	default:
		BUG();
	}
}

#if __GNUC__ < 9
struct GTY(()) cpp_macro { /* libcpp/include/cpp-id-data.h */
	cpp_hashnode **GTY((nested_ptr(union tree_node,
			"%h?CPP_HASHNODE(GCC_IDENT_TO_HT_IDENT(%h)):NULL",
			"%h?HT_IDENT_TO_GCC_IDENT(HT_NODE(%h)):NULL"),
			length("%h.paramc"))) params;
	union cpp_macro_u {
		cpp_token *GTY((tag("0"), length("%0.count"))) tokens;
		const unsigned char *GTY((tag("1"))) text;
	} GTY((desc("%1.traditional"))) exp;
	source_location line;
	unsigned int count;
	unsigned short paramc;
	unsigned int fun_like : 1;
	unsigned int variadic : 1;
	unsigned int syshdr : 1;
	unsigned int traditional : 1;
	unsigned int extra_tokens : 1;
};
static void __maybe_unused do_cpp_macro(struct cpp_macro *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;
	if (flag)
		mem_write((void *)node, sizeof(*node));
	do_location(node->line);
	if (node->params && !is_obj_checked(node->params)) {
		mem_write((void *)node->params,
				node->paramc*sizeof(node->params[1]));
		cpp_hashnode **addr = node->params;
		for (unsigned short i = 0; i < node->paramc; i++) {
			do_cpp_hashnode(addr[i], 1);
		}
	}
	switch (node->traditional) {
	case 0:
		if (node->exp.tokens && !is_obj_checked(node->exp.tokens)) {
			mem_write((void *)node->exp.tokens,
				sizeof(struct cpp_token) * node->count);
			struct cpp_token *addr = node->exp.tokens;
			for (unsigned int i = 0; i < node->count; i++) {
				do_cpp_token(&addr[i], 0);
			}
		}
		break;
	case 1:
		/* FIXME, the text looks like an address */
		if (!is_obj_checked((void *)(node->exp.text)))
			mem_write((void *)node->exp.text,
				strlen((const char *)(node->exp.text))+1);
		break;
	default:
		BUG();
	}
}
#else
static void __maybe_unused do_cpp_macro(struct cpp_macro *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;
	if (flag)
		mem_write((void *)node, sizeof(*node));
	do_location(node->line);
	if (node->kind == cmk_assert) {
		do_cpp_macro(node->parm.next, 1);
	} else {
		cpp_hashnode **addr = node->parm.params;
		if (addr && !is_obj_checked(addr)) {
			mem_write((void *)addr, node->paramc*sizeof(addr[1]));
			for (unsigned short i = 0; i < node->paramc; i++) {
				do_cpp_hashnode(addr[i], 1);
			}
		}
	}
	if (node->kind == cmk_traditional) {
		/* FIXME, the text looks like an address */
		if (!is_obj_checked((void *)(node->exp.text)))
			mem_write((void *)node->exp.text,
				strlen((const char *)(node->exp.text))+1);
	} else {
		if (node->exp.tokens && !is_obj_checked(node->exp.tokens)) {
			mem_write((void *)node->exp.tokens,
				sizeof(struct cpp_token) * node->count);
			struct cpp_token *addr = node->exp.tokens;
			for (unsigned int i = 0; i < node->count; i++) {
				do_cpp_token(&addr[i], 0);
			}
		}
	}
}
#endif

struct GTY(()) answer {
	struct answer *next;
	unsigned int count;
	cpp_token GTY((length("%h.count"))) first[1];
};
static void do_answer(struct answer *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;
	unsigned int len = node->count;
	BUG_ON(!len);
	if (flag)
		mem_write((void *)node,
				sizeof(*node)+(len-1)*sizeof(cpp_token));
	do_answer(node->next, 1);
	for (unsigned int i = 0; i < len; i++) {
		do_cpp_token(&node->first[i], 0);
	}
}

static void do_common(tree node, int flag);
static void do_c_common_identifier(struct c_common_identifier *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;
	if (flag)
		mem_write((void *)node, sizeof(*node));
	do_common((tree)&node->common, 0);
	do_cpp_hashnode(&node->node, 0);
}

static void do_real_value(struct real_value *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;
	if (flag)
		mem_write(node, sizeof(*node));
}

static void do_sorted_fields_type(struct sorted_fields_type *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;
	BUG_ON(!node->len);
	int cnt = node->len;
	if (flag)
		mem_write((void *)node, sizeof(*node) + (cnt-1)*sizeof(tree));
	for (int i = 0; i < cnt; i++) {
		do_tree(node->elts[i]);
	}
}

struct GTY(()) c_lang_type {
	struct sorted_fields_type * GTY ((reorder("resort_sorted_fields"))) s;
	tree enum_min;
	tree enum_max;
	tree objc_info;
};
static void do_c_lang_type(void *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;

	struct c_lang_type *node0 = (struct c_lang_type *)node;
	if (flag)
		mem_write(node, sizeof(*node0));
	do_sorted_fields_type(node0->s, 1);
	do_tree(node0->enum_min);
	do_tree(node0->enum_max);
	do_tree(node0->objc_info);
}

struct GTY(()) c_lang_decl {
	char dummy;
};
static void do_c_lang_decl(struct c_lang_decl *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;
	if (flag)
		mem_write((void *)node, sizeof(*node));
}

static void do_symtab_node(void *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;

	struct symtab_node *node0 = (struct symtab_node *)node;
	if (flag)
		mem_write(node, sizeof(*node0));

	do_tree(node0->decl);
	do_symtab_node((void *)node0->next, 1);
	do_symtab_node((void *)node0->previous, 1);
	do_symtab_node((void *)node0->next_sharing_asm_name, 1);
	do_symtab_node((void *)node0->previous_sharing_asm_name, 1);
	do_symtab_node((void *)node0->same_comdat_group, 1);
	do_tree(node0->alias_target);
	do_tree(node0->x_comdat_group);
	/* TODO */
#if 0
	BUG_ON(node0->ref_list.references);
	vec<ipa_ref_ptr> tmp_referring = node0->ref_list.referring;
	unsigned long length = tmp_referring.length();
	ipa_ref_ptr *addr = tmp_referring.address();
	for (unsigned long i = 0; i < length; i++)
		BUG_ON(addr[i]);
	BUG_ON(node0->lto_file_data);
	BUG_ON(node0->aux);
	BUG_ON(node0->x_section);
#endif
}

#if 0
struct GTY(()) stmt_tree_s {
	vec<tree, va_gc> *x_cur_stmt_list;
	int stmts_are_full_exprs_p;
};
#endif
static void do_stmt_tree_s(struct stmt_tree_s *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;
	if (flag)
		mem_write((void *)node, sizeof(*node));
	do_vec_tree((void *)node->x_cur_stmt_list, 1);
}

#if 0
struct GTY(()) c_language_function {
	struct stmt_tree_s x_stmt_tree;
	vec<tree, va_gc> *local_typedefs;
};
#endif
static void do_c_language_function(struct c_language_function *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;
	if (flag)
		mem_write((void *)node, sizeof(*node));
	do_stmt_tree_s(&node->x_stmt_tree, 0);
	do_vec_tree((void *)node->local_typedefs, 1);
}

#if 0
typedef uintptr_t splay_tree_key;
typedef uintptr_t splay_tree_value;
struct splay_tree_node_s {
	splay_tree_key key;
	splay_tree_value value;
	splay_tree_node left;
	splay_tree_node right;
};
typedef struct splay_tree_node_s *splay_tree_node;
#endif
static void do_splay_tree_node_s(struct splay_tree_node_s *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;
	if (flag)
		mem_write((void *)node, sizeof(*node));
	do_splay_tree_node_s(node->left, 1);
	do_splay_tree_node_s(node->right, 1);
}

#if 0
struct splay_tree_s {
	splay_tree_node root;
	splay_tree_compare_fn comp;
	splay_tree_delete_key_fn delete_key;
	splay_tree_delete_value_fn delete_value;
	splay_tree_allocate_fn allocate;
	splay_tree_deallocate_fn deallocate;
	void *allocate_data;
};
typedef struct splay_tree_s *splay_tree;
#endif
static void do_splay_tree_s(struct splay_tree_s *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;
	if (flag)
		mem_write((void *)node, sizeof(*node));
	do_splay_tree_node_s(node->root, 1);
	/* TODO, allocate_data */
}

struct c_switch {
	tree switch_expr;
	tree orig_type;
	splay_tree cases;
	struct c_spot_bindings *bindings;
	struct c_switch *next;
	bool bool_cond_p;
	bool outside_range_p;
};
static void do_c_switch(struct c_switch *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;
	if (flag)
		mem_write((void *)node, sizeof(*node));
	do_tree(node->switch_expr);
	do_tree(node->orig_type);
	do_splay_tree_s(node->cases, 1);
	do_c_spot_bindings(node->bindings, 1);
	do_c_switch(node->next, 1);
}

#if 0
struct c_arg_tag { /* gcc/c/c-tree.h */
	tree id;
	tree type;
};
#endif
static void do_c_arg_tag(c_arg_tag *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;
	if (flag)
		mem_write((void *)node, sizeof(*node));
	do_tree(node->id);
	do_tree(node->type);
}
static void do_vec_c_arg_tag(vec<c_arg_tag, va_gc> *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;

	unsigned long len = node->length();
	c_arg_tag *addr = node->address();
	if (flag) {
		if (len)
			mem_write((void *)node, sizeof(*node)+
					(len-1)*sizeof(*addr));
		else
			mem_write((void *)node, sizeof(*node));
	}
	for (unsigned long i = 0; i < len; i++) {
		do_c_arg_tag(&addr[i], 0);
	}
}

#if 0
struct c_arg_info { /* gcc/c/c-tree.h */
	tree parms;
	vec<c_arg_tag, va_gc> *tags;
	tree types;
	tree others;
	tree pending_sizes;
	BOOL_BITFIELD had_vla_unspec : 1;
};
#endif
static void do_c_arg_info(struct c_arg_info *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;
	if (flag)
		mem_write((void *)node, sizeof(*node));
	do_tree(node->parms);
	do_vec_c_arg_tag(node->tags, 1);
	do_tree(node->types);
	do_tree(node->others);
	do_tree(node->pending_sizes);
}

struct GTY(()) language_function {
	struct c_language_function base;
	tree x_break_label;
	tree x_cont_label;
	struct c_switch * GTY((skip)) x_switch_stack;
	struct c_arg_info * GTY((skip)) arg_info;
	int returns_value;
	int returns_null;
	int returns_abnormally;
	int warn_about_return_type;
};
static void do_language_function(struct language_function *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;

	if (flag)
		mem_write((void *)node, sizeof(*node));
	do_c_language_function(&node->base, 0);
	do_tree(node->x_break_label);
	do_tree(node->x_cont_label);
	do_c_switch(node->x_switch_stack, 1);
	do_c_arg_info(node->arg_info, 1);
}

static void do_basic_block(basic_block bb, int flag);
static void do_gimple_seq(gimple_seq gs);
static size_t gsstruct_size(gimple_seq gs)
{
	return gsstruct_code_size[gss_for_code(gimple_code(gs))];
}

static size_t gimple_total_size(gimple_seq gs)
{
	size_t base_size = gsstruct_size(gs);

	/* there is an exception: gphi */
	if (gimple_code(gs) == GIMPLE_PHI) {
		struct gphi *node;
		node = (struct gphi *)gs;

		base_size -= sizeof(*node);
		base_size += sizeof(*node) * node->nargs;
		return base_size;
	}

	if (!gimple_has_ops(gs) || (!gimple_num_ops(gs)))
		return base_size;
	return base_size + (gimple_num_ops(gs)-1) * sizeof(tree);
}

static inline void gimple_write(gimple *gs)
{
	mem_write((void *)gs, gimple_total_size(gs));
}

/* for struct gimple */
static void do_gimple_base(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;
	if (flag)
		gimple_write(gs);

	do_location(gs->location);
	do_basic_block(gs->bb, 1);
	do_gimple_seq(gs->next);
	do_gimple_seq(gs->prev);

	return;
}

static void do_ssa_use_operand(void *node, int flag);
static void do_use_optype_d(struct use_optype_d *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;
	if (flag)
		mem_write((void *)node, sizeof(*node));

	do_use_optype_d(node->next, 1);
	do_ssa_use_operand(&node->use_ptr, 0);
}

static void do_gs_with_ops_base(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;
	if (flag)
		gimple_write(gs);

	struct gimple_statement_with_ops_base *node0;
	node0 = (struct gimple_statement_with_ops_base *)gs;

	do_gimple_base(gs, 0);
	do_use_optype_d(node0->use_ops, 1);
}

static inline void do_gs_ops(gimple *gs)
{
	tree *ops;
	ops = gimple_ops(gs);
	for (unsigned int i = 0; i < gimple_num_ops(gs); i++) {
		/* FIXME: handle function decl? */
		do_tree(ops[i]);
	}
}
static void do_gs_with_ops(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;
	if (flag)
		gimple_write(gs);

	struct gimple_statement_with_ops __maybe_unused *node0;
	node0 = (struct gimple_statement_with_ops *)gs;

	do_gs_with_ops_base(gs, 0);
	do_gs_ops(gs);
}

static void do_gs_with_mem_ops_base(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;
	if (flag)
		gimple_write(gs);

	struct gimple_statement_with_memory_ops_base *node0;
	node0 = (struct gimple_statement_with_memory_ops_base *)gs;

	do_gs_with_ops_base(gs, 0);
	do_tree(node0->vdef);
	do_tree(node0->vuse);
}

static void do_gs_with_mem_ops(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;
	if (flag)
		gimple_write(gs);

	struct gimple_statement_with_memory_ops __maybe_unused *node0;
	node0 = (struct gimple_statement_with_memory_ops *)gs;

	do_gs_with_mem_ops_base(gs, 0);
	do_gs_ops(gs);
}

static void do_gcall(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;
	if (flag)
		gimple_write(gs);

	struct gcall __maybe_unused *node0;
	node0 = (struct gcall *)gs;

	do_gs_with_mem_ops_base(gs, 0);

	if (!(gs->subcode & GF_CALL_INTERNAL)) {
		do_tree(node0->u.fntype);
	}
	do_gs_ops(gs);
}

static void do_gs_omp(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;
	if (flag)
		gimple_write(gs);

	struct gimple_statement_omp *node0;
	node0 = (struct gimple_statement_omp *)gs;

	do_gimple_base(gs, 0);
	do_gimple_seq(node0->body);
}

static void do_gbind(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;
	if (flag)
		gimple_write(gs);

	struct gbind *node0;
	node0 = (struct gbind *)gs;

	do_gimple_base(gs, 0);
	do_tree(node0->vars);
	do_tree(node0->block);
	do_gimple_seq(node0->body);
}

static void do_gcatch(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;
	if (flag)
		gimple_write(gs);

	struct gcatch *node0;
	node0 = (struct gcatch *)gs;

	do_gimple_base(gs, 0);
	do_tree(node0->types);
	do_gimple_seq(node0->handler);
}

static void do_geh_filter(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;
	if (flag)
		gimple_write(gs);

	struct geh_filter *node0;
	node0 = (struct geh_filter *)gs;

	do_gimple_base(gs, 0);
	do_tree(node0->types);
	do_gimple_seq(node0->failure);
}

static void do_geh_else(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;
	if (flag)
		gimple_write(gs);

	struct geh_else *node0;
	node0 = (struct geh_else *)gs;

	do_gimple_base(gs, 0);
	do_gimple_seq(node0->n_body);
	do_gimple_seq(node0->e_body);
}

static void do_geh_mnt(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;
	if (flag)
		gimple_write(gs);

	struct geh_mnt *node0;
	node0 = (struct geh_mnt *)gs;

	do_gimple_base(gs, 0);
	do_tree(node0->fndecl);
}

static void do_phi_arg_d(struct phi_arg_d *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;
	if (flag)
		mem_write((void *)node, sizeof(*node));

	/* ignore: do_location(node->locus); */
	do_ssa_use_operand(&node->imm_use, 0);
	do_tree(node->def);
}

static void do_gphi(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;
	if (flag)
		gimple_write(gs);

	struct gphi *node0;
	node0 = (struct gphi *)gs;

	do_gimple_base(gs, 0);
	do_tree(node0->result);
	for (unsigned i = 0; i < node0->nargs; i++) {
		do_phi_arg_d(&node0->args[i], 0);
	}
}

static void do_gs_eh_ctrl(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;
	if (flag)
		gimple_write(gs);

	struct gimple_statement_eh_ctrl __maybe_unused *node0;
	node0 = (struct gimple_statement_eh_ctrl *)gs;

	do_gimple_base(gs, 0);
}

static void do_gresx(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;
	if (flag)
		gimple_write(gs);

	struct gresx __maybe_unused *node0;
	node0 = (struct gresx *)gs;

	do_gs_eh_ctrl(gs, 0);
}

static void do_geh_dispatch(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;
	if (flag)
		gimple_write(gs);

	struct geh_dispatch __maybe_unused *node0;
	node0 = (struct geh_dispatch *)gs;

	do_gs_eh_ctrl(gs, 0);
}

static void do_gtry(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;
	if (flag)
		gimple_write(gs);

	struct gtry *node0;
	node0 = (struct gtry *)gs;

	do_gimple_base(gs, 0);
	do_gimple_seq(node0->eval);
	do_gimple_seq(node0->cleanup);
}

static __maybe_unused void do_gs_wce(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;
	if (flag)
		gimple_write(gs);

	struct gimple_statement_wce *node0;
	node0 = (struct gimple_statement_wce *)gs;

	do_gimple_base(gs, 0);
	do_gimple_seq(node0->cleanup);
}

static void do_gasm(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;
	if (flag)
		gimple_write(gs);

	struct gasm *node0;
	node0 = (struct gasm *)gs;
	do_gs_with_mem_ops_base(gs, 0);

	if (!is_obj_checked((void *)node0->string))
		mem_write((void *)node0->string, strlen(node0->string) + 1);

	do_gs_ops(gs);
}

static void do_gomp_critical(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;
	if (flag)
		gimple_write(gs);

	struct gomp_critical *node0;
	node0 = (struct gomp_critical *)gs;

	do_gs_omp(gs, 0);
	do_tree(node0->clauses);
	do_tree(node0->name);
}

static void do_gomp_for_iter(struct gimple_omp_for_iter *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;
	if (flag)
		mem_write((void *)node, sizeof(*node));

	do_tree(node->index);
	do_tree(node->initial);
	do_tree(node->final);
	do_tree(node->incr);
}

static void do_gomp_for(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;
	if (flag)
		gimple_write(gs);

	struct gomp_for *node0;
	node0 = (struct gomp_for *)gs;

	do_gs_omp(gs, 0);
	do_tree(node0->clauses);
	do_gomp_for_iter(node0->iter, 1);
	do_gimple_seq(node0->pre_body);
}

static void do_gs_omp_parallel_layout(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;
	if (flag)
		gimple_write(gs);

	struct gimple_statement_omp_parallel_layout *node0;
	node0 = (struct gimple_statement_omp_parallel_layout *)gs;

	do_gs_omp(gs, 0);
	do_tree(node0->clauses);
	do_tree(node0->child_fn);
	do_tree(node0->data_arg);
}

static void do_gs_omp_taskreg(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;
	if (flag)
		gimple_write(gs);

	struct gimple_statement_omp_taskreg __maybe_unused *node0;
	node0 = (struct gimple_statement_omp_taskreg *)gs;

	do_gs_omp_parallel_layout(gs, 0);
}

static void do_gomp_parallel(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;
	if (flag)
		gimple_write(gs);

	struct gomp_parallel __maybe_unused *node0;
	node0 = (struct gomp_parallel *)gs;

	do_gs_omp_taskreg(gs, 0);
}

static void do_gomp_target(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;
	if (flag)
		gimple_write(gs);

	struct gomp_target __maybe_unused *node0;
	node0 = (struct gomp_target *)gs;

	do_gs_omp_parallel_layout(gs, 0);
}

static void do_gomp_task(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;
	if (flag)
		gimple_write(gs);

	struct gomp_task *node0;
	node0 = (struct gomp_task *)gs;

	do_gs_omp_taskreg(gs, 0);
	do_tree(node0->copy_fn);
	do_tree(node0->arg_size);
	do_tree(node0->arg_align);
}

static void do_gomp_sections(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;
	if (flag)
		gimple_write(gs);

	struct gomp_sections *node0;
	node0 = (struct gomp_sections *)gs;

	do_gs_omp(gs, 0);
	do_tree(node0->clauses);
	do_tree(node0->control);
}

static void do_gomp_continue(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;
	if (flag)
		gimple_write(gs);

	struct gomp_continue *node0;
	node0 = (struct gomp_continue *)gs;

	do_gimple_base(gs, 0);
	do_tree(node0->control_def);
	do_tree(node0->control_use);
}

static void do_gs_omp_single_layout(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;
	if (flag)
		gimple_write(gs);

	struct gimple_statement_omp_single_layout *node0;
	node0 = (struct gimple_statement_omp_single_layout *)gs;

	do_gs_omp(gs, 0);
	do_tree(node0->clauses);
}

static void do_gomp_single(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;
	if (flag)
		gimple_write(gs);

	struct gomp_single __maybe_unused *node0;
	node0 = (struct gomp_single *)gs;

	do_gs_omp_single_layout(gs, 0);
}

static void do_gomp_teams(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;
	if (flag)
		gimple_write(gs);

	struct gomp_teams __maybe_unused *node0;
	node0 = (struct gomp_teams *)gs;

	do_gs_omp_single_layout(gs, 0);
}

static void do_gomp_ordered(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;
	if (flag)
		gimple_write(gs);

	struct gomp_ordered __maybe_unused *node0;
	node0 = (struct gomp_ordered *)gs;

	do_gs_omp_single_layout(gs, 0);
}

static void do_gomp_atomic_load(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;
	if (flag)
		gimple_write(gs);

	struct gomp_atomic_load *node0;
	node0 = (struct gomp_atomic_load *)gs;

	do_gimple_base(gs, 0);
	do_tree(node0->rhs);
	do_tree(node0->lhs);
}

static void do_gs_omp_atomic_store_layout(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;
	if (flag)
		gimple_write(gs);

	struct gimple_statement_omp_atomic_store_layout *node0;
	node0 = (struct gimple_statement_omp_atomic_store_layout *)gs;

	do_gimple_base(gs, 0);
	do_tree(node0->val);
}

static void do_gomp_atomic_store(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;
	if (flag)
		gimple_write(gs);

	struct gomp_atomic_store __maybe_unused *node0;
	node0 = (struct gomp_atomic_store *)gs;

	do_gs_omp_atomic_store_layout(gs, 0);
}

static void do_gs_omp_return(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;
	if (flag)
		gimple_write(gs);

	struct gimple_statement_omp_return __maybe_unused *node0;
	node0 = (struct gimple_statement_omp_return *)gs;

	do_gs_omp_atomic_store_layout(gs, 0);
}

static void do_gtransaction(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;
	if (flag)
		gimple_write(gs);

	struct gtransaction *node0;
	node0 = (struct gtransaction *)gs;

	do_gs_with_mem_ops_base(gs, 0);
	do_gimple_seq(node0->body);
	do_tree(node0->label_norm);
	do_tree(node0->label_uninst);
	do_tree(node0->label_over);
}

static void do_gcond(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;
	if (flag)
		gimple_write(gs);

	struct gcond __maybe_unused *node0;
	node0 = (struct gcond *)gs;

	do_gs_with_ops(gs, 0);
}

static void do_gdebug(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;
	if (flag)
		gimple_write(gs);

	struct gdebug __maybe_unused *node0;
	node0 = (struct gdebug *)gs;

	do_gs_with_ops(gs, 0);
}

static void do_ggoto(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;
	if (flag)
		gimple_write(gs);

	struct ggoto __maybe_unused *node0;
	node0 = (struct ggoto *)gs;

	do_gs_with_ops(gs, 0);
}

static void do_glabel(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;
	if (flag)
		gimple_write(gs);

	struct glabel __maybe_unused *node0;
	node0 = (struct glabel *)gs;

	do_gs_with_ops(gs, 0);
}

static void do_gswitch(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;
	if (flag)
		gimple_write(gs);

	struct gswitch __maybe_unused *node0;
	node0 = (struct gswitch *)gs;

	do_gs_with_ops(gs, 0);
}

static void do_gassign(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;
	if (flag)
		gimple_write(gs);

	struct gassign __maybe_unused *node0;
	node0 = (struct gassign *)gs;

	do_gs_with_mem_ops(gs, 0);
}

static void do_greturn(gimple *gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;
	if (flag)
		gimple_write(gs);

	struct greturn __maybe_unused *node0;
	node0 = (struct greturn *)gs;

	do_gs_with_mem_ops(gs, 0);
}

static void do_gimple_seq(gimple_seq gs)
{
	if (!gs)
		return;

	BUG_ON(check_gimple_code(gs));

	switch (gimple_code(gs)) {
	case GIMPLE_ASM:
	{
		do_gasm(gs, 1);
		break;
	}
	case GIMPLE_ASSIGN:
	{
		do_gassign(gs, 1);
		break;
	}
	case GIMPLE_CALL:
	{
		do_gcall(gs, 1);
		break;
	}
	case GIMPLE_COND:
	{
		do_gcond(gs, 1);
		break;
	}
	case GIMPLE_LABEL:
	{
		do_glabel(gs, 1);
		break;
	}
	case GIMPLE_GOTO:
	{
		do_ggoto(gs, 1);
		break;
	}
	case GIMPLE_NOP:
	{
		do_gimple_base(gs, 1);
		break;
	}
	case GIMPLE_RETURN:
	{
		do_greturn(gs, 1);
		break;
	}
	case GIMPLE_SWITCH:
	{
		do_gswitch(gs, 1);
		break;
	}
	case GIMPLE_PHI:
	{
		do_gphi(gs, 1);
		break;
	}
	case GIMPLE_OMP_PARALLEL:
	{
		do_gomp_parallel(gs, 1);
		break;
	}
	case GIMPLE_OMP_TASK:
	{
		do_gomp_task(gs, 1);
		break;
	}
	case GIMPLE_OMP_ATOMIC_LOAD:
	{
		do_gomp_atomic_load(gs, 1);
		break;
	}
	case GIMPLE_OMP_ATOMIC_STORE:
	{
		do_gomp_atomic_store(gs, 1);
		break;
	}
	case GIMPLE_OMP_FOR:
	{
		do_gomp_for(gs, 1);
		break;
	}
	case GIMPLE_OMP_CONTINUE:
	{
		do_gomp_continue(gs, 1);
		break;
	}
	case GIMPLE_OMP_SINGLE:
	{
		do_gomp_single(gs, 1);
		break;
	}
	case GIMPLE_OMP_TARGET:
	{
		do_gomp_target(gs, 1);
		break;
	}
	case GIMPLE_OMP_TEAMS:
	{
		do_gomp_teams(gs, 1);
		break;
	}
	case GIMPLE_OMP_RETURN:
	{
		do_gs_omp_return(gs, 1);
		break;
	}
	case GIMPLE_OMP_SECTIONS:
	{
		do_gomp_sections(gs, 1);
		break;
	}
	case GIMPLE_OMP_SECTIONS_SWITCH:
	{
		/* FIXME: is do_gimple_base() right? */
		do_gimple_base(gs, 1);
		break;
	}
	case GIMPLE_OMP_MASTER:
	case GIMPLE_OMP_TASKGROUP:
	case GIMPLE_OMP_SECTION:
	case GIMPLE_OMP_GRID_BODY:
	{
		do_gs_omp(gs, 1);
		break;
	}
	case GIMPLE_OMP_ORDERED:
	{
		do_gomp_ordered(gs, 1);
		break;
	}
	case GIMPLE_OMP_CRITICAL:
	{
		do_gomp_critical(gs, 1);
		break;
	}
	case GIMPLE_BIND:
	{
		do_gbind(gs, 1);
		break;
	}
	case GIMPLE_TRY:
	{
		do_gtry(gs, 1);
		break;
	}
	case GIMPLE_CATCH:
	{
		do_gcatch(gs, 1);
		break;
	}
	case GIMPLE_EH_FILTER:
	{
		do_geh_filter(gs, 1);
		break;
	}
	case GIMPLE_EH_MUST_NOT_THROW:
	{
		do_geh_mnt(gs, 1);
		break;
	}
	case GIMPLE_EH_ELSE:
	{
		do_geh_else(gs, 1);
		break;
	}
	case GIMPLE_RESX:
	{
		do_gresx(gs, 1);
		break;
	}
	case GIMPLE_EH_DISPATCH:
	{
		do_geh_dispatch(gs, 1);
		break;
	}
	case GIMPLE_DEBUG:
	{
		do_gdebug(gs, 1);
		break;
	}
	case GIMPLE_PREDICT:
	{
		/* FIXME: is do_gimple_base() right? */
		do_gimple_base(gs, 1);
		break;
	}
	case GIMPLE_TRANSACTION:
	{
		do_gtransaction(gs, 1);
		break;
	}
	default:
	{
		BUG();
	}
	}

	return;
}

/*
 * TODO: re-check the following functions mem_write whether the size is
 * right?
 */

/* basic-block.h: struct edge_def */
static void do_edge(void *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;

	struct edge_def *node0;
	node0 = (struct edge_def *)node;
	if (flag)
		mem_write(node, sizeof(*node0));

	/* location_t goto_locus */
	do_location(node0->goto_locus);

	do_basic_block(node0->src, 1);
	do_basic_block(node0->dest, 1);

	/* insns? */
	if (current_ir_type() == IR_GIMPLE)
		do_gimple_seq(node0->insns.g);
	else
		BUG();
}

static void do_vec_edge(void *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;

	vec<edge, va_gc> *node0;
	node0 = (vec<edge, va_gc> *)node;
	unsigned long length = node0->length();

	edge *addr = node0->address();
	if (flag) {
		if (length)
			mem_write(node, sizeof(*node0)+
					(length-1)*sizeof(*addr));
		else
			mem_write(node, sizeof(*node0));
	}
	for (unsigned long i = 0; i < length; i++) {
		do_edge(addr[i], 1);
	}
}

static void do_nb_iter_bound(void *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;
	struct nb_iter_bound *node0;
	node0 = (struct nb_iter_bound *)node;
	if (flag)
		mem_write(node, sizeof(*node0));
	do_gimple_seq(node0->stmt);
	do_nb_iter_bound(node0->next, 1);
}

static void do_control_iv(void *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;
	struct control_iv *node0;
	node0 = (struct control_iv *)node;
	if (flag)
		mem_write(node, sizeof(*node0));
	do_tree(node0->base);
	do_tree(node0->step);
	do_control_iv(node0->next, 1);
}

static void do_loop_exit(void *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;
	struct loop_exit *node0;
	node0 = (struct loop_exit *)node;
	if (flag)
		mem_write(node, sizeof(*node0));
	do_edge(node0->e, 1);
	do_loop_exit(node0->prev, 1);
	do_loop_exit(node0->next, 1);
	do_loop_exit(node0->next_e, 1);
}

static void do_niter_desc(void *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;
	struct niter_desc *node0;
	node0 = (struct niter_desc *)node;
	if (flag)
		mem_write(node, sizeof(*node0));
	do_edge(node0->out_edge, 1);
	do_edge(node0->in_edge, 1);
	/* ignore assumptions, noloop_assumptions, infinite, niter_expr */
}

static void do_vec_loop(void *node, int flag);
/* cfgloop.h */
static void do_loop(void *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;

	struct loop *node0;
	node0 = (struct loop *)node;
	if (flag)
		mem_write(node, sizeof(*node0));

	do_basic_block(node0->header, 1);
	do_basic_block(node0->latch, 1);

	do_vec_loop(node0->superloops, 1);
	do_loop(node0->inner, 1);
	do_loop(node0->next, 1);
	do_tree(node0->nb_iterations);
	do_tree(node0->simduid);
	do_nb_iter_bound(node0->bounds, 1);
	do_control_iv(node0->control_ivs, 1);
	do_loop_exit(node0->exits, 1);
	do_niter_desc(node0->simple_loop_desc, 1);
	do_basic_block(node0->former_header, 1);
}

static void do_vec_loop(void *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;

	vec<loop_p,va_gc> *node0;
	node0 = (vec<loop_p,va_gc> *)node;
	unsigned long length = node0->length();
	loop_p *addr = node0->address();
	if (flag) {
		if (length)
			mem_write(node, sizeof(*node0)+
					(length-1)*sizeof(*addr));
		else
			mem_write(node, sizeof(*node0));
	}
	for (unsigned long i = 0; i < length; i++) {
		do_loop(addr[i], 1);
	}
}

/* basic-block.h: struct basic_block_def */
static void do_basic_block(basic_block bb, int flag)
{
	if (!bb)
		return;
	if (flag && is_obj_checked((void *)bb))
		return;
	if (flag)
		mem_write((void *)bb, sizeof(*bb));

	/* vec<edge> preds,succs */
	do_vec_edge(bb->preds, 1);
	do_vec_edge(bb->succs, 1);

	/* PTR aux? ignore */

	/* struct loop loop_father */
	do_loop(bb->loop_father, 1);

	/* struct et_node dom, ignore */

	/* basic_block prev_bb, next_bb; */
	do_basic_block(bb->prev_bb, 1);
	do_basic_block(bb->next_bb, 1);

	/* basic_block_il_dependent */
	BUG_ON(bb->flags & BB_RTL);
	do_gimple_seq(bb->il.gimple.seq);
	do_gimple_seq(bb->il.gimple.phi_nodes);
}

static void do_vec_basic_block(void *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;

	vec<basic_block, va_gc> *node0;
	node0 = (vec<basic_block, va_gc> *)node;

	unsigned long length = node0->length();
	basic_block *addr = node0->address();
	if (flag) {
		if (length)
			mem_write(node, sizeof(*node0)+
					(length-1)*sizeof(*addr));
		else
			mem_write(node, sizeof(*node0));
	}
	for (unsigned long i = 0; i < length; i++) {
		do_basic_block(addr[i], 1);
	}
}

/* cfg.h: The control flow graph for this function */
static void do_cfg(struct control_flow_graph *cfg, int flag)
{
	if (!cfg)
		return;
	if (flag && is_obj_checked((void *)cfg))
		return;
	if (flag)
		mem_write((void *)cfg, sizeof(*cfg));

	/* basic_block x_entry_block_ptr, x_exit_block_ptr */
	do_basic_block(cfg->x_entry_block_ptr, 1);
	do_basic_block(cfg->x_exit_block_ptr, 1);

	/* vec<basic_block> x_basic_block_info, x_label_to_block_map */
	do_vec_basic_block(cfg->x_basic_block_info, 1);
	do_vec_basic_block(cfg->x_label_to_block_map, 1);
}

static void do_ssa_operands(void *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;

	struct ssa_operands *node0;
	node0 = (struct ssa_operands *)node;
	if (flag)
		mem_write(node, sizeof(*node0));

	/* TODO: struct ssa_operand_memory_d *operand_memory */
	/* ignore: struct use_optype_d *free_uses */
}

/* gimple-ssa.h: SSA and dataflow datastructure */
static void do_gimple_df(struct gimple_df *df, int flag)
{
	if (!df)
		return;
	if (flag && is_obj_checked((void *)df))
		return;
	if (flag)
		mem_write((void *)df, sizeof(*df));

	/* vec<tree> ssa_names */
	do_vec_tree((void *)df->ssa_names, 1);

	/* tree vop */
	do_tree(df->vop);

	/*
	 * decls_to_pointers used in:
	 *	gcc/alias.c
	 *	gcc/cfgexpand.c
	 *	gcc/dse.c
	 *	gcc/gimple-expr.c
	 */
	/* TODO: do_hash_map(df->decls_to_pointers, 1); */

	/* vec<tree> free_ssanames free_ssanemes_queue */
	/* might be better ignore these two fields */
#ifdef GCC_CONTAIN_FREE_SSANAMES
	do_vec_tree((void *)df->free_ssanames, 1);
	do_vec_tree((void *)df->free_ssanames_queue, 1);
#endif

	/* hash_table is borrowed from libiberty htab_t in hashtab.h */
	/* TODO: do_hash_table(df->default_defs, 1); */

	/* ssa_operands ssa_operands */
	do_ssa_operands(&df->ssa_operands, 0);

	/* ignore: hash_table<tm_restart_hasher tm_restart. Howto? */
}

/* cfgloop.h: the loops in this function */
static void do_loops(struct loops *loop, int flag)
{
	if (!loop)
		return;
	if (flag && is_obj_checked((void *)loop))
		return;
	if (flag)
		mem_write((void *)loop, sizeof(*loop));

	/* vec<loop_p> larray */
	do_vec_loop(loop->larray, 1);

	/* TODO: hash_table<loop_exit_hasher> exits. likewise */

	/* struct loop *tree_root */
	do_loop(loop->tree_root, 1);
}

static void do_function(struct function *node, int flag);
static void do_histogram_value(void *node)
{
	if (!node)
		return;
	if (is_obj_checked(node))
		return;

	struct histogram_value_t *node0;
	node0 = (struct histogram_value_t *)node;

	mem_write(node, sizeof(*node0));
	if (!is_obj_checked((void *)node0->hvalue.counters))
		mem_write(node0->hvalue.counters,
			node0->n_counters * sizeof(node0->hvalue.counters[0]));

	do_function(node0->fun, 1);

	do_tree(node0->hvalue.value);
	do_gimple_seq(node0->hvalue.stmt);
	do_histogram_value(node0->hvalue.next);
}

static void do_histogram_values(void *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;

	struct htab *node0;
	node0 = (struct htab *)node;

	if (flag)
		mem_write(node, sizeof(*node0));

	if (!is_obj_checked((void *)node0->entries))
		mem_write(node0->entries,
				node0->size * sizeof(node0->entries[0]));
	for (size_t i = 0; i < node0->size; i++) {
		if ((node0->entries[i] == HTAB_EMPTY_ENTRY) ||
			node0->entries[i] == HTAB_DELETED_ENTRY)
			continue;
		do_histogram_value(node0->entries[i]);
	}
}

static void do_function(struct function *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;

	struct function *node0 = (struct function *)node;
	if (flag)
		mem_write(node, sizeof(*node0));
	do_location(node0->function_start_locus);
	do_location(node0->function_end_locus);
	do_language_function(node0->language, 1);
	do_tree(node0->decl);
	do_tree(node0->static_chain_decl);
	do_tree(node0->nonlocal_goto_save_area);
	do_vec_tree(node0->local_decls, 1);
#if __GNUC__ < 8
	do_tree(node0->cilk_frame_decl);
#endif
	do_gimple_seq(node0->gimple_body);
	do_cfg(node0->cfg, 1);
	do_gimple_df(node0->gimple_df, 1);
	do_loops(node0->x_current_loops, 1);
	do_histogram_values(node0->value_histograms, 1);
	/* TODO, node0->eh, except.h, ignore it */
	/* TODO, node0->su */
	/* TODO, node0->used_types_hash */
	/* TODO, node0->fde */
	/* TODO, node0->cannot_be_copied_reason */
	/* TODO, node0->machine, config/i386/i386.h, ignore */
}

static void do_statement_list_node(void *node, int flag)
{
	if (!node)
		return;
	/* FIXME, flag is always 1 */
	if (unlikely(flag && is_obj_checked((void *)node)))
		return;

	struct tree_statement_list_node *node0 =
			(struct tree_statement_list_node *)node;
	if (likely(flag))
		mem_write(node, sizeof(*node0));
	do_statement_list_node((void *)node0->prev, 1);
	do_statement_list_node((void *)node0->next, 1);
	do_tree(node0->stmt);
}

static void do_tree(tree node);
static void do_base(tree node, int flag);
static void do_typed(tree node, int flag);
static void do_string(tree node, int flag);
static void do_real_cst(tree node, int flag);
static void do_int_cst(tree node, int flag);
static void do_common(tree node, int flag);
static void do_identifier(tree node, int flag);
static void do_c_lang_identifier(tree node, int flag);
static void do_constructor(tree node, int flag);
static void do_statement_list(tree node, int flag);
static void do_block(tree node, int flag);
static void do_exp(tree node, int flag);
static void do_list(tree node, int flag);
static void do_type_with_lang_specific(tree node, int flag);
static void do_type_common(tree node, int flag);
static void do_type_non_common(tree node, int flag);
static void do_decl_minimal(tree node, int flag);
static void do_decl_common(tree node, int flag);
static void do_field_decl(tree node, int flag);
static void do_decl_with_rtl(tree node, int flag);
static void do_label_decl(tree node, int flag);
static void do_result_decl(tree node, int flag);
static void do_parm_decl(tree node, int flag);
static void do_decl_with_vis(tree node, int flag);
static void do_var_decl(tree node, int flag);
static void do_decl_non_common(tree node, int flag);
static void do_type_decl(tree node, int flag);
static void do_translation_unit_decl(tree node, int flag);
static void do_function_decl(tree node, int flag);
static void do_const_decl(tree node, int flag);
static void do_vec(tree node, int flag);

static void do_fixed_cst(tree node, int flag);
static void do_vector(tree node, int flag);
static void do_complex(tree node, int flag);
static void do_ssa_name(tree node, int flag);
static void do_binfo(tree node, int flag);
static void do_omp_clause(tree node, int flag);
static void do_optimization_option(tree node, int flag);
static void do_target_option(tree node, int flag);

static void do_tree(tree node)
{
	if (!node)
		return;

	BUG_ON(check_tree_code(node));

	enum tree_code code = TREE_CODE(node);
	enum tree_code_class tc = TREE_CODE_CLASS(code);
	switch (code) {
	case INTEGER_CST:
		return do_int_cst(node, 1);
	case TREE_BINFO:
		return do_binfo(node, 1);
	case TREE_VEC:
		return do_vec(node, 1);
	case VECTOR_CST:
		return do_vector(node, 1);
	case STRING_CST:
		return do_string(node, 1);
	case OMP_CLAUSE:
		return do_omp_clause(node, 1);
	default:
		if (tc == tcc_vl_exp)
			return do_exp(node, 1);
	}

	switch (tc) {
	case tcc_declaration:
	{
		switch (code) {
		case FIELD_DECL:
			return do_field_decl(node, 1);
		case PARM_DECL:
			return do_parm_decl(node, 1);
		case VAR_DECL:
			return do_var_decl(node, 1);
		case LABEL_DECL:
			return do_label_decl(node, 1);
		case RESULT_DECL:
			return do_result_decl(node, 1);
		case CONST_DECL:
			return do_const_decl(node, 1);
		case TYPE_DECL:
			return do_type_decl(node, 1);
		case FUNCTION_DECL:
			return do_function_decl(node, 1);
		case DEBUG_EXPR_DECL:
			return do_decl_with_rtl(node, 1);
		case TRANSLATION_UNIT_DECL:
			return do_translation_unit_decl(node, 1);
		case NAMESPACE_DECL:
		case IMPORTED_DECL:
		case NAMELIST_DECL:
			return do_decl_non_common(node, 1);
		default:
			BUG();
		}
	}
	case tcc_type:
		return do_type_non_common(node, 1);
	case tcc_reference:
	case tcc_expression:
	case tcc_statement:
	case tcc_comparison:
	case tcc_unary:
	case tcc_binary:
		return do_exp(node, 1);
	case tcc_constant:
	{
		switch (code) {
		case VOID_CST:
			return do_typed(node, 1);
		case INTEGER_CST:
			BUG();
		case REAL_CST:
			return do_real_cst(node, 1);
		case FIXED_CST:
			return do_fixed_cst(node, 1);
		case COMPLEX_CST:
			return do_complex(node, 1);
		case VECTOR_CST:
			return do_vector(node, 1);
		case STRING_CST:
			BUG();
		default:
			BUG();
		}
	}
	case tcc_exceptional:
	{
		switch (code) {
		case IDENTIFIER_NODE:
			return do_c_lang_identifier(node, 1);
#if 0
			return do_identifier(node, 1);
#endif
		case TREE_LIST:
			return do_list(node, 1);
		case ERROR_MARK:
		case PLACEHOLDER_EXPR:
			return do_common(node, 1);
		case TREE_VEC:
		case OMP_CLAUSE:
			BUG();
		case SSA_NAME:
			return do_ssa_name(node, 1);
		case STATEMENT_LIST:
			return do_statement_list(node, 1);
		case BLOCK:
			return do_block(node, 1);
		case CONSTRUCTOR:
			return do_constructor(node, 1);
		case OPTIMIZATION_NODE:
			return do_optimization_option(node, 1);
		case TARGET_OPTION_NODE:
			return do_target_option(node, 1);
		default:
			BUG();
		}
	}
	default:
		BUG();
	}
}

static void do_base(tree node, int flag)
{
	if (!node)
		return;
	BUG_ON(flag);
}

static void do_typed(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;
	if (flag)
		node_write(node);

	struct tree_typed *node0 = (struct tree_typed *)node;
	do_base((tree)&node0->base, 0);
	do_tree(node0->type);
}

static void do_string(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;
	if (flag)
		node_write(node);

	struct tree_string *node0 = (struct tree_string *)node;
	do_typed((tree)&node0->typed, 0);
}

static void do_real_cst(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;
	if (flag)
		node_write(node);

	struct tree_real_cst *node0 = (struct tree_real_cst *)node;
	do_typed((tree)&node0->typed, 0);
	do_real_value(node0->real_cst_ptr, 1);
}

static void do_int_cst(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;
	if (flag)
		node_write(node);

	struct tree_int_cst *node0 = (struct tree_int_cst *)node;
	do_typed((tree)&node0->typed, 0);
}

static void do_common(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;
	if (flag)
		node_write(node);

	struct tree_common *node0 = (struct tree_common *)node;
	do_typed((tree)&node0->typed, 0);
	do_tree(node0->chain);
}

static void __maybe_unused do_identifier(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;
	if (flag)
		node_write(node);

	struct tree_identifier *node0 = (struct tree_identifier *)node;
	do_common((tree)&node0->common, 0);
	do_ht_identifier(&node0->id, 0);
}
struct GTY(()) c_lang_identifier { /* gcc/c/c-decl.c */
	struct c_common_identifier common_id;
	struct c_binding *symbol_binding;
	struct c_binding *tag_binding;
	struct c_binding *label_binding;
};
static void do_c_lang_identifier(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;
	if (flag)
		mem_write((void *)node, sizeof(struct c_lang_identifier));

	struct c_lang_identifier *node0 = (struct c_lang_identifier *)node;
	do_c_common_identifier(&node0->common_id, 0);
#ifdef COLLECT_MORE
	do_c_binding(node0->symbol_binding, 1);
	do_c_binding(node0->tag_binding, 1);
	do_c_binding(node0->label_binding, 1);
#endif
}

static void do_constructor(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;
	if (flag)
		node_write(node);

	struct tree_constructor *node0 = (struct tree_constructor *)node;
	do_typed((tree)&node0->typed, 0);
	do_vec_constructor((void *)node0->elts, 1);
}

static void do_statement_list(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;
	if (flag)
		node_write(node);

	struct tree_statement_list *node0 = (struct tree_statement_list *)node;
	do_typed((tree)&node0->typed, 0);
	do_statement_list_node((void *)node0->head, 1);
	do_statement_list_node((void *)node0->tail, 1);
}

static void do_block(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;
	if (flag)
		node_write(node);

	struct tree_block *node0 = (struct tree_block *)node;
	do_location(node0->locus);
	do_location(node0->end_locus);
	do_base((tree)&node0->base, 0);
	do_tree(node0->chain);
	do_tree(node0->vars);
	do_vec_tree(node0->nonlocalized_vars, 1);
	do_tree(node0->subblocks);
	do_tree(node0->supercontext);
	do_tree(node0->abstract_origin);
	do_tree(node0->fragment_origin);
	do_tree(node0->fragment_chain);
	/* TODO: node0->die */
}

static void do_exp(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;
	if (flag)
		node_write(node);

	struct tree_exp *node0 = (struct tree_exp *)node;
	do_location(node0->locus);
	do_typed((tree)&node0->typed, 0);
	if (TREE_CODE_CLASS(TREE_CODE(node)) == tcc_vl_exp) {
		for (int i = 0; i < VL_EXP_OPERAND_LENGTH(node); i++)
			do_tree(node0->operands[i]);
	} else {
		for (int i = 0; i < TREE_CODE_LENGTH(TREE_CODE(node)); i++)
			do_tree(node0->operands[i]);
	}
}

static void do_list(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;
	if (flag)
		node_write(node);

	struct tree_list *node0 = (struct tree_list *)node;
	do_common((tree)&node0->common, 0);
	do_tree(node0->purpose);
	do_tree(node0->value);
}

static void do_type_with_lang_specific(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;
	if (flag)
		node_write(node);

	struct tree_type_with_lang_specific *node0 =
			(struct tree_type_with_lang_specific *)node;
	do_type_common((tree)&node0->common, 0);
	do_c_lang_type((struct c_lang_type *)node0->lang_specific, 1);
}
static void do_type_common(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;
	if (flag)
		node_write(node);

	struct tree_type_common *node0 = (struct tree_type_common *)node;
	/* TODO: union tree_type_symtab, tree-core.h, ignore it */
	do_common((tree)&node0->common, 0);
	do_tree(node0->size);
	do_tree(node0->size_unit);
	do_tree(node0->attributes);
	do_tree(node0->pointer_to);
	do_tree(node0->reference_to);
	do_tree(node0->canonical);
	do_tree(node0->next_variant);
	do_tree(node0->main_variant);
	do_tree(node0->context);
	do_tree(node0->name);
}

static void do_type_non_common(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;
	if (flag) {
		node_write(node);
		size_t start = obj_idx - 1;
		objs[start].is_type = 1;
	}

	struct tree_type_non_common *node0;
	node0 = (struct tree_type_non_common *)node;
	do_type_with_lang_specific((tree)&node0->with_lang_specific, 0);
	do_tree(node0->values);
	do_tree(node0->minval);
	do_tree(node0->maxval);
#if __GNUC__ < 8
	do_tree(node0->binfo);
#else
	do_tree(node0->lang_1);
#endif
}

static void do_decl_minimal(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;
	if (flag)
		node_write(node);

	struct tree_decl_minimal *node0 = (struct tree_decl_minimal *)node;
	do_location(node0->locus);
	do_common((tree)&node0->common, 0);
	do_tree(node0->name);
	do_tree(node0->context);
}

static void do_decl_common(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;
	if (flag)
		node_write(node);

	struct tree_decl_common *node0 = (struct tree_decl_common *)node;
	do_decl_minimal((tree)&node0->common, 0);
	do_tree(node0->size);
	do_tree(node0->size_unit);
	do_tree(node0->initial);
	do_tree(node0->attributes);
	do_tree(node0->abstract_origin);
	do_c_lang_decl((struct c_lang_decl *)node0->lang_specific, 1);
}

static void do_field_decl(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;
	if (flag)
		node_write(node);

	struct tree_field_decl *node0 = (struct tree_field_decl *)node;
	do_decl_common((tree)&node0->common, 0);
	do_tree(node0->offset);
	do_tree(node0->bit_field_type);
	do_tree(node0->qualifier);
	do_tree(node0->bit_offset);
	do_tree(node0->fcontext);
}

static void do_decl_with_rtl(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;
	if (flag)
		node_write(node);

	struct tree_decl_with_rtl *node0 = (struct tree_decl_with_rtl *)node;
	do_decl_common((tree)&node0->common, 0);
	/* TODO: (node0->rtl); */
}

static void do_label_decl(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;
	if (flag)
		node_write(node);

	struct tree_label_decl *node0 = (struct tree_label_decl *)node;
	do_decl_with_rtl((tree)&node0->common, 0);
}

static void do_result_decl(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;
	if (flag)
		node_write(node);

	struct tree_result_decl *node0 = (struct tree_result_decl *)node;
	do_decl_with_rtl((tree)&node0->common, 0);
}

static void do_parm_decl(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;
	if (flag)
		node_write(node);

	struct tree_parm_decl *node0 = (struct tree_parm_decl *)node;
	do_decl_with_rtl((tree)&node0->common, 0);
	/* TODO: node0->incoming_rtl */
}

static void do_decl_with_vis(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;
	if (flag)
		node_write(node);

	struct tree_decl_with_vis *node0 = (struct tree_decl_with_vis *)node;
	do_decl_with_rtl((tree)&node0->common, 0);
	do_tree(node0->assembler_name);
	do_symtab_node((void *)node0->symtab_node, 1);
}

static void do_var_decl(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;
	if (flag) {
		node_write(node);
		size_t start = obj_idx - 1;
		expanded_location xloc;
		xloc = expand_location(DECL_SOURCE_LOCATION(node));
		if (si_is_global_var(node, &xloc)) {
			objs[start].is_global_var = 1;
		}
	}

	struct tree_var_decl *node0 = (struct tree_var_decl *)node;
	do_decl_with_vis((tree)&node0->common, 0);
}

static void do_decl_non_common(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;
	if (flag)
		node_write(node);

	struct tree_decl_non_common *node0;
	node0 = (struct tree_decl_non_common *)node;
	do_decl_with_vis((tree)&node0->common, 0);
	do_tree(node0->result);
}

static void do_type_decl(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;
	if (flag)
		node_write(node);

	struct tree_type_decl *node0 = (struct tree_type_decl *)node;
	do_decl_non_common((tree)&node0->common, 0);
}

static void do_translation_unit_decl(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;
	if (flag)
		node_write(node);

	struct tree_translation_unit_decl *node0 =
			(struct tree_translation_unit_decl *)node;
	do_decl_common((tree)&node0->common, 0);
	if (!is_obj_checked((void *)node0->language))
		mem_write((void *)node0->language,
				strlen(node0->language) + 1);
}

static void do_function_decl(tree node, int flag)
{
	if (!node)
		return;
	BUG_ON(!flag);

	struct tree_function_decl *node0;
	node0 = (struct tree_function_decl *)node;

	/*
	 * plugin/include/tree.h:
	 *	For example, for a FUNCTION_DECL, DECL_SAVED_TREE may be
	 *	non-NULL and DECL_EXTERNAL may be true simultaneously; that
	 *	can be the case for a C99 "extern inline" function.
	 */

#if 0
	/*
	 * UPDATE: as we modify the collection of gimples, there could be
	 * some function_decl there, which may still contain saved_tree.
	 */
	BUG_ON(node0->saved_tree);
#endif

	if (is_obj_checked((void *)node))
		return;
	node_write(node);

#if 0
	/*
	 * XXX: update: maybe we donot need to do this?
	 *
	 * XXX: update
	 *	an OOB access on tree_code_class happened in
	 *	net/core/sock_reuseport.c.
	 *	so first we decrease the function collection size
	 *	then add a check assert on tree_code_name gimple_code_name.
	 * NOTE: we need to save the name node
	 */
	if (!node0->f) {
		tree name = DECL_NAME(node);
		do_tree(name);
		return;
	}
#endif

	size_t start = obj_idx - 1;
	objs[start].is_function = 1;

	do_decl_non_common((tree)&node0->common, 0);
	do_function(node0->f, 1);
	do_tree(node0->arguments);
	do_tree(node0->personality);
	do_tree(node0->function_specific_target);
	do_tree(node0->function_specific_optimization);
	do_tree(node0->saved_tree);
	do_tree(node0->vindex);
}

static void do_const_decl(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;
	if (flag)
		node_write(node);

	struct tree_const_decl *node0 = (struct tree_const_decl *)node;
	do_decl_common((tree)&node0->common, 0);
}

static void do_vec(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;
	if (flag)
		node_write(node);

	struct tree_vec *node0 = (struct tree_vec *)node;
	do_common((tree)&node0->common, 0);
	for (int i = 0; i < TREE_VEC_LENGTH(node); i++)
		do_tree(node0->a[i]);
}

static void do_fixed_cst(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;
	if (flag)
		node_write(node);

	return;
}

static void do_vector(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;
	if (flag)
		node_write(node);

	return;
}

static void do_complex(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;
	if (flag)
		node_write(node);

	return;
}

static void do_tree_p(void *node)
{
	if (!node)
		return;
	if (is_obj_checked(node))
		return;

	tree *node0;
	node0 = (tree *)node;
	mem_write(node, sizeof(*node0));
	do_tree(*node0);
}

/*
 * check imm_use_iterator structure
 * TODO, still confused..
 *	the root ssa_use_operand_t use loc.ssa_name while others use
 *	loc.stmt?
 *	the root ssa_use_operand_t->use is NULL
 */
static void do_ssa_use_operand(void *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;

	struct ssa_use_operand_t *node0;
	node0 = (struct ssa_use_operand_t *)node;
	if (flag)
		mem_write(node, sizeof(*node0));

	do_ssa_use_operand(node0->prev, 1);
	do_ssa_use_operand(node0->next, 1);

	/*
	 * FIXME: stmt gimple or ssa_name tree?
	 *	if node0->use is NULL, this is a root node, use the ssa_name
	 *	otherwise, use the stmt
	 */
	if (node0->use) {
		do_gimple_seq(node0->loc.stmt);
		do_tree_p(node0->use);
	} else {
		/* this is the root node */
		BUG_ON(TREE_CODE(node0->loc.ssa_name) != SSA_NAME);
		do_tree(node0->loc.ssa_name);
	}
}

static void do_ptr_info_def(void *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;

	struct ptr_info_def *node0;
	node0 = (struct ptr_info_def *)node;
	if (flag)
		mem_write(node, sizeof(*node0));
}

static void do_range_info_def(void *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;

	struct range_info_def *node0;
	node0 = (struct range_info_def *)node;
	if (flag)
		mem_write(node, sizeof(*node0));
}

static void do_ssa_name(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;
	if (flag)
		node_write(node);

	struct tree_ssa_name *node0 = (struct tree_ssa_name *)node;
	do_typed((tree)&node0->typed, 0);
	do_tree(node0->var);
	do_gimple_seq(node0->def_stmt);

	int testv;
	testv = node0->typed.type ? (!POINTER_TYPE_P(TREE_TYPE(node))) : 2;
	if (!testv) {
		do_ptr_info_def(node0->info.ptr_info, 1);
	} else if (testv == 1) {
		do_range_info_def(node0->info.range_info, 1);
	}

	do_ssa_use_operand(&node0->imm_uses, 0);
}

static void do_binfo(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;
	if (flag)
		node_write(node);

	return;
}

static void do_omp_clause(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;
	if (flag)
		node_write(node);

	return;
}

static void do_optimization_option(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;
	if (flag)
		node_write(node);

	/* kernel/kexec_core.o */
	struct tree_optimization_option *node0;
	node0 = (struct tree_optimization_option *)node;
#if __GNUC__ < 8
	do_common((tree)&node0->common, 0);
#else
	do_base((tree)&node0->base, 0);
#endif
	/* TODO, opts, optabs, base_optabs */
}

static void do_target_option(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;
	if (flag)
		node_write(node);

	return;
}

#if 0
static int has_function = 0;
static void ast_collect(void *gcc_data, void *user_data)
{
	if (!has_function) {
		struct varpool_node *node;
		FOR_EACH_VARIABLE(node) {
			tree var = node->decl;
			do_tree(var);
		}
	}

	nodes_flush(outfd);
	close(outfd);
}
#endif

static int global_collected = 0;
static void do_global_tree(tree obj, unsigned short idx)
{
	BUG_ON(idx >= (1ULL<<(sizeof(idx)*8)));
	if (is_obj_checked((void *)obj)) {
		objs[obj_idx].fake_addr = (unsigned long)obj;
		objs[obj_idx].gcc_global_varidx = idx;
		obj_idx++;
	} else {
		int cur_idx = obj_idx;
		do_tree(obj);
		BUG_ON(cur_idx >= obj_idx);
		objs[cur_idx].gcc_global_varidx = idx;
	}
}

static void collect_global_data(void)
{
	/*
	 * use objs to track all gcc global vars
	 * check if the current object is already checked?
	 * if not, do the collection as usual
	 */

	unsigned short idx = 1; /* start with 1 */

	/* collect global_trees data */
	for (int i = 0; i < TI_MAX; i++) {
		do_global_tree(global_trees[i], idx);
		idx++;
	}

	/* collect the standard C integer types */
	for (int i = 0; i < itk_none; i++) {
		do_global_tree(integer_types[i], idx);
		idx++;
	}

	/* collect the types used to represent sizes */
	for (int i = 0; i < stk_type_kind_last; i++) {
		do_global_tree(sizetype_tab[i], idx);
		idx++;
	}
}

static void ast_collect(void *gcc_data, void *user_data)
{
	/*
	 * check gcc/cgraph.h FOR_EACH_SYMBOL:
	 *	for ((node) = symtab->first_symbol(); (node); ->next)
	 *	the first_symbol() just return nodes
	 * the file contains some other macros:
	 *	FOR_EACH_DEFINED_SYMBOL
	 *	FOR_EACH_DEFINED_FUNCTION
	 */
	symtab_node *node = symtab->nodes;
	while (node) {
		tree decl = node->decl;
		do_tree(decl);
		node = node->next;
	}

	/*
	 * collect some global data first
	 * make sure we only call collect_global_data() once
	 */
	if (!global_collected) {
		collect_global_data();
		global_collected = 1;
	}

	nodes_flush(outfd);
	close(outfd);
}

int plugin_init(struct plugin_name_args *plugin_info,
		struct plugin_gcc_version *version)
{
	int err = 0;

	/* XXX: match the current gcc version with version_needed */
	memset(ver, 0, sizeof(ver));
	snprintf(ver, sizeof(ver), "%d.%d", gcc_ver, gcc_ver_minor);
	version_needed.basever = ver;
	if (strncmp(version->basever, version_needed.basever,
				strlen(version_needed.basever))) {
		fprintf(stderr, "version not match: needed(%s) given(%s)\n",
				version_needed.basever,
				version->basever);
		return -1;
	}

	if (plugin_info->argc != 2) {
		fprintf(stderr, "plugin %s usage:\n"
				" -fplugin=.../%s.so"
				" -fplugin-arg-%s-outpath=..."
				" -fplugin-arg-%s-kernel=[0|1]\n",
				plugin_info->base_name,
				plugin_info->base_name,
				plugin_info->base_name,
				plugin_info->base_name);
		return -1;
	}
	outpath = plugin_info->argv[0].value;
	is_kernel = !!atoi(plugin_info->argv[1].value);

	if (tree_code_size(IDENTIFIER_NODE) != 0x50)
		BUG();

	const char *file = main_input_filename;
	/* this is for linux kernel modules .mod.c detection */
	if ((strlen(file) > 6) &&
			(strncmp(&file[strlen(file)-6], ".mod.c", 6) == 0))
		return 0;
	if (strcmp(&file[strlen(file)-2], ".c"))
		return 0;

	err = get_compiling_args();
	if (err) {
		fprintf(stderr, "get_compiling_args err\n");
		return -1;
	}

	/* kicking out some folders */
	const char *exclude_folders[] = {
		"scripts/", "Documentation/", "debian/", "debian.master/",
		"tools/", "samples/", "spl/", "usr/",
	};
	unsigned int i = 0;
	for (i = 0; i < sizeof(exclude_folders) / sizeof(char *); i++) {
		if (strstr(write_ctx->path, exclude_folders[i]))
			return 0;
	}

	err = prepare_outfile();
	if (err) {
		fprintf(stderr, "prepare_outfile err\n");
		return -1;
	}

	register_callback(plugin_info->base_name, PLUGIN_INFO, NULL,
				&this_plugin_info);

	/* collect data, flush data to file */
	register_callback(plugin_info->base_name, PLUGIN_ALL_IPA_PASSES_END,
				ast_collect, NULL);
	return 0;
}
