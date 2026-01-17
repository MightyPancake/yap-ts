#include "parser.h"
#include "ts_yap.h"
#include "types.h"

yap_parser* yap_new_parser(){
    yap_log("Creating new parser");
    TSParser *ts_parser = ts_parser_new();
    ts_parser_set_language(ts_parser, tree_sitter_yap());
    yap_parser* parser = mem_one_cpy(((yap_parser){
         .ctx=yap_ctx_new(),
         .source_stack=darr_new(int, 64),
         .parser=ts_parser,
         .tree=NULL,
     }));
    return parser;
}

size_t yap_read_file_to_string(const char *path, char **out) {
    *out = NULL;  // default to NULL in case of failure

    FILE *f = fopen(path, "rb");
    if (!f) {
        return 0; // failed to open
    }

    // Seek to end to determine size
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return 0;
    }
    long size = ftell(f);
    if (size < 0) {
        fclose(f);
        return 0;
    }
    rewind(f);

    // Allocate buffer (+1 for null terminator)
    char *buffer = malloc((size_t)size + 1);
    if (!buffer) {
        fclose(f);
        return 0;
    }

    // Read file contents
    size_t read_size = fread(buffer, 1, (size_t)size, f);
    fclose(f);

    if (read_size != (size_t)size) {
        free(buffer);
        return 0;
    }

    buffer[size] = '\0'; // null terminate
    *out = buffer;
    return (size_t)size;
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
    size_t size = yap_read_file_to_string(path, &content);
    yap_parser_push_source(ps, ((yap_source){
        .parent = NULL,
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
    yap_parser_print_tree(ps);
    //TODO: This should append to state/parser?
    //TODO: Uncomment to actually parse. This is commented to not display errors
    yap_source_code src_c = yap_parse_source_file(yap_parser_top_source(ps), root);
    darr_push(yap_source_code, ps->ctx->source_codes, src_c);
}

yap_source* yap_parser_top_source(yap_parser* ps){
    return &darr_at(yap_source, ps->ctx->sources, darr_last(int, ps->source_stack));
}

void yap_parser_push_source(yap_parser* ps, yap_source src){
    yap_ctx_push_source(ps->ctx, src);
    darr_push(int, ps->source_stack, darr_len(ps->ctx->sources)-1);
}

void yap_parser_print_tree(yap_parser* ps){
    yap_log("Printing source tree");
    TSNode root = ts_tree_root_node(ps->tree);
    yap_print_tree(root, 0);
}

