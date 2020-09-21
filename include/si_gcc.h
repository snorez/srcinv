/*
 * Copyright (C) 2019  zerons
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
#ifndef SI_GCC_H_LETQ5PZR
#define SI_GCC_H_LETQ5PZR

#ifndef __cplusplus
#error Must use g++ to compile, you may need to link the compiler_gcc too.
#endif

#ifndef _FILE_OFFSET_BITS
#define	_FILE_OFFSET_BITS 64
#endif

#ifndef xmalloc
#define	xmalloc malloc
#endif

#ifndef vecpfx
#define	vecpfx m_vecpfx
#endif

#ifndef vecdata
#define	vecdata m_vecdata
#endif

#include <gcc-plugin.h>
//#include <plugin-version.h>
#include <tree.h>
#include <c-tree.h>
#include <context.h>
#include <function.h>
#include <internal-fn.h>
#include <is-a.h>
#include <predict.h>
#include <basic-block.h>
#include <tree-ssa-alias.h>
#include <gimple-expr.h>
#include <gimple.h>
#include <gimple-ssa.h>
#include <tree-pretty-print.h>
#include <tree-pass.h>
#include <tree-ssa-operands.h>
#include <tree-phinodes.h>
#include <gimple-pretty-print.h>
#include <gimple-iterator.h>
#include <gimple-walk.h>
#include <tree-core.h>
#include <dumpfile.h>
#include <print-tree.h>
#include <rtl.h>
#include <cgraph.h>
#include <cfg.h>
#include <cfgloop.h>
#include <except.h>
#include <real.h>
#include <function.h>
#include <input.h>
#include <typed-splay-tree.h>
#include <tree-pass.h>
#include <tree-phinodes.h>
#include <tree-cfg.h>
#include <value-prof.h>
#include <stringpool.h>
#include <tree-vrp.h>
#include <tree-ssanames.h>
#include <cpplib.h>
#include <tm-preds.h>
#include <safe-ctype.h>
#include <tree-vector-builder.h>
#include <tree-dfa.h>
#include <dfp.h>
#include <calls.h>

#include "si_core.h"

#ifndef TYPE_MODE_RAW
#define	TYPE_MODE_RAW(NODE) (TYPE_CHECK(NODE)->type_common.mode)
#endif

static inline tree si_gcc_global(const char *str, void *n, int idx)
{
	tree ret = NULL_TREE;
	if (idx < 0)
		return ret;

	struct sibuf *b;
	b = find_target_sibuf(n);
	if (!b)
		return ret;
	analysis__resfile_load(b);

	int maxlen;
	tree *ptr = (tree *)analysis__sibuf_get_global(b, str, &maxlen);
	if (!ptr)
		return ret;
	else if (idx >= maxlen)
		return ret;

	ret = ptr[idx];
	return ret;
}

#define	SI_GCC_GLOBAL(str, n, idx) \
	({\
		tree ____ret;\
		____ret = si_gcc_global(str, n, idx);\
		____ret;\
	})

#define	si_global_trees(n, idx)		SI_GCC_GLOBAL("global_trees", n, idx)
#define	si_integer_types(n, idx)	SI_GCC_GLOBAL("integer_types", n, idx)
#define	si_sizetype_tab(n, idx)		SI_GCC_GLOBAL("sizetype_tab", n, idx)

#define si_error_mark_node(n)	\
	si_global_trees(n, TI_ERROR_MARK)
#define si_intQI_type_node(n)	\
	si_global_trees(n, TI_INTQI_TYPE)
#define si_intHI_type_node(n)	\
	si_global_trees(n, TI_INTHI_TYPE)
#define si_intSI_type_node(n)	\
	si_global_trees(n, TI_INTSI_TYPE)
#define si_intDI_type_node(n)	\
	si_global_trees(n, TI_INTDI_TYPE)
#define si_intTI_type_node(n)	\
	si_global_trees(n, TI_INTTI_TYPE)
#define si_unsigned_intQI_type_node(n)	\
	si_global_trees(n, TI_UINTQI_TYPE)
#define si_unsigned_intHI_type_node(n)	\
	si_global_trees(n, TI_UINTHI_TYPE)
#define si_unsigned_intSI_type_node(n)	\
	si_global_trees(n, TI_UINTSI_TYPE)
#define si_unsigned_intDI_type_node(n)	\
	si_global_trees(n, TI_UINTDI_TYPE)
#define si_unsigned_intTI_type_node(n)	\
	si_global_trees(n, TI_UINTTI_TYPE)
#define si_atomicQI_type_node(n)	\
	si_global_trees(n, TI_ATOMICQI_TYPE)
#define si_atomicHI_type_node(n)	\
	si_global_trees(n, TI_ATOMICHI_TYPE)
#define si_atomicSI_type_node(n)	\
	si_global_trees(n, TI_ATOMICSI_TYPE)
#define si_atomicDI_type_node(n)	\
	si_global_trees(n, TI_ATOMICDI_TYPE)
#define si_atomicTI_type_node(n)	\
	si_global_trees(n, TI_ATOMICTI_TYPE)
#define si_uint16_type_node(n)	\
	si_global_trees(n, TI_UINT16_TYPE)
#define si_uint32_type_node(n)	\
	si_global_trees(n, TI_UINT32_TYPE)
#define si_uint64_type_node(n)	\
	si_global_trees(n, TI_UINT64_TYPE)
#define si_void_node(n)	\
	si_global_trees(n, TI_VOID)
#define si_integer_zero_node(n)	\
	si_global_trees(n, TI_INTEGER_ZERO)
#define si_integer_one_node(n)	\
	si_global_trees(n, TI_INTEGER_ONE)
#define si_integer_three_node(n)	\
	si_global_trees(n, TI_INTEGER_THREE)
#define si_integer_minus_one_node(n)	\
	si_global_trees(n, TI_INTEGER_MINUS_ONE)
#define si_size_zero_node(n)	\
	si_global_trees(n, TI_SIZE_ZERO)
#define si_size_one_node(n)	\
	si_global_trees(n, TI_SIZE_ONE)
#define si_bitsize_zero_node(n)	\
	si_global_trees(n, TI_BITSIZE_ZERO)
#define si_bitsize_one_node(n)	\
	si_global_trees(n, TI_BITSIZE_ONE)
#define si_bitsize_unit_node(n)	\
	si_global_trees(n, TI_BITSIZE_UNIT)
#define si_access_public_node(n)	\
	si_global_trees(n, TI_PUBLIC)
#define si_access_protected_node(n)	\
	si_global_trees(n, TI_PROTECTED)
#define si_access_private_node(n)	\
	si_global_trees(n, TI_PRIVATE)
#define si_null_pointer_node(n)	\
	si_global_trees(n, TI_NULL_POINTER)
#define si_float_type_node(n)	\
	si_global_trees(n, TI_FLOAT_TYPE)
#define si_double_type_node(n)	\
	si_global_trees(n, TI_DOUBLE_TYPE)
#define si_long_double_type_node(n)	\
	si_global_trees(n, TI_LONG_DOUBLE_TYPE)
#define SI_FLOATN_TYPE_NODE(n, IDX)	\
	si_global_trees(n, TI_FLOATN_TYPE_FIRST + (IDX))
#define SI_FLOATN_NX_TYPE_NODE(n, IDX)	\
	si_global_trees(n, TI_FLOATN_NX_TYPE_FIRST + (IDX))
#define SI_FLOATNX_TYPE_NODE(n, IDX)	\
	si_global_trees(n, TI_FLOATNX_TYPE_FIRST + (IDX))
#define si_float16_type_node(n)	\
	si_global_trees(n, TI_FLOAT16_TYPE)
#define si_float32_type_node(n)	\
	si_global_trees(n, TI_FLOAT32_TYPE)
#define si_float64_type_node(n)	\
	si_global_trees(n, TI_FLOAT64_TYPE)
#define si_float128_type_node(n)	\
	si_global_trees(n, TI_FLOAT128_TYPE)
#define si_float32x_type_node(n)	\
	si_global_trees(n, TI_FLOAT32X_TYPE)
#define si_float64x_type_node(n)	\
	si_global_trees(n, TI_FLOAT64X_TYPE)
#define si_float128x_type_node(n)	\
	si_global_trees(n, TI_FLOAT128X_TYPE)
#define si_float_ptr_type_node(n)	\
	si_global_trees(n, TI_FLOAT_PTR_TYPE)
#define si_double_ptr_type_node(n)	\
	si_global_trees(n, TI_DOUBLE_PTR_TYPE)
#define si_long_double_ptr_type_node(n)	\
	si_global_trees(n, TI_LONG_DOUBLE_PTR_TYPE)
#define si_integer_ptr_type_node(n)	\
	si_global_trees(n, TI_INTEGER_PTR_TYPE)
#define si_complex_integer_type_node(n)	\
	si_global_trees(n, TI_COMPLEX_INTEGER_TYPE)
#define si_complex_float_type_node(n)	\
	si_global_trees(n, TI_COMPLEX_FLOAT_TYPE)
#define si_complex_double_type_node(n)	\
	si_global_trees(n, TI_COMPLEX_DOUBLE_TYPE)
#define si_complex_long_double_type_node(n)	\
	si_global_trees(n, TI_COMPLEX_LONG_DOUBLE_TYPE)
#define SI_COMPLEX_FLOATN_NX_TYPE_NODE(n, IDX)	\
	si_global_trees(n, TI_COMPLEX_FLOATN_NX_TYPE_FIRST + (IDX))
#define si_void_type_node(n)	\
	si_global_trees(n, TI_VOID_TYPE)
#define si_ptr_type_node(n)	\
	si_global_trees(n, TI_PTR_TYPE)
#define si_const_ptr_type_node(n)	\
	si_global_trees(n, TI_CONST_PTR_TYPE)
#define si_size_type_node(n)	\
	si_global_trees(n, TI_SIZE_TYPE)
#define si_pid_type_node(n)	\
	si_global_trees(n, TI_PID_TYPE)
#define si_ptrdiff_type_node(n)	\
	si_global_trees(n, TI_PTRDIFF_TYPE)
#define si_va_list_type_node(n)	\
	si_global_trees(n, TI_VA_LIST_TYPE)
#define si_va_list_gpr_counter_field(n)	\
	si_global_trees(n, TI_VA_LIST_GPR_COUNTER_FIELD)
#define si_va_list_fpr_counter_field(n)	\
	si_global_trees(n, TI_VA_LIST_FPR_COUNTER_FIELD)
#define si_fileptr_type_node(n)	\
	si_global_trees(n, TI_FILEPTR_TYPE)
#define si_const_tm_ptr_type_node(n)	\
	si_global_trees(n, TI_CONST_TM_PTR_TYPE)
#define si_fenv_t_ptr_type_node(n)	\
	si_global_trees(n, TI_FENV_T_PTR_TYPE)
#define si_const_fenv_t_ptr_type_node(n)	\
	si_global_trees(n, TI_CONST_FENV_T_PTR_TYPE)
#define si_fexcept_t_ptr_type_node(n)	\
	si_global_trees(n, TI_FEXCEPT_T_PTR_TYPE)
#define si_const_fexcept_t_ptr_type_node(n)	\
	si_global_trees(n, TI_CONST_FEXCEPT_T_PTR_TYPE)
#define si_pointer_sized_int_node(n)	\
	si_global_trees(n, TI_POINTER_SIZED_TYPE)
#define si_boolean_type_node(n)	\
	si_global_trees(n, TI_BOOLEAN_TYPE)
#define si_boolean_false_node(n)	\
	si_global_trees(n, TI_BOOLEAN_FALSE)
#define si_boolean_true_node(n)	\
	si_global_trees(n, TI_BOOLEAN_TRUE)
#define si_dfloat32_type_node(n)	\
	si_global_trees(n, TI_DFLOAT32_TYPE)
#define si_dfloat64_type_node(n)	\
	si_global_trees(n, TI_DFLOAT64_TYPE)
#define si_dfloat128_type_node(n)	\
	si_global_trees(n, TI_DFLOAT128_TYPE)
#define si_dfloat32_ptr_type_node(n)	\
	si_global_trees(n, TI_DFLOAT32_PTR_TYPE)
#define si_dfloat64_ptr_type_node(n)	\
	si_global_trees(n, TI_DFLOAT64_PTR_TYPE)
#define si_dfloat128_ptr_type_node(n)	\
	si_global_trees(n, TI_DFLOAT128_PTR_TYPE)
#define si_sat_short_fract_type_node(n)	\
	si_global_trees(n, TI_SAT_SFRACT_TYPE)
#define si_sat_fract_type_node(n)	\
	si_global_trees(n, TI_SAT_FRACT_TYPE)
#define si_sat_long_fract_type_node(n)	\
	si_global_trees(n, TI_SAT_LFRACT_TYPE)
#define si_sat_long_long_fract_type_node(n)	\
	si_global_trees(n, TI_SAT_LLFRACT_TYPE)
#define si_sat_unsigned_short_fract_type_node(n)	\
	si_global_trees(n, TI_SAT_USFRACT_TYPE)
#define si_sat_unsigned_fract_type_node(n)	\
	si_global_trees(n, TI_SAT_UFRACT_TYPE)
#define si_sat_unsigned_long_fract_type_node(n)	\
	si_global_trees(n, TI_SAT_ULFRACT_TYPE)
#define si_sat_unsigned_long_long_fract_type_node(n)	\
	si_global_trees(n, TI_SAT_ULLFRACT_TYPE)
#define si_short_fract_type_node(n)	\
	si_global_trees(n, TI_SFRACT_TYPE)
#define si_fract_type_node(n)	\
	si_global_trees(n, TI_FRACT_TYPE)
#define si_long_fract_type_node(n)	\
	si_global_trees(n, TI_LFRACT_TYPE)
#define si_long_long_fract_type_node(n)	\
	si_global_trees(n, TI_LLFRACT_TYPE)
#define si_unsigned_short_fract_type_node(n)	\
	si_global_trees(n, TI_USFRACT_TYPE)
#define si_unsigned_fract_type_node(n)	\
	si_global_trees(n, TI_UFRACT_TYPE)
#define si_unsigned_long_fract_type_node(n)	\
	si_global_trees(n, TI_ULFRACT_TYPE)
#define si_unsigned_long_long_fract_type_node(n)	\
	si_global_trees(n, TI_ULLFRACT_TYPE)
#define si_sat_short_accum_type_node(n)	\
	si_global_trees(n, TI_SAT_SACCUM_TYPE)
#define si_sat_accum_type_node(n)	\
	si_global_trees(n, TI_SAT_ACCUM_TYPE)
#define si_sat_long_accum_type_node(n)	\
	si_global_trees(n, TI_SAT_LACCUM_TYPE)
#define si_sat_long_long_accum_type_node(n)	\
	si_global_trees(n, TI_SAT_LLACCUM_TYPE)
#define si_sat_unsigned_short_accum_type_node(n)	\
	si_global_trees(n, TI_SAT_USACCUM_TYPE)
#define si_sat_unsigned_accum_type_node(n)	\
	si_global_trees(n, TI_SAT_UACCUM_TYPE)
#define si_sat_unsigned_long_accum_type_node(n)	\
	si_global_trees(n, TI_SAT_ULACCUM_TYPE)
#define si_sat_unsigned_long_long_accum_type_node(n)	\
	si_global_trees(n, TI_SAT_ULLACCUM_TYPE)
#define si_short_accum_type_node(n)	\
	si_global_trees(n, TI_SACCUM_TYPE)
#define si_accum_type_node(n)	\
	si_global_trees(n, TI_ACCUM_TYPE)
#define si_long_accum_type_node(n)	\
	si_global_trees(n, TI_LACCUM_TYPE)
#define si_long_long_accum_type_node(n)	\
	si_global_trees(n, TI_LLACCUM_TYPE)
#define si_unsigned_short_accum_type_node(n)	\
	si_global_trees(n, TI_USACCUM_TYPE)
#define si_unsigned_accum_type_node(n)	\
	si_global_trees(n, TI_UACCUM_TYPE)
#define si_unsigned_long_accum_type_node(n)	\
	si_global_trees(n, TI_ULACCUM_TYPE)
#define si_unsigned_long_long_accum_type_node(n)	\
	si_global_trees(n, TI_ULLACCUM_TYPE)
#define si_qq_type_node(n)	\
	si_global_trees(n, TI_QQ_TYPE)
#define si_hq_type_node(n)	\
	si_global_trees(n, TI_HQ_TYPE)
#define si_sq_type_node(n)	\
	si_global_trees(n, TI_SQ_TYPE)
#define si_dq_type_node(n)	\
	si_global_trees(n, TI_DQ_TYPE)
#define si_tq_type_node(n)	\
	si_global_trees(n, TI_TQ_TYPE)
#define si_uqq_type_node(n)	\
	si_global_trees(n, TI_UQQ_TYPE)
#define si_uhq_type_node(n)	\
	si_global_trees(n, TI_UHQ_TYPE)
#define si_usq_type_node(n)	\
	si_global_trees(n, TI_USQ_TYPE)
#define si_udq_type_node(n)	\
	si_global_trees(n, TI_UDQ_TYPE)
#define si_utq_type_node(n)	\
	si_global_trees(n, TI_UTQ_TYPE)
#define si_sat_qq_type_node(n)	\
	si_global_trees(n, TI_SAT_QQ_TYPE)
#define si_sat_hq_type_node(n)	\
	si_global_trees(n, TI_SAT_HQ_TYPE)
#define si_sat_sq_type_node(n)	\
	si_global_trees(n, TI_SAT_SQ_TYPE)
#define si_sat_dq_type_node(n)	\
	si_global_trees(n, TI_SAT_DQ_TYPE)
#define si_sat_tq_type_node(n)	\
	si_global_trees(n, TI_SAT_TQ_TYPE)
#define si_sat_uqq_type_node(n)	\
	si_global_trees(n, TI_SAT_UQQ_TYPE)
#define si_sat_uhq_type_node(n)	\
	si_global_trees(n, TI_SAT_UHQ_TYPE)
#define si_sat_usq_type_node(n)	\
	si_global_trees(n, TI_SAT_USQ_TYPE)
#define si_sat_udq_type_node(n)	\
	si_global_trees(n, TI_SAT_UDQ_TYPE)
#define si_sat_utq_type_node(n)	\
	si_global_trees(n, TI_SAT_UTQ_TYPE)
#define si_ha_type_node(n)	\
	si_global_trees(n, TI_HA_TYPE)
#define si_sa_type_node(n)	\
	si_global_trees(n, TI_SA_TYPE)
#define si_da_type_node(n)	\
	si_global_trees(n, TI_DA_TYPE)
#define si_ta_type_node(n)	\
	si_global_trees(n, TI_TA_TYPE)
#define si_uha_type_node(n)	\
	si_global_trees(n, TI_UHA_TYPE)
#define si_usa_type_node(n)	\
	si_global_trees(n, TI_USA_TYPE)
#define si_uda_type_node(n)	\
	si_global_trees(n, TI_UDA_TYPE)
#define si_uta_type_node(n)	\
	si_global_trees(n, TI_UTA_TYPE)
#define si_sat_ha_type_node(n)	\
	si_global_trees(n, TI_SAT_HA_TYPE)
#define si_sat_sa_type_node(n)	\
	si_global_trees[TI_SAT_SA_TYPE]
#define si_sat_da_type_node(n)	\
	si_global_trees(n, TI_SAT_DA_TYPE)
#define si_sat_ta_type_node(n)	\
	si_global_trees(n, TI_SAT_TA_TYPE)
#define si_sat_uha_type_node(n)	\
	si_global_trees(n, TI_SAT_UHA_TYPE)
#define si_sat_usa_type_node(n)	\
	si_global_trees(n, TI_SAT_USA_TYPE)
#define si_sat_uda_type_node(n)	\
	si_global_trees(n, TI_SAT_UDA_TYPE)
#define si_sat_uta_type_node(n)	\
	si_global_trees(n, TI_SAT_UTA_TYPE)
#define si_void_list_node(n)	\
	si_global_trees(n, TI_VOID_LIST_NODE)
#define si_main_identifier_node(n)	\
	si_global_trees(n, TI_MAIN_IDENTIFIER)
#define SI_MAIN_NAME_P(NODE) \
	(IDENTIFIER_NODE_CHECK(NODE) == si_main_identifier_node(NODE))
#define si_optimization_default_node(n)	\
	si_global_trees(n, TI_OPTIMIZATION_DEFAULT)
#define si_optimization_current_node(n)	\
	si_global_trees(n, TI_OPTIMIZATION_CURRENT)
#define si_target_option_default_node(n)	\
	si_global_trees(n, TI_TARGET_OPTION_DEFAULT)
#define si_target_option_current_node(n)	\
	si_global_trees(n, TI_TARGET_OPTION_CURRENT)
#define si_current_target_pragma(n)	\
	si_global_trees(n, TI_CURRENT_TARGET_PRAGMA)
#define si_current_optimize_pragma(n)	\
	si_global_trees(n, TI_CURRENT_OPTIMIZE_PRAGMA)

#define si_char_type_node(n)	\
	si_integer_types(n, itk_char)
#define si_signed_char_type_node(n)	\
	si_integer_types(n, itk_signed_char)
#define si_unsigned_char_type_node(n)	\
	si_integer_types(n, itk_unsigned_char)
#define si_short_integer_type_node(n)	\
	si_integer_types(n, itk_short)
#define si_short_unsigned_type_node(n)	\
	si_integer_types(n, itk_unsigned_short)
#define si_integer_type_node(n)	\
	si_integer_types(n, itk_int)
#define si_unsigned_type_node(n)	\
	si_integer_types(n, itk_unsigned_int)
#define si_long_integer_type_node(n)	\
	si_integer_types(n, itk_long)
#define si_long_unsigned_type_node(n)	\
	si_integer_types(n, itk_unsigned_long)
#define si_long_long_integer_type_node(n)	\
	si_integer_types(n, itk_long_long)
#define si_long_long_unsigned_type_node(n)	\
	si_integer_types(n, itk_unsigned_long_long)

#define si_sizetype(n)	\
	si_sizetype_tab(n, (int)stk_sizetype)
#define si_bitsizetype(n)	\
	si_sizetype_tab(n, (int)stk_bitsizetype)
#define si_ssizetype(n)	\
	si_sizetype_tab(n, (int)stk_ssizetype)
#define si_sbitsizetype(n)	\
	si_sizetype_tab(n, (int)stk_sbitsizetype)

DECL_BEGIN

C_SYM const char *const tree_code_name[];
C_SYM const size_t gsstruct_code_size[];
C_SYM void show_gimple(gimple_seq);
C_SYM tree si_private_lookup_attribute(const char *attr_name, size_t attr_len,
					tree list);
C_SYM int check_tree_code(tree n);
C_SYM int check_gimple_code(gimple_seq gs);
C_SYM const unsigned char si_lookup_constraint_array[];
C_SYM histogram_value si_gimple_histogram_value(struct function *, gimple *);
C_SYM tree si_build1(enum tree_code code, tree type, tree node);
C_SYM tree si_size_int_kind(poly_int64 number, enum size_type_kind kind);
C_SYM const unsigned short si_sch_istable[256];
C_SYM tree si_build0(enum tree_code code, tree tt);
C_SYM tree si_ss_ph_in_expr(tree, tree);
C_SYM tree si_fold_build1_loc(location_t loc, enum tree_code code,
			tree type, tree op0);
C_SYM tree si_fold_build2_loc(location_t loc, enum tree_code code,
			tree type, tree op0, tree op1);
C_SYM tree si_build_fixed(tree type, FIXED_VALUE_TYPE f);
C_SYM tree si_build_poly_int_cst(tree type, const poly_wide_int_ref &values);
C_SYM REAL_VALUE_TYPE si_real_value_from_int_cst(const_tree type,const_tree i);
C_SYM tree si_build_real_from_int_cst(tree type, const_tree i);
C_SYM tree si_fold_ignored_result(tree t);
C_SYM tree si_build_vector_from_val(tree vectype, tree sc);
C_SYM tree si_decl_function_context(const_tree decl);
C_SYM bool si_decl_address_invariant_p(const_tree op);
C_SYM bool si_tree_invariant_p(tree t);
C_SYM tree si_skip_simple_arithmetic(tree expr);
C_SYM bool si_contains_placeholder_p(const_tree exp);
C_SYM tree si_save_expr(tree expr);
C_SYM tree si_fold_convert_loc(location_t loc, tree type, tree arg);
C_SYM tree si_component_ref_field_offset(tree exp);
C_SYM bool si_parse_output_constraint(const char **constraint_p, int op_num,
				int ninputs, int noutputs,
				bool *allows_mem, bool *allows_reg,
				bool *is_inout);
C_SYM int si_flags_from_decl_or_type(const_tree exp);
C_SYM const int si_internal_fn_flags_array[];
C_SYM int si_gimple_call_flags(const gimple *stmt);
C_SYM void si_parse_ssa_operands(struct function *fn, gimple *gs);
C_SYM edge si_find_taken_edge_switch_expr(struct func_node *fn,
				    const gswitch *switch_stmt, tree val);
C_SYM edge si_find_taken_edge(struct func_node *fn, basic_block bb, tree val);

#if __GNUC__ >= 8
struct GTY(()) sorted_fields_type {
	int len;
	tree GTY((length("%h.len"))) elts[1];
};
#endif

#if __GNUC__ < 8
void symtab_node::dump_table(FILE *f)
{
}
#endif

static inline int check_file_type(tree node)
{
	enum tree_code tc = TREE_CODE(node);

	if (!TYPE_CANONICAL(node)) {
		return TYPE_DEFINED;
	} else if (TYPE_CANONICAL(node) != node) {
		return TYPE_CANONICALED;
	} else if (((tc == RECORD_TYPE) || (tc == UNION_TYPE)) &&
		(TYPE_CANONICAL(node) == node) &&
		(!TYPE_VALUES(node))) {
		return TYPE_UNDEFINED;
	} else {
		return TYPE_DEFINED;
	}
}

static inline int check_file_var(tree node)
{
	BUG_ON(TREE_CODE(node) != VAR_DECL);
	if (!DECL_EXTERNAL(node)) {
		if (TREE_PUBLIC(node)) {
			BUG_ON(!DECL_NAME(node));
			return VAR_IS_GLOBAL;
		} else {
			if (unlikely(!TREE_STATIC(node))) {
				return VAR_IS_LOCAL;
			}
			return VAR_IS_STATIC;
		}
	} else {
		BUG_ON(!DECL_NAME(node));
		return VAR_IS_EXTERN;
	}
}

/*
 * Now, we collect data at IPA_ALL_PASSES_END, which is after cfg,
 * the gimple_body is cleared, and the cfg is set.
 * FIXME: is this right?
 */
