/*
 * list functions in target directories/files
 *	list_function [directory/file]
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

CLIB_MODULE_NAME(list_function);
CLIB_MODULE_NEEDED0();
static char modname[] = "list_function";

static void usage(void)
{
	fprintf(stdout, "\t[dir/file] (format)\n"
			"\tList functions in dir or file\n"
			"\tformat:\n"
			"\t\t0(normal output)\n"
			"\t\t1(markdown output)\n");
}

static void find_match(char *target, int format);
static long cb(int argc, char *argv[])
{
	if ((argc != 2) && (argc != 3)) {
		err_msg("argc invalid");
		return -1;
	}

	char *target = argv[1];
	int format = 0;
	if (argc == 3)
		format = atoi(argv[2]);

	find_match(target, format);
	return 0;
}

static void __match(union siid *tid, char *target, int format)
{
	struct sinode *fsn;
	fsn = analysis__sinode_search(siid_type(tid), SEARCH_BY_ID, tid);
	if ((!fsn) || (!fsn->loc_file))
		return;

	/* XXX: no need to load resfile */
	char *filename = fsn->loc_file->name;
	if (strncmp(target, filename, strlen(target)))
		return;

	switch (format) {
	case 1:
	{
		/* for markdown */
		fprintf(stdout, "- `%s`\n", fsn->name);
		break;
	}
	case 0:
	default:
	{
		fprintf(stdout, "%s\n", fsn->name);
		break;
	}
	}

	return;
}

static void find_match(char *target, int format)
{
	unsigned long func_id = 0;
	union siid *tid = (union siid *)&func_id;

	tid->id0.id_type = TYPE_FUNC_GLOBAL;
	for (; func_id < si->id_idx[TYPE_FUNC_GLOBAL].id1; func_id++) {
		__match(tid, target, format);
	}

	func_id = 0;
	tid->id0.id_type = TYPE_FUNC_STATIC;
	for (; func_id < si->id_idx[TYPE_FUNC_STATIC].id1; func_id++) {
		__match(tid, target, format);
	}

	return;
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
