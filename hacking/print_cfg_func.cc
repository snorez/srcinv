/*
 * output call graph.
 *
 * Copyright (C) 2020 zerons
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
#include "si_hacking.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <getopt.h>

CLIB_MODULE_NAME(print_cfg_func);
CLIB_MODULE_NEEDED0();
static char cmdname[] = "print_cfg_func";
static unsigned long func_id[CALL_DEPTH_MAX];
static const char *suffix = "nested";
static const char *tab_ch = "    ";
static int max_tabs = 0;

static int __match(struct sinode *sn)
{
	struct func_node *fn = (struct func_node *)sn->data;
	if (!fn)
		return 0;

	if (fn->call_depth == 1) {
		return 1;
	} else {
		return 0;
	}
}

static void __print(FILE *s, char *name, int level, const char *suffix, int tabs)
{
	char _ident[(tabs+1) * strlen(tab_ch)];
	_ident[tabs * strlen(tab_ch)] = 0;
	for (int i = 0; i < tabs; i++) {
		memcpy(&_ident[i * strlen(tab_ch)], tab_ch, strlen(tab_ch));
	}

	if (suffix)
		fprintf(s, "%s%s#%s(%d)\n", _ident, name, suffix, level);
	else
		fprintf(s, "%s%s(%d)\n", _ident, name, level);
}

static inline void reset_func_id_arr(void)
{
	memset(func_id, 0, sizeof(func_id));
}

static inline int func_id_arr_find(unsigned long _func_id, int depth)
{
	for (int i = 0; i < depth; i++) {
		if (func_id[i] == _func_id)
			return 1;
	}

	return 0;
}

static void __init(void)
{
	reset_func_id_arr();
	max_tabs = 0;

	opterr = 0;
	optind = 0;
}

static long __do_single_func(FILE *s, struct sinode *fsn, int tabs)
{
	struct func_node *fn;
	fn = (struct func_node *)fsn->data;
	int call_depth = 0;
	if (fn)
		call_depth = fn->call_depth;

	if (!tabs)
		reset_func_id_arr();
	else if (tabs > CALL_DEPTH_MAX) {
		err_dbg(0, "exceed CALL_DEPTH_MAX");
		return -1;
	}

	if (func_id_arr_find(fsn->node_id.id.id1, tabs)) {
		/* just output the func name and the suffix */
		__print(s, fsn->name, call_depth, suffix, tabs);
		return 0;
	} else if (!fn) {
		__print(s, fsn->name, call_depth, NULL, tabs);
		return 0;
	}

	__print(s, fsn->name, call_depth, NULL, tabs);
	func_id[tabs] = fsn->node_id.id.id1;

	struct callf_list *tmp;
	slist_for_each_entry(tmp, &fn->callees, sibling) {
		union siid *_id;
		struct sinode *callee_fsn;

		if (tmp->value_flag == 1) {
			char name[32];
			snprintf(name, 32, "0x%lx", tmp->value);
			__print(s, name, call_depth+1, NULL, tabs+1);
			continue;
		}

		_id = (union siid *)&tmp->value;
		if (!siid_type(_id)) {
			/* internal_fn */
			char name[32];
			snprintf(name, 32, "internal_fn(%ld)", tmp->value);
			__print(s, name, call_depth+1, NULL, tabs+1);
			continue;
		}

		callee_fsn = analysis__sinode_search(siid_type(_id),
							SEARCH_BY_ID, _id);
		if (!callee_fsn) {
			err_dbg(0, "sinode_search() not found. "
					"Should not happen\n");
			continue;
		}
		if ((!max_tabs) || (tabs < max_tabs))
			(void)__do_single_func(s, callee_fsn, tabs+1);
	}

	return 0;
}

static long do_single_func(FILE *s, unsigned long funcid)
{
	union siid *_id;
	struct sinode *fsn;

	_id = (union siid *)(&funcid);
	fsn = analysis__sinode_search(siid_type(_id), SEARCH_BY_ID, _id);
	if (!fsn) {
		err_dbg(0, "target funcid(%ld) not found", funcid);
		return -1;
	} else if (!fsn->data){
		err_dbg(0, "target funcid(%ld) has no body", funcid);
		return 0;
	}

	return __do_single_func(s, fsn, 0);
}

static void __do_all_func(struct sinode *fsn, void *arg)
{
	if (!__match(fsn))
		return;

	FILE *s = (FILE *)arg;
	(void)__do_single_func(s, fsn, 0);
}

static long do_all_func(FILE *s)
{
	analysis__sinode_match("func_global", __do_all_func, (void *)s);
	return 0;
}

static long do_cb(FILE *s, unsigned long funcid)
{
	if (funcid) {
		return do_single_func(s, funcid);
	} else {
		return do_all_func(s);
	}
}

static void usage(void)
{
	fprintf(stdout, "\tOutput @FUNC_ID/all(level1) call graph to "
			"stderr/@OUTFILE\n"
			"\t-o (outfile)\n"
			"\t-f (funcid)\n"
			"\t-r: Run mark_entry before print\n"
			"\t-l (level): check callees less than level\n");
}

static long cb(int argc, char *argv[])
{
	__init();

	unsigned long tfun = 0;
	FILE *s = stderr;
	long ret = 0;

	int ch;
	while ((ch = getopt(argc, argv, "o:f:rl:"))) {
		if (ch == -1)
			break;

		switch (ch) {
		case 'o':
		{
			s = fopen(optarg, "w+");
			if (!s) {
				err_dbg(1, "fopen %s", optarg);
				return -1;
			}
			break;
		}
		case 'f':
		{
			tfun = (unsigned long)atol(optarg);
			break;
		}
		case 'r':
		{
			if (analysis__mark_entry() != 0) {
				ret = -1;
				err_dbg(0, "analysis__mark_entry err");
				goto out;
			}
			break;
		}
		case 'l':
		{
			max_tabs = (unsigned long)atoi(optarg);
			break;
		}
		default:
		{
			ret = -1;
			err_dbg(0, "argc invalid(%c)", ch);
			goto out;
		}
		}
	}

	ret = do_cb(s, tfun);

out:
	if (s != stderr)
		fclose(s);

	return ret;
}

CLIB_MODULE_INIT()
{
	int err;

	err = clib_cmd_ac_add(cmdname, cb, usage);
	if (err) {
		err_msg("clib_cmd_ac_add err");
		return -1;
	}

	return 0;
}

CLIB_MODULE_EXIT()
{
	clib_cmd_ac_del(cmdname);
}
