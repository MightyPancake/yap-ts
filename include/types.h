#ifndef TS_YAP_TYPES_H
#define TS_YAP_TYPES_H

typedef struct yap_parser{
  yap_state* state;
  //
  darr source_stack;
  //
  TSParser* parser;
  TSTree* tree;
}yap_parser;

#endif //TS_YAP_TYPES_H
