#ifndef YAP_PARSER_H
#define YAP_PARSER_H

//Basic
yap_parser* yap_new_parser();
void yap_free_parser(yap_parser* ps);

//Sources
void yap_parser_push_source(yap_parser* ps, yap_source src);
yap_source yap_parser_pop_source(yap_parser* ps);

void yap_parser_open_file(yap_parser* ps, char* path);
yap_source* yap_parser_top_source(yap_parser* ps);

//Parsing
void yap_parser_parse(yap_parser* ps);
void yap_parse_source_file(yap_source* src, TSNode node);

//Misc
TSNode yap_parser_root(yap_parser* ps);
void yap_parser_parse_file(yap_parser* ps, char* src_path);
void yap_parser_print_tree(yap_parser* ps);

#endif //YAP_PARSER_H
