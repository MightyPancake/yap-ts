#ifndef YAP_PARSE_H
#define YAP_PARSE_H

#include "yap/types.h"
#include "tree_sitter/api.h"

//Source
yap_ctx* yap_parse(yap_ctx* ctx, yap_args args);
void yap_parse_source(yap_source* src);

//Misc parse results
yap_identifier_node yap_parse_identifier(yap_source* src, TSNode node);
yap_type_node yap_parse_type_node(yap_source* src, TSNode type_node);
darr(yap_func_arg_node) yap_parse_func_args(yap_source* src, TSNode node);
yap_func_arg_node yap_parse_func_arg(yap_source* src, TSNode node);

//Expressions
yap_expr_node yap_parse_expr(yap_source* src, TSNode node);
//Expressions - literals
yap_literal_node yap_parse_literal(yap_source* src, TSNode node);
yap_literal_node yap_parse_num_literal(yap_source* src, TSNode node);
yap_literal_node yap_parse_string_literal(yap_source* src, TSNode node);
yap_literal_node yap_parse_bool_literal(yap_source* src, TSNode node);
yap_literal_node yap_parse_null_literal(yap_source* src, TSNode node);
yap_literal_node yap_parse_byte_literal(yap_source* src, TSNode node);
yap_literal_node yap_parse_blob_literal(yap_source* src, TSNode node);

//Statements
yap_statement_node yap_parse_statement(yap_source* src, TSNode node);
yap_block_node yap_parse_block(yap_source* src, TSNode node);
yap_var_decl_node yap_parse_var_decl(yap_source* src, TSNode node);

//Declarations
yap_decl_node yap_parse_decl(yap_source* src, TSNode node);
yap_decl_node yap_parse_func_decl(yap_source* src, TSNode node);
yap_decl_node yap_parse_type_declaration(yap_source* src, TSNode node);
yap_decl_node yap_parse_struct_decl(yap_source* src, TSNode node);
yap_decl_node yap_parse_enum_decl(yap_source* src, TSNode node);
yap_decl_node yap_parse_union_decl(yap_source* src, TSNode node);
yap_decl_node yap_parse_module_import_decl(yap_source* src, TSNode node);
yap_decl_node yap_parse_file_import_decl(yap_source* src, TSNode node);
yap_decl_node yap_parse_module_decl(yap_source* src, TSNode node);

//Other
bool yap_identifier_node_is_valid(yap_identifier_node node);

#endif //YAP_PARSE_H