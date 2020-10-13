/*
 * TODO
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

#include "si_gcc.h"
#include "si_hacking.h"

CLIB_MODULE_NAME(test);
CLIB_MODULE_NEEDED0();

static char modname[] = "test";

static void usage(void)
{
	fprintf(stdout, "\t(subcmd)\n"
			"\tFor test case\n");
}

static void __test_cond_or_goto_is_tail(struct sinode *sn, void *arg)
{
	if (!sn->data)
		return;

	analysis__sibuf_hold(sn->buf);

	struct func_node *fn;
	fn = (struct func_node *)sn->data;
	fprintf(stdout, "__test_cond_or_goto_is_tail %s\n", fn->name);
	for (int i = 0; i < fn->cp_cnt; i++) {
		basic_block bb;
		gimple_seq gs;

		bb = (basic_block)fn->cps[i]->cp;
		gs = bb->il.gimple.seq;
		while (gs) {
			enum gimple_code gc = gimple_code(gs);
			if ((gc == GIMPLE_COND) && gs->next) {
				fprintf(stdout, "COND is not tail\n");
				goto out;
			}
			if ((gc == GIMPLE_GOTO) && gs->next) {
				fprintf(stdout, "GOTO is not tail\n");
				goto out;
			}
			gs = gs->next;
		}
	}

out:
	analysis__sibuf_drop(sn->buf);
	fprintf(stdout, "__test_cond_or_goto_is_tail %s done\n", fn->name);
	fprintf(stdout, "\n");
}

static void test_cond_or_goto_is_tail(void)
{
	analysis__sinode_match("func", __test_cond_or_goto_is_tail, NULL);
}

struct test_subcmd {
	const char	*cmd;
	void		(*cb)(void);
} subcmds[] = {
	{"cond_or_goto_is_tail", test_cond_or_goto_is_tail},
};

static long cb(int argc, char *argv[])
{
	if (argc != 2) {
		usage();
		err_msg("argc invalid");
		return -1;
	}

	char *subcmd = argv[1];
	for (size_t i = 0; i < sizeof(subcmds) / sizeof(subcmds[0]); i++) {
		if (strcmp(subcmd, subcmds[i].cmd))
			continue;
		if (subcmds[i].cb)
			subcmds[i].cb();
		break;
	}

	return 0;
}

CLIB_MODULE_INIT()
{
	int err;

	err = clib_cmd_ac_add(modname, cb, usage);
	if (err == -1) {
		err_msg("clib_cmd_ac_add err");
		return -1;
	}

	return 0;
}

CLIB_MODULE_EXIT()
{
	clib_cmd_ac_del(modname);
	return;
}