static inline int check_file_func(tree node)
{
	BUG_ON(TREE_CODE(node) != FUNCTION_DECL);
	BUG_ON(!DECL_NAME(node));

	/*
	 * update: we ignore DECL_SAVED_TREE check here, just
	 * check functions with f->cfg
	 * in the meanwhile, we should handle the caller
	 */
	if ((!DECL_EXTERNAL(node)) ||
		(DECL_STRUCT_FUNCTION(node) &&
		 (DECL_STRUCT_FUNCTION(node)->cfg))) {
		if (TREE_PUBLIC(node)) {
			return FUNC_IS_GLOBAL;
		} else {
			BUG_ON(!TREE_STATIC(node));
			return FUNC_IS_STATIC;
		}
	} else {
		return FUNC_IS_EXTERN;
	}
}

/* get location of TYPE_TYPE TYPE_VAR TYPE_FUNC */
#define	GET_LOC_TYPE	0
#define	GET_LOC_VAR	1
#define	GET_LOC_FUNC	2
static inline expanded_location *get_location(int flag, char *payload,
						tree node)
{
	if ((flag == GET_LOC_VAR) || (flag == GET_LOC_FUNC)) {
		return (expanded_location *)(payload +
						DECL_SOURCE_LOCATION(node));
	}

	/* for TYPE_TYPE */
	if (TYPE_NAME(node) && (TREE_CODE(TYPE_NAME(node)) == TYPE_DECL)) {
		return (expanded_location *)(payload +
					DECL_SOURCE_LOCATION(TYPE_NAME(node)));
	}

	/* try best to get a location of the type */
	if (((TREE_CODE(node) == RECORD_TYPE) ||
		(TREE_CODE(node) == UNION_TYPE)) &&
		TYPE_FIELDS(node)) {
		return (expanded_location *)(payload +
				DECL_SOURCE_LOCATION(TYPE_FIELDS(node)));
	}

	return NULL;
}

