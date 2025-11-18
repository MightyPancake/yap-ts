#ifndef TS_YAP_H
#define TS_YAP_H

#include "utils/utils.h"
#define YAP_LOG_TAG "YAP-TS"
#define YAP_LOG_TAG_COLOR aesc_magenta
#define YAP_LOG_MSG_COLOR aesc_white

#include "tree_sitter/api.h"
const TSLanguage *tree_sitter_yap(void);

#include "yap/all.h"
//TS related
#include "tree_sitter/api.h"
#include "node.h"

//Rest
#include "log.h"
#include "types.h"
#include "parse.h"
#include "parser.h"
#include "error.h"

#endif //TS_YAP_H
