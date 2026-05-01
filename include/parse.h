#ifndef YAP_PARSE_H
#define YAP_PARSE_H

#include <ts_yap.h>

// Top level parsing functions
void yap_parse_top_level_declaration(yap_source* src, TSNode node);
void yap_parse_top_level_func_decl(yap_source *src, TSNode node);

// Main parsing functions
yap_ctx* yap_parse(yap_ctx* ctx, yap_args args);
yap_source_code yap_parse_source_file(yap_source* src, TSNode node);
// Declarations
yap_decl yap_parse_decl(yap_source* src, TSNode node);
yap_decl yap_parse_fn_decl(yap_source* src, TSNode node);

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

darr(yap_func_arg) yap_parse_fn_args(yap_source* src, TSNode node);
yap_func_arg yap_parse_fn_arg(yap_source* src, TSNode node);

yap_type_id yap_parse_type(yap_source* src, TSNode node);
yap_type_id yap_parse_pointer_type(yap_source* src, TSNode node);
yap_type_id yap_parse_function_type(yap_source* src, TSNode node);

#endif //YAP_PARSE_H
