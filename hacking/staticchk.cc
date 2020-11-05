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
static char modname0[] = "staticchk";
static char modname1[] = "staticchk_quick";

static int staticchk_sample_set(struct sample_set *sset, char *modname)
{
	int threads = (int)sset->count;
	int err = 0;

	while (1) {
		int i = s_random() % threads;
		int done = 0;
		struct hacking_module *tmp_mod;

		if (analysis__sample_set_stuck(sset)) {
			/* deadlock happens */
			sample_set_set_flag(sset, SAMPLE_SF_DEADLK);
			goto check_saved;
		}

		if (!analysis__sample_can_run(sset, i))
			continue;

		err = analysis__dec_next(sset, i);
		if (err == -1) {
			si_log2_todo("analysis__dec_next(%d) err\n", i);
			goto out;
		}

		done = sample_set_done(sset);
		if (modname) {
			slist_for_each_entry(tmp_mod, &hacking_module_head,
						sibling) {
				if (tmp_mod->flag != HACKING_FLAG_STATIC)
					continue;
				if ((strcmp(modname, "all")) &&
					(strcmp(modname, tmp_mod->name)))
					continue;
				if (done && tmp_mod->done)
					tmp_mod->done(sset, i);
				else if ((!done) && tmp_mod->doit)
					tmp_mod->doit(sset, i);
			}
		}

		if (done)
			break;

		if (!sample_set_zeroflag(sset))
			break;
	}

check_saved:
	if ((!analysis__sample_set_exists(sset)) &&
		(!analysis__sample_set_validate(sset))) {
		if (!sample_set_zeroflag(sset))
			analysis__sample_set_dump(sset);
		save_sample_set(sset);
		src_set_sset_curid(sset->id+1);
	}

out:
	return err;
}

/*
 * return value:
 * 0: sample_set not saved
 * 1: sample_set saved
 */
static int _do_staticchk(int threads, char *modname)
{
	struct sample_set *sset;

	int err = 0;
	sset = sample_set_alloc(1, threads);
	sset->id = src_get_sset_curid();
	sset->staticchk_mode = SAMPLE_SET_STATICCHK_MODE_FULL;

	err = analysis__sample_set_select_entries(sset);
	if (err == -1)
		goto out;

	err = staticchk_sample_set(sset, modname);

out:
	sample_set_free(sset);
	return (err == 1) ? 1 : 0;
}

static void staticchk_usage(void)
{
	fprintf(stdout, "\t(thread cnt) (submodule) [how_many_sset]\n"
			"\tRun submodule for static checks.\n"
			"\tSubmodule could be 'all' or any specific module.\n"
			"\tPlease run this using (thread cnt)=1 first.\n");
}

/*
 * @threads: how many threads(sample_states) for this sample_set.
 * @modname: the target sub static module to run. NULL for all.
 * @how_many_sset: count of sample_set to save. If 0, run forever.
 *
 * TODO:
 * 	add a signal to stop the loop, till the next sample_set saved.
 *
 * ) generate a new sample_set, and sample_states.
 * ) check saved sample_sets, select entries.
 * 	) if all saved sample_sets are full-coverage tested, choose a new entry,
 * 	and related entries.
 * 	) if not, trigger the new branch for this sample_set.
 * ) Once entries are selected, we are ready to go.
 */
static long staticchk_cb(int argc, char *argv[])
{
	if ((argc != 3) && (argc != 4)) {
		staticchk_usage();
		err_msg("argc invalid");
		return -1;
	}

	analysis__mark_entry();

	si_log2("run staticchk\n");

	int threads = atoi(argv[1]);
	char *modname = argv[2];
	size_t how_many_sset = 0;
	if (argc == 4)
		how_many_sset = atol(argv[3]);

	int saved = 0;
	size_t num_saved = 0;
	while (1) {
		saved = _do_staticchk(threads, modname);
		if (saved)
			num_saved++;

		if (!how_many_sset)
			continue;
		else if (how_many_sset == num_saved)
			break;
	}

	fprintf(stdout, "%ld sample_set(s) saved\n", num_saved);

	si_log2("run staticchk done\n");

	return 0;
}

static void staticchk_quick_single_func(unsigned long funcid)
{
	int err = 0;
	int threads = 1;
	int entry_count = 1;
	struct sample_set *sset;
	struct sinode **entries;
	struct sinode *target_fsn;
	union siid *id = (union siid *)&funcid;

	target_fsn = analysis__sinode_search(siid_type(id), SEARCH_BY_ID, id);
	if (!target_fsn)
		return;

	sset = sample_set_alloc(1, threads);
	entries = sample_state_entry_alloc(entry_count, 1);
	sset->staticchk_mode = SAMPLE_SET_STATICCHK_MODE_QUICK;

	entries[0] = target_fsn;
	sset->samples[0]->entries = entries;
	sset->samples[0]->entry_count = entry_count;

	fprintf(stdout, "checking %s ", target_fsn->name);
	fflush(stdout);
	si_log2("checking %s\n", target_fsn->name);

	err = staticchk_sample_set(sset, NULL);
	(void)err;
	sample_set_free(sset);

	if (!err) {
		fprintf(stdout, "done\n");
		si_log2("checking %s done\n", target_fsn->name);
	} else {
		fprintf(stdout, "err\n");
		si_log2("checking %s err\n", target_fsn->name);
	}
	fflush(stdout);

	return;
}

static void staticchk_quick_usage(void)
{
	fprintf(stdout, "\t[funcid]\n"
			"\trun staticchk(quick) for each/specific function\n");
}

static long staticchk_quick_cb(int argc, char *argv[])
{
	if ((argc != 1) && (argc != 2)) {
		staticchk_quick_usage();
		err_msg("argc invalid");
		return -1;
	}

	analysis__mark_entry();

	si_log2("run staticchk(quick mode)\n");

	unsigned long funcid = 0;
	if (argc == 2) {
		funcid = atol(argv[1]);
		staticchk_quick_single_func(funcid);
	} else {
		union siid *id = (union siid *)&funcid;

		id->id0.id_type = TYPE_FUNC_GLOBAL;
		for (; funcid < si->id_idx[TYPE_FUNC_GLOBAL].id1; funcid++)
			staticchk_quick_single_func(funcid);

		funcid = 0;
		id->id0.id_type = TYPE_FUNC_STATIC;
		for (; funcid < si->id_idx[TYPE_FUNC_STATIC].id1; funcid++)
			staticchk_quick_single_func(funcid);
	}

	si_log2("run staticchk(quick mode) done\n");

	return 0;
}

CLIB_MODULE_INIT()
{
	int err = 0;

	err = clib_cmd_ac_add(modname0, staticchk_cb, staticchk_usage);
	if (err) {
		err_msg("clib_cmd_ac_add err");
		err = -1;
		goto out;
	}

	err = clib_cmd_ac_add(modname1, staticchk_quick_cb,
				staticchk_quick_usage);
	if (err) {
		err_msg("clib_cmd_ac_add err");
		err = -1;
		goto del_0;
	}

	return err;

del_0:
	clib_cmd_ac_del(modname0);
out:
	return err;
}

CLIB_MODULE_EXIT()
{
	clib_cmd_ac_del(modname0);
	clib_cmd_ac_del(modname1);
	return;
}
