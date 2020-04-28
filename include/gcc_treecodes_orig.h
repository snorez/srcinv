/*
 * TODO
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

#ifndef TREE_CODES_H_3GN2AKZB
#define TREE_CODES_H_3GN2AKZB

#ifndef FOR_CPP
enum tree_code_class {
  tcc_exceptional, /* An exceptional code (fits no category).  */
  tcc_constant,    /* A constant.  */
  /* Order of tcc_type and tcc_declaration is important.  */
  tcc_type,        /* A type object code.  */
  tcc_declaration, /* A declaration (also serving as variable refs).  */
  tcc_reference,   /* A reference to storage.  */
  tcc_comparison,  /* A comparison expression.  */
  tcc_unary,       /* A unary arithmetic expression.  */
  tcc_binary,      /* A binary arithmetic expression.  */
  tcc_statement,   /* A statement expression, which have side effects
		      but usually no interesting value.  */
  tcc_vl_exp,      /* A function call or other expression with a
		      variable-length operand vector.  */
  tcc_expression   /* Any other expression.  */
};
#endif

#ifndef FOR_CPP
#define DEFTREECODE(SYM, NAME, TYPE, LENGTH) SYM,
#define END_OF_BASE_TREE_CODES LAST_AND_UNUSED_TREE_CODE,
enum tree_code {
#include "all-tree.def"
MAX_TREE_CODES
};
#undef DEFTREECODE
#undef END_OF_BASE_TREE_CODES
#endif

#define DEFTREECODE(SYM, NAME, TYPE, LENGTH) TYPE,
#define END_OF_BASE_TREE_CODES tcc_exceptional,
const enum tree_code_class tree_code_type[] = {
#include "all-tree.def"
};
#undef DEFTREECODE
#undef END_OF_BASE_TREE_CODES

#define DEFTREECODE(SYM, NAME, TYPE, LENGTH) LENGTH,
#define END_OF_BASE_TREE_CODES 0,
const unsigned char tree_code_length[] = {
#include "all-tree.def"
};
#undef DEFTREECODE
#undef END_OF_BASE_TREE_CODES

#define DEFTREECODE(SYM, NAME, TYPE, LEN) NAME,
#define END_OF_BASE_TREE_CODES "@dummy",
static const char *const tree_code_name[] = {
#include "all-tree.def"
};
#undef DEFTREECODE
#undef END_OF_BASE_TREE_CODES

#ifdef FOR_CPP
#define DEFGSSTRUCT(SYM, STRUCT, HAS_TREE_OP) \
	(HAS_TREE_OP ? sizeof (struct STRUCT) - sizeof (tree) : 0),
EXPORTED_CONST size_t gimple_ops_offset_[] = {
#include "gsstruct.def"
};
#undef DEFGSSTRUCT
#endif

#ifdef FOR_CPP
#define DEFGSSTRUCT(SYM, STRUCT, HAS_TREE_OP) sizeof (struct STRUCT),
static const size_t gsstruct_code_size[] = {
#include "gsstruct.def"
};
#undef DEFGSSTRUCT
#endif

#ifdef FOR_CPP
#define DEFGSCODE(SYM, NAME, GSSCODE)	NAME,
const char *const gimple_code_name[] = {
#include "gimple.def"
};
#undef DEFGSCODE
#endif

#ifdef FOR_CPP
#define DEFGSCODE(SYM, NAME, GSSCODE)	GSSCODE,
EXPORTED_CONST enum gimple_statement_structure_enum gss_for_code_[] = {
#include "gimple.def"
};
#undef DEFGSCODE
#endif

