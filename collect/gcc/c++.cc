#include "./core.cc"
#include <cp/cp-tree.h>

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

static void do_specific_identifier(tree node, int flag)
{
	struct lang_identifier *node0 = (struct lang_identifier *)node;
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;
	if (flag)
		mem_write((void *)node, sizeof(*node0));

	do_c_common_identifier(&node0->c_common, 0);
	/* TODO: cxx_binding *bindings; */
}

static void do_specific_lang_type(void *node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;

	struct lang_type *node0 = (struct lang_type *)node;
	if (flag)
		mem_write(node, sizeof(*node0));
	do_tree(node0->primary_base);
	/* TODO: vec<tree_pair_s, va_gc> *vcall_indices; */
	do_tree(node0->vtables);
	do_tree(node0->typeinfo_var);
	do_vec_tree(node0->vbases, 1);
	/* TODO: binding_table nested_udts; */
	do_tree(node0->as_base);
	do_vec_tree(node0->pure_virtuals, 1);
	do_tree(node0->friend_classes);
	do_vec_tree(node0->members, 1);
	do_tree(node0->key_method);
	do_tree(node0->decl_list);
	do_tree(node0->befriending_classes);
	do_tree(node0->objc_info);
	do_tree(node0->lambda_expr);
}

static void do_lang_decl_base(struct lang_decl_base *n, int flag)
{
	if (!n)
		return;
	if (flag && is_obj_checked(n))
		return;
	if (flag)
		mem_write((void *)n, sizeof(*n));
	return;
}

static void do_lang_decl_min(struct lang_decl_min *n, int flag)
{
	if (!n)
		return;
	if (flag && is_obj_checked(n))
		return;
	if (flag)
		mem_write((void *)n, sizeof(*n));

	do_tree(n->template_info);
	do_tree(n->access);
	return;
}

static void do_lang_decl_fn(struct lang_decl_fn *n, int flag)
{
	if (!n)
		return;
	if (flag && is_obj_checked(n))
		return;
	if (flag)
		mem_write((void *)n, sizeof(*n));

	do_lang_decl_min(&n->min, 0);
	do_tree(n->befriending_classes);
	do_tree(n->context);
	if (n->thunk_p == 0)
		do_tree(n->u5.cloned_function);
	/* TODO: lang_decl_u3 */
}

static void do_lang_decl_ns(struct lang_decl_ns *n, int flag)
{
	if (!n)
		return;
	if (flag && is_obj_checked(n))
		return;
	if (flag)
		mem_write((void *)n, sizeof(*n));

	do_lang_decl_base(&n->base, 0);
	/* TODO: cp_binding_level *level; */
	do_vec_tree(n->usings, 1);
	do_vec_tree(n->inlinees, 1);
	/* TODO: hash_table<named_label_hash> *bindings; */
}

static void do_lang_decl_parm(struct lang_decl_parm *n, int flag)
{
	if (!n)
		return;
	if (flag && is_obj_checked(n))
		return;
	if (flag)
		mem_write((void *)n, sizeof(*n));

	do_lang_decl_base(&n->base, 0);
	return;
}

static void do_lang_decl_decomp(struct lang_decl_decomp *n, int flag)
{
	if (!n)
		return;
	if (flag && is_obj_checked(n))
		return;
	if (flag)
		mem_write((void *)n, sizeof(*n));

	do_lang_decl_min(&n->min, 0);
	do_tree(n->base);
	return;
}

