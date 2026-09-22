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
  //Module blocks already read. Resolution asks for the same manifest repeatedly while
  //walking importers, and each read is a full tree-sitter parse.
  map manifest_cache;
  //Versions chosen by the solver, consulted by module lookup before latest-wins.
  darr(yap_module_pin) pins;
  //Versions fixed by yap.lock. These outrank anything a manifest asks for, which is what
  //makes a locked build reproducible.
  darr(yap_module_pin) locked;
}yap_parser;

kenobi_new_struct(yap_cached_manifest,
    char* name;                 //key = absolute path
    yap_module_decl_node decl;
    bool found;                 //false when the file has no module block
);

kenobi_new_struct(yap_cached_source,
    char* name;              //key = absolute path (hashed/compared by declare_map_for)
    yap_source_node* node;   //cached parsed source node
);

#endif //TS_YAP_TYPES_H
