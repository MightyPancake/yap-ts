#ifndef YAP_PARSER_H
#define YAP_PARSER_H

#include "ts_yap.h"

//Basic
yap_ctx* yap_ctx_init_ts_parser(yap_ctx* ctx);
yap_parser* yap_new_parser(yap_ctx* ctx);
void yap_free_parser(yap_parser* ps);

void yap_parser_open_file(yap_parser* ps, char* path);

//Parsing
void yap_parse_file(yap_ctx* ctx, char* path, char* identity);

//Cache
void yap_cache_source_node(yap_parser* parser, const char* absolute_path, yap_source_node* node);

//Misc
size_t yap_read_file_to_string(const char *path, char **out);
TSNode yap_parser_root(yap_parser* ps);
void yap_parser_parse_file(yap_parser* ps, char* src_path);
void yap_parser_print_tree(yap_parser* ps);

#endif //YAP_PARSER_H