/* EXPR_LOCATION */
static inline expanded_location *si_expr_location(tree expr)
{
	struct sibuf *b;
	b = find_target_sibuf((void *)expr);
	BUG_ON(!b);
	analysis__resfile_load(b);

	if (CAN_HAVE_LOCATION_P(expr))
		return (expanded_location *)(b->payload + (expr)->exp.locus);
	else
		return NULL;
}

static inline expanded_location *get_gimple_loc(char *payload, location_t *loc)
{
	return (expanded_location *)(payload + *loc);
}

static inline int gimple_loc_string(char *ret, size_t retlen, gimple_seq gs)
{
	expanded_location *xloc;
	struct sibuf *buf;

	memset(ret, 0, retlen);

	buf = find_target_sibuf((void *)gs);
	if (!buf) {
		snprintf(ret, retlen, "%s", "get loc failed");
		return -1;
	}

	xloc = get_gimple_loc(buf->payload, &gs->location);
	snprintf(ret, retlen, "%s %d %d",
			xloc ? xloc->file : NULL,
			xloc ? xloc->line : 0,
			xloc ? xloc->column : 0);
	return 0;
}

/* get tree_identifier node name, the IDENTIFIER_NODE must not be NULL */
static inline void get_node_name(tree name, char *ret)
{
	BUG_ON(!name);
	BUG_ON(TREE_CODE(name) != IDENTIFIER_NODE);

	struct tree_identifier *node = (struct tree_identifier *)name;
	BUG_ON(!node->id.len);
	BUG_ON(node->id.len >= NAME_MAX);
	memcpy(ret, node->id.str, node->id.len);
}

