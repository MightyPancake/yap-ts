#ifndef YAP_PARSE_H
#define YAP_PARSE_H

#include <ts_yap.h>

yap_ctx* yap_parse(yap_args args);
yap_source_code yap_parse_source_file(yap_source* src, TSNode node);
yap_def yap_parse_decl(yap_source* src, TSNode node);
yap_def yap_parse_fn_def(yap_source* src, TSNode node);
yap_block yap_parse_block(yap_source* src, TSNode node);
yap_statement yap_parse_statement(yap_source* src, TSNode node);
yap_statement yap_parse_expr_statement(yap_source* src, TSNode node);
yap_expr yap_parse_expr(yap_source* src, TSNode node);
yap_expr yap_parse_literal(yap_source* src, TSNode node);
yap_expr yap_parse_bin_expr(yap_source* src, TSNode node);
yap_assignment yap_parse_assignment(yap_source* src, TSNode node);

#endif //YAP_PARSE_H
