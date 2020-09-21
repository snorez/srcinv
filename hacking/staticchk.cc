/*
 * hacking module, do static checks.
 * It pick up some random function as an entry, and for each call dec_next(),
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

CLIB_MODULE_NAME(staticchk);
CLIB_MODULE_NEEDED0();
static char modname[] = "staticchk";
static int cur_thread_cnt = 1;
static char *request_submodname = NULL;

#define	SUB_MOD_MAX	64
static struct hacking_module *submods[SUB_MOD_MAX];

static void usage(void)
{
	fprintf(stdout, "\t(thread cnt) (submodule) [funcid]\n"
			"\tRun submodule for static checks.\n"
			"\tsubmodule could be 'all' or any specific module.\n");
}

static int chk_func(struct sinode *fsn)
{
	struct func_node *fn;
	fn = (struct func_node *)fsn->data;
	if (!fn)
		return 0;

	int err = 0;
	struct sample_set *sset;
	struct hm_arg arg;
	struct func_node *fn_array[cur_thread_cnt];

	sset = sample_set_alloc(1, cur_thread_cnt);
	sset->id = src_get_sset_curid();
	analysis__pick_related_func(fsn, fn_array, cur_thread_cnt);

	/* XXX: init sample states */
	for (int i = 0; i < cur_thread_cnt; i++) {
		err = analysis__dec_next(sset, i, fn_array[i]);
		if (err == -1) {
			si_log2_todo("dec_next(%d) err\n", i);
			goto out;
		}
	}

	while (1) {
		/* choose a sample to run */
		int i = s_random() % cur_thread_cnt;

		if (analysis__sample_set_stuck(sset)) {
			/* deadlock happens */
			sample_set_set_flag(sset, SAMPLE_SF_DEADLK);
			break;
		}

		if (!analysis__sample_can_run(sset, i))
			continue;

		err = analysis__dec_next(sset, i, fn_array[i]);
		if (err == -1) {
			si_log2_todo("dec_next(%d) err\n", i);
			goto out;
		}

		/* Time to do the check */
		arg.sid = sset->id;
		arg.sset = sset;
		arg.sstate = sset->samples[i];

		for (int mod_i = 0; mod_i < SUB_MOD_MAX; mod_i++) {
			if (!submods[mod_i])
				break;
			submods[mod_i]->callback(&arg);
		}

		if ((!sample_set_zeroflag(sset)) || sample_set_done(sset))
			break;
	}

	if ((!analysis__sample_set_exists(sset)) &&
		(!analysis__sample_set_validate(sset))) {
		if (!sample_set_zeroflag(sset))
			analysis__sample_set_dump(sset);
		save_sample_set(sset);
		src_set_sset_curid(sset->id+1);
	}

out:
	sample_set_free(sset);
	return err;
}

static void check_funcid_for_submods(unsigned long func_id)
{
	int i = 0;
	for (i = 0; i < SUB_MOD_MAX; i++) {
		if (!submods[i])
			break;
		submods[i] = NULL;
	}

	i = 0;
	struct hacking_module *tmp;
	slist_for_each_entry(tmp, &hacking_module_head, sibling) {
		if (tmp->flag != HACKING_FLAG_STATIC)
			continue;
		if (request_submodname && strcmp(request_submodname, tmp->name))
			continue;
		if (tmp->check_id && (!tmp->check_id(func_id)))
			continue;
		if (!tmp->callback)
			continue;
		if (i >= SUB_MOD_MAX) {
			fprintf(stderr, "submods exceeds SUB_MOD_MAX\n");
			return;
		}
		submods[i] = tmp;
		i++;
	}
}

static void chk_funcid(unsigned long func_id)
{
	union siid *tid = (union siid *)&func_id;
	struct sinode *fsn;

	check_funcid_for_submods(func_id);
	if (!submods[0])
		return;

	fsn = analysis__sinode_search(siid_type(tid), SEARCH_BY_ID, tid);
	if (!fsn)
		return;
	fprintf(stdout, "checking %s ", fsn->name);
	fflush(stdout);
	si_log2("checking %s\n", fsn->name);
	if (!chk_func(fsn)) {
		fprintf(stdout, "done\n");
		si_log2("checking %s done\n", fsn->name);
	} else {
		fprintf(stdout, "err\n");
		si_log2("checking %s err\n", fsn->name);
	}
	fflush(stdout);
}

static long cb(int argc, char *argv[])
{
	if ((argc != 3) && (argc != 4)) {
		usage();
		err_msg("argc invalid");
		return -1;
	}

	cur_thread_cnt = atoi(argv[1]);
	request_submodname = argv[2];

	if (!cur_thread_cnt)
		cur_thread_cnt = 1;

	if (!strcmp(request_submodname, "all"))
		request_submodname = NULL;

	si_log2("run staticchk\n");

	unsigned long func_id = 0;
	if (argc == 4) {
		func_id = atol(argv[3]);
		chk_funcid(func_id);
	} else {
		union siid *tid = (union siid *)&func_id;

		tid->id0.id_type = TYPE_FUNC_GLOBAL;
		for (; func_id < si->id_idx[TYPE_FUNC_GLOBAL].id1; func_id++)
			chk_funcid(func_id);

		func_id = 0;
		tid->id0.id_type = TYPE_FUNC_STATIC;
		for (; func_id < si->id_idx[TYPE_FUNC_STATIC].id1; func_id++)
			chk_funcid(func_id);
	}

	si_log2("run staticchk done\n");

	return 0;
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
	return;
}
