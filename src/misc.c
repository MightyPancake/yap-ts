#include <stdio.h>
#include <string.h>
#include "ts_yap.h"

void yap_print_tree(TSNode node, int depth){
    for(int i=0;i<depth;i++) printf("  ");
    const char* str = ts_node_type(node);
    printf("%s\n", str);
    for_ts_children(node, n){
        yap_print_tree(n, depth+1);
    }
}

