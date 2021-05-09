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

CLIB_MODULE_NAME(list_fn_call_level);
CLIB_MODULE_NEEDED0();
static char modname0[] = "list_fn_call_level";

static void usage0(void)
{
	fprintf(stdout, "\t(call_level) [output_path]\n");
	fprintf(stdout, "\tDump all func_node at specific call_level\n");
	return;
}

static void output_entry_at_level(FILE *s, union siid *id, struct func_node *fn)
{
	clib_pretty_fprint(s, 16, "LEVEL: %02lx", fn->call_depth);
	clib_pretty_fprint(s, 16, "id: %08lx", siid_all(id));
	clib_pretty_fprint(s, 32, "name: %s", fn->name);
	fprintf(s, "\n");
	fflush(s);
	return;
}

static long cb0(int argc, char *argv[])
{
	if ((argc != 2) && (argc != 3)) {
		usage0();
		err_msg("argc invalid");
		return -1;
	}

	int call_level = atoi(argv[1]);
	FILE *s = stderr;
	if (argc == 3) {
		s = fopen(argv[2], "w+");
		if (!s) {
			err_dbg(1, "fopen");
			return -1;
		}
	}

	analysis__mark_entry();
	analysis__list_entry_at_level(call_level, s, output_entry_at_level);

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
