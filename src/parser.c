#include "ts_yap.h"

yap_parser* yap_new_parser(){
    yap_log("Creating new parser");
    TSParser *ts_parser = ts_parser_new();
    ts_parser_set_language(ts_parser, tree_sitter_yap());
    yap_parser* parser = mem_one_cpy(((yap_parser){
         .state=yap_new_state(),
         .source_stack=darr_new(int, 64),
         .parser=ts_parser,
         .tree=NULL,
     }));
    return parser;
}

TSNode yap_parser_root(yap_parser* ps){
    return ts_tree_root_node(ps->tree);
}

void yap_parser_parse_file(yap_parser* ps, char* src_path){
    yap_parser_open_file(ps, src_path);
    yap_parser_parse(ps);
}

void yap_free_parser(yap_parser* ps){
    darr_free(ps->source_stack);

    ts_tree_delete(ps->tree);
    ts_parser_delete(ps->parser);

    free(ps);
}

void yap_parser_open_file(yap_parser* ps, char* path){
    yap_log("Opening file '%s'", path);
    char *content = NULL;
    size_t size = read_file_to_string(path, &content);
    yap_parser_push_source(ps, ((yap_source){
        .path=strus_copy(path),
        .sz=size,
        .content=content
    }));
}

void yap_parser_parse(yap_parser* ps){
    yap_source* src = yap_parser_top_source(ps);
    yap_log("Parsing source:\n%s", src->content);
    ps->tree = ts_parser_parse_string(ps->parser, NULL, src->content, src->sz);
    TSNode root = ts_tree_root_node(ps->tree);
    yap_parse_source_file(yap_parser_top_source(ps), root);
}

yap_source* yap_parser_top_source(yap_parser* ps){
    return &darr_at(yap_source, ps->state->sources, darr_last(int, ps->source_stack));
}

void yap_parser_push_source(yap_parser* ps, yap_source src){
    yap_state_push_source(ps->state, src);
    darr_push(int, ps->source_stack, darr_len(ps->state->sources)-1);
}

void yap_parser_print_tree(yap_parser* ps){
    yap_log("Printing source tree");
    TSNode root = ts_tree_root_node(ps->tree);
    yap_print_tree(root, 0);
}

