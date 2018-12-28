/*
 * collect C project info compiled by GCC
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
/*
 *	some tree_node is not handled for now, just raise a BUG
 *	this collect information of lower gimple form before cfg pass
 *
 * refs:
 *	https://gcc.gnu.org/onlinedocs/gcc-6.4.0/gccint/GTY-Options.html
 */
#include "../si_core.h"

struct plugin_info this_plugin_info = {
	.version = "0.1",
	.help = "collect functions' Abstract Syntax Tree, gcc version >= 5.4.0",
};

struct plugin_gcc_version version_needed = {
	.basever = "5.4",
};

int plugin_is_GPL_compatible;

static int has_function = 0;
static char nodes_mem[MAX_SIZE_PER_FILE];
static char *mem_ptr_start = NULL;
static char *mem_ptr = NULL;
static struct file_context *write_ctx;

static const char *outpath = "/tmp/c_ast";
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
	write_ctx = (struct file_context *)nodes_mem;
	write_ctx->type = SI_LANG_TYPE_C;
	write_ctx->status = FC_STATUS_NONE;
	write_ctx->format = FC_FORMAT_BEFORE_CFG;

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
		char *cmd = (char *)file_context_cmd_position((void *)nodes_mem);
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
	mem_ptr_start = (char *)file_context_payload_position((void *)nodes_mem);
	mem_ptr = mem_ptr_start;
	return 0;
}

static struct file_obj objs[MAX_OBJS_PER_FILE];
/* XXX: for is_obj_checked optimization */
static void *fake_addrs[MAX_OBJS_PER_FILE];
static int objs_idx = 0;
static inline int is_obj_checked(void *obj)
{
	if (unlikely(!obj))
		return 1;

	if (unlikely(objs_idx >= MAX_OBJS_PER_FILE))
		BUG();

	int i = 0;
	int loop_limit = objs_idx;
	if (unlikely(loop_limit > (MAX_OBJS_PER_FILE / 8))) {
		loop_limit = MAX_OBJS_PER_FILE / 8;
		for (i = loop_limit; i < objs_idx; i++) {
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
	objs[objs_idx].fake_addr = (unsigned long)obj;
	fake_addrs[objs_idx] = obj;
	objs[objs_idx].size = size;
	objs_idx++;
}

static void nodes_flush(int fd)
{
	if (mem_ptr == mem_ptr_start)
		return;

	int i = 0;
	for (i = 0; i < MAX_OBJS_PER_FILE; i++) {
		if ((!objs[i].fake_addr))
			break;
	}

	write_ctx->objs_offs = mem_ptr - nodes_mem;
	write_ctx->objs_cnt = i;
	write_ctx->total_size = write_ctx->objs_offs + i * sizeof(objs[0]);
	write_ctx->total_size = clib_round_up(write_ctx->total_size, PAGE_SIZE);
	if ((write_ctx->total_size < write_ctx->objs_offs) ||
			(write_ctx->total_size > MAX_SIZE_PER_FILE))
		BUG();

	memcpy(mem_ptr, (char *)objs, i*sizeof(objs[0]));

	/* XXX: important, only one write syscall */
	int err = write(fd, nodes_mem, write_ctx->total_size);
	if (err == -1)
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
				((mem_ptr - nodes_mem) > MAX_SIZE_PER_FILE)))
		BUG();
}

/* NOTE, we need to keep a expanded_location tracked */
static unsigned long xloc_addr = (unsigned long)-1;
static void mem_write_1(void *addr, size_t size)
{
	memcpy(mem_ptr, addr, size);
	insert_obj((void *)xloc_addr, size);
	xloc_addr--;
	mem_ptr += size;
	if (unlikely((mem_ptr < nodes_mem) ||
				((mem_ptr - nodes_mem) > MAX_SIZE_PER_FILE)))
		BUG();
}

static void mem_write_2(char c)
{
	*(mem_ptr-1) = c;
}

static void mem_write_3(void *addr)
{
	char path[PATH_MAX];
	BUG_ON(!realpath((char *)addr, path));

	memcpy(mem_ptr, path, strlen(path)+1);
	insert_obj((void *)addr, strlen(path)+1);
	mem_ptr += strlen(path)+1;
	if (unlikely((mem_ptr < nodes_mem) ||
				((mem_ptr - nodes_mem) > MAX_SIZE_PER_FILE)))
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
	/* NOTE, all do_location calls must put at the first place */
	expanded_location xloc = expand_location(loc);
	mem_write_1((void *)&xloc, sizeof(xloc));
	if (!is_obj_checked((void *)xloc.file)) {
		if (unlikely(!strncmp(xloc.file, "<built-in>", 10)))
			mem_write((void *)xloc.file, strlen(xloc.file)+1);
		else
			mem_write_3((void *)xloc.file);
	}
}

