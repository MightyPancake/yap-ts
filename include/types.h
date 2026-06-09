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
}yap_parser;

kenobi_new_struct(yap_cached_source,
    char* name;              //key = absolute path (hashed/compared by declare_map_for)
    yap_source_node* node;   //cached parsed source node
);

#endif //TS_YAP_TYPES_H
