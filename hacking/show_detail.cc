/*
 * output detail of variables, functions, etc.
 *
 * Copyright (C) 2020  zerons
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
#include "si_gcc.h"
#include "./hacking.h"

CLIB_MODULE_NAME(show_detail);
CLIB_MODULE_NEEDED0();
static char modname[] = "show_detail";

static void usage(void)
{
	fprintf(stdout, "\t(name) (var|func|type) [extra opts]\n"
			"\tShow detail of var, func, or data type maybe?\n"
			"\tIf type, the name could be xxx.yyy.zzz\n"
			"\tSupported extra options now:\n"
			"\tFor all:\n"
			"\t\tall: output all\n"
			"\t\tsrc: output where it is defined\n"
			"\tFor func:\n"
			"\t\tcaller: output callers\n"
			"\t\tcallee: output callees\n"
			"\t\tbody: output function body(pointer now)\n"
			"\tFor var:\n"
			"\t\tused: output functions use this var\n"
			"\tFor type:\n"
			"\t\tused: output functions use this field\n"
			"\t\toffset: output offset in the parent type\n"
			"\t\tsize: bits of this field\n");
}

static char *argv_name;
#define	TYPE_MAX_DEPTH	8
static char *split_names[TYPE_MAX_DEPTH];
static char *argv_opt;
static int __match(struct sinode *sn);
static void __cb(struct sinode *sn);
static long cb(int argc, char *argv[])
{
	long err = 0;

	if ((argc != 3) && (argc != 4)) {
		err_msg("argc invalid");
		return -1;
	}

	argv_name = argv[1];
	char *type = argv[2];
	argv_opt = (char *)"all";
	if (argc == 4)
		argv_opt = argv[3];

	if (!strcmp(type, "type")) {
		/* split argv_name */
		char *ptr_b = argv_name;
		char *ptr_e = NULL;
		int i = 0;
		while ((ptr_e = strchr(ptr_b, '.'))) {
			split_names[i] = ptr_b;
			i++;
			if (i >= TYPE_MAX_DEPTH) {
				err_msg("input type exceed the max depth");
				err = -1;
				goto out;
			}
			*ptr_e = '\0';
			ptr_b = ptr_e + 1;
		}
		split_names[i] = ptr_b;
	}

	analysis__sinode_match(type, __match, __cb);

out:
	argv_name = NULL;
	argv_opt = NULL;
	for (int i = 0; i < TYPE_MAX_DEPTH; i++) {
		split_names[i] = NULL;
	}
	return err;
}

static int __match(struct sinode *sn)
{
	enum sinode_type type;
	type = sinode_idtype(sn);

	if (type != TYPE_TYPE) {
		if (!strcmp(sn->name, argv_name))
			return 1;
		return 0;
	} else {
		if (!strcmp(sn->name, split_names[0]))
			return 1;

		return 0;
	}
}

static inline void output_src(struct sinode *sn)
{
	fprintf(stdout, "src: %s %d %d\n",
		sn->loc_file ? sn->loc_file->name : "NULL",
		sn->loc_line, sn->loc_col);
}

static void output_func(struct sinode *sn)
{
	struct func_node *fn;
	int all;

	fn = (struct func_node *)sn->data;
	all = !strcmp(argv_opt, "all");

	if (all || (!strcmp(argv_opt, "src"))) {
		output_src(sn);
	}

	if (fn && (all || (!strcmp(argv_opt, "caller")))) {
		fprintf(stdout, "Callers:\n");

		struct callf_list *tmp;
		list_for_each_entry(tmp, &fn->callers, sibling) {
			if (tmp->value_flag) /* TODO: this is a tree */
				continue;

			union siid *id = (union siid *)&tmp->value;
			struct sinode *caller_sn;
			caller_sn = analysis__sinode_search(siid_type(id),
							SEARCH_BY_ID, id);
			fprintf(stdout, "\t0x%lx %s\n",
					tmp->value,
					caller_sn ? caller_sn->name : "NULL");
		}
	}

	if (fn && (all || (!strcmp(argv_opt, "callee")))) {
		fprintf(stdout, "Callees:\n");

		struct callf_list *tmp;
		list_for_each_entry(tmp, &fn->callees, sibling) {
			if (tmp->value_flag) /* TODO: this is a tree */
				continue;

			union siid *id = (union siid *)&tmp->value;
			struct sinode *caller_sn;
			caller_sn = analysis__sinode_search(siid_type(id),
							SEARCH_BY_ID, id);
			fprintf(stdout, "\t0x%lx %s\n",
					tmp->value,
					caller_sn ? caller_sn->name : "NULL");
		}
	}

	if (all || (!strcmp(argv_opt, "body"))) {
		fprintf(stdout, "Body:\n"
				"\t%p\n", fn);
	}
}

