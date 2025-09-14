#include <stdio.h>
#include <string.h>
#include "ts_yap.h"

void yap_print_tree(TSNode node, int depth){
    for(int i=0;i<depth;i++) printf("  ");
    const char* str = ts_node_type(node);
    printf("%s%s"aesc_reset"\n", ts_node_is_missing(node) ? "MISSING: "aesc_red : "", str);
    for_ts_children(node, n){
        yap_print_tree(n, depth+1);
    }
}

