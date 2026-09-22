#ifndef TS_YAP_TYPES_H
#define TS_YAP_TYPES_H

kenobi_new_struct(yap_module_pin,
    char* name;
    yap_version version;
);

typedef struct yap_parser{
  //Context pointer
  yap_ctx* ctx;
  //Tree sitter stuff
  TSParser* parser;
  TSTree* tree;
  //Parsed source nodes cache. Key is:
  //files - Absolute path
  map source_nodes_cache;
  //Absolute paths currently being parsed. A file only reaches source_nodes_cache
  //once its parse finishes, so this is what catches one that imports itself back.
  darr(char*) parsing_stack;
  //Versions chosen by the solver, consulted by module lookup before latest-wins.
  darr(yap_module_pin) pins;
}yap_parser;

kenobi_new_struct(yap_cached_source,
    char* name;              //key = absolute path (hashed/compared by declare_map_for)
    yap_source_node* node;   //cached parsed source node
);

#endif //TS_YAP_TYPES_H
