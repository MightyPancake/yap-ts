#include "parser.h"
#include "ts_yap.h"
#include "types.h"

declare_map_for(cached_source);

yap_ctx* yap_ctx_init_ts_parser(yap_ctx* ctx){
    ctx->parser_ctx = yap_new_parser(ctx);
    return ctx;
}

yap_parser* yap_new_parser(yap_ctx* ctx){
    yap_log("Creating new parser");
    TSParser *ts_parser = ts_parser_new();
    ts_parser_set_language(ts_parser, tree_sitter_yap());
    yap_parser* parser = mem_one_cpy(((yap_parser){
         .ctx=ctx,
         .parser=ts_parser,
         .tree=NULL,
         .source_nodes_cache=new_cached_source_map(),
     }));
    return parser;
}

TSNode yap_parser_root(yap_parser* ps){
    return ts_tree_root_node(ps->tree);
}

void yap_free_parser(yap_parser* ps){
    ts_tree_delete(ps->tree);
    ts_parser_delete(ps->parser);
    if (ps->source_nodes_cache) {
        hashmap_free(ps->source_nodes_cache);
    }
    free(ps);
}

void yap_cache_source_node(yap_parser* parser, const char* absolute_path, yap_source_node* node){
    if (!parser || !parser->source_nodes_cache || !absolute_path || !node) return;
    yap_cached_source entry = {
        .name = (char*)absolute_path,
        .node = node,
    };
    hashmap_set(parser->source_nodes_cache, &entry);
}

static yap_source_node* yap_get_cached_source_node(yap_parser* parser, const char* absolute_path){
    if (!parser || !parser->source_nodes_cache || !absolute_path) return NULL;
    yap_cached_source key = { .name = (char*)absolute_path, .node = NULL };
    const yap_cached_source* found = hashmap_get(parser->source_nodes_cache, &key);
    return found ? found->node : NULL;
}

void yap_parse_file(yap_ctx* ctx, char* path, char* absolute_path, yap_loc import_loc){
    yap_parser* parser = ctx->parser_ctx;
     if (!parser) {
        yap_log("Error: No parser context found in ctx");
        return;
    }
    yap_source* parent = yap_ctx_top_source(ctx);
    yap_source* src = yap_ctx_new_file_source(ctx, parent, path, absolute_path);
    if (!src){
        yap_log("Error: Failed to create source for file '%s'", path);
        return;
    }
    src->import_loc = import_loc;
    yap_ctx_push_source(ctx, src);

    // If absolute_path has been parsed before, skip reparsing and reuse the cached source node
    yap_source_node* cached = yap_get_cached_source_node(parser, absolute_path);
    if (cached){
        yap_log("Reusing cached source node for '%s'", absolute_path);
        src->source_node = cached;
    } else {
        yap_parse_source(src);
        yap_cache_source_node(parser, absolute_path, src->source_node);
    }

    yap_ctx_pop_source(ctx);
}

void yap_parser_print_tree(yap_parser* ps){
    yap_log("Printing source tree");
    TSNode root = ts_tree_root_node(ps->tree);
    yap_print_tree(root, 0);
}