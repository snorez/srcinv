/*
 * hacking module, do static checks.
 * It pick up some random function as an entry, and for each call sl_next_insn(),
 * give the static check submodules a chance to check the data states.
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

CLIB_MODULE_NAME(find_extern_call);
CLIB_MODULE_NEEDED0();
static char modname0[] = "find_extern_call";

static void usage0(void)
{
	fprintf(stdout, "\t(extern_funcname) [output_path]\n");
	fprintf(stdout, "\tDump all functions that call extern_funcname\n");
	return;
}

static void output(FILE *s, struct sinode *sn)
{
	clib_pretty_fprint(s, 32, "name: %s", sn->name);
	fprintf(s, "\n");
	fflush(s);
	return;
}

static void match_cb(struct sinode *sn, void *arg)
{
	if (!sn)
		return;
	if (!sn->data)
		return;

	void **args = (void **)arg;

	struct func_node *fn;
	fn = (struct func_node *)sn->data;

	if (slist_empty(&fn->callees))
		return;

	struct callf_list *tmp;
	slist_for_each_entry(tmp, &fn->callees, sibling) {
		if (!tmp->value_flag) {
			union siid *_id;
			_id = (union siid *)&tmp->value;

			if (siid_type(_id) == TYPE_FILE) {
				/* TODO */
				continue;
			}

			struct sinode *tmp_sn;
			tmp_sn = analysis__sinode_search(siid_type(_id),
							 SEARCH_BY_ID, _id);
			if (!tmp_sn) {
				continue;
			}

			if (!strcmp((char *)args[0], tmp_sn->name))
				output((FILE *)args[1], sn);
		} else {
			/* TODO: use module functions instead */
			tree n;
			struct sibuf *b;
			char name[NAME_MAX];

			n = (tree)tmp->value;
			b = find_target_sibuf((void *)n);
			int held = analysis__sibuf_hold(b);
			memset(name, 0, NAME_MAX);

			if (TREE_CODE(n) == FUNCTION_DECL) {
				get_function_name(n, name);
				if (!strcmp((char *)args[0], name))
					output((FILE *)args[1], sn);
			} else {
				fprintf(stderr, "miss %s\n",
					tree_code_name[TREE_CODE(n)]);
			}
			if (!held)
				analysis__sibuf_drop(b);
		}
	}
}

static long cb0(int argc, char *argv[])
{
	if ((argc != 2) && (argc != 3)) {
		usage0();
		err_msg("argc invalid");
		return -1;
	}

	FILE *s = stderr;
	if (argc == 3) {
		s = fopen(argv[2], "w+");
		if (!s) {
			err_dbg(1, "fopen");
			return -1;
		}
	}

	void *args[2] = { argv[1], s };

	analysis__sinode_match("func", match_cb, (void *)args);

	if (argc == 3)
		fclose(s);

	s = NULL;

	return 0;
}

CLIB_MODULE_INIT()
{
	int err = 0;

	err = clib_cmd_ac_add(modname0, cb0, usage0);
	if (err) {
		err_msg("clib_cmd_ac_add err");
		err = -1;
	}

	return err;
}

CLIB_MODULE_EXIT()
{
	clib_cmd_ac_del(modname0);
	return;
}
