/*
 * this is a TEST gcc plugin, for collecting ast info
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
 * traversal the ast tree, keep as much information as you can.
 * if there are multiple source files, we may need to reduce the info we collected
 */

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
#include <diagnostic.h>
#include <stringpool.h>
#include <ssa-iterators.h>
#include <iostream>
#include <cassert>
#include <set>
#include <utility>
#include <algorithm>
#include <toplev.h>
#include <opts.h>
#include <clib.h>

struct plugin_info this_plugin_info = {
	.version = "0.1",
	.help = "collect functions' Abstract Syntax Tree, gcc version >= 5.4.0",
};

struct plugin_gcc_version version_needed = {
	.basever = "5.4",
};

int plugin_is_GPL_compatible;

namespace {
	const pass_data plugin_pass_data = {
		GIMPLE_PASS,
		"plugin_pass",
		OPTGROUP_NONE,
		TV_NONE,
		PROP_gimple_any,
		0,
		0,
		0,
		0
	};

	struct plugin_pass : gimple_opt_pass {
		plugin_pass(gcc::context *ctx) : gimple_opt_pass(plugin_pass_data,ctx)
		{}

		virtual unsigned int execute(function *fun) override {
			gimple_seq gimple_body = fun->gimple_body;

			struct walk_stmt_info walk_stmt_info;
			memset(&walk_stmt_info, 0, sizeof(walk_stmt_info));

			walk_gimple_seq(gimple_body, callback_stmt, callback_op,
					&walk_stmt_info);
			return 0;
		}

		virtual plugin_pass *clone() override {
			return this;
		}

		private:

		static tree callback_stmt(gimple_stmt_iterator *gsi,
					  bool *handled_all_ops,
					  struct walk_stmt_info *wi) {
			gimple *g = gsi_stmt(*gsi);

			location_t l = gimple_location(g);

			enum gimple_code code = gimple_code(g);

			fprintf(stderr, "Statement of type: %s at %s:%d\n",
					gimple_code_name[code],
					LOCATION_FILE(l),
					LOCATION_LINE(l));
			return NULL;
		}

		static tree callback_op(tree *t, int *arg, void *data) {
			enum tree_code code = TREE_CODE(*t);
			fprintf(stderr, "\tOperand: %s\n", get_tree_code_name(code));
			return NULL;
		}
	};
}

#define	DEF_CB_FUNC(name) \
void name(void *gcc_data, void *user_data) \
{\
	fprintf(stderr, "%s\n", #name);\
}\
void name(void *, void *user_data)

DEF_CB_FUNC(start_parse_function);
DEF_CB_FUNC(finish_parse_function);
//DEF_CB_FUNC(finish_type);
void finish_type(void *gcc_data, void *user_data)
{
	fprintf(stderr, "%s\n", __FUNCTION__);
	tree node = (tree)gcc_data;
	dump_node(node, TDF_TREE, stderr);
}
//DEF_CB_FUNC(finish_decl);
void finish_decl(void *gcc_data, void *user_data)
{
	fprintf(stderr, "%s\n", __FUNCTION__);
	tree node = (tree)gcc_data;
	if (TREE_CODE(node) == VAR_DECL) {
		fprintf(stderr, "%p %s %d file: %s, line: %d, column: %d\n", node,
				IDENTIFIER_POINTER(DECL_NAME(node)),
				TREE_STATIC(node),
				DECL_SOURCE_FILE(node), DECL_SOURCE_LINE(node),
				DECL_SOURCE_COLUMN(node));
	}
}
DEF_CB_FUNC(finish_unit);
DEF_CB_FUNC(pre_genericize);
DEF_CB_FUNC(finish);
DEF_CB_FUNC(ggc_start);
DEF_CB_FUNC(ggc_marking);
DEF_CB_FUNC(ggc_end);
DEF_CB_FUNC(attributes);
//DEF_CB_FUNC(start_unit);
void start_unit(void *gcc_data, void *user_data)
{
#if 0
	fprintf(stderr, "start_unit\n");
	struct cl_decoded_option *tmp;
	for (int i = 0; i < save_decoded_options_count; i++) {
		tmp = &save_decoded_options[i];
		fprintf(stderr, "%s\n", tmp->orig_option_with_args_text);
	}
#endif
}
DEF_CB_FUNC(pragmas);
DEF_CB_FUNC(all_passes_start);
DEF_CB_FUNC(all_passes_end);
DEF_CB_FUNC(all_ipa_passes_start);
DEF_CB_FUNC(all_ipa_passes_end);
DEF_CB_FUNC(early_gimple_passes_start);
DEF_CB_FUNC(early_gimple_passes_end);
DEF_CB_FUNC(new_pass);
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
	tree node = func->decl;
	if (strcmp(IDENTIFIER_POINTER(DECL_NAME(node)), "get_dict_value"))
		return 0;
	gimple_seq gimple_body = func->gimple_body;
	struct walk_stmt_info walk_stmt_info;
	memset(&walk_stmt_info, 0, sizeof(walk_stmt_info));
	walk_gimple_seq(gimple_body, callback_stmt, callback_op,
				&walk_stmt_info);
	return 0;
}