static inline void get_function_name(void *addr, char *ret)
{
	tree node = (tree)addr;
	get_node_name(DECL_NAME(node), ret);
}

static inline void get_var_name(void *addr, char *ret)
{
	tree node = (tree)addr;
	if (DECL_NAME(node)) {
		get_node_name(DECL_NAME(node), ret);
	} else {
		snprintf(ret, NAME_MAX, "_%08lx", (long)addr);
		return;
	}
}

/* for type, if it doesn't have a name, use _%ld */
static inline void get_type_name(void *addr, char *ret)
{
	tree node = (tree)addr;
	if (!TYPE_NAME(node)) {
		return;
	}

	if (TREE_CODE(TYPE_NAME(node)) == IDENTIFIER_NODE) {
		get_node_name(TYPE_NAME(node), ret);
	} else {
		if (!DECL_NAME(TYPE_NAME(node))) {
			return;
		}
		get_node_name(DECL_NAME(TYPE_NAME(node)), ret);
	}
}

/*
 * FIXME: only generate name for UNION_TYPE RECORD_TYPE
 * we use tree_code, the number of the children, max to 10 names of the fields,
 * location to ident the type
 */
#define	FIELD_NAME_MAX	0x8
static inline void gen_type_name(char *name, size_t namelen, tree node,
				struct sinode *loc_file,
				expanded_location *xloc)
{
	enum tree_code tc = TREE_CODE(node);
	if ((tc != UNION_TYPE) && (tc != RECORD_TYPE))
		return;

	int copied_cnt = 0;
	int field_cnt = 0;
	char tmp_name[NAME_MAX];
	char *p = name;
	const char *sep = "_";

	tree fields = TYPE_FIELDS(node);
	while (fields) {
		field_cnt++;

		if ((copied_cnt < FIELD_NAME_MAX) && DECL_NAME(fields)) {
			memset(tmp_name, 0, NAME_MAX);
			get_node_name(DECL_NAME(fields), tmp_name);
			if (tmp_name[0]) {
				memcpy(p, sep, strlen(sep)+1);
				p += strlen(sep);
				memcpy(p, tmp_name, strlen(tmp_name)+1);
				p += strlen(tmp_name);
				copied_cnt++;
			}
		}
		fields = DECL_CHAIN(fields);
	}

	snprintf(tmp_name, NAME_MAX, "%s%d%s%d%s%ld%s%d%s%d",
			sep, (int)tc, sep, field_cnt,
			sep, (unsigned long)loc_file,
			sep, xloc->line, sep, xloc->column);
	memcpy(p, tmp_name, strlen(tmp_name)+1);

	return;
}