#ifdef FOR_CPP
#if __GNUC__ < 9
#define DEFTREECODE(SYM, STRING, TYPE, NARGS)   			    \
  (unsigned char)							    \
  ((TYPE) == tcc_unary ? GIMPLE_UNARY_RHS				    \
   : ((TYPE) == tcc_binary						    \
      || (TYPE) == tcc_comparison) ? GIMPLE_BINARY_RHS   		    \
   : ((TYPE) == tcc_constant						    \
      || (TYPE) == tcc_declaration					    \
      || (TYPE) == tcc_reference) ? GIMPLE_SINGLE_RHS			    \
   : ((SYM) == TRUTH_AND_EXPR						    \
      || (SYM) == TRUTH_OR_EXPR						    \
      || (SYM) == TRUTH_XOR_EXPR) ? GIMPLE_BINARY_RHS			    \
   : (SYM) == TRUTH_NOT_EXPR ? GIMPLE_UNARY_RHS				    \
   : ((SYM) == COND_EXPR						    \
      || (SYM) == WIDEN_MULT_PLUS_EXPR					    \
      || (SYM) == WIDEN_MULT_MINUS_EXPR					    \
      || (SYM) == DOT_PROD_EXPR						    \
      || (SYM) == SAD_EXPR						    \
      || (SYM) == REALIGN_LOAD_EXPR					    \
      || (SYM) == VEC_COND_EXPR						    \
      || (SYM) == VEC_PERM_EXPR                                             \
      || (SYM) == BIT_INSERT_EXPR					    \
      || (SYM) == FMA_EXPR) ? GIMPLE_TERNARY_RHS			    \
   : ((SYM) == CONSTRUCTOR						    \
      || (SYM) == OBJ_TYPE_REF						    \
      || (SYM) == ASSERT_EXPR						    \
      || (SYM) == ADDR_EXPR						    \
      || (SYM) == WITH_SIZE_EXPR					    \
      || (SYM) == SSA_NAME) ? GIMPLE_SINGLE_RHS				    \
   : GIMPLE_INVALID_RHS),
#else
#define DEFTREECODE(SYM, STRING, TYPE, NARGS)   			    \
  (unsigned char)							    \
  ((TYPE) == tcc_unary ? GIMPLE_UNARY_RHS				    \
   : ((TYPE) == tcc_binary						    \
      || (TYPE) == tcc_comparison) ? GIMPLE_BINARY_RHS   		    \
   : ((TYPE) == tcc_constant						    \
      || (TYPE) == tcc_declaration					    \
      || (TYPE) == tcc_reference) ? GIMPLE_SINGLE_RHS			    \
   : ((SYM) == TRUTH_AND_EXPR						    \
      || (SYM) == TRUTH_OR_EXPR						    \
      || (SYM) == TRUTH_XOR_EXPR) ? GIMPLE_BINARY_RHS			    \
   : (SYM) == TRUTH_NOT_EXPR ? GIMPLE_UNARY_RHS				    \
   : ((SYM) == COND_EXPR						    \
      || (SYM) == WIDEN_MULT_PLUS_EXPR					    \
      || (SYM) == WIDEN_MULT_MINUS_EXPR					    \
      || (SYM) == DOT_PROD_EXPR						    \
      || (SYM) == SAD_EXPR						    \
      || (SYM) == REALIGN_LOAD_EXPR					    \
      || (SYM) == VEC_COND_EXPR						    \
      || (SYM) == VEC_PERM_EXPR                                             \
      || (SYM) == BIT_INSERT_EXPR) ? GIMPLE_TERNARY_RHS			    \
   : ((SYM) == CONSTRUCTOR						    \
      || (SYM) == OBJ_TYPE_REF						    \
      || (SYM) == ASSERT_EXPR						    \
      || (SYM) == ADDR_EXPR						    \
      || (SYM) == WITH_SIZE_EXPR					    \
      || (SYM) == SSA_NAME) ? GIMPLE_SINGLE_RHS				    \
   : GIMPLE_INVALID_RHS),
#endif
#define END_OF_BASE_TREE_CODES (unsigned char) GIMPLE_INVALID_RHS,
const unsigned char gimple_rhs_class_table[] = {
#include "all-tree.def"
};
#undef DEFTREECODE
#undef END_OF_BASE_TREE_CODES
#endif

#endif /* end of include guard: TREE_CODES_H_3GN2AKZB */