virtual pass_before_cfg *clone() override
{
	return this;
}

private:
static tree callback_stmt(gimple_stmt_iterator *gsi, bool *handled_all_ops,
			  struct walk_stmt_info *wi)
{
	gimple *g = gsi_stmt(*gsi);
	enum gimple_code code = gimple_code(g);
	if (code != GIMPLE_LABEL)
		fprintf(stderr, "Statement: %s\n", gimple_code_name[code]);
	else
		fprintf(stderr, "Statement: %s %p\n", gimple_code_name[code],
				((struct glabel *)g)->op[0]);
	return NULL;
}

static tree callback_op(tree *t, int *arg, void *data)
{
	enum tree_code code = TREE_CODE(*t);
	fprintf(stderr, "\t Op: %s %p\n", get_tree_code_name(code), *t);
	return NULL;
}
};	/* class pass_before_cfg */

}	/* namespace */

int plugin_init(struct plugin_name_args *plugin_info,
		struct plugin_gcc_version *version)
{
	int err = 0;

	/* XXX check current gcc version */
	if (strncmp(version->basever, version_needed.basever,
				strlen(version_needed.basever)) < 0) {
		fprintf(stderr, "version error: %s\n", this_plugin_info.help);
		return -1;
	}

	char *ptr;
	ptr = getenv("COLLECT_GCC_OPTIONS");
	if (ptr)
		fprintf(stderr, "COLLECT_GCC_OPTIONS = %s\n", ptr);
	register_callback(plugin_info->base_name, PLUGIN_INFO, NULL,
				&this_plugin_info);

#if 0
	register_callback(plugin_info->base_name, PLUGIN_START_PARSE_FUNCTION,
			start_parse_function, NULL);
	register_callback(plugin_info->base_name, PLUGIN_FINISH_PARSE_FUNCTION,
			finish_parse_function, NULL);
	register_callback(plugin_info->base_name, PLUGIN_FINISH_TYPE,
			finish_type, NULL);
	register_callback(plugin_info->base_name, PLUGIN_FINISH_DECL,
			finish_decl, NULL);
	register_callback(plugin_info->base_name, PLUGIN_FINISH_UNIT,
			finish_unit, NULL);
	register_callback(plugin_info->base_name, PLUGIN_PRE_GENERICIZE,
			pre_genericize, NULL);
	register_callback(plugin_info->base_name, PLUGIN_FINISH,
			finish, NULL);
	register_callback(plugin_info->base_name, PLUGIN_GGC_START,
			ggc_start, NULL);
	register_callback(plugin_info->base_name, PLUGIN_GGC_MARKING,
			ggc_marking, NULL);
	register_callback(plugin_info->base_name, PLUGIN_GGC_END,
			ggc_end, NULL);
	register_callback(plugin_info->base_name, PLUGIN_ATTRIBUTES,
			attributes, NULL);
	register_callback(plugin_info->base_name, PLUGIN_START_UNIT,
			start_unit, NULL);
	register_callback(plugin_info->base_name, PLUGIN_PRAGMAS,
			pragmas, NULL);
	register_callback(plugin_info->base_name, PLUGIN_ALL_PASSES_START,
			all_passes_start, NULL);
	register_callback(plugin_info->base_name, PLUGIN_ALL_PASSES_END,
			all_passes_end, NULL);
	register_callback(plugin_info->base_name, PLUGIN_ALL_IPA_PASSES_START,
			all_ipa_passes_start, NULL);
	register_callback(plugin_info->base_name, PLUGIN_ALL_IPA_PASSES_END,
			all_ipa_passes_end, NULL);
	register_callback(plugin_info->base_name, PLUGIN_EARLY_GIMPLE_PASSES_START,
			early_gimple_passes_start, NULL);
	register_callback(plugin_info->base_name, PLUGIN_EARLY_GIMPLE_PASSES_END,
			early_gimple_passes_end, NULL);
	register_callback(plugin_info->base_name, PLUGIN_NEW_PASS,
			new_pass, NULL);
#endif

	struct register_pass_info pass_info;
	pass_info.pass = new pass_before_cfg(g);
	pass_info.reference_pass_name = "cfg";
	pass_info.ref_pass_instance_number = 1;
	pass_info.pos_op = PASS_POS_INSERT_BEFORE;

	register_callback(plugin_info->base_name, PLUGIN_PASS_MANAGER_SETUP, NULL,
				&pass_info);
	return 0;
}
