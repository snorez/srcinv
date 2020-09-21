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

tree get_addr_base_and_unit_offset_1(tree exp, poly_int64_pod *poffset,
					tree (*valueize)(tree))
{
	poly_int64 byte_offset = 0;

	while (1) {
		switch (TREE_CODE(exp)) {
		case BIT_FIELD_REF:
		{
			poly_int64 this_byte_offset;
			poly_uint64 this_bit_offset;
			if ((!poly_int_tree_p(TREE_OPERAND(exp, 2),
						&this_bit_offset)) ||
				(!multiple_p(this_bit_offset, BITS_PER_UNIT,
						&this_byte_offset)))
				return NULL_TREE;
			byte_offset += this_byte_offset;
		}
		break;
		case COMPONENT_REF:
		{
			tree field = TREE_OPERAND(exp, 1);
			tree this_offset = component_ref_field_offset(exp);
			poly_int64 hthis_offset;

			if ((!this_offset) ||
			    (!poly_int_tree_p(this_offset, &hthis_offset)) ||
			    (TREE_INT_CST_LOW(DECL_FIELD_BIT_OFFSET(field)) %
			     BITS_PER_UNIT))
				return NULL_TREE;

			hthis_offset += (TREE_INT_CST_LOW(
						DECL_FIELD_BIT_OFFSET(field)) /
					BITS_PER_UNIT);
			byte_offset += hthis_offset;
		}
		break;
		case ARRAY_REF:
		case ARRAY_RANGE_REF:
		{
			tree index = TREE_OPERAND(exp, 1);
			tree low_bound, unit_size;

			if (valueize && (TREE_CODE(index) == SSA_NAME))
				index = (*valueize)(index);

			if (poly_int_tree_p(index) &&
				(low_bound = array_ref_low_bound(exp),
				 poly_int_tree_p(low_bound)) &&
				(unit_size = array_ref_element_size(exp),
				 TREE_CODE(unit_size) == INTEGER_CST)) {
				poly_offset_int woffset;
				woffset = wi::sext(wi::to_poly_offset(index)
					  - wi::to_poly_offset(low_bound),
					    TYPE_PRECISION(TREE_TYPE(index)));
				woffset *= wi::to_offset(unit_size);
				byte_offset += woffset.force_shwi();
			} else
				return NULL_TREE;
		}
		break;
		case REALPART_EXPR:
			break;
		case IMAGPART_EXPR:
			byte_offset += TREE_INT_CST_LOW(
					TYPE_SIZE_UNIT(TREE_TYPE(exp)));
			break;
		case VIEW_CONVERT_EXPR:
			break;
		case MEM_REF:
		{
			tree base = TREE_OPERAND(exp, 0);
			if (valueize && (TREE_CODE(base) == SSA_NAME))
				base = (*valueize)(base);

			if (TREE_CODE(base) == ADDR_EXPR) {
				if (!integer_zerop(TREE_OPERAND(exp, 1))) {
					poly_offset_int off;
					off = mem_ref_offset(exp);
					byte_offset += off.force_shwi();
				}
				exp = TREE_OPERAND(base, 0);
			}
			goto done;
		}
		case TARGET_MEM_REF:
		{
			tree base = TREE_OPERAND(exp, 0);
			if (valueize && (TREE_CODE (base) == SSA_NAME))
				base = (*valueize)(base);

			if (TREE_CODE(base) == ADDR_EXPR) {
				if (TMR_INDEX(exp) || TMR_INDEX2(exp))
					return NULL_TREE;
				if (!integer_zerop(TMR_OFFSET(exp))) {
					poly_offset_int off;
					off = mem_ref_offset(exp);
					byte_offset += off.force_shwi();
				}
				exp = TREE_OPERAND(base, 0);
			}
			goto done;
		}
		default:
			goto done;
		}

		exp = TREE_OPERAND(exp, 0);
	}

done:
	*poffset = byte_offset;
	return exp;
}

tree get_addr_base_and_unit_offset(tree exp, poly_int64_pod *poffset)
{
	return get_addr_base_and_unit_offset_1(exp, poffset, NULL);
}
