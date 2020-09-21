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
#include "si_core.h"

static const char *sample_set_flag_string[SAMPLE_SF_MAX] = {
	"use-after-free",
	"ignore return value",
	"take void return value",
	"uninitialized variable used",
	"out-of-bound read",
	"out-of-bound write",
	"information leak",
	"memory leak",
	"dead lock",
};

const char *sample_set_flag_str(int nr)
{
	if (nr >= SAMPLE_SF_MAX)
		return NULL;

	return sample_set_flag_string[nr];
}

void sample_state_dump_cp(struct sample_state *sstate, int ident)
{
	struct cp_list *tmp;
	slist_for_each_entry(tmp, &sstate->cp_list_head, sibling) {
		char _ident[ident+1];
		for (int i = 0; i < ident; i++)
			_ident[i] = '\t';
		_ident[ident] = 0;
		si_log("%s%s %p\n", _ident, tmp->cp->func->name, tmp->cp);
	}
}

void sample_set_dump(struct sample_set *sset)
{
	si_log("Dump sample set 0x%lx\n", sset->id);
	si_log("Issues:\n");
	for (int i = 0; i < SAMPLE_SF_MAX; i++) {
		if (!sample_set_chk_flag(sset, i))
			continue;
		si_log("\t%s\n", sample_set_flag_str(i));
	}
	for (u64 i = 0; i < sset->count; i++) {
		si_log("State[%d]:\n", i);
		sample_state_dump_cp(sset->samples[i], 1);
	}
}

/*
 * @sample: sample_state for current sample
 * @cp_entry: the entry of the current sample
 * @state_entry: cp_state for the entry code_path
 * @till: stop building till which code_path
 *
 * If the sample is empty, then the @cp_entry MUST NOT be NULL.
 * Otherwise, take the last code_path in sample, and ignore @cp_entry and
 * @state_entry.
 */
int build_sample_state_till(struct sample_set *sset, int idx,
			  struct code_path *cp_entry,
			  struct code_path *till)
{
	si_log1_todo("not implemented yet\n");
	return 0;
}

int sample_can_run(struct sample_set *sset, int idx)
{
	if (sset->count == 1)
		return 1;

	si_log1_todo("not implemented yet\n");
	return 0;
}

int sample_set_exists(struct sample_set *sset)
{
	si_log1_todo("not implemented yet\n");
	return 0;
}

int sample_set_stuck(struct sample_set *sset)
{
	si_log1_todo("not implemented yet\n");
	return 0;
}

int sample_set_replay(struct sample_set *sset)
{
	si_log1_todo("not implemented yet\n");
	return 0;
}

int sample_set_validate(struct sample_set *sset)
{
	/* In case we guess some wrong value, take to the wrong code path */
	si_log1_todo("not implemented yet\n");
	return 0;
}
