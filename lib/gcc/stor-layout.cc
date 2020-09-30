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

static tree bits_from_bytes(tree x)
{
	if (POLY_INT_CST_P(x))
		return build_poly_int_cst(si_bitsizetype((void *)x),
				poly_wide_int::from(poly_int_cst_value(x),
				      TYPE_PRECISION(si_bitsizetype((void *)x)),
				      TYPE_SIGN(TREE_TYPE(x))));
	x = fold_convert(si_bitsizetype((void *)x), x);
	return x;
}

tree bit_from_pos(tree offset, tree bitpos)
{
	return size_binop(PLUS_EXPR, bitpos,
			  size_binop(MULT_EXPR, bits_from_bytes(offset),
				     si_bitsize_unit_node((void *)bitpos)));
}

tree byte_from_pos(tree offset, tree bitpos)
{
	tree bytepos;
	if ((TREE_CODE(bitpos) == MULT_EXPR) &&
		tree_int_cst_equal(TREE_OPERAND(bitpos, 1),
				   si_bitsize_unit_node((void *)bitpos)))
		bytepos = TREE_OPERAND(bitpos, 0);
	else
		bytepos = size_binop(TRUNC_DIV_EXPR, bitpos,
					si_bitsize_unit_node((void *)bitpos));
	return size_binop(PLUS_EXPR, offset,
			  fold_convert(si_sizetype((void *)bitpos), bytepos));
}
