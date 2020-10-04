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
static void __cb(struct sinode *sn, void *arg);
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

	analysis__sinode_match(type, __cb, NULL);

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
		slist_for_each_entry(tmp, &fn->callers, sibling) {
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
		slist_for_each_entry(tmp, &fn->callees, sibling) {
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

	fprintf(stdout, "Call depth: %d\n", fn->call_depth);
}

static void output_used_at(struct slist_head *head, char *name)
{
	fprintf(stdout, "Used at(%s):\n", name);

	struct use_at_list *tmp;
	slist_for_each_entry(tmp, head, sibling) {
		struct sinode *fsn;
		expanded_location *xloc;

		fsn = analysis__sinode_search(siid_type(&tmp->func_id),
				SEARCH_BY_ID, &tmp->func_id);

		if (tmp->type == SI_TYPE_DF_GIMPLE) {
			gimple_seq gs;
			gs = (gimple_seq)tmp->where;
			analysis__resfile_load(fsn->buf);
			xloc = get_gimple_loc(fsn->buf->payload, &gs->location);
			fprintf(stdout, "\tFunction: %s(), gimple: %p %ld\n"
					"\t________: %s %d %d\n",
					fsn ? fsn->name : "NULL",
					tmp->where,
					tmp->extra_info,
					xloc ? xloc->file : NULL,
					xloc ? xloc->line : 0,
					xloc ? xloc->column : 0);
		}
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
		output_used_at(&vn->used_at, vn->name);
	}
}

static void vl_add(struct var_list *vl, struct var_list **res, int entries,
			int *idx)
{
	int cur_idx = *idx;
	if (cur_idx < entries) {
		res[cur_idx] = vl;
		*idx = cur_idx + 1;
	} else {
		fprintf(stdout, "var_list too many\n");
	}
}

static int find_target(struct type_node *tn, int level,
			struct var_list **res, int entries, int *idx)
{
	if (!tn)
		return 0;

	char *name = split_names[level];
	if ((!name) || (!name[0]))
		return 1;

	struct var_list *vl_tmp = NULL;

	if (*name == '*') {
		slist_for_each_entry(vl_tmp, &tn->children, sibling) {
			if (find_target(vl_tmp->var.type, level+1,
					res, entries, idx)) {
				vl_add(vl_tmp, res, entries, idx);
			}
		}
	} else {
		vl_tmp = get_tn_field(tn, name);
		if (vl_tmp &&
			find_target(vl_tmp->var.type, level+1,
				    res, entries, idx)) {
			vl_add(vl_tmp, res, entries, idx);
		}
	}

	return 0;
}

static void output_type(struct sinode *sn)
{
	int all;
	struct type_node *tn;

	tn = (struct type_node *)sn->data;
	all = !strcmp(argv_opt, "all");

	if (!tn)
		return;

	int vls_max = 0x100;
	int idx = 0;
	struct var_list *vls[vls_max] = {0};

	int ret = find_target(tn, 1, vls, vls_max, &idx);
	if ((!idx) && (!ret))
		return;

	if (all || (!strcmp(argv_opt, "src"))) {
		output_src(sn);
	}

	if (!idx) {
		/* ignore argv_opt, handle the used_at */
		output_used_at(&tn->used_at, tn->type_name);
		return;
	}

	for (int i = 0; i < idx; i++) {
		struct var_list *vl = vls[i];

		if (all || (!strcmp(argv_opt, "used"))) {
			output_used_at(&vl->var.used_at, vl->var.name);
		}

		if (all || (!strcmp(argv_opt, "offset"))) {
			fprintf(stdout, "Offset(in bits):\n");

			unsigned long offset;
			tree field;

			field = (tree)vl->var.node;
			analysis__resfile_load(find_target_sibuf(field));

			offset = get_field_offset(field);

			fprintf(stdout, "\t%ld\n", offset);
		}

		if (all || (!strcmp(argv_opt, "size"))) {
			fprintf(stdout, "Size(in bits):\n");

			unsigned long sz = 0;
			if (vl->var.type)
				sz = vl->var.type->ofsize * BITS_PER_UNIT;

			fprintf(stdout, "\t%ld\n", sz);
		}
	}

	return;
}

static void __cb(struct sinode *sn, void *arg)
{
	if (!__match(sn))
		return;

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
