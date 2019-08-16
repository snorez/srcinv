/*
 * output all sinodes
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
#include "si_core.h"
#include "si_gcc.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

static FILE *s = NULL;

static const char *tree_code_string(int code)
{
	return tree_code_name[code];
#if 0
	switch (code) {
	case ENUMERAL_TYPE:
		return "ENUMERAL_TYPE";
	case BOOLEAN_TYPE:
		return "BOOLEAN_TYPE";
	case INTEGER_TYPE:
		return "INTEGER_TYPE";
	case REAL_TYPE:
		return "REAL_TYPE";
	case POINTER_TYPE:
		return "POINTER_TYPE";
	case COMPLEX_TYPE:
		return "COMPLEX_TYPE";
	case ARRAY_TYPE:
		return "ARRAY_TYPE";
	case RECORD_TYPE:
		return "RECORD_TYPE";
	case UNION_TYPE:
		return "UNION_TYPE";
	case VOID_TYPE:
		return "VOID_TYPE";
	case FUNCTION_TYPE:
		return "FUNCTION_TYPE";
	case OFFSET_TYPE:
	case REFERENCE_TYPE:
	case NULLPTR_TYPE:
	case FIXED_POINT_TYPE:
	case VECTOR_TYPE:
	case QUAL_UNION_TYPE:
	case POINTER_BOUNDS_TYPE:
	case METHOD_TYPE:
	case LANG_TYPE:
	default:
		return "TYPE_WEIRD";
	}
#endif
}

static void sinode_print_file(struct sinode *sin)
{
	clib_pretty_fprint(s, 16, "id: %08lx", sinode_get_id_whole(sin));
	clib_pretty_fprint(s, 24, "data: %012lx", (long)sin->data);
	clib_pretty_fprint(s, 24, "type: file");

	fprintf(s, "name: %s\n", sin->name);
	fflush(s);
	return;
}

static void sinode_print_type(struct sinode *sin)
{
	struct type_node *t = NULL;
	clib_pretty_fprint(s, 16, "id: %08lx", sinode_get_id_whole(sin));
	clib_pretty_fprint(s, 24, "data: %012lx", (long)sin->data);
	if (sin->data)
		t = (struct type_node *)sin->data;
	clib_pretty_fprint(s, 24, "type: %s",
			t ? tree_code_string(t->type_code) : "TYPE_TYPE");
	clib_pretty_fprint(s, 32, "name: %s", sin->name);
	if (sin->loc_file)
		clib_pretty_fprint(s, 108, "loc: %s %d %d", sin->loc_file->name,
					sin->loc_line, sin->loc_col);

	fprintf(s, "\n");
	fflush(s);
	return;
}

static void sinode_print_var(struct sinode *sin)
{
	clib_pretty_fprint(s, 16, "id: %08lx", sinode_get_id_whole(sin));
	clib_pretty_fprint(s, 24, "data: %012lx", (long)sin->data);
	clib_pretty_fprint(s, 24, "type: var");
	clib_pretty_fprint(s, 32, "name: %s", sin->name);
	if (sin->loc_file)
		clib_pretty_fprint(s, 108, "loc: %s %d %d", sin->loc_file->name,
					sin->loc_line, sin->loc_col);

	fprintf(s, "\n");
	fflush(s);
	return;
}

static void sinode_print_func(struct sinode *sin)
{
	struct func_node *f = (struct func_node *)sin->data;
	clib_pretty_fprint(s, 16, "id: %08lx", sinode_get_id_whole(sin));
	clib_pretty_fprint(s, 24, "data: %012lx", (long)f);
	clib_pretty_fprint(s, 24, "type: func");
	clib_pretty_fprint(s, 32, "name: %s", sin->name);
	if (sin->loc_file)
		clib_pretty_fprint(s, 108, "loc: %s %d %d", sin->loc_file->name,
					sin->loc_line, sin->loc_col);
	if (f && (!list_empty(&f->unreachable_stmts))) {
		struct unr_stmt *tmp;
		list_for_each_entry(tmp, &f->unreachable_stmts,	sibling) {
			int line = tmp->line;
			int col = tmp->col;
			clib_pretty_fprint(s, 32, "unreachable_stmt: %d %d",
						line, col);
		}
	}

	if (f && (!list_empty(&f->callers))) {
		struct call_func_list *tmp;
		list_for_each_entry(tmp, &f->callers, sibling) {
			clib_pretty_fprint(s, 24, "caller: %d %08lx",
						tmp->value_flag,
						tmp->value);
		}
	}

	if (f && (!list_empty(&f->callees))) {
		struct call_func_list *tmp;
		list_for_each_entry(tmp, &f->callees, sibling) {
			clib_pretty_fprint(s, 24, "callee: %d %08lx",
						tmp->value_flag,
						tmp->value);
		}
	}

	if (f && (!list_empty(&f->global_vars))) {
		struct id_list *tmp;
		list_for_each_entry(tmp, &f->global_vars, sibling) {
			clib_pretty_fprint(s, 24, "gvar: %d %08lx",
						tmp->value_flag,
						tmp->value);
		}
	}

	if (f && (!list_empty(&f->local_vars))) {
		struct var_node_list *tmp;
		list_for_each_entry(tmp, &f->local_vars, sibling) {
			clib_pretty_fprint(s, 32, "lvar: %016lx",
							(long)tmp->var.node);
		}
	}

	fprintf(s, "\n");
	fflush(s);
	return;
}

static void sinode_print_codep(struct sinode *sin)
{
	struct code_path *c = (struct code_path *)sin;
	clib_pretty_fprint(s, 16, "id: %08lx", sinode_get_id_whole(sin));
	clib_pretty_fprint(s, 24, "data: %012lx", (long)c);
	clib_pretty_fprint(s, 24, "type: code_path");
	clib_pretty_fprint(s, 32, "func: %s", c->func->name);
	for (unsigned long i = 0; i < c->branches; i++) {
		clib_pretty_fprint(s, 24, "next[%ld]: %012lx", i,
					(long)c->next[i]);
	}

	fprintf(s, "\n");
	fflush(s);
	return;
}
static void sinode_print(struct sinode *sin)
{
	switch (sinode_get_id_type(sin)) {
	case TYPE_FILE:
		sinode_print_file(sin);
		break;
	case TYPE_TYPE:
		sinode_print_type(sin);
		break;
	case TYPE_FUNC_GLOBAL:
	case TYPE_FUNC_STATIC:
		sinode_print_func(sin);
		break;
	case TYPE_VAR_GLOBAL:
	case TYPE_VAR_STATIC:
		sinode_print_var(sin);
		break;
	case TYPE_CODEP:
		sinode_print_codep(sin);
		break;
	default:
		BUG();
	}

	return;
}

static void cb(struct rb_node *node)
{
	struct sinode *sin;
	sin = container_of(node, struct sinode, node_id.node[RB_NODE_BY_ID]);
	sinode_print(sin);
}

static void do_iter(void)
{
	enum sinode_type type_min = TYPE_FILE, type_max = TYPE_MAX;
	enum sinode_type i;
	for (i = type_min; i < type_max; i=(enum sinode_type)(i+1)) {
		struct rb_root *root = &si->sinodes[i][RB_NODE_BY_ID];
		analysis__sinode_iter(root->rb_node, cb);
	}
}

void itersn_usage(void)
{
	fprintf(stdout, "\t\t[output_path]\n");
	fprintf(stdout, "\t\tTraversal all sinodes to stderr/file\n");
}

long itersn_cb(int argc, char *argv[])
{
	if ((argc > 2) || (argc <= 0)) {
		err_dbg(0, "argc invalid");
		return -1;
	} else if (argc == 1) {
		s = stderr;
	} else if (argc == 2) {
		s = fopen(argv[1], "w+");
		if (!s) {
			err_dbg(1, "fopen");
			return -1;
		}
	}

	do_iter();
	if (argc == 2)
		fclose(s);
	s = NULL;
	return 0;
}
