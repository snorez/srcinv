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

bool computed_goto_p(gimple *t)
{
  return ((gimple_code(t) == GIMPLE_GOTO) &&
		  (TREE_CODE(gimple_goto_dest(t)) != LABEL_DECL));
}

gimple *last_stmt(basic_block bb)
{
	gimple_stmt_iterator i;
	gimple *stmt = NULL;

	i = gsi_last_bb(bb);
	while ((!gsi_end_p(i)) && is_gimple_debug((stmt = gsi_stmt(i)))) {
		gsi_prev(&i);
		stmt = NULL;
	}

	return stmt;
}

void extract_true_false_edges_from_block(basic_block b,
					 edge *true_edge, edge *false_edge)
{
	edge e = EDGE_SUCC(b, 0);

	if (e->flags & EDGE_TRUE_VALUE) {
		*true_edge = e;
		*false_edge = EDGE_SUCC(b, 1);
	} else {
		*false_edge = e;
		*true_edge = EDGE_SUCC(b, 1);
	}
}

basic_block label_to_block(struct function *ifun, tree dest)
{
	int uid = LABEL_DECL_UID(dest);

	if (uid < 0)
		return NULL;

	if (vec_safe_length(ifun->cfg->x_label_to_block_map) <=
			(unsigned int)uid)
		return NULL;
	return (*ifun->cfg->x_label_to_block_map)[uid];
}

tree find_case_label_for_value(const gswitch *switch_stmt, tree val)
{
	size_t low, high, n = gimple_switch_num_labels(switch_stmt);
	tree default_case = gimple_switch_default_label(switch_stmt);

	for (low = 0, high = n; high - low > 1; ) {
		size_t i = (high + low) / 2;
		tree t = gimple_switch_label(switch_stmt, i);
		int cmp;

		cmp = tree_int_cst_compare(CASE_LOW(t), val);

		if (cmp > 0)
			high = i;
		else
			low = i;

		if (CASE_HIGH(t) == NULL) {
			if (cmp == 0)
				return t;
		} else {
			if ((cmp <= 0) &&
				(tree_int_cst_compare(CASE_HIGH(t), val) >= 0))
				return t;
		}
	}

	return default_case;
}

edge si_find_taken_edge_switch_expr(struct func_node *fn,
				    const gswitch *switch_stmt, tree val)
{
	basic_block dest_bb;
	edge e;
	tree taken_case;

	if (gimple_switch_num_labels(switch_stmt) == 1)
		taken_case = gimple_switch_default_label(switch_stmt);
	else {
		if (val == NULL_TREE)
			val = gimple_switch_index(switch_stmt);
		if (TREE_CODE(val) != INTEGER_CST)
			return NULL;
		else
			taken_case = find_case_label_for_value(switch_stmt,val);
	}
	dest_bb = label_to_block(DECL_STRUCT_FUNCTION((tree)fn->node),
				 CASE_LABEL(taken_case));

	e = find_edge(gimple_bb(switch_stmt), dest_bb);
	BUG_ON(!e);
	return e;
}

static edge si_find_taken_edge_computed_goto(struct func_node *fn,
						basic_block bb, tree val)
{
	basic_block dest;
	edge e = NULL;

	dest = label_to_block(DECL_STRUCT_FUNCTION((tree)fn->node), val);
	if (dest)
		e = find_edge(bb, dest);

	return e;
}

static edge find_taken_edge_cond_expr(const gcond *cond_stmt, tree val)
{
	edge true_edge, false_edge;

	if (val == NULL_TREE) {
		if (si_gimple_cond_true_p(cond_stmt))
			val = si_integer_one_node((void *)cond_stmt);
		else if (si_gimple_cond_false_p(cond_stmt))
			val = si_integer_zero_node((void *)cond_stmt);
		else
			return NULL;
	} else if (TREE_CODE(val) != INTEGER_CST)
		return NULL;

	extract_true_false_edges_from_block(gimple_bb(cond_stmt),
						&true_edge, &false_edge);

	return (integer_zerop(val) ? false_edge : true_edge);
}

edge si_find_taken_edge(struct func_node *fn, basic_block bb, tree val)
{
	gimple *stmt;

	stmt = last_stmt(bb);

	if (!stmt)
		return NULL;

	if (gimple_code(stmt) == GIMPLE_COND)
		return find_taken_edge_cond_expr(as_a<gcond *>(stmt), val);

	if (gimple_code(stmt) == GIMPLE_SWITCH)
		return si_find_taken_edge_switch_expr(fn, as_a<gswitch *>(stmt),
						   val);

	if (computed_goto_p(stmt)) {
		if (val &&
		    (TREE_CODE(val)==ADDR_EXPR || TREE_CODE(val)==LABEL_EXPR) &&
		    (TREE_CODE(TREE_OPERAND(val, 0)) == LABEL_DECL))
			return si_find_taken_edge_computed_goto(fn, bb,
							 TREE_OPERAND(val, 0));
	}

	return single_succ_p(bb) ? single_succ_edge(bb) : NULL;
}
