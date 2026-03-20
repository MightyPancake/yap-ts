#ifndef YAP_NODE_H
#define YAP_NODE_H

#include "tree_sitter/api.h"
#include <stddef.h>
#include <string.h>

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
#define yap_node_get_val_ctx(SRC, NODE) ({ \
  uint32_t _yap_node_start = ts_node_start_byte((NODE)); \
  uint32_t _yap_node_end = ts_node_end_byte((NODE)); \
  size_t _yap_node_len = (size_t)(_yap_node_end - _yap_node_start); \
  yap_ctx* _yap_ctx = (yap_ctx*)((SRC)->ctx); \
  char* _yap_node_out = _yap_ctx ? (char*)quake_alloc(&_yap_ctx->arena, _yap_node_len + 1) : NULL; \
  if (_yap_node_out) { \
    memcpy(_yap_node_out, (SRC)->content + _yap_node_start, _yap_node_len); \
    _yap_node_out[_yap_node_len] = '\0'; \
  } \
  _yap_node_out; \
})
#define yap_node_val_ctx(NODE) char* NODE##_val = yap_node_get_val_ctx(src, NODE);
#define yap_node_str(NODE) char* NODE##_str = ts_node_string(NODE)

#define for_ts_children(N, C) for(TSNode C=ts_node_child(N, 0); !ts_node_is_null(C); C=ts_node_next_sibling(C))
// Only named nodes — skips anonymous nodes (punctuation, keywords, etc.)
#define for_ts_named_children(N, C) for(TSNode C=ts_node_named_child(N, 0); !ts_node_is_null(C); C=ts_node_next_named_sibling(C))

#define yap_node_get_field(NODE, NAME) (ts_node_child_by_field_name(NODE, NAME, strlen(NAME)))
#define yap_node_field_var(VAR_NAME, NODE, NAME) TSNode VAR_NAME = yap_node_get_field(NODE, NAME)

// Checks null only — does NOT push an error, caller is responsible
#define yap_node_field_var_check(VAR_NAME, NODE, NAME, RET_TYP, MSG) yap_node_field_var(VAR_NAME, NODE, NAME); \
if (ts_node_error_or_null(VAR_NAME)) return yap_error_result(RET_TYP, MSG)

// Checks null/error AND pushes to ctx->errors — use at the detection site
#define yap_node_field_var_check_push(VAR_NAME, NODE, NAME, RET_TYP, MSG, SRC) \
  yap_node_field_var(VAR_NAME, NODE, NAME); \
  if (ts_node_error_or_null(VAR_NAME)) { \
    yap_ctx_push_error((SRC)->ctx, yap_node_error(SRC, NODE, MSG)); \
    return yap_error_result(RET_TYP, MSG); \
  }

// Guard for the node passed into a parse function — catches already-poisoned input
#define yap_node_guard(NODE, RET_TYP, MSG, SRC) \
  if (ts_node_error_or_null(NODE)) { \
    yap_ctx_push_error((SRC)->ctx, yap_node_error(SRC, NODE, MSG)); \
    return yap_error_result(RET_TYP, MSG); \
  }

#define yap_node_field_by_name_var(NODE, NAME) yap_node_field_var(NAME##_node, NODE, #NAME)
#define yap_node_field_by_name_var_check(NODE, NAME, RET_TYP, MSG) yap_node_field_var_check(NAME##_node, NODE, #NAME, RET_TYP, MSG)
#define yap_node_field_by_name_var_check_push(NODE, NAME, RET_TYP, MSG, SRC) yap_node_field_var_check_push(NAME##_node, NODE, #NAME, RET_TYP, MSG, SRC)

#define ts_node_error_or_null(N) (ts_node_is_error(N) || ts_node_is_null(N))

void yap_print_tree(TSNode root, int depth);

#endif //YAP_NODE_H
