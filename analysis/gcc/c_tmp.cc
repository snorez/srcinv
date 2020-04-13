/* like dump_gimple_phi() in gcc/gimple-pretty-print.c */
static void do_gimple_phi_phase4(gphi *phi)
{
	CLIB_DBG_FUNC_ENTER();

	size_t i;
	tree lhs = gimple_phi_result(phi);
	do_phase4_tree(lhs);

	for (i = 0; i < gimple_phi_num_args(phi); i++) {
		/* ignore flags & TDF_GIMPLE */
		do_phase4_tree(gimple_phi_arg_def(phi, i));
	}

	CLIB_DBG_FUNC_EXIT();
	return;
}

/* this is like dump_phi_nodes() in gcc/gimple-pretty-print.c */
static void do_phi_nodes_phase4(basic_block bb)
{
	CLIB_DBG_FUNC_ENTER();

	gphi_iterator i;
	for (i = si_gsi_start_phis(bb); !gsi_end_p(i); gsi_next(&i)) {
		gphi *phi = i.phi();
		if (!virtual_operand_p(gimple_phi_result(phi))) {
			do_gimple_phi_phase4(phi);
		}
	}

	CLIB_DBG_FUNC_EXIT();
	return;
}

/* like pp_cfg_jump() in gcc/gimple-pretty-print.c */
static void do_edge_phase4(edge e)
{
	/* XXX: nothing to do here */
	return;
}

/* this is like dump_implicit_edges() in gcc/gimple-pretty-print.c */
static void do_implicit_edges(basic_block bb)
{
	CLIB_DBG_FUNC_ENTER();

	edge e;
	gimple *stmt;

	stmt = si_last_stmt(bb);
	if (stmt && (gimple_code(stmt) == GIMPLE_COND)) {
		edge true_edge, false_edge;

		if (EDGE_COUNT(bb->succs) != 2)
			goto out;

		si_extract_true_false_edges_from_block(bb,
							&true_edge,
							&false_edge);
		do_edge_phase4(true_edge);
		do_edge_phase4(false_edge);
		goto out;
	}

	e = find_fallthru_edge(bb->succs);
	if (e && (e->dest != bb->next_bb)) {
		do_edge_phase4(e);
	}

out:
	CLIB_DBG_FUNC_EXIT();
	return;
}

