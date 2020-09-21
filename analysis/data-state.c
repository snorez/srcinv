/*
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

int dsv_compute(struct data_state_val *l, struct data_state_val *r,
		int flag, int extra_flag, cur_max_signint *retval)
{
	if (DSV_TYPE(l) != DSV_TYPE(r)) {
		si_log1_todo("type of lhs and rhs are not the same\n");
		return -1;
	}

	if (DSV_TYPE(l) != DSVT_INT_CST) {
		/* TODO: DSVT_REAL_CST */
		si_log1_todo("miss %d\n", DSV_TYPE(l));
		return -1;
	}

	int res;
	cur_max_signint _retval;
	void *lpos = DSV_SEC1_VAL(l);
	void *rpos = DSV_SEC1_VAL(r);
	size_t lbytes = l->info.v1_info.bytes;
	size_t rbytes = r->info.v1_info.bytes;

	res = clib_compute_bits(lpos, lbytes, l->info.v1_info.sign,
				rpos, rbytes, r->info.v1_info.sign,
				flag, &_retval);
	if (res == -1) {
		si_log1_warn("clib_compute_bits err\n");
		return -1;
	}

	switch (flag) {
	case CLIB_COMPUTE_F_COMPARE:
	{
		switch (extra_flag) {
		case DSV_COMP_F_EQ:
		{
			if (!_retval)
				*retval = 1;
			else
				*retval = 0;
			break;
		}
		case DSV_COMP_F_NE:
		{
			if (!_retval)
				*retval = 0;
			else
				*retval = 1;
			break;
		}
		case DSV_COMP_F_GE:
		{
			if (_retval == -1)
				*retval = 0;
			else
				*retval = 1;
			break;
		}
		case DSV_COMP_F_GT:
		{
			if (_retval == 1)
				*retval = 1;
			else
				*retval = 0;
			break;
		}
		case DSV_COMP_F_LE:
		{
			if (_retval == 1)
				*retval = 0;
			else
				*retval = 1;
			break;
		}
		case DSV_COMP_F_LT:
		{
			if (_retval == -1)
				*retval = 1;
			else
				*retval = 0;
			break;
		}
		default:
		{
			BUG();
		}
		}
		break;
	}
	default:
	{
		*retval = _retval;
		break;
	}
	}

	return 0;
}