static inline void get_type_xnode(tree node, struct sinode **sn_ret,
					struct type_node **tn_ret)
{
	char name[NAME_MAX];
	expanded_location *xloc;
	enum sinode_type type;
	struct sinode *sn = NULL;
	struct type_node *tn = NULL;
	*sn_ret = NULL;
	*tn_ret = NULL;

	struct sibuf *b = find_target_sibuf((void *)node);
	if (unlikely(!b)) {
		si_log("target_sibuf not found, %p\n", node);
		return;
	}
	analysis__resfile_load(b);

	memset(name, 0, NAME_MAX);
	get_type_name((void *)node, name);
	xloc = get_location(GET_LOC_TYPE, b->payload, node);

	int flag;
	flag = check_file_type(node);
	if (flag == TYPE_CANONICALED) {
		tree tmp_node = TYPE_CANONICAL(node);

		b = find_target_sibuf((void *)tmp_node);
		if (unlikely(!b)) {
			si_log("target_sibuf not found, %p\n", tmp_node);
			/* FIXME: should we fall through or just return? */
			return;
		} else {
			node = tmp_node;
			analysis__resfile_load(b);

			memset(name, 0, NAME_MAX);
			get_type_name((void *)node, name);
			xloc = get_location(GET_LOC_TYPE, b->payload, node);
		}
	} else if (flag == TYPE_UNDEFINED) {
#if 0
		BUG_ON(!name[0]);
		sn = analysis__sinode_search(TYPE_TYPE, SEARCH_BY_TYPE_NAME,
						name);
		*sn_ret = sn;
#endif
		return;
	}

	struct sinode *loc_file = NULL;
	if (xloc && xloc->file) {
		loc_file = analysis__sinode_search(TYPE_FILE, SEARCH_BY_SPEC,
							(void *)xloc->file);
		BUG_ON(!loc_file);
	}

	if (loc_file && (!name[0]))
		gen_type_name(name, NAME_MAX, node, loc_file, xloc);

