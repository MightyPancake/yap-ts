#ifndef YAP_NODE_H
#define YAP_NODE_H

#include "tree_sitter/api.h"

#define yap_node_type(NODE) const char* NODE##_type = ts_node_type(NODE)
#define yap_node_start(NODE) uint32_t NODE##_start = ts_node_start_byte(NODE)
#define yap_node_end(NODE) uint32_t NODE##_end = ts_node_end_byte(NODE)

#define yap_node_lim(NODE) yap_node_start(NODE); yap_node_end(NODE)

#define yap_node_start_point(NODE) TSPoint NODE##_start_point = ts_node_start_point(NODE)
#define yap_node_end_point(NODE) TSPoint NODE##_end_point = ts_node_end_point(NODE)

#define yap_node_point(NODE) yap_node_start_point(NODE); yap_node_end_point(NODE)

#define yap_common_parse(NODE) \
  yap_node_type(NODE); \
  yap_node_start(NODE); \
  yap_node_end(NODE)

#define yap_node_val_start(SRC, NODE) ((SRC)->content + ts_node_start_byte(NODE)) 

//These require to be freed after use
#define yap_node_get_val(SRC, NODE) (strndup(yap_node_val_start(SRC, NODE), ts_node_end_byte(NODE) - ts_node_start_byte(NODE)))
#define yap_node_val(NODE) char* NODE##_val = yap_node_get_val(src, NODE);
#define yap_node_str(NODE) char* NODE##_str = ts_node_string(NODE)

#define for_ts_children(N, C) for(TSNode C=ts_node_child(N, 0); !ts_node_is_null(C); C=ts_node_next_sibling(C))
#define yap_node_get_field(NODE, NAME) (ts_node_child_by_field_name(NODE, NAME, strlen(NAME)))
#define yap_node_field_var(VAR_NAME, NODE, NAME) TSNode VAR_NAME = yap_node_get_field(NODE, NAME)
#define yap_node_field_var_check(VAR_NAME, NODE, NAME, RET_TYP, MSG) yap_node_field_var(VAR_NAME, NODE, NAME); \
if (ts_node_is_null(VAR_NAME)) return yap_error_result(RET_TYP, MSG)

#define yap_node_field_by_name_var(NODE, NAME) yap_node_field_var(NAME##_node, NODE, #NAME)
#define yap_node_field_by_name_var_check(NODE, NAME, RET_TYP, MSG) yap_node_field_var_check(NAME##_node, NODE, #NAME, RET_TYP, MSG)

#define ts_node_error_or_null(N) (ts_node_is_error(N) || ts_node_is_null(N))

void yap_print_tree(TSNode root, int depth);

#endif //YAP_NODE_H
