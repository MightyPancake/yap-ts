#include "ts_yap.h"

yap_state* yap_parse(yap_args args){
    yap_parser* parser = yap_new_parser();
    if (darr_empty(args.extra)){
        printf("No sources!\n");
        exit(1);
    }
    yap_parser_parse_file(parser, darr_first(char*, args.extra));
    yap_parser_print_tree(parser);
    yap_state* ret = parser->state;
    yap_free_parser(parser);
    return ret;
}

void yap_parse_source_file(yap_source* src, TSNode node){
    const char* typ = ts_node_type(node);
    darr defs = darr_new(yap_def, 64);
    if (strus_eq(typ, "source_file")){
     for_ts_children(node, n){
         darr_push(yap_def, defs, yap_parse_def(src, n));
     }
    }else{
        yap_log("Error! Not valid source");
    }
    darr_free(defs);
}

yap_def yap_parse_def(yap_source *src, TSNode node){
    const char* typ = ts_node_type(node);
    if (ts_node_has_error(node)){
        printf(aesc_red"ERROR!\n"aesc_reset);
    }
    yap_log("Parsing definition: %s", typ);
    strus_switch(typ, "ERROR"){
        return (yap_def){.kind=yap_def_kind_error};
    }strus_case(typ, "function_definition"){
        return yap_parse_fn_def(src, node);
    }else{
        //error out probably?
        return (yap_def){};
    }
}

yap_def yap_parse_fn_def(yap_source *src, TSNode node){
    TSNode name_node = ts_node_child_by_field_name(node, "name", strlen("name"));
    yap_node_lim(name_node);
    yap_node_str(name_node);
    yap_node_val(name_node);
    yap_node_start_point(node);
    char* pos_str = yap_pos_string(*src, node_start_point.row, node_start_point.column);
    yap_log("Parsing function\n\t\t%s\n\t\tat %s", name_node_val, pos_str);
    free(pos_str);
    const char* typ = ts_node_type(node);
    yap_log("fn def type: %s", typ);
    if (!ts_node_is_null(name_node)){
        yap_log("node: %s", name_node_str);
    }else{
        yap_log("No function name!");
    }
    free(name_node_str);
    free(name_node_val);
    return (yap_def){};
}

