#ifndef YAP_PARSE_H
#define YAP_PARSE_H

#include <ts_yap.h>

// Top level parsing functions
void yap_parse_top_level_declaration(yap_source* src, TSNode node);
void yap_parse_top_level_func_decl(yap_source *src, TSNode node);

// Main parsing functions
yap_ctx* yap_parse(yap_ctx* ctx, yap_args args);
void yap_parse_source_file(yap_source* src, TSNode node);
// Declarations
yap_decl yap_parse_decl(yap_source* src, TSNode node);
yap_decl yap_parse_fn_decl(yap_source* src, TSNode node);
yap_decl yap_parse_type_declaration(yap_source* src, TSNode node);
yap_decl yap_parse_struct_declaration(yap_source* src, TSNode node);
yap_decl yap_parse_enum_declaration(yap_source* src, TSNode node);
yap_decl yap_parse_union_declaration(yap_source* src, TSNode node);

// Block
yap_block yap_parse_block(yap_source* src, TSNode node);

yap_assignment yap_parse_assignment(yap_source* src, TSNode node);
//statement
yap_statement yap_parse_statement(yap_source* src, TSNode node);

// yap_statement
//yap_parse_macro_statement(yap_source* src, TSNode node);
yap_statement yap_parse_empty_statement(yap_source* src, TSNode node);
yap_statement yap_parse_expr_statement(yap_source* src, TSNode node);
yap_statement yap_parse_if_statement(yap_source* src, TSNode node);
yap_statement yap_parse_if_else_statement(yap_source* src, TSNode node);
yap_statement yap_parse_var_decl(yap_source* src, TSNode node);
yap_statement yap_parse_return_statement(yap_source* src, TSNode node);
yap_statement yap_parse_while_loop(yap_source* src, TSNode node);
yap_statement yap_parse_for_loop(yap_source* src, TSNode node);
yap_statement yap_parse_break_statement(yap_source* src, TSNode node);
yap_statement yap_parse_continue_statement(yap_source* src, TSNode node);
yap_statement yap_parse_block_statement(yap_source* src, TSNode node);

//expr
yap_expr yap_parse_expr(yap_source* src, TSNode node);
yap_expr yap_parse_literal(yap_source* src, TSNode node);
yap_expr yap_parse_bin_expr(yap_source* src, TSNode node);
yap_expr yap_parse_var_access(yap_source* src, TSNode node);
yap_expr yap_parse_func_call(yap_source* src, TSNode node);
yap_expr yap_parse_cast_expr(yap_source* src, TSNode node);
yap_expr yap_parse_at_op(yap_source* src, TSNode node);
yap_expr yap_parse_paren_expr(yap_source* src, TSNode node);
yap_expr yap_parse_incr_expr(yap_source* src, TSNode node);
yap_expr yap_parse_ternary_expr(yap_source* src, TSNode node);

darr(yap_func_arg) yap_parse_fn_args(yap_source* src, TSNode node);
yap_func_arg yap_parse_fn_arg(yap_source* src, TSNode node);

//Types
yap_type_id yap_parse_type(yap_source* src, TSNode node);
yap_type_id yap_parse_const_type(yap_source* src, TSNode node);
yap_type_id yap_parse_paren_type(yap_source* src, TSNode node);
yap_type_id yap_parse_pointer_type(yap_source* src, TSNode node);
yap_type_id yap_parse_function_type(yap_source* src, TSNode node);
// Anonymous types
yap_type_id yap_parse_anon_struct_type(yap_source* src, TSNode node);
yap_type_id yap_parse_anon_union_type(yap_source* src, TSNode node);
yap_type_id yap_parse_anon_enum_type(yap_source* src, TSNode node);

//Other
yap_struct_field yap_parse_struct_field(yap_source* src, TSNode node);
yap_enum_variant yap_parse_enum_variant(yap_source* src, TSNode node);
yap_struct_field yap_parse_union_variant(yap_source* src, TSNode node);
yap_type_id yap_parse_type_annotation(yap_source* src, TSNode node);
darr(yap_struct_field) yap_parse_struct_fields(yap_source* src, TSNode fields_node);
darr(yap_struct_field) yap_parse_union_variants(yap_source* src, TSNode variants_node);
darr(yap_enum_variant) yap_parse_enum_variants(yap_source* src, TSNode variants_node);

//Misc
yap_type yap_empty_type(yap_type_kind kind);

#endif //YAP_PARSE_H