static void do_vec_tree(void *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;

	vec<tree, va_gc, vl_embed> *node0 = (vec<tree, va_gc, vl_embed> *)node;

	unsigned long lenth = node0->length();
	BUG_ON(!lenth);
	tree *addr = node0->address();
	if (flag)
		mem_write(node, sizeof(*node0)+(lenth-1)*sizeof(*addr));
	for (unsigned long i = 0; i < lenth; i++) {
		do_tree(addr[i]);
	}
}

static void do_vec_constructor(void *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;

	vec<constructor_elt, va_gc> *node0 = (vec<constructor_elt, va_gc> *)node;

	unsigned long lenth = node0->length();
	BUG_ON(!lenth);
	struct constructor_elt *addr = node0->address();
	if (flag)
		mem_write(node, sizeof(*node0)+(lenth-1)*sizeof(*addr));
	for (unsigned long i = 0; i < lenth; i++) {
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

	vec<c_goto_bindings_p, va_gc> *node0 = (vec<c_goto_bindings_p, va_gc> *)node;
	unsigned long len = node0->length();
	BUG_ON(!len);
	c_goto_bindings_p *addr = node0->address();
	if (flag)
		mem_write(node, sizeof(*node0)+(len-1)*sizeof(*addr));
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
		mem_write_2((char)cpp_token_val_index(node));
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
static void do_cpp_macro(struct cpp_macro *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;
	if (flag)
		mem_write((void *)node, sizeof(*node));
	do_location(node->line);
	if (node->params && !is_obj_checked(node->params)) {
		mem_write((void *)node->params,node->paramc*sizeof(node->params[1]));
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
		mem_write((void *)node, sizeof(*node)+(len-1)*sizeof(cpp_token));
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

#if 0
struct GTY(()) sorted_fields_type {
	int len;
	tree GTY((length("%h.len"))) elts[1];
};
#endif
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
static void do_c_arg_tag(struct c_arg_tag *node, int flag)
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
	BUG_ON(len==0);
	c_arg_tag *addr = node->address();
	if (flag)
		mem_write((void *)node, sizeof(*node)+(len-1)*sizeof(*addr));
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
static size_t gimple_total_size(gimple_seq gs)
{
	size_t base_size = gsstruct_code_size[gss_for_code(gimple_code(gs))];
	if (!gimple_has_ops(gs) || (!gimple_num_ops(gs)))
		return base_size;
	return base_size + (gimple_num_ops(gs)-1) * sizeof(tree);
}
static void do_gimple_seq(gimple_seq gs, int flag)
{
	if (!gs)
		return;
	if (flag && is_obj_checked((void *)gs))
		return;
	if (flag)
		mem_write((void *)gs, gimple_total_size(gs));

	do_location(gs->location);
	tree *ops = gimple_ops(gs);
	for (int i = 0; i < gimple_num_ops(gs); i++) {
		/* TODO, handle function decl */
		do_tree(ops[i]);
	}

	do_gimple_seq(gs->next, 1);
	do_gimple_seq(gs->prev, 1);
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
	do_tree(node0->cilk_frame_decl);
	do_gimple_seq(node0->gimple_body, 1);
	/* TODO, node0->eh, except.h, ignore it */
	/* TODO, node0->cfg, ignore */
	/* TODO, node0->gimple_df */
	/* TODO, node0->x_current_loops */
	/* TODO, node0->su */
	/* TODO, node0->value_histograms */
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
	if (unlikely(is_obj_checked((void *)node)))
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
#if 0	/* flag will never be 1 */
	if (flag && is_obj_checked((void *)node))
		return;
	if (flag)
		node_write(node);
#endif
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

static void do_identifier(tree node, int flag)
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
	if (TREE_CODE_CLASS(TREE_CODE(node)) == tcc_vl_exp)
		for (int i = 0; i < VL_EXP_OPERAND_LENGTH(node); i++)
			do_tree(node0->operands[i]);
	else
		for (int i = 0; i < TREE_CODE_LENGTH(TREE_CODE(node)); i++)
			do_tree(node0->operands[i]);
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
		size_t start = objs_idx - 1;
#if 0
		if ((TYPE_NAME(node)) &&
				((TREE_CODE(TYPE_NAME(node)) == IDENTIFIER_NODE) ||
				 (DECL_NAME(TYPE_NAME(node)))))
#endif
			objs[start].is_type = 1;
	}

	struct tree_type_non_common *node0 = (struct tree_type_non_common *)node;
	do_type_with_lang_specific((tree)&node0->with_lang_specific, 0);
	do_tree(node0->values);
	do_tree(node0->minval);
	do_tree(node0->maxval);
	do_tree(node0->binfo);
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
		size_t start = objs_idx - 1;
		if (is_global_var(node) &&
			((!DECL_CONTEXT(node)) ||
			 (TREE_CODE(DECL_CONTEXT(node)) == TRANSLATION_UNIT_DECL))) {
			objs[start].is_global_var = 1;
		}
#if 0
		if ((!DECL_EXTERNAL(node)) && (DECL_NAME(node)) &&
				((!DECL_CONTEXT(node)) ||
				 (TREE_CODE(DECL_CONTEXT(node)) ==
					TRANSLATION_UNIT_DECL)) &&
				TREE_STATIC(node))
			objs[start].is_global_var = 1;
		if ((DECL_NAME(node)) &&
				((!DECL_CONTEXT(node)) ||
				 (TREE_CODE(DECL_CONTEXT(node)) ==
					TRANSLATION_UNIT_DECL)))
			objs[start].is_global_var = 1;
		if ((!DECL_CONTEXT(node)) ||
			(TREE_CODE(DECL_CONTEXT(node)) == TRANSLATION_UNIT_DECL)) {
			objs[start].is_global_var = 1;
		}
#endif
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

	struct tree_decl_non_common *node0 = (struct tree_decl_non_common *)node;
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
		mem_write((void *)node0->language, strlen(node0->language) + 1);
}

static void do_function_decl(tree node, int flag)
{
	if (!node)
		return;
	BUG_ON(!flag);
	if (is_obj_checked((void *)node))
		return;

	/* XXX, check the gimple_body first, if null, do not handle it now */
	struct tree_function_decl *node0 = (struct tree_function_decl *)node;
	if (node0->saved_tree)
		return;

#if 0
	if ((!node0->f) || (!node0->f->gimple_body))
		return;
#endif

	node_write(node);
	size_t start = objs_idx - 1;
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
#if 0
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;
	if (flag)
		node_write(node);
#endif

	BUG();
}

static void do_vector(tree node, int flag)
{
#if 0
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;
	if (flag)
		node_write(node);
#endif

	BUG();
}

static void do_complex(tree node, int flag)
{
#if 0
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;
	if (flag)
		node_write(node);
#endif

	BUG();
}

static void do_ssa_name(tree node, int flag)
{
#if 0
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;
	if (flag)
		node_write(node);
#endif

	BUG();
}

static void do_binfo(tree node, int flag)
{
#if 0
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;
	if (flag)
		node_write(node);
#endif

	BUG();
}

static void do_omp_clause(tree node, int flag)
{
#if 0
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;
	if (flag)
		node_write(node);
#endif

	BUG();
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
	struct tree_optimization_option *node0 =
					(struct tree_optimization_option *)node;
	do_common((tree)&node0->common, 0);
	/* TODO, opts, optabs, base_optabs */
}

static void do_target_option(tree node, int flag)
{
#if 0
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;
	if (flag)
		node_write(node);
#endif

	BUG();
}

#if 0
#define	MAX_FUNCS_PER_FILE	0x1000
static void *func_nodes[MAX_FUNCS_PER_FILE];
static int node_idx = 0;
static int collected = 0;
static void parse_function(void *gcc_data, void *user_data)
{
	tree node = (tree)gcc_data;

	if (unlikely(node == NULL_TREE)) {
		fprintf(stderr, "NULL_TREE happened\n");
		return;
	}

	enum tree_code code = TREE_CODE(node);
	if (DECL_EXTERNAL(node))
		return;
	if (DECL_IS_BUILTIN(node))
		return;
	if (unlikely(code != FUNCTION_DECL))
		return;
#if 0
#0  0x000000000067d357 in release_function_body(tree_node*) ()
#1  0x000000000067d3f3 in cgraph_node::release_body(bool) ()
#2  0x0000000000680b73 in cgraph_node::remove() ()
#3  0x000000000068aa9f in ?? ()	analyze_functions
#4  0x000000000068af2b in symbol_table::finalize_compilation_unit() ()
#5  0x0000000000997c8a in ?? () compile_file
#6  0x0000000000572310 in toplev::main(int, char**) ()
#endif
	/* function body, will be released at release_function_body */
	if (DECL_SAVED_TREE(node) == NULL_TREE)
		return;

	func_nodes[node_idx] = gcc_data;
	node_idx++;
}
#endif

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
}

namespace {
const pass_data pass_data_before_cfg = {
	GIMPLE_PASS,
	"before_cfg",
	OPTGROUP_NONE,
	TV_NONE,
	PROP_gimple_lcf,
	0,
	0,
	0,
	0
};
const pass_data pass_data_after_cfg = {
	GIMPLE_PASS,
	"after_cfg",
	OPTGROUP_NONE,
	TV_NONE,
	PROP_gimple_lcf,
	0,
	0,
	0,
	0
};

class pass_before_cfg : public gimple_opt_pass {
public:
pass_before_cfg(gcc::context *ctx) : gimple_opt_pass(pass_data_before_cfg, ctx)
{}

virtual unsigned int execute(function *func) override
{
	has_function = 1;
	tree func_node = func->decl;
	BUG_ON(TREE_CODE(func_node) != FUNCTION_DECL);
	BUG_ON(DECL_STRUCT_FUNCTION(func_node)->gimple_body != func->gimple_body);
	do_function_decl(func_node, 1);
	return 0;
}

virtual pass_before_cfg *clone() override
{
	return this;
}

private:
#if 0
static tree callback_stmt(gimple_stmt_iterator *gsi, bool *handled_all_ops,
			  struct walk_stmt_info *wi)
{
	gimple *g = gsi_stmt(*gsi);
	location_t l = gimple_location(g);
	enum gimple_code code = gimple_code(g);
	fprintf(stderr, "Statement of type: %s at %s: %d\n", gimple_code_name[code],
			LOCATION_FILE(l), LOCATION_LINE(l));
	return NULL;
}

static tree callback_op(tree *t, int *arg, void *data)
{
	enum tree_code code = TREE_CODE(*t);
	fprintf(stderr, "\t Operand: %s\n", get_tree_code_name(code));
	return NULL;
}
#endif
};	/* class pass_before_cfg */

class pass_after_cfg : public gimple_opt_pass {
public:
pass_after_cfg(gcc::context *ctx) : gimple_opt_pass(pass_data_after_cfg, ctx)
{}

virtual unsigned int execute(function *func) override
{
	return 0;
}

virtual pass_after_cfg *clone() override
{
	return this;
}

private:
};	/* class pass_after_cfg */
}	/* namespace */

int plugin_init(struct plugin_name_args *plugin_info,
		struct plugin_gcc_version *version)
{
	int err = 0;
	/* XXX: we need the current gcc version larger than version_needed */
	if (strncmp(version->basever, version_needed.basever,
				strlen(version_needed.basever)) < 0) {
		fprintf(stderr, "version error: %s\n", this_plugin_info.help);
		return -1;
	}

	if (plugin_info->argc != 1) {
		fprintf(stderr, "plugin %s usage: -fplugin=.../%s.so"
				" -fplugin-arg-%s-outpath=...\n",
				plugin_info->base_name,
				plugin_info->base_name,
				plugin_info->base_name);
		return -1;
	}
	outpath = plugin_info->argv[0].value;

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

	/* this is for kicking some folder */
	const char *exclude_folders[] = {
		"scripts/", "Documentation/", "debian/", "debian.master/",
		"tools/", "samples/", "spl/", "usr/",
	};
	for (int i = 0; i < sizeof(exclude_folders) / sizeof(char *); i++) {
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

	/* collect the lower GIMPLE before cfg pass */
	struct register_pass_info before_info;
	before_info.pass = new pass_before_cfg(g);
	before_info.reference_pass_name = "cfg";
	before_info.ref_pass_instance_number = 1;
	before_info.pos_op = PASS_POS_INSERT_BEFORE;
	register_callback(plugin_info->base_name, PLUGIN_PASS_MANAGER_SETUP,
				NULL, &before_info);

	/* flush data to file */
	register_callback(plugin_info->base_name, PLUGIN_ALL_IPA_PASSES_START,
				ast_collect, NULL);
#if 0
	/* could not do the flush this way */
	struct register_pass_info after_info;
	after_info.pass = new pass_after_cfg(g);
	after_info.reference_pass_name = "cfg";
	after_info.ref_pass_instance_number = 1;
	after_info.pos_op = PASS_POS_INSERT_AFTER;
	register_callback(plugin_info->base_name, PLUGIN_PASS_MANAGER_SETUP,
				NULL, &after_info);
	register_callback(plugin_info->base_name, PLUGIN_FINISH_PARSE_FUNCTION,
				parse_function, NULL);
	/* lower GIMPLE */
	register_callback(plugin_info->base_name, PLUGIN_ALL_IPA_PASSES_START,
				ast_collect, NULL);
	register_callback(plugin_info->base_name, PLUGIN_PRE_GENERICIZE,
				ast_collect, NULL);
	register_callback(plugin_info->base_name, PLUGIN_FINISH,
				plugin_exit, NULL);
#endif
	return 0;
}
