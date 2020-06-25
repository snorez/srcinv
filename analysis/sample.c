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
#include "./analysis.h"

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
int build_sample_state_till(struct sample_state *sample,
			  struct code_path *cp_entry,
			  struct cp_state *state_entry,
			  struct code_path *till)
{
	int ret = 0;
	struct code_path *cur_cp;

	cur_cp = sample_last_cp(sample);
	if (!cur_cp)
		cur_cp = cp_entry;

	if (cur_cp->state->status == CSS_EMPTY) {
		si_log1_todo("CSS_EMPTY in %s\n", cur_cp->func->name);
		return 0;
	} else if (cur_cp->state->status == CSS_INITIALIZED) {
		cp_state_copy(cur_cp->state, state_entry);
	}

	ret = analysis__dec_next(sample, cur_cp);
	return 0;
}