static void output_var(struct sinode *sn)
{
	int all;
	struct var_node *vn;

	vn = (struct var_node *)sn->data;
	all = !strcmp(argv_opt, "all");

	if (all || (!strcmp(argv_opt, "src"))) {
		output_src(sn);
	}

	if (vn && (all || (!strcmp(argv_opt, "used")))) {
		fprintf(stdout, "Used at:\n");

		struct use_at_list *tmp;
		list_for_each_entry(tmp, &vn->used_at, sibling) {
			struct sinode *fsn;
			fsn = analysis__sinode_search(siid_type(&tmp->func_id),
					SEARCH_BY_ID, &tmp->func_id);
			fprintf(stdout, "\t%s %p %ld\n",
					fsn ? fsn->name : "NULL",
					tmp->gimple_stmt,
					tmp->op_idx);
		}
	}
}

static void output_type(struct sinode *sn)
{
	int all;
	struct type_node *tn;

	tn = (struct type_node *)sn->data;
	all = !strcmp(argv_opt, "all");

	if (!tn)
		return;

	struct var_list *vl_tmp = NULL;
	struct type_node *tn_tmp = tn;
	int i = 1;
	int found = 0;
	for (; i < TYPE_MAX_DEPTH; i++) {
		if ((!split_names[i]) || (!split_names[i][0])) {
			found = 1;
			break;
		}

		vl_tmp = get_tn_field(tn_tmp, split_names[i]);
		if (!vl_tmp)
			break;

		tn_tmp = vl_tmp->var.type;
	}

	if (!found)
		return;

	if (all || (!strcmp(argv_opt, "src"))) {
		output_src(sn);
	}

	if (!vl_tmp) {
		/* ignore argv_opt, handle the used_at */
		fprintf(stdout, "Used at:\n");

		struct use_at_list *tmp;
		list_for_each_entry(tmp, &tn_tmp->used_at, sibling) {
			struct sinode *fsn;
			fsn = analysis__sinode_search(siid_type(&tmp->func_id),
					SEARCH_BY_ID, &tmp->func_id);
			fprintf(stdout, "\t%s %p %ld\n",
					fsn ? fsn->name : "NULL",
					tmp->gimple_stmt,
					tmp->op_idx);
		}

		return;
	}

	if (all || (!strcmp(argv_opt, "used"))) {
		fprintf(stdout, "Used at:\n");

		struct use_at_list *tmp;
		list_for_each_entry(tmp, &vl_tmp->var.used_at, sibling) {
			struct sinode *fsn;
			fsn = analysis__sinode_search(siid_type(&tmp->func_id),
					SEARCH_BY_ID, &tmp->func_id);
			fprintf(stdout, "\t%s %p %ld\n",
					fsn ? fsn->name : "NULL",
					tmp->gimple_stmt,
					tmp->op_idx);
		}
	}

	if (all || (!strcmp(argv_opt, "offset"))) {
		fprintf(stdout, "Offset:\n");

		analysis__resfile_load(sn->buf);

		unsigned long offset;
		tree field;

		field = (tree)vl_tmp->var.node;
		offset = get_field_offset(field);

		fprintf(stdout, "\t%ld\n", offset);
	}

	if (all || (!strcmp(argv_opt, "size"))) {
		fprintf(stdout, "Size:\n");

		unsigned long sz = 0;
		if (vl_tmp->var.type)
			sz = vl_tmp->var.type->ofsize;

		fprintf(stdout, "\t%ld\n", sz);
	}

	return;
}

static void __cb(struct sinode *sn)
{
	enum sinode_type type;
	type = sinode_idtype(sn);

	if ((type == TYPE_FUNC_GLOBAL) || (type == TYPE_FUNC_STATIC)) {
		output_func(sn);
	} else if ((type == TYPE_VAR_GLOBAL) || (type == TYPE_VAR_STATIC)) {
		output_var(sn);
	} else if (type == TYPE_TYPE) {
		output_type(sn);
	} else {
		;/* TODO */
	}
}

CLIB_MODULE_INIT()
{
	int err;

	err = clib_cmd_ac_add(modname, cb, usage);
	if (err) {
		err_msg("clib_cmd_ac_add err");
		return -1;
	}

	return 0;
}

CLIB_MODULE_EXIT()
{
	clib_cmd_ac_del(modname);
}
