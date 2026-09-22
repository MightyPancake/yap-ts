#ifndef TS_YAP_TYPES_H
#define TS_YAP_TYPES_H

typedef struct yap_parser{
  //Context pointer
  yap_ctx* ctx;
  //Tree sitter stuff
  TSParser* parser;
  TSTree* tree;
  //Parsed source nodes cache. Key is:
  //files - Absolute path
  map source_nodes_cache;
  //Absolute paths currently being parsed, outermost first. A file only reaches
  //source_nodes_cache once its parse finishes, so this is what catches a file that
  //imports its way back to itself while still in flight.
  darr(char*) parsing_stack;
}yap_parser;

kenobi_new_struct(yap_cached_source,
    char* name;              //key = absolute path (hashed/compared by declare_map_for)
    yap_source_node* node;   //cached parsed source node
);

#endif //TS_YAP_TYPES_H
