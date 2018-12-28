/*
 * TODO
 * Copyright (C) 2018  zerons
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
#include "../si_core.h"

CLIB_PLUGIN_NAME(debuild);
CLIB_PLUGIN_NEEDED1(getinfo);
CLIB_PLUGIN_INIT()
{
	return 0;
}
CLIB_PLUGIN_EXIT()
{
	return;
}

void debuild_type(struct type_node *type)
{
	if (!type)
		return;
	int code = type->type_code;

	switch (code) {
	case OFFSET_TYPE:
	case REFERENCE_TYPE:
	case NULLPTR_TYPE:
	case FIXED_POINT_TYPE:
	case QUAL_UNION_TYPE:
	case POINTER_BOUNDS_TYPE:
	case METHOD_TYPE:
	case LANG_TYPE:
	default:
		BUG();
		break;
	case BOOLEAN_TYPE:
	{
		fprintf(stderr, "bool ");
		break;
	}
	case VOID_TYPE:
	{
		fprintf(stderr, "void ");
		break;
	}
	case ENUMERAL_TYPE:
	{
		fprintf(stderr, "enum %s {\n", type->type_name);

		struct var_node_list *tmp;
		list_for_each_entry(tmp, &type->children, sibling) {
			struct var_node *n = &tmp->var;
			struct possible_value_list *pv;
			fprintf(stderr, "%s = ", n->name);
			list_for_each_entry(pv, &n->possible_values, sibling) {
				fprintf(stderr, "%ld,", pv->value);
			}
			fprintf(stderr, "\n");
		}
		fprintf(stderr, "}\n");
		break;
	}
	case INTEGER_TYPE:
	{
		fprintf(stderr, "%s ", type->type_name);
		break;
	}
	case REAL_TYPE:
	{
		fprintf(stderr, "%s ", type->type_name);
		break;
	}
	case POINTER_TYPE:
	{
		debuild_type(type->next);
		fprintf(stderr, "* ");
		break;
	}
	case COMPLEX_TYPE:
	{
		fprintf(stderr, "%s ", type->type_name);
		break;
	}
	case VECTOR_TYPE:
	{
		fprintf(stderr, "vec %s ", type->type_name);
		break;
	}
	case ARRAY_TYPE:
	{
		/* TODO */
		break;
	}
	case RECORD_TYPE:
	case UNION_TYPE:
	{
		if (code == RECORD_TYPE) {
			fprintf(stderr, "struct %s {\n", type->type_name);
		} else {
			fprintf(stderr, "union %s {\n", type->type_name);
		}

		list_comm *tmp;
		list_for_each_entry(tmp, &type->children, list_head) {
			struct var_node *tmp0 = (struct var_node *)tmp->data;
			debuild_type(tmp0->type);
			fprintf(stderr, "%s;\n", tmp0->name);
		}
		fprintf(stderr, "};\n");
		break;
	}
	case FUNCTION_TYPE:
	{
		/* TODO */
		break;
	}
	}
}