/* like pp_gimple_stmt_1() in gcc/gimple-pretty-print.c */
static void do_gimple_stmt_1_phase4(gimple *gs)
{
	if (!gs)
		return;

	CLIB_DBG_FUNC_ENTER();

	cur_gimple = gs;
	enum gimple_code gc = gimple_code(gs);
	tree *ops = gimple_ops(gs);
	for (unsigned i = 0; i < gimple_num_ops(gs); i++) {
		if ((gc == GIMPLE_CALL) && (i == 1))
			continue;

		if (!ops[i])
			continue;

		cur_gimple_op_idx = i;
		phase4_obj_idx = 0;
		for (size_t i = 0; i < obj_cnt; i++)
			phase4_obj_checked[i] = NULL;

		do_phase4_tree(ops[i]);
	}

	switch (gimple_code(gs)) {
	case GIMPLE_ASM:
	{
		/* TODO */
		break;
	}
	case GIMPLE_ASSIGN:
	{
		/* TODO */
		break;
	}
	case GIMPLE_CALL:
	{
		/* TODO */
		break;
	}
	case GIMPLE_COND:
	{
		/* TODO */
		break;
	}
	case GIMPLE_LABEL:
	{
		/* TODO */
		break;
	}
	case GIMPLE_GOTO:
	{
		/* TODO */
		break;
	}
	case GIMPLE_NOP:
	{
		/* TODO */
		break;
	}
	case GIMPLE_RETURN:
	{
		/* TODO */
		break;
	}
	case GIMPLE_SWITCH:
	{
		/* TODO */
		break;
	}
	case GIMPLE_PHI:
	{
		do_gimple_phi_phase4(as_a<gphi *>(gs));
		break;
	}
	case GIMPLE_OMP_PARALLEL:
	{
		/* TODO */
		break;
	}
	case GIMPLE_OMP_TASK:
	{
		/* TODO */
		break;
	}
	case GIMPLE_OMP_ATOMIC_LOAD:
	{
		/* TODO */
		break;
	}
	case GIMPLE_OMP_ATOMIC_STORE:
	{
		/* TODO */
		break;
	}
	case GIMPLE_OMP_FOR:
	{
		/* TODO */
		break;
	}
	case GIMPLE_OMP_CONTINUE:
	{
		/* TODO */
		break;
	}
	case GIMPLE_OMP_SINGLE:
	{
		/* TODO */
		break;
	}
	case GIMPLE_OMP_TARGET:
	{
		/* TODO */
		break;
	}
	case GIMPLE_OMP_TEAMS:
	{
		/* TODO */
		break;
	}
	case GIMPLE_OMP_RETURN:
	{
		/* TODO */
		break;
	}
	case GIMPLE_OMP_SECTIONS:
	{
		/* TODO */
		break;
	}
	case GIMPLE_OMP_SECTIONS_SWITCH:
	{
		/* TODO */
		break;
	}
	case GIMPLE_OMP_MASTER:
	case GIMPLE_OMP_TASKGROUP:
	case GIMPLE_OMP_SECTION:
	case GIMPLE_OMP_GRID_BODY:
	{
		/* TODO */
		break;
	}
	case GIMPLE_OMP_ORDERED:
	{
		/* TODO */
		break;
	}
	case GIMPLE_OMP_CRITICAL:
	{
		/* TODO */
		break;
	}
	case GIMPLE_BIND:
	case GIMPLE_TRY:
	case GIMPLE_CATCH:
	case GIMPLE_EH_FILTER:
	{
		si_log1("GIMPLE stmt: %s should NOT be in lower GIMPLE\n",
				gimple_code_name[gimple_code(gs)]);
		break;
	}
	case GIMPLE_EH_MUST_NOT_THROW:
	{
		/* TODO */
		break;
	}
	case GIMPLE_EH_ELSE:
	{
		/* TODO */
		break;
	}
	case GIMPLE_RESX:
	{
		/* TODO */
		break;
	}
	case GIMPLE_EH_DISPATCH:
	{
		/* TODO */
		break;
	}
	case GIMPLE_DEBUG:
	{
		/* TODO */
		break;
	}
	case GIMPLE_PREDICT:
	{
		/* TODO */
		break;
	}
	case GIMPLE_TRANSACTION:
	{
		/* TODO */
		break;
	}
	default:
	{
		si_log1("unknown GIMPLE stmt: %s\n",
			gimple_code_name[gimple_code(gs)]);
		break;
	}
	}

	CLIB_DBG_FUNC_EXIT();
	return;
}

/* like dump_histogram_value() in gcc/value-prof.c */
static void do_histogram_value_phase4(histogram_value hist)
{
	CLIB_DBG_FUNC_ENTER();

	switch (hist->type) {
	case HIST_TYPE_INTERVAL:
	{
		/* TODO */
		break;
	}
	case HIST_TYPE_POW2:
	{
		/* TODO */
		break;
	}
	case HIST_TYPE_SINGLE_VALUE:
	{
		/* TODO */
		break;
	}
	case HIST_TYPE_AVERAGE:
	{
		/* TODO */
		break;
	}
	case HIST_TYPE_IOR:
	{
		/* TODO */
		break;
	}
	case HIST_TYPE_INDIR_CALL:
	{
		/* TODO */
		break;
	}
	case HIST_TYPE_TIME_PROFILE:
	{
		/* TODO */
		break;
	}
	case HIST_TYPE_INDIR_CALL_TOPN:
	{
		/* TODO */
		break;
	}
	case HIST_TYPE_MAX:
	default:
	{
		break;
	}
	}

	CLIB_DBG_FUNC_EXIT();
	return;
}

