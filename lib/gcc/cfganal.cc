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

edge find_edge(basic_block pred, basic_block succ)
{
	edge e;
	edge_iterator ei;

	if (EDGE_COUNT(pred->succs) <= EDGE_COUNT(succ->preds)) {
		FOR_EACH_EDGE(e, ei, pred->succs)
			if (e->dest == succ)
				return e;
	} else {
		FOR_EACH_EDGE(e, ei, succ->preds)
			if (e->src == pred)
				return e;
	}

	return NULL;
}
