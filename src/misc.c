#include <stdio.h>
#include <string.h>
#include "ts_yap.h"

void yap_print_tree(TSNode node, int depth){
    for(int i=0;i<depth;i++) printf("  ");
    const char* str = ts_node_type(node);
    printf("%s %s %s %s %s"aesc_reset"\n",
        str,
        ts_node_is_missing(node) ? aesc_red"missing"aesc_reset : "",
        ts_node_is_error(node) ? aesc_red"error"aesc_reset : "",
        ts_node_has_error(node) ? aesc_red"has error"aesc_reset : "",
        ts_node_is_null(node) ? aesc_red"null"aesc_reset : ""
        );
    for_ts_children(node, n){
        yap_print_tree(n, depth+1);
    }
}

