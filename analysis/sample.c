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
	[SAMPLE_SF_UAF] = "use-after-free",
	[SAMPLE_SF_NCHKRV] = "ignore return value",
	[SAMPLE_SF_VOIDRV] = "take void return value",
	[SAMPLE_SF_UNINIT] = "uninitialized variable used",
	[SAMPLE_SF_OOBR] = "out-of-bound read",
	[SAMPLE_SF_OOBW] = "out-of-bound write",
	[SAMPLE_SF_INFOLK] = "information leak",
	[SAMPLE_SF_MEMLK] = "memory leak",
	[SAMPLE_SF_DEADLK] = "dead lock",
	[SAMPLE_SF_NULLREF] = "NULL-deref(maybe not init well?)",
	[SAMPLE_SF_INFLOOP] = "infinite loop",

	[SAMPLE_SF_DECERR] = "dec*() mishandled data_states?",
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

static int sample_state_same(struct sample_state *state0,
				struct sample_state *state1)
{
	size_t cnt0 = slist_count(&state0->cp_list_head);
	size_t cnt1 = slist_count(&state1->cp_list_head);
	if (cnt0 != cnt1)
		return 0;

	struct cp_list *tmp0, *tmp1;
	for (int i = 0; i < (int)cnt0; i++) {
		tmp0 = slist_entry(slist_idx_ele(&state0->cp_list_head, i),
					struct cp_list, sibling);
		tmp1 = slist_entry(slist_idx_ele(&state1->cp_list_head, i),
					struct cp_list, sibling);
		if (tmp0->cp != tmp1->cp)
			return 0;
	}

	return 1;
}

static int sample_set_same(struct sample_set *saved, struct sample_set *_new)
{
	if (saved->count != _new->count)
		return 0;

	struct sample_state *tmp0, *tmp1;
	int matched_idx[saved->count];
	int matched_idx_i = 0;
	memset(matched_idx, 0xff, sizeof(matched_idx[0]) * saved->count);

	for (u32 i = 0; i < saved->count; i++) {
		int matched = 0;
		for (u32 j = 0; j < saved->count; j++) {
			int ignore = 0;
			for (u32 k = 0; k < matched_idx_i; k++) {
				if (j == matched_idx[k]) {
					ignore = 1;
					break;
				}
			}
			if (ignore)
				continue;

			tmp0 = saved->samples[i];
			tmp1 = saved->samples[j];
			matched = sample_state_same(tmp0, tmp1);
			if (matched) {
				matched_idx[matched_idx_i] = j;
				matched_idx_i++;
			}
		}
		if (!matched)
			return 0;
	}

	return 1;
}

int sample_set_exists(struct sample_set *sset)
{
	int ret = 0;
	struct sample_set *tmp;

	si_lock_r();
	slist_for_each_entry(tmp, &si->sample_set_head, sibling) {
		if (sample_set_same(tmp, sset)) {
			ret = 1;
			break;
		}
	}
	si_unlock_r();

	return ret;
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
	/* TODO */
	return 0;
}

/* FIXME: this can not check all kinds of loop */
int sample_state_check_loop(struct sample_set *sset, int idx,
			    struct data_state_val *lhs_dsv,
			    struct data_state_val *rhs_dsv,
			    struct code_path *next_cp)
{
	struct sample_state *sstate = sset->samples[idx];

	size_t count = slist_count(&sstate->cp_list_head);
	count += 1;	/* for next_cp */
	void **arr = (void **)xmalloc(count * sizeof(void *));

	int _idx = 0;
	struct cp_list *tmp;
	slist_for_each_entry(tmp, &sstate->cp_list_head, sibling) {
		arr[_idx] = (void *)tmp->cp;
		_idx++;
	}
	arr[_idx] = (void *)next_cp;
	_idx++;

	int start = 0, end = 0, head = -1, tail = -1;
	if (sstate->loop_info.start != -1)
		start = sstate->loop_info.start;
	if (sstate->loop_info.end != -1)
		end = sstate->loop_info.end;
	if (sstate->loop_info.head != -1)
		head = sstate->loop_info.head;
	if (sstate->loop_info.tail != -1)
		tail = sstate->loop_info.tail;

	int in_loop;
	in_loop = clib_in_loop(arr, count, sizeof(void *),
				&start, &end, &head, &tail);
	free(arr);
	sstate->loop_info.start = start;
	sstate->loop_info.end = end;

	if (in_loop == -1) {
		return 0;
	} else if (!in_loop) {
		sample_state_cleanup_loopinfo(sstate);
		return 0;
	}

	int err;
	if (sstate->loop_info.head == -1) {
		err = dsv_copy_data(&sstate->loop_info.lhs_val, lhs_dsv);
		if (err == -1) {
			si_log1_todo("dsv_copy_data err\n");
			return -1;
		}
		err = dsv_copy_data(&sstate->loop_info.rhs_val, rhs_dsv);
		if (err == -1) {
			si_log1_todo("dsv_copy_data err\n");
			dsv_free_data(&sstate->loop_info.lhs_val);
			return -1;
		}
		sstate->loop_info.head = head;
		sstate->loop_info.tail = tail;
		return 0;
	}

	/* now, we can compare the given lhs/rhs against the saved ones */
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
	sample_state_cleanup_loopinfo(sstate);
	return 0;
}

int sample_set_select_entries(struct sample_set *sset)
{
	si_log1_todo("not implemented yet\n");
	return 0;
}