	if (name[0] && loc_file)
		type = TYPE_TYPE;
	else
		type = TYPE_NONE;

	if (type == TYPE_TYPE) {
		long args[4];
		args[0] = (long)loc_file;
		args[1] = xloc->line;
		args[2] = xloc->column;
		args[3] = (long)name;
		sn = analysis__sinode_search(type, SEARCH_BY_SPEC,
						(void *)args);
	} else {
		tn = analysis__sibuf_typenode_search(b, TREE_CODE(node), node);
	}

	if (sn) {
		*sn_ret = sn;
	} else if (tn) {
		*tn_ret = tn;
	}
}

static inline void get_func_sinode(tree node, struct sinode **sn_ret, int flag)
{
	char name[NAME_MAX];
	expanded_location *xloc;
	struct sinode *sn = NULL;

	*sn_ret = NULL;
	struct sibuf *b = find_target_sibuf((void *)node);
	BUG_ON(!b);
	analysis__resfile_load(b);

	memset(name, 0, NAME_MAX);
	get_function_name((void *)node, name);

	int func_flag = check_file_func(node);
	if (func_flag == FUNC_IS_EXTERN) {
		if (!flag)
			return;
		long args[3];
		args[0] = (long)b->rf;
		args[1] = (long)get_builtin_resfile();
		args[2] = (long)name;
		sn = analysis__sinode_search(TYPE_FUNC_GLOBAL, SEARCH_BY_SPEC,
						(void *)args);
	} else if (func_flag == FUNC_IS_GLOBAL) {
		long args[3];
		args[0] = (long)b->rf;
		args[1] = (long)get_builtin_resfile();
		args[2] = (long)name;
		sn = analysis__sinode_search(TYPE_FUNC_GLOBAL, SEARCH_BY_SPEC,
						(void *)args);
		BUG_ON(!sn);
	} else if (func_flag == FUNC_IS_STATIC) {
		xloc = get_location(GET_LOC_FUNC, b->payload, node);
		struct sinode *loc_file;
		loc_file = analysis__sinode_search(TYPE_FILE, SEARCH_BY_SPEC,
							(void *)xloc->file);
		BUG_ON(!loc_file);
		long args[4];
		args[0] = (long)loc_file;
		args[1] = xloc->line;
		args[2] = xloc->column;
		args[3] = (long)name;
		sn = analysis__sinode_search(TYPE_FUNC_STATIC, SEARCH_BY_SPEC,
						(void *)args);
	} else {
		BUG();
	}

	*sn_ret = sn;
}

static inline void get_var_sinode(tree node, struct sinode **sn_ret, int flag)
{
	char name[NAME_MAX];
	expanded_location *xloc;
	struct sinode *sn = NULL;

	*sn_ret = NULL;
	struct sibuf *b = find_target_sibuf((void *)node);
	BUG_ON(!b);
	analysis__resfile_load(b);

	memset(name, 0, NAME_MAX);
	get_var_name((void *)node, name);

	int var_flag = check_file_var(node);
	if (var_flag == VAR_IS_EXTERN) {
		if (!flag)
			return;
		long args[3];
		args[0] = (long)b->rf;
		args[1] = (long)get_builtin_resfile();
		args[2] = (long)name;
		sn = analysis__sinode_search(TYPE_VAR_GLOBAL, SEARCH_BY_SPEC,
						(void *)args);
	} else if (var_flag == VAR_IS_GLOBAL) {
		long args[3];
		args[0] = (long)b->rf;
		args[1] = (long)get_builtin_resfile();
		args[2] = (long)name;
		sn = analysis__sinode_search(TYPE_VAR_GLOBAL, SEARCH_BY_SPEC,
						(void *)args);
		BUG_ON(!sn);
	} else if (var_flag == VAR_IS_STATIC) {
		xloc = get_location(GET_LOC_VAR, b->payload, node);
		struct sinode *loc_file;
		loc_file = analysis__sinode_search(TYPE_FILE, SEARCH_BY_SPEC,
							(void *)xloc->file);
		BUG_ON(!loc_file);
		long args[4];
		args[0] = (long)loc_file;
		args[1] = xloc->line;
		args[2] = xloc->column;
		args[3] = (long)name;
		sn = analysis__sinode_search(TYPE_VAR_STATIC, SEARCH_BY_SPEC,
						(void *)args);
	} else {
		/* VAR_IS_LOCAL */
		return;
	}

	*sn_ret = sn;
	return;
}

static inline bool si_cmp_attribs(const char *attr1, size_t attr1_len,
				  const char *attr2, size_t attr2_len)
{
	return (attr1_len == attr2_len) &&
		(strncmp(attr1, attr2, attr1_len) == 0);
}

static inline bool si_cxx11_attribute_p(const_tree attr)
{
	if ((attr == NULL_TREE) || ((TREE_CODE(attr) != TREE_LIST)))
		return false;

	return (TREE_CODE(TREE_PURPOSE(attr)) == TREE_LIST);
}

static inline tree si_get_attribute_name(const_tree attr)
{
	if (si_cxx11_attribute_p(attr))
		return TREE_VALUE(TREE_PURPOSE(attr));
	return TREE_PURPOSE(attr);
}

/* gcc/tree-cfg.c dump_function_to_file() */
static inline void get_attributes(struct slist_head *head, tree attr_node)
{
	if (!attr_node)
		return;

	BUG_ON(TREE_CODE(attr_node) != TREE_LIST);
	char name[NAME_MAX];

	tree tl = attr_node;
	while (tl) {
		tree attr_name = si_get_attribute_name(tl);
		BUG_ON(TREE_CODE(attr_name) != IDENTIFIER_NODE);
		memset(name, 0, NAME_MAX);
		get_node_name(attr_name, name);

		struct attr_list *newal;
		newal = attr_list_add(name);

		tree tl2;
		tl2 = TREE_VALUE(tl);
		while (tl2) {
			tree valnode = TREE_VALUE(tl2);
			struct attrval_list *newavl;
			newavl = attrval_list_new();
			newavl->node = (void *)valnode;

			slist_add_tail(&newavl->sibling, &newal->values);

			tl2 = TREE_CHAIN(tl2);
		}

		slist_add_tail(&newal->sibling, head);

		tl = TREE_CHAIN(tl);
	}
}

static inline tree si_lookup_attribute(const char *attr_name, tree list)
{
	if (list == NULL_TREE) {
		return NULL_TREE;
	} else {
		size_t attr_len = strlen(attr_name);
		return si_private_lookup_attribute(attr_name, attr_len, list);
	}
}

/*
 * some functions in gcc header files
 */
static inline int si_is_global_var(tree node, expanded_location *xloc)
{
	int ret = 0;
	tree ctx = DECL_CONTEXT(node);
	if (is_global_var(node) &&
		((!ctx) || (TREE_CODE(ctx) == TRANSLATION_UNIT_DECL)))
		ret = 1;

	if (xloc)
		ret = (ret && (xloc->file));

	return !!ret;
}

static inline int si_gimple_has_body_p(tree fndecl)
{
	struct function *f;
	f = DECL_STRUCT_FUNCTION(fndecl);
	return (gimple_body(fndecl) ||
		(f && f->cfg && !(f->curr_properties & PROP_rtl)));
}