/* like dump_histograms_for_stmt() in gcc/value-prof.c */
static void do_histograms_for_stmt(struct function *f, gimple *gs)
{
	CLIB_DBG_FUNC_ENTER();

	histogram_value hist;
	for (hist = si_gimple_histogram_value(f, gs); hist;
			hist = hist->hvalue.next) {
		do_histogram_value_phase4(hist);
	}

	CLIB_DBG_FUNC_EXIT();
	return;
}

/* this is like gimple_dump_bb_buff() */
static void __do_gimple_bb_phase4(basic_block bb)
{
	CLIB_DBG_FUNC_ENTER();

	gimple_stmt_iterator gsi;
	gimple *stmt;

	do_phi_nodes_phase4(bb);

	for (gsi = gsi_start_bb(bb); !gsi_end_p(gsi); gsi_next(&gsi)) {
		stmt = gsi_stmt(gsi);
		cur_gimple = stmt;
		do_gimple_stmt_1_phase4(stmt);
		do_histograms_for_stmt(DECL_STRUCT_FUNCTION(
					si_current_function_decl), stmt);
	}

	do_implicit_edges(bb);

	CLIB_DBG_FUNC_EXIT();
	return;
}

/* this is like gimple_dump_bb() in gcc/gimple-pretty-print.c */
static void do_gimple_bb_phase4(basic_block bb)
{
	CLIB_DBG_FUNC_ENTER();

	if (bb->index >= NUM_FIXED_BLOCKS) {
		__do_gimple_bb_phase4(bb);
	}

	CLIB_DBG_FUNC_EXIT();
	return;
}

/* this is like dump_bb() in gcc/cfghooks.c */
static void do_bb_phase4(basic_block bb)
{
	CLIB_DBG_FUNC_ENTER();

	/* cfg_hooks->dump_bb() */
	do_gimple_bb_phase4(bb);

	CLIB_DBG_FUNC_EXIT();
	return;
}

/*
 */
static void do_phase4_mark_var_func(struct sinode *sn)
{
	CLIB_DBG_FUNC_ENTER();

	struct func_node *fn;
	tree fndecl;

	fn = (struct func_node *)sn->data;
	fndecl = (tree)(long)sn->obj->real_addr;

	cur_fsn = sn;
	cur_fn = fn;
	si_current_function_decl = fndecl;

	struct function *f;
	f = DECL_STRUCT_FUNCTION(fndecl);

	/* ignore: handle the attributes */
	/* ignore: handle the arguments */

	/*
	 * when GIMPLE is lowered, the variables are no longer available in
	 * BIND_EXPRs, so handle them separately
	 *
	 * in gcc/tree-cfg.c, handle the vars here
	 */
	if (si_function_lowered(fndecl)) {
		unsigned i;

		/* ignore TDF_ALIAS */

		if (!gcc_vec_safe_is_empty(f->local_decls)) {
			unsigned length;
			tree *addr;
			gcc_vec_length_address(f->local_decls, &length, &addr);

			tree var;
			for (i = 0; i < length; i++) {
				var = addr[i];
				si_log1("func: %s, local_var_type: %s\n",
					fn->name,
					tree_code_name[TREE_CODE(var)]);
				/* do_tree(var); */
			}
		}

		/* FIXME: gcc use cfun here */
		if (si_gimple_in_ssa_p(f)) {
			unsigned length;
			tree *addr;
			gcc_vec_length_address(f->gimple_df->ssa_names,
						&length, &addr);

			tree name;
			/* FIXME: the first SSA_NAME reserved? */
			for (i = 1; i < length; i++) {
				name = addr[i];
				if (!name)
					continue;
				/* FIXME: what should we do here? */
				if (SSA_NAME_VAR(name)) {
					continue;
				}
			}
		}
	}

	if (si_function_bb(fndecl)) {
		basic_block bb;

		/*
		 * XXX: FOR_EACH_BB_FN skip the x_entry_block_ptr,
		 *	it is from x_entry_block_ptr->next_bb till
		 *	x_exit_block_ptr
		 */
		FOR_EACH_BB_FN(bb, f)
			do_bb_phase4(bb);
	} else {
		si_log1("todo\n");
	}

	CLIB_DBG_FUNC_EXIT();
	return;
}
