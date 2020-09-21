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

int call_expr_flags(const_tree t)
{
	int flags;
	tree decl = get_callee_fndecl(t);

	if (decl)
		flags = flags_from_decl_or_type(decl);
	else if (CALL_EXPR_FN (t) == NULL_TREE)
		flags = internal_fn_flags(CALL_EXPR_IFN(t));
	else {
		tree type = TREE_TYPE(CALL_EXPR_FN(t));
		if (type && TREE_CODE(type) == POINTER_TYPE)
			flags = flags_from_decl_or_type(TREE_TYPE(type));
		else
			flags = 0;
		if (CALL_EXPR_BY_DESCRIPTOR(t))
			flags |= ECF_BY_DESCRIPTOR;
	}

	return flags;
}