static inline int si_function_lowered(tree node)
{
	struct function *f;
	f = DECL_STRUCT_FUNCTION(node);
	return (f && (f->decl == node) &&
			(f->curr_properties & PROP_gimple_lcf));
}

static inline int si_gimple_in_ssa_p(struct function *f)
{
	return (f && f->gimple_df && f->gimple_df->in_ssa_p);
}

static inline tree si_gimple_vop(struct function *f)
{
	BUG_ON((!f) || (!f->gimple_df));
	return f->gimple_df->vop;
}

static inline int si_function_bb(tree fndecl)
{
	struct function *f;
	f = DECL_STRUCT_FUNCTION(fndecl);

	return (f && (f->decl == fndecl) && f->cfg &&
			f->cfg->x_basic_block_info);
}

static inline int si_n_basic_blocks_for_fn(struct function *f)
{
	BUG_ON((!f) || (!f->cfg));
	return f->cfg->x_n_basic_blocks;
}

static inline gphi_iterator si_gsi_start_phis(basic_block bb)
{
	gimple_seq *pseq = phi_nodes_ptr(bb);

	gphi_iterator i;

	i.ptr = gimple_seq_first(*pseq);
	i.seq = pseq;
	i.bb = i.ptr ? gimple_bb(i.ptr) : NULL;

	return i;
}

/* Check gimple_compare_field_offset() */
static inline unsigned long get_field_offset(tree field)
{
	unsigned long ret = 0;

	if (DECL_FIELD_OFFSET(field)) {
		ret += TREE_INT_CST_LOW(DECL_FIELD_OFFSET(field)) * 8;
	}

	if (DECL_FIELD_BIT_OFFSET(field)) {
		ret += TREE_INT_CST_LOW(DECL_FIELD_BIT_OFFSET(field));
	}

	return ret;
}

static inline tree si_get_containing_scope(const_tree t)
{
	return (TYPE_P(t) ? TYPE_CONTEXT(t) : DECL_CONTEXT(t));
}

static inline enum constraint_num si_lookup_constraint_1(const char *str)
{
	switch (str[0]) {
	case '<':
		return CONSTRAINT__l;
	case '>':
		return CONSTRAINT__g;
	case 'A':
		return CONSTRAINT_A;
	case 'B':
		if (!strncmp (str + 1, "g", 1))
			return CONSTRAINT_Bg;
		if (!strncmp (str + 1, "m", 1))
			return CONSTRAINT_Bm;
		if (!strncmp (str + 1, "c", 1))
			return CONSTRAINT_Bc;
		if (!strncmp (str + 1, "n", 1))
			return CONSTRAINT_Bn;
		if (!strncmp (str + 1, "s", 1))
			return CONSTRAINT_Bs;
		if (!strncmp (str + 1, "w", 1))
			return CONSTRAINT_Bw;
		if (!strncmp (str + 1, "z", 1))
			return CONSTRAINT_Bz;
		if (!strncmp (str + 1, "C", 1))
			return CONSTRAINT_BC;
		if (!strncmp (str + 1, "f", 1))
			return CONSTRAINT_Bf;
		break;
	case 'C':
		return CONSTRAINT_C;
	case 'D':
		return CONSTRAINT_D;
	case 'E':
		return CONSTRAINT_E;
	case 'F':
		return CONSTRAINT_F;
	case 'G':
		return CONSTRAINT_G;
	case 'I':
		return CONSTRAINT_I;
	case 'J':
		return CONSTRAINT_J;
	case 'K':
		return CONSTRAINT_K;
	case 'L':
		return CONSTRAINT_L;
	case 'M':
		return CONSTRAINT_M;
	case 'N':
		return CONSTRAINT_N;
	case 'O':
		return CONSTRAINT_O;
	case 'Q':
		return CONSTRAINT_Q;
	case 'R':
		return CONSTRAINT_R;
	case 'S':
		return CONSTRAINT_S;
	case 'T':
		if (!strncmp (str + 1, "s", 1))
			return CONSTRAINT_Ts;
		if (!strncmp (str + 1, "v", 1))
			return CONSTRAINT_Tv;
		break;
	case 'U':
		return CONSTRAINT_U;
	case 'V':
		return CONSTRAINT_V;
	case 'W':
		if (!strncmp (str + 1, "z", 1))
			return CONSTRAINT_Wz;
		if (!strncmp (str + 1, "d", 1))
			return CONSTRAINT_Wd;
		if (!strncmp (str + 1, "f", 1))
			return CONSTRAINT_Wf;
		if (!strncmp (str + 1, "e", 1))
			return CONSTRAINT_We;
		break;
	case 'X':
		return CONSTRAINT_X;
	case 'Y':
		if (!strncmp (str + 1, "z", 1))
			return CONSTRAINT_Yz;
		if (!strncmp (str + 1, "d", 1))
			return CONSTRAINT_Yd;
		if (!strncmp (str + 1, "p", 1))
			return CONSTRAINT_Yp;
		if (!strncmp (str + 1, "a", 1))
			return CONSTRAINT_Ya;
		if (!strncmp (str + 1, "b", 1))
			return CONSTRAINT_Yb;
		if (!strncmp (str + 1, "f", 1))
			return CONSTRAINT_Yf;
		if (!strncmp (str + 1, "r", 1))
			return CONSTRAINT_Yr;
		if (!strncmp (str + 1, "v", 1))
			return CONSTRAINT_Yv;
		if (!strncmp (str + 1, "k", 1))
			return CONSTRAINT_Yk;
		break;
	case 'Z':
		return CONSTRAINT_Z;
	case 'a':
		return CONSTRAINT_a;
	case 'b':
		return CONSTRAINT_b;
	case 'c':
		return CONSTRAINT_c;
	case 'd':
		return CONSTRAINT_d;
	case 'e':
		return CONSTRAINT_e;
	case 'f':
		return CONSTRAINT_f;
	case 'i':
		return CONSTRAINT_i;
	case 'k':
		return CONSTRAINT_k;
	case 'l':
		return CONSTRAINT_l;
	case 'm':
		return CONSTRAINT_m;
	case 'n':
		return CONSTRAINT_n;
	case 'o':
		return CONSTRAINT_o;
	case 'p':
		return CONSTRAINT_p;
	case 'q':
		return CONSTRAINT_q;
	case 'r':
		return CONSTRAINT_r;
	case 's':
		return CONSTRAINT_s;
	case 't':
		return CONSTRAINT_t;
	case 'u':
		return CONSTRAINT_u;
	case 'v':
		return CONSTRAINT_v;
	case 'x':
		return CONSTRAINT_x;
	case 'y':
		return CONSTRAINT_y;
	default:
		break;
	}

	return CONSTRAINT__UNKNOWN;
}

static inline enum constraint_num si_lookup_constraint(const char *p)
{
	unsigned int index = si_lookup_constraint_array[(unsigned char)*p];
	return (index == UCHAR_MAX ? si_lookup_constraint_1(p) :
			(enum constraint_num)index);
}

