#ifndef YAP_TS_ERROR_H
#define YAP_TS_ERROR_H
#include <stdarg.h>
#include "yap/all.h"

#define yap_ts_error_result_node(T, MSG, SRC, NODE) yap_error_range_result(T, MSG, SRC, yap_node_get_range(NODE))

yap_code_pos yap_node_get_start_point(TSNode node);
yap_code_pos yap_node_get_end_point(TSNode node);
yap_code_range yap_node_get_range(TSNode node);

yap_error yap_node_error(yap_source* src, TSNode node, char* msg);

void yap_print_error(yap_error err);

#endif //YAP_TS_ERROR