static void do_specific_lang_decl(void *n, int flag)
{
	struct lang_decl *node = (struct lang_decl *)n;
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;
	if (flag)
		mem_write((void *)node, sizeof(*node));

	struct lang_decl *node0 = (struct lang_decl *)node;
	switch (node0->u.base.selector) {
	case lds_min:
		do_lang_decl_min(&node0->u.min, 0);
		break;
	case lds_fn:
		do_lang_decl_fn(&node0->u.fn, 0);
		break;
	case lds_ns:
		do_lang_decl_ns(&node0->u.ns, 0);
		break;
	case lds_parm:
		do_lang_decl_parm(&node0->u.parm, 0);
		break;
	case lds_decomp:
		do_lang_decl_decomp(&node0->u.decomp, 0);
		break;
	default:
		do_lang_decl_base(&node0->u.base, 0);
		break;
	}
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

static void do_specific_language_function(void *n, int flag)
{
	struct language_function *node = (struct language_function *)n;
	if (!node)
		return;
	if (flag && is_obj_checked(node))
		return;

	if (flag)
		mem_write((void *)node, sizeof(*node));
	do_c_language_function(&node->base, 0);
	do_tree(node->x_cdtor_label);
	do_tree(node->x_current_class_ptr);
	do_tree(node->x_current_class_ref);
	do_tree(node->x_eh_spec_block);
	do_tree(node->x_in_charge_parm);
	do_tree(node->x_vtt_parm);
	do_tree(node->x_return_value);
	do_tree(node->x_auto_return_pattern);

	/* TODO: hash_table<named_label_hash> *x_named_labels; */
	/* TODO: cp_binding_level *bindings; */
	/* TODO: vec<tree, va_gc> *infinite_loops; */
	/* TODO: hash_table<cxx_int_tree_map_hasher> *extern_decl_map; */
}

static void do_template_parm_index(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;
	if (flag)
		node_write(node);

	struct template_parm_index *n;
	n = (struct template_parm_index *)node;
	do_common((tree)&n->common, 0);
	do_tree(n->decl);
	return;
}

static void do_template_info(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;
	if (flag)
		node_write(node);

	struct tree_template_info *n;
	n = (struct tree_template_info *)node;
	do_common((tree)&n->common, 0);
	/* TODO: vec<qualified_typedef_usage_t, va_gc> *xxx; */
	return;
}

static void do_template_decl(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;
	if (flag)
		node_write(node);

	struct tree_template_decl *n;
	n = (struct tree_template_decl *)node;
	do_decl_common((tree)&n->common, 0);
	do_tree(n->arguments);
	do_tree(n->result);
}

static void do_using_decl(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;
	if (flag)
		node_write(node);

	/* TODO */
}

static void do_overload(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;
	if (flag)
		node_write(node);

	struct tree_overload *node0 = (struct tree_overload *)node;
	do_common((tree)&node0->common, 0);
	do_tree(node0->function);
}

static void do_deferred_noexcept(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;
	if (flag)
		node_write(node);

	struct tree_deferred_noexcept *node0;
	node0 = (struct tree_deferred_noexcept *)node;
	do_base((tree)&node0->base, 0);
	do_tree(node0->pattern);
	do_tree(node0->args);
}

static void do_baselink(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;
	if (flag)
		node_write(node);

	struct tree_baselink *node0;
	node0 = (struct tree_baselink *)node;
	do_common((tree)&node0->common, 0);
	do_tree(node0->binfo);
	do_tree(node0->functions);
	do_tree(node0->access_binfo);
}

static void do_static_assert(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;
	if (flag)
		node_write(node);

	struct tree_static_assert *node0;
	node0 = (struct tree_static_assert *)node;
	do_location(node0->location);
	do_common((tree)&node0->common, 0);
	do_tree(node0->condition);
	do_tree(node0->message);
}

static void do_trait_expr(tree node, int flag)
{
	if (!node)
		return;
	if (flag && is_obj_checked((void *)node))
		return;
	if (flag)
		node_write(node);

	struct tree_trait_expr *node0;
	node0 = (struct tree_trait_expr *)node;
	do_common((tree)&node0->common, 0);
	do_tree(node0->type1);
	do_tree(node0->type2);
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

	const char *file = main_input_filename;
	if (strcmp(&file[strlen(file)-3], ".cc") &&
		strcmp(&file[strlen(file)-3], ".CC"))
		return 0;

	err = get_compiling_args(SI_TYPE_DF_GCC_CPP);
	if (err) {
		fprintf(stderr, "get_compiling_args err\n");
		return -1;
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
