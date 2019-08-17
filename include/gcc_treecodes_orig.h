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

#define DEFTREECODE(SYM, NAME, TYPE, LENGTH) SYM,
#define END_OF_BASE_TREE_CODES LAST_AND_UNUSED_TREE_CODE,
enum tree_code {
#include "all-tree.def"
MAX_TREE_CODES
};
#undef DEFTREECODE
#undef END_OF_BASE_TREE_CODES

#define DEFTREECODE(SYM, NAME, TYPE, LENGTH) TYPE,
#define END_OF_BASE_TREE_CODES tcc_exceptional,
static const enum tree_code_class tree_code_type[] = {
#include "all-tree.def"
};
#undef DEFTREECODE
#undef END_OF_BASE_TREE_CODES

#define DEFTREECODE(SYM, NAME, TYPE, LENGTH) LENGTH,
#define END_OF_BASE_TREE_CODES 0,
static const unsigned char tree_code_length[] = {
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

#endif /* end of include guard: TREE_CODES_H_3GN2AKZB */
