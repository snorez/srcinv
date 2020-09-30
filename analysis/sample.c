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
	"NULL-deref(maybe not init well?)",
	"infinite loop",
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

	/* For now, we assume all sample_state can run */
	return 1;
}

int sample_set_exists(struct sample_set *sset)
{
	si_log1_todo("not implemented yet\n");
	return 0;
}

int sample_set_stuck(struct sample_set *sset)
{
	for (u32 i = 0; i < sset->count; i++) {
		if (sample_can_run(sset, i))
			return 0;
	}
	return 1;
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

/* FIXME: this can not check all kinds of loop */
int sample_state_check_loop(struct sample_set *sset, int idx,
			    struct data_state_val *lhs_dsv,
			    struct data_state_val *rhs_dsv,
			    struct code_path *next_cp)
{
	struct sample_state *sstate = sset->samples[idx];
	struct cp_list *start_cpl, *head_cpl, *tail_cpl;

	size_t count = slist_count(&sstate->cp_list_head);
	void **arr = (void **)xmalloc(count * sizeof(void *));

	int _idx = 0;
	struct cp_list *tmp;
	slist_for_each_entry(tmp, &sstate->cp_list_head, sibling) {
		arr[_idx] = (void *)tmp->cp;
		_idx++;
	}

	int start = 0, head = -1, tail = -1;
	if (sstate->loop_info.start)
		start = slist_ele_idx(&sstate->cp_list_head,
					&sstate->loop_info.start->sibling);
	if (sstate->loop_info.head)
		head = slist_ele_idx(&sstate->cp_list_head,
					&sstate->loop_info.head->sibling);
	if (sstate->loop_info.tail)
		tail = slist_ele_idx(&sstate->cp_list_head,
					&sstate->loop_info.tail->sibling);

	int in_loop;
	in_loop = clib_in_loop(arr, count, sizeof(void *), &start,
				&head, &tail, (void *)&next_cp);
	free(arr);
	start_cpl = slist_entry(slist_idx_ele(&sstate->cp_list_head, start),
				struct cp_list, sibling);
	sstate->loop_info.start = start_cpl;

	if (!in_loop) {
		sample_state_cleanup_loopinfo(sstate);
		return 0;
	}

	head_cpl = slist_entry(slist_idx_ele(&sstate->cp_list_head, head),
				struct cp_list, sibling);
	tail_cpl = slist_entry(slist_idx_ele(&sstate->cp_list_head, tail),
				struct cp_list, sibling);

	if (!sstate->loop_info.head) {
		sstate->loop_info.head = head_cpl;
		sstate->loop_info.tail = tail_cpl;
		dsv_copy_data(&sstate->loop_info.lhs_val, lhs_dsv);
		dsv_copy_data(&sstate->loop_info.rhs_val, rhs_dsv);
		return 0;
	}

	if ((count - head) % (tail - head + 1))
		return 0;

	/* now, we can compare the given lhs/rhs against the saved ones */
	int err;
	cur_max_signint _retval;
	err = analysis__dsv_compute(&sstate->loop_info.lhs_val, lhs_dsv,
					CLIB_COMPUTE_F_COMPARE,
					DSV_COMP_F_EQ, &_retval);
	if (err == -1) {
		si_log1_todo("analysis__dsv_compute err\n");
		return -1;
	} else if (!_retval) {
		goto not_infinite;
	}

	err = analysis__dsv_compute(&sstate->loop_info.rhs_val, rhs_dsv,
					CLIB_COMPUTE_F_COMPARE,
					DSV_COMP_F_EQ, &_retval);
	if (err == -1) {
		si_log1_todo("analysis__dsv_compute err\n");
		return -1;
	} else if (!_retval) {
		goto not_infinite;
	}

	return 1;

not_infinite:
	sstate->loop_info.start =
		slist_entry(slist_idx_ele(&sstate->cp_list_head, count - 1),
				struct cp_list, sibling);
	sample_state_cleanup_loopinfo(sstate);
	return 0;
}