static inline tree si_build1_loc(location_t loc, enum tree_code code,
				tree type, tree arg1)
{
	tree t = si_build1(code, type, arg1);
	if (CAN_HAVE_LOCATION_P(t))
		SET_EXPR_LOCATION(t, loc);
	return t;
}

/* these two function may return NULL */
static inline gimple_seq cp_first_gimple(struct code_path *cp)
{
	basic_block bb;
	bb = (basic_block)cp->cp;
	return bb->il.gimple.seq;
}

static inline gimple_seq fn_first_gimple(struct func_node *fn)
{
	gimple_seq ret = NULL;
	basic_block entry;
	entry = DECL_STRUCT_FUNCTION((tree)fn->node)->cfg->x_entry_block_ptr;
	ret = entry->next_bb->il.gimple.seq;
	return ret;
}

static inline u32 get_type_bits(tree node)
{
	tree type = NULL; 
	if (TREE_CODE_CLASS(TREE_CODE(node)) == tcc_type)
		type = node;
	else
		type = TREE_TYPE(node);

	return TREE_INT_CST_LOW(TYPE_SIZE(type));
}

static inline u32 get_type_bytes(tree node)
{
	tree type = NULL;
	if (TREE_CODE_CLASS(TREE_CODE(node)) == tcc_type)
		type = node;
	else
		type = TREE_TYPE(node);

	return TREE_INT_CST_LOW(TYPE_SIZE_UNIT(type));
}

static inline bool
si_gimple_cond_true_p(const gcond *gs)
{
	tree lhs = gimple_cond_lhs(gs);
	tree rhs = gimple_cond_rhs(gs);
	enum tree_code code = gimple_cond_code(gs);

	if ((lhs != si_boolean_true_node((void *)gs)) &&
		(lhs != si_boolean_false_node((void *)gs)))
		return false;

	if ((rhs != si_boolean_true_node((void *)gs)) &&
		(rhs != si_boolean_false_node((void *)gs)))
		return false;

	if (code == NE_EXPR && lhs != rhs)
		return true;

	if (code == EQ_EXPR && lhs == rhs)
		return true;

	return false;
}

static inline bool
si_gimple_cond_false_p(const gcond *gs)
{
	tree lhs = gimple_cond_lhs(gs);
	tree rhs = gimple_cond_rhs(gs);
	enum tree_code code = gimple_cond_code(gs);

	if ((lhs != si_boolean_true_node((void *)gs)) &&
		(lhs != si_boolean_false_node((void *)gs)))
		return false;

	if ((rhs != si_boolean_true_node((void *)gs)) &&
		(rhs != si_boolean_false_node((void *)gs)))
		return false;

	if (code == NE_EXPR && lhs == rhs)
		return true;

	if (code == EQ_EXPR && lhs != rhs)
		return true;

	return false;
}

static inline
struct code_path *find_cp_by_bb(struct func_node *fn, basic_block bb)
{
	struct code_path *ret = NULL;
	for (int i = 0; i < fn->cp_cnt; i++) {
		if (fn->cps[i]->cp != bb)
			continue;
		ret = fn->cps[i];
		break;
	}
	return ret;
}

static inline
struct code_path *find_cp_by_gs(struct func_node *fn, gimple_seq gs)
{
	return find_cp_by_bb(fn, gs->bb);
}

#define si_sch_test(c, bit) (si_sch_istable[(c) & 0xff] & (unsigned short)(bit))

#define SI_ISALPHA(c)  si_sch_test(c, _sch_isalpha)
#define SI_ISALNUM(c)  si_sch_test(c, _sch_isalnum)
#define SI_ISBLANK(c)  si_sch_test(c, _sch_isblank)
#define SI_ISCNTRL(c)  si_sch_test(c, _sch_iscntrl)
#define SI_ISDIGIT(c)  si_sch_test(c, _sch_isdigit)
#define SI_ISGRAPH(c)  si_sch_test(c, _sch_isgraph)
#define SI_ISLOWER(c)  si_sch_test(c, _sch_islower)
#define SI_ISPRINT(c)  si_sch_test(c, _sch_isprint)
#define SI_ISPUNCT(c)  si_sch_test(c, _sch_ispunct)
#define SI_ISSPACE(c)  si_sch_test(c, _sch_isspace)
#define SI_ISUPPER(c)  si_sch_test(c, _sch_isupper)
#define SI_ISXDIGIT(c) si_sch_test(c, _sch_isxdigit)
#define SI_ISIDNUM(c)	si_sch_test(c, _sch_isidnum)
#define SI_ISIDST(c)	si_sch_test(c, _sch_isidst)
#define SI_IS_ISOBASIC(c)	si_sch_test(c, _sch_isbasic)
#define SI_IS_VSPACE(c)	si_sch_test(c, _sch_isvsp)
#define SI_IS_NVSPACE(c)	si_sch_test(c, _sch_isnvsp)
#define SI_IS_SPACE_OR_NUL(c)	si_sch_test(c, _sch_iscppsp)

#define	SI_SS_PH_IN_EXPR(EXP,OBJ) \
	((EXP) == 0 || TREE_CONSTANT(EXP) ? (EXP) : \
	 si_ss_ph_in_expr(EXP,OBJ))

static inline int si_internal_fn_flags(enum internal_fn fn)
{
	return si_internal_fn_flags_array[(int)fn];
}

DECL_END

template<typename T, typename A>
inline unsigned gcc_vec_safe_length(const vec<T, A, vl_embed> *v)
{
	if (!v)
		return 0;

	CLIB_DBG_FUNC_ENTER();
	struct vec_prefix *pfx;
	pfx = (struct vec_prefix *)&v->vecpfx;

	unsigned length = pfx->m_num;
	CLIB_DBG_FUNC_EXIT();
	return length;
}

template<typename T, typename A>
inline T *gcc_vec_safe_address(vec<T, A, vl_embed> *v)
{
	if (!v)
		return NULL;

	CLIB_DBG_FUNC_ENTER();
	T *addr = &v->vecdata[0];
	CLIB_DBG_FUNC_EXIT();
	return addr;
}

template<typename T, typename A>
inline int gcc_vec_safe_is_empty(const vec<T, A, vl_embed> *v)
{
	return gcc_vec_safe_length(v) == 0;
}

template<typename T, typename A>
inline void gcc_vec_length_address(vec<T, A, vl_embed> *v,
				unsigned *length, T **addr)
{
	CLIB_DBG_FUNC_ENTER();
	*length = gcc_vec_safe_length(v);
	*addr = gcc_vec_safe_address(v);
	CLIB_DBG_FUNC_EXIT();
}

static inline struct var_list *get_tn_field(struct type_node *tn, char *name)
{
	if (!tn)
		return NULL;

	struct var_list *tmp;
	slist_for_each_entry(tmp, &tn->children, sibling) {
		if (!tmp->var.name)
			continue;

		if (!strcmp(tmp->var.name, name))
			return tmp;
	}

	return NULL;
}

#endif /* end of include guard: SI_GCC_H_LETQ5PZR */
