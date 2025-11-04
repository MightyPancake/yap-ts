#include <tree_sitter/parser.h>

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif

#define LANGUAGE_VERSION 14
#define STATE_COUNT 35
#define LARGE_STATE_COUNT 2
#define SYMBOL_COUNT 29
#define ALIAS_COUNT 0
#define TOKEN_COUNT 15
#define EXTERNAL_TOKEN_COUNT 0
#define FIELD_COUNT 18
#define MAX_ALIAS_SEQUENCE_LENGTH 5
#define PRODUCTION_ID_COUNT 13

enum {
  anon_sym_fn = 1,
  anon_sym_LPAREN = 2,
  anon_sym_RPAREN = 3,
  anon_sym_LBRACE = 4,
  anon_sym_RBRACE = 5,
  anon_sym_EQ = 6,
  anon_sym_PLUS = 7,
  anon_sym_DASH = 8,
  anon_sym_STAR = 9,
  anon_sym_SLASH = 10,
  anon_sym_PERCENT = 11,
  anon_sym_SEMI = 12,
  sym_num_literal = 13,
  sym_identifier = 14,
  sym_source = 15,
  sym__definition = 16,
  sym_function_definition = 17,
  sym_block = 18,
  sym_statement = 19,
  sym_bin_op = 20,
  sym_assignment = 21,
  sym_binary_operator = 22,
  sym_lvalue = 23,
  sym_expr_statement = 24,
  sym_literal = 25,
  sym_expr = 26,
  aux_sym_source_repeat1 = 27,
  aux_sym_block_repeat1 = 28,
};

static const char * const ts_symbol_names[] = {
  [ts_builtin_sym_end] = "end",
  [anon_sym_fn] = "fn",
  [anon_sym_LPAREN] = "(",
  [anon_sym_RPAREN] = ")",
  [anon_sym_LBRACE] = "{",
  [anon_sym_RBRACE] = "}",
  [anon_sym_EQ] = "=",
  [anon_sym_PLUS] = "+",
  [anon_sym_DASH] = "-",
  [anon_sym_STAR] = "*",
  [anon_sym_SLASH] = "/",
  [anon_sym_PERCENT] = "%",
  [anon_sym_SEMI] = ";",
  [sym_num_literal] = "num_literal",
  [sym_identifier] = "identifier",
  [sym_source] = "source",
  [sym__definition] = "_definition",
  [sym_function_definition] = "function_definition",
  [sym_block] = "block",
  [sym_statement] = "statement",
  [sym_bin_op] = "bin_op",
  [sym_assignment] = "assignment",
  [sym_binary_operator] = "binary_operator",
  [sym_lvalue] = "lvalue",
  [sym_expr_statement] = "expr_statement",
  [sym_literal] = "literal",
  [sym_expr] = "expr",
  [aux_sym_source_repeat1] = "source_repeat1",
  [aux_sym_block_repeat1] = "block_repeat1",
};

static const TSSymbol ts_symbol_map[] = {
  [ts_builtin_sym_end] = ts_builtin_sym_end,
  [anon_sym_fn] = anon_sym_fn,
  [anon_sym_LPAREN] = anon_sym_LPAREN,
  [anon_sym_RPAREN] = anon_sym_RPAREN,
  [anon_sym_LBRACE] = anon_sym_LBRACE,
  [anon_sym_RBRACE] = anon_sym_RBRACE,
  [anon_sym_EQ] = anon_sym_EQ,
  [anon_sym_PLUS] = anon_sym_PLUS,
  [anon_sym_DASH] = anon_sym_DASH,
  [anon_sym_STAR] = anon_sym_STAR,
  [anon_sym_SLASH] = anon_sym_SLASH,
  [anon_sym_PERCENT] = anon_sym_PERCENT,
  [anon_sym_SEMI] = anon_sym_SEMI,
  [sym_num_literal] = sym_num_literal,
  [sym_identifier] = sym_identifier,
  [sym_source] = sym_source,
  [sym__definition] = sym__definition,
  [sym_function_definition] = sym_function_definition,
  [sym_block] = sym_block,
  [sym_statement] = sym_statement,
  [sym_bin_op] = sym_bin_op,
  [sym_assignment] = sym_assignment,
  [sym_binary_operator] = sym_binary_operator,
  [sym_lvalue] = sym_lvalue,
  [sym_expr_statement] = sym_expr_statement,
  [sym_literal] = sym_literal,
  [sym_expr] = sym_expr,
  [aux_sym_source_repeat1] = aux_sym_source_repeat1,
  [aux_sym_block_repeat1] = aux_sym_block_repeat1,
};

static const TSSymbolMetadata ts_symbol_metadata[] = {
  [ts_builtin_sym_end] = {
    .visible = false,
    .named = true,
  },
  [anon_sym_fn] = {
    .visible = true,
    .named = false,
  },
  [anon_sym_LPAREN] = {
    .visible = true,
    .named = false,
  },
  [anon_sym_RPAREN] = {
    .visible = true,
    .named = false,
  },
  [anon_sym_LBRACE] = {
    .visible = true,
    .named = false,
  },
  [anon_sym_RBRACE] = {
    .visible = true,
    .named = false,
  },
  [anon_sym_EQ] = {
    .visible = true,
    .named = false,
  },
  [anon_sym_PLUS] = {
    .visible = true,
    .named = false,
  },
  [anon_sym_DASH] = {
    .visible = true,
    .named = false,
  },
  [anon_sym_STAR] = {
    .visible = true,
    .named = false,
  },
  [anon_sym_SLASH] = {
    .visible = true,
    .named = false,
  },
  [anon_sym_PERCENT] = {
    .visible = true,
    .named = false,
  },
  [anon_sym_SEMI] = {
    .visible = true,
    .named = false,
  },
  [sym_num_literal] = {
    .visible = true,
    .named = true,
  },
  [sym_identifier] = {
    .visible = true,
    .named = true,
  },
  [sym_source] = {
    .visible = true,
    .named = true,
  },
  [sym__definition] = {
    .visible = false,
    .named = true,
  },
  [sym_function_definition] = {
    .visible = true,
    .named = true,
  },
  [sym_block] = {
    .visible = true,
    .named = true,
  },
  [sym_statement] = {
    .visible = true,
    .named = true,
  },
  [sym_bin_op] = {
    .visible = true,
    .named = true,
  },
  [sym_assignment] = {
    .visible = true,
    .named = true,
  },
  [sym_binary_operator] = {
    .visible = true,
    .named = true,
  },
  [sym_lvalue] = {
    .visible = true,
    .named = true,
  },
  [sym_expr_statement] = {
    .visible = true,
    .named = true,
  },
  [sym_literal] = {
    .visible = true,
    .named = true,
  },
  [sym_expr] = {
    .visible = true,
    .named = true,
  },
  [aux_sym_source_repeat1] = {
    .visible = false,
    .named = false,
  },
  [aux_sym_block_repeat1] = {
    .visible = false,
    .named = false,
  },
};

enum {
  field_add = 1,
  field_assign = 2,
  field_body = 3,
  field_closing_bracket = 4,
  field_div = 5,
  field_expr = 6,
  field_fn = 7,
  field_left = 8,
  field_lvalue = 9,
  field_mod = 10,
  field_mul = 11,
  field_name = 12,
  field_opening_bracket = 13,
  field_operator = 14,
  field_right = 15,
  field_semicolon = 16,
  field_sub = 17,
  field_var = 18,
};

static const char * const ts_field_names[] = {
  [0] = NULL,
  [field_add] = "add",
  [field_assign] = "assign",
  [field_body] = "body",
  [field_closing_bracket] = "closing_bracket",
  [field_div] = "div",
  [field_expr] = "expr",
  [field_fn] = "fn",
  [field_left] = "left",
  [field_lvalue] = "lvalue",
  [field_mod] = "mod",
  [field_mul] = "mul",
  [field_name] = "name",
  [field_opening_bracket] = "opening_bracket",
  [field_operator] = "operator",
  [field_right] = "right",
  [field_semicolon] = "semicolon",
  [field_sub] = "sub",
  [field_var] = "var",
};

static const TSFieldMapSlice ts_field_map_slices[PRODUCTION_ID_COUNT] = {
  [1] = {.index = 0, .length = 3},
  [2] = {.index = 3, .length = 2},
  [3] = {.index = 5, .length = 1},
  [4] = {.index = 6, .length = 1},
  [5] = {.index = 7, .length = 1},
  [6] = {.index = 8, .length = 1},
  [7] = {.index = 9, .length = 1},
  [8] = {.index = 10, .length = 1},
  [9] = {.index = 11, .length = 2},
  [10] = {.index = 13, .length = 2},
  [11] = {.index = 15, .length = 3},
  [12] = {.index = 18, .length = 3},
};

static const TSFieldMapEntry ts_field_map_entries[] = {
  [0] =
    {field_body, 4},
    {field_fn, 0},
    {field_name, 1},
  [3] =
    {field_closing_bracket, 1},
    {field_opening_bracket, 0},
  [5] =
    {field_var, 0},
  [6] =
    {field_add, 0},
  [7] =
    {field_sub, 0},
  [8] =
    {field_mul, 0},
  [9] =
    {field_div, 0},
  [10] =
    {field_mod, 0},
  [11] =
    {field_expr, 0},
    {field_semicolon, 1},
  [13] =
    {field_closing_bracket, 2},
    {field_opening_bracket, 0},
  [15] =
    {field_assign, 1},
    {field_expr, 2},
    {field_lvalue, 0},
  [18] =
    {field_left, 0},
    {field_operator, 1},
    {field_right, 2},
};

static const TSSymbol ts_alias_sequences[PRODUCTION_ID_COUNT][MAX_ALIAS_SEQUENCE_LENGTH] = {
  [0] = {0},
};

static const uint16_t ts_non_terminal_alias_map[] = {
  0,
};

static const TSStateId ts_primary_state_ids[STATE_COUNT] = {
  [0] = 0,
  [1] = 1,
  [2] = 2,
  [3] = 3,
  [4] = 2,
  [5] = 3,
  [6] = 6,
  [7] = 7,
  [8] = 8,
  [9] = 9,
  [10] = 10,
  [11] = 11,
  [12] = 12,
  [13] = 13,
  [14] = 14,
  [15] = 15,
  [16] = 16,
  [17] = 17,
  [18] = 18,
  [19] = 19,
  [20] = 20,
  [21] = 21,
  [22] = 22,
  [23] = 23,
  [24] = 24,
  [25] = 25,
  [26] = 20,
  [27] = 19,
  [28] = 28,
  [29] = 29,
  [30] = 30,
  [31] = 31,
  [32] = 32,
  [33] = 33,
  [34] = 34,
};

static bool ts_lex(TSLexer *lexer, TSStateId state) {
  START_LEXER();
  eof = lexer->eof(lexer);
  switch (state) {
    case 0:
      if (eof) ADVANCE(12);
      if (lookahead == '%') ADVANCE(23);
      if (lookahead == '(') ADVANCE(14);
      if (lookahead == ')') ADVANCE(15);
      if (lookahead == '*') ADVANCE(21);
      if (lookahead == '+') ADVANCE(19);
      if (lookahead == '-') ADVANCE(20);
      if (lookahead == '/') SKIP(8)
      if (lookahead == ';') ADVANCE(24);
      if (lookahead == '=') ADVANCE(18);
      if (lookahead == 'f') ADVANCE(6);
      if (lookahead == '{') ADVANCE(16);
      if (lookahead == '}') ADVANCE(17);
      if (lookahead == '\t' ||
          lookahead == '\n' ||
          lookahead == '\r' ||
          lookahead == ' ') SKIP(0)
      if (('0' <= lookahead && lookahead <= '9')) ADVANCE(25);
      END_STATE();
    case 1:
      if (lookahead == '%') ADVANCE(23);
      if (lookahead == '*') ADVANCE(21);
      if (lookahead == '+') ADVANCE(19);
      if (lookahead == '-') ADVANCE(20);
      if (lookahead == '/') ADVANCE(22);
      if (lookahead == ';') ADVANCE(24);
      if (lookahead == '=') ADVANCE(18);
      if (lookahead == '\t' ||
          lookahead == '\n' ||
          lookahead == '\r' ||
          lookahead == ' ') SKIP(1)
      END_STATE();
    case 2:
      if (lookahead == '*') SKIP(4)
      if (lookahead == '/') SKIP(7)
      END_STATE();
    case 3:
      if (lookahead == '*') SKIP(3)
      if (lookahead == '/') SKIP(5)
      if (lookahead != 0) SKIP(4)
      END_STATE();
    case 4:
      if (lookahead == '*') SKIP(3)
      if (lookahead != 0) SKIP(4)
      END_STATE();
    case 5:
      if (lookahead == '/') SKIP(2)
      if (lookahead == '{') ADVANCE(16);
      if (lookahead == '}') ADVANCE(17);
      if (lookahead == '\t' ||
          lookahead == '\n' ||
          lookahead == '\r' ||
          lookahead == ' ') SKIP(5)
      if (('0' <= lookahead && lookahead <= '9')) ADVANCE(25);
      if (('A' <= lookahead && lookahead <= 'Z') ||
          lookahead == '_' ||
          ('a' <= lookahead && lookahead <= 'z')) ADVANCE(26);
      END_STATE();
    case 6:
      if (lookahead == 'n') ADVANCE(13);
      END_STATE();
    case 7:
      if (lookahead == '\n' ||
          lookahead == '\r') SKIP(5)
      if (lookahead != 0) SKIP(7)
      END_STATE();
    case 8:
      if (eof) ADVANCE(12);
      if (lookahead == '*') SKIP(10)
      if (lookahead == '/') SKIP(11)
      END_STATE();
    case 9:
      if (eof) ADVANCE(12);
      if (lookahead == '*') SKIP(9)
      if (lookahead == '/') SKIP(0)
      if (lookahead != 0) SKIP(10)
      END_STATE();
    case 10:
      if (eof) ADVANCE(12);
      if (lookahead == '*') SKIP(9)
      if (lookahead != 0) SKIP(10)
      END_STATE();
    case 11:
      if (eof) ADVANCE(12);
      if (lookahead == '\n' ||
          lookahead == '\r') SKIP(0)
      if (lookahead != 0) SKIP(11)
      END_STATE();
    case 12:
      ACCEPT_TOKEN(ts_builtin_sym_end);
      END_STATE();
    case 13:
      ACCEPT_TOKEN(anon_sym_fn);
      END_STATE();
    case 14:
      ACCEPT_TOKEN(anon_sym_LPAREN);
      END_STATE();
    case 15:
      ACCEPT_TOKEN(anon_sym_RPAREN);
      END_STATE();
    case 16:
      ACCEPT_TOKEN(anon_sym_LBRACE);
      END_STATE();
    case 17:
      ACCEPT_TOKEN(anon_sym_RBRACE);
      END_STATE();
    case 18:
      ACCEPT_TOKEN(anon_sym_EQ);
      END_STATE();
    case 19:
      ACCEPT_TOKEN(anon_sym_PLUS);
      END_STATE();
    case 20:
      ACCEPT_TOKEN(anon_sym_DASH);
      END_STATE();
    case 21:
      ACCEPT_TOKEN(anon_sym_STAR);
      END_STATE();
    case 22:
      ACCEPT_TOKEN(anon_sym_SLASH);
      END_STATE();
    case 23:
      ACCEPT_TOKEN(anon_sym_PERCENT);
      END_STATE();
    case 24:
      ACCEPT_TOKEN(anon_sym_SEMI);
      END_STATE();
    case 25:
      ACCEPT_TOKEN(sym_num_literal);
      if (('0' <= lookahead && lookahead <= '9')) ADVANCE(25);
      END_STATE();
    case 26:
      ACCEPT_TOKEN(sym_identifier);
      if (('0' <= lookahead && lookahead <= '9') ||
          ('A' <= lookahead && lookahead <= 'Z') ||
          lookahead == '_' ||
          ('a' <= lookahead && lookahead <= 'z')) ADVANCE(26);
      END_STATE();
    default:
      return false;
  }
}

static const TSLexMode ts_lex_modes[STATE_COUNT] = {
  [0] = {.lex_state = 0},
  [1] = {.lex_state = 0},
  [2] = {.lex_state = 5},
  [3] = {.lex_state = 5},
  [4] = {.lex_state = 5},
  [5] = {.lex_state = 5},
  [6] = {.lex_state = 5},
  [7] = {.lex_state = 5},
  [8] = {.lex_state = 5},
  [9] = {.lex_state = 1},
  [10] = {.lex_state = 1},
  [11] = {.lex_state = 1},
  [12] = {.lex_state = 1},
  [13] = {.lex_state = 1},
  [14] = {.lex_state = 1},
  [15] = {.lex_state = 0},
  [16] = {.lex_state = 0},
  [17] = {.lex_state = 5},
  [18] = {.lex_state = 5},
  [19] = {.lex_state = 5},
  [20] = {.lex_state = 5},
  [21] = {.lex_state = 5},
  [22] = {.lex_state = 5},
  [23] = {.lex_state = 5},
  [24] = {.lex_state = 5},
  [25] = {.lex_state = 5},
  [26] = {.lex_state = 0},
  [27] = {.lex_state = 0},
  [28] = {.lex_state = 0},
  [29] = {.lex_state = 0},
  [30] = {.lex_state = 5},
  [31] = {.lex_state = 0},
  [32] = {.lex_state = 0},
  [33] = {.lex_state = 0},
  [34] = {.lex_state = 0},
};

static const uint16_t ts_parse_table[LARGE_STATE_COUNT][SYMBOL_COUNT] = {
  [0] = {
    [ts_builtin_sym_end] = ACTIONS(1),
    [anon_sym_fn] = ACTIONS(1),
    [anon_sym_LPAREN] = ACTIONS(1),
    [anon_sym_RPAREN] = ACTIONS(1),
    [anon_sym_LBRACE] = ACTIONS(1),
    [anon_sym_RBRACE] = ACTIONS(1),
    [anon_sym_EQ] = ACTIONS(1),
    [anon_sym_PLUS] = ACTIONS(1),
    [anon_sym_DASH] = ACTIONS(1),
    [anon_sym_STAR] = ACTIONS(1),
    [anon_sym_PERCENT] = ACTIONS(1),
    [anon_sym_SEMI] = ACTIONS(1),
    [sym_num_literal] = ACTIONS(1),
  },
  [1] = {
    [sym_source] = STATE(34),
    [sym__definition] = STATE(15),
    [sym_function_definition] = STATE(15),
    [aux_sym_source_repeat1] = STATE(15),
    [ts_builtin_sym_end] = ACTIONS(3),
    [anon_sym_fn] = ACTIONS(5),
  },
};

static const uint16_t ts_small_parse_table[] = {
  [0] = 9,
    ACTIONS(7), 1,
      anon_sym_LBRACE,
    ACTIONS(9), 1,
      anon_sym_RBRACE,
    ACTIONS(11), 1,
      sym_num_literal,
    ACTIONS(13), 1,
      sym_identifier,
    STATE(9), 1,
      sym_expr,
    STATE(31), 1,
      sym_lvalue,
    STATE(6), 2,
      sym_statement,
      aux_sym_block_repeat1,
    STATE(17), 2,
      sym_block,
      sym_expr_statement,
    STATE(13), 3,
      sym_bin_op,
      sym_assignment,
      sym_literal,
  [32] = 9,
    ACTIONS(7), 1,
      anon_sym_LBRACE,
    ACTIONS(11), 1,
      sym_num_literal,
    ACTIONS(13), 1,
      sym_identifier,
    ACTIONS(15), 1,
      anon_sym_RBRACE,
    STATE(9), 1,
      sym_expr,
    STATE(31), 1,
      sym_lvalue,
    STATE(2), 2,
      sym_statement,
      aux_sym_block_repeat1,
    STATE(17), 2,
      sym_block,
      sym_expr_statement,
    STATE(13), 3,
      sym_bin_op,
      sym_assignment,
      sym_literal,
  [64] = 9,
    ACTIONS(7), 1,
      anon_sym_LBRACE,
    ACTIONS(11), 1,
      sym_num_literal,
    ACTIONS(13), 1,
      sym_identifier,
    ACTIONS(17), 1,
      anon_sym_RBRACE,
    STATE(9), 1,
      sym_expr,
    STATE(31), 1,
      sym_lvalue,
    STATE(6), 2,
      sym_statement,
      aux_sym_block_repeat1,
    STATE(17), 2,
      sym_block,
      sym_expr_statement,
    STATE(13), 3,
      sym_bin_op,
      sym_assignment,
      sym_literal,
  [96] = 9,
    ACTIONS(7), 1,
      anon_sym_LBRACE,
    ACTIONS(11), 1,
      sym_num_literal,
    ACTIONS(13), 1,
      sym_identifier,
    ACTIONS(19), 1,
      anon_sym_RBRACE,
    STATE(9), 1,
      sym_expr,
    STATE(31), 1,
      sym_lvalue,
    STATE(4), 2,
      sym_statement,
      aux_sym_block_repeat1,
    STATE(17), 2,
      sym_block,
      sym_expr_statement,
    STATE(13), 3,
      sym_bin_op,
      sym_assignment,
      sym_literal,
  [128] = 9,
    ACTIONS(21), 1,
      anon_sym_LBRACE,
    ACTIONS(24), 1,
      anon_sym_RBRACE,
    ACTIONS(26), 1,
      sym_num_literal,
    ACTIONS(29), 1,
      sym_identifier,
    STATE(9), 1,
      sym_expr,
    STATE(31), 1,
      sym_lvalue,
    STATE(6), 2,
      sym_statement,
      aux_sym_block_repeat1,
    STATE(17), 2,
      sym_block,
      sym_expr_statement,
    STATE(13), 3,
      sym_bin_op,
      sym_assignment,
      sym_literal,
  [160] = 5,
    ACTIONS(11), 1,
      sym_num_literal,
    ACTIONS(13), 1,
      sym_identifier,
    STATE(10), 1,
      sym_expr,
    STATE(31), 1,
      sym_lvalue,
    STATE(13), 3,
      sym_bin_op,
      sym_assignment,
      sym_literal,
  [178] = 5,
    ACTIONS(11), 1,
      sym_num_literal,
    ACTIONS(13), 1,
      sym_identifier,
    STATE(11), 1,
      sym_expr,
    STATE(31), 1,
      sym_lvalue,
    STATE(13), 3,
      sym_bin_op,
      sym_assignment,
      sym_literal,
  [196] = 7,
    ACTIONS(32), 1,
      anon_sym_PLUS,
    ACTIONS(34), 1,
      anon_sym_DASH,
    ACTIONS(36), 1,
      anon_sym_STAR,
    ACTIONS(38), 1,
      anon_sym_SLASH,
    ACTIONS(40), 1,
      anon_sym_PERCENT,
    ACTIONS(42), 1,
      anon_sym_SEMI,
    STATE(7), 1,
      sym_binary_operator,
  [218] = 3,
    ACTIONS(46), 1,
      anon_sym_SLASH,
    STATE(7), 1,
      sym_binary_operator,
    ACTIONS(44), 5,
      anon_sym_PLUS,
      anon_sym_DASH,
      anon_sym_STAR,
      anon_sym_PERCENT,
      anon_sym_SEMI,
  [232] = 7,
    ACTIONS(32), 1,
      anon_sym_PLUS,
    ACTIONS(34), 1,
      anon_sym_DASH,
    ACTIONS(36), 1,
      anon_sym_STAR,
    ACTIONS(38), 1,
      anon_sym_SLASH,
    ACTIONS(40), 1,
      anon_sym_PERCENT,
    ACTIONS(48), 1,
      anon_sym_SEMI,
    STATE(7), 1,
      sym_binary_operator,
  [254] = 3,
    ACTIONS(50), 1,
      anon_sym_EQ,
    ACTIONS(54), 1,
      anon_sym_SLASH,
    ACTIONS(52), 5,
      anon_sym_PLUS,
      anon_sym_DASH,
      anon_sym_STAR,
      anon_sym_PERCENT,
      anon_sym_SEMI,
  [268] = 2,
    ACTIONS(54), 1,
      anon_sym_SLASH,
    ACTIONS(52), 5,
      anon_sym_PLUS,
      anon_sym_DASH,
      anon_sym_STAR,
      anon_sym_PERCENT,
      anon_sym_SEMI,
  [279] = 2,
    ACTIONS(58), 1,
      anon_sym_SLASH,
    ACTIONS(56), 5,
      anon_sym_PLUS,
      anon_sym_DASH,
      anon_sym_STAR,
      anon_sym_PERCENT,
      anon_sym_SEMI,
  [290] = 3,
    ACTIONS(5), 1,
      anon_sym_fn,
    ACTIONS(60), 1,
      ts_builtin_sym_end,
    STATE(16), 3,
      sym__definition,
      sym_function_definition,
      aux_sym_source_repeat1,
  [302] = 3,
    ACTIONS(62), 1,
      ts_builtin_sym_end,
    ACTIONS(64), 1,
      anon_sym_fn,
    STATE(16), 3,
      sym__definition,
      sym_function_definition,
      aux_sym_source_repeat1,
  [314] = 1,
    ACTIONS(67), 4,
      anon_sym_LBRACE,
      anon_sym_RBRACE,
      sym_num_literal,
      sym_identifier,
  [321] = 1,
    ACTIONS(69), 4,
      anon_sym_LBRACE,
      anon_sym_RBRACE,
      sym_num_literal,
      sym_identifier,
  [328] = 1,
    ACTIONS(71), 4,
      anon_sym_LBRACE,
      anon_sym_RBRACE,
      sym_num_literal,
      sym_identifier,
  [335] = 1,
    ACTIONS(73), 4,
      anon_sym_LBRACE,
      anon_sym_RBRACE,
      sym_num_literal,
      sym_identifier,
  [342] = 1,
    ACTIONS(75), 2,
      sym_num_literal,
      sym_identifier,
  [347] = 1,
    ACTIONS(77), 2,
      sym_num_literal,
      sym_identifier,
  [352] = 1,
    ACTIONS(79), 2,
      sym_num_literal,
      sym_identifier,
  [357] = 1,
    ACTIONS(81), 2,
      sym_num_literal,
      sym_identifier,
  [362] = 1,
    ACTIONS(83), 2,
      sym_num_literal,
      sym_identifier,
  [367] = 1,
    ACTIONS(73), 2,
      ts_builtin_sym_end,
      anon_sym_fn,
  [372] = 1,
    ACTIONS(71), 2,
      ts_builtin_sym_end,
      anon_sym_fn,
  [377] = 1,
    ACTIONS(85), 2,
      ts_builtin_sym_end,
      anon_sym_fn,
  [382] = 2,
    ACTIONS(87), 1,
      anon_sym_LBRACE,
    STATE(28), 1,
      sym_block,
  [389] = 1,
    ACTIONS(89), 1,
      sym_identifier,
  [393] = 1,
    ACTIONS(91), 1,
      anon_sym_EQ,
  [397] = 1,
    ACTIONS(93), 1,
      anon_sym_RPAREN,
  [401] = 1,
    ACTIONS(95), 1,
      anon_sym_LPAREN,
  [405] = 1,
    ACTIONS(97), 1,
      ts_builtin_sym_end,
};

static const uint32_t ts_small_parse_table_map[] = {
  [SMALL_STATE(2)] = 0,
  [SMALL_STATE(3)] = 32,
  [SMALL_STATE(4)] = 64,
  [SMALL_STATE(5)] = 96,
  [SMALL_STATE(6)] = 128,
  [SMALL_STATE(7)] = 160,
  [SMALL_STATE(8)] = 178,
  [SMALL_STATE(9)] = 196,
  [SMALL_STATE(10)] = 218,
  [SMALL_STATE(11)] = 232,
  [SMALL_STATE(12)] = 254,
  [SMALL_STATE(13)] = 268,
  [SMALL_STATE(14)] = 279,
  [SMALL_STATE(15)] = 290,
  [SMALL_STATE(16)] = 302,
  [SMALL_STATE(17)] = 314,
  [SMALL_STATE(18)] = 321,
  [SMALL_STATE(19)] = 328,
  [SMALL_STATE(20)] = 335,
  [SMALL_STATE(21)] = 342,
  [SMALL_STATE(22)] = 347,
  [SMALL_STATE(23)] = 352,
  [SMALL_STATE(24)] = 357,
  [SMALL_STATE(25)] = 362,
  [SMALL_STATE(26)] = 367,
  [SMALL_STATE(27)] = 372,
  [SMALL_STATE(28)] = 377,
  [SMALL_STATE(29)] = 382,
  [SMALL_STATE(30)] = 389,
  [SMALL_STATE(31)] = 393,
  [SMALL_STATE(32)] = 397,
  [SMALL_STATE(33)] = 401,
  [SMALL_STATE(34)] = 405,
};

static const TSParseActionEntry ts_parse_actions[] = {
  [0] = {.entry = {.count = 0, .reusable = false}},
  [1] = {.entry = {.count = 1, .reusable = false}}, RECOVER(),
  [3] = {.entry = {.count = 1, .reusable = true}}, REDUCE(sym_source, 0),
  [5] = {.entry = {.count = 1, .reusable = true}}, SHIFT(30),
  [7] = {.entry = {.count = 1, .reusable = true}}, SHIFT(3),
  [9] = {.entry = {.count = 1, .reusable = true}}, SHIFT(20),
  [11] = {.entry = {.count = 1, .reusable = true}}, SHIFT(14),
  [13] = {.entry = {.count = 1, .reusable = true}}, SHIFT(12),
  [15] = {.entry = {.count = 1, .reusable = true}}, SHIFT(19),
  [17] = {.entry = {.count = 1, .reusable = true}}, SHIFT(26),
  [19] = {.entry = {.count = 1, .reusable = true}}, SHIFT(27),
  [21] = {.entry = {.count = 2, .reusable = true}}, REDUCE(aux_sym_block_repeat1, 2), SHIFT_REPEAT(3),
  [24] = {.entry = {.count = 1, .reusable = true}}, REDUCE(aux_sym_block_repeat1, 2),
  [26] = {.entry = {.count = 2, .reusable = true}}, REDUCE(aux_sym_block_repeat1, 2), SHIFT_REPEAT(14),
  [29] = {.entry = {.count = 2, .reusable = true}}, REDUCE(aux_sym_block_repeat1, 2), SHIFT_REPEAT(12),
  [32] = {.entry = {.count = 1, .reusable = false}}, SHIFT(24),
  [34] = {.entry = {.count = 1, .reusable = false}}, SHIFT(25),
  [36] = {.entry = {.count = 1, .reusable = false}}, SHIFT(22),
  [38] = {.entry = {.count = 1, .reusable = true}}, SHIFT(23),
  [40] = {.entry = {.count = 1, .reusable = false}}, SHIFT(21),
  [42] = {.entry = {.count = 1, .reusable = false}}, SHIFT(18),
  [44] = {.entry = {.count = 1, .reusable = false}}, REDUCE(sym_bin_op, 3, .production_id = 12),
  [46] = {.entry = {.count = 1, .reusable = true}}, REDUCE(sym_bin_op, 3, .production_id = 12),
  [48] = {.entry = {.count = 1, .reusable = false}}, REDUCE(sym_assignment, 3, .production_id = 11),
  [50] = {.entry = {.count = 1, .reusable = false}}, REDUCE(sym_lvalue, 1, .production_id = 3),
  [52] = {.entry = {.count = 1, .reusable = false}}, REDUCE(sym_expr, 1),
  [54] = {.entry = {.count = 1, .reusable = true}}, REDUCE(sym_expr, 1),
  [56] = {.entry = {.count = 1, .reusable = false}}, REDUCE(sym_literal, 1),
  [58] = {.entry = {.count = 1, .reusable = true}}, REDUCE(sym_literal, 1),
  [60] = {.entry = {.count = 1, .reusable = true}}, REDUCE(sym_source, 1),
  [62] = {.entry = {.count = 1, .reusable = true}}, REDUCE(aux_sym_source_repeat1, 2),
  [64] = {.entry = {.count = 2, .reusable = true}}, REDUCE(aux_sym_source_repeat1, 2), SHIFT_REPEAT(30),
  [67] = {.entry = {.count = 1, .reusable = true}}, REDUCE(sym_statement, 1),
  [69] = {.entry = {.count = 1, .reusable = true}}, REDUCE(sym_expr_statement, 2, .production_id = 9),
  [71] = {.entry = {.count = 1, .reusable = true}}, REDUCE(sym_block, 2, .production_id = 2),
  [73] = {.entry = {.count = 1, .reusable = true}}, REDUCE(sym_block, 3, .production_id = 10),
  [75] = {.entry = {.count = 1, .reusable = true}}, REDUCE(sym_binary_operator, 1, .production_id = 8),
  [77] = {.entry = {.count = 1, .reusable = true}}, REDUCE(sym_binary_operator, 1, .production_id = 6),
  [79] = {.entry = {.count = 1, .reusable = true}}, REDUCE(sym_binary_operator, 1, .production_id = 7),
  [81] = {.entry = {.count = 1, .reusable = true}}, REDUCE(sym_binary_operator, 1, .production_id = 4),
  [83] = {.entry = {.count = 1, .reusable = true}}, REDUCE(sym_binary_operator, 1, .production_id = 5),
  [85] = {.entry = {.count = 1, .reusable = true}}, REDUCE(sym_function_definition, 5, .production_id = 1),
  [87] = {.entry = {.count = 1, .reusable = true}}, SHIFT(5),
  [89] = {.entry = {.count = 1, .reusable = true}}, SHIFT(33),
  [91] = {.entry = {.count = 1, .reusable = true}}, SHIFT(8),
  [93] = {.entry = {.count = 1, .reusable = true}}, SHIFT(29),
  [95] = {.entry = {.count = 1, .reusable = true}}, SHIFT(32),
  [97] = {.entry = {.count = 1, .reusable = true}},  ACCEPT_INPUT(),
};

#ifdef __cplusplus
extern "C" {
#endif
#ifdef _WIN32
#define extern __declspec(dllexport)
#endif

extern const TSLanguage *tree_sitter_yap(void) {
  static const TSLanguage language = {
    .version = LANGUAGE_VERSION,
    .symbol_count = SYMBOL_COUNT,
    .alias_count = ALIAS_COUNT,
    .token_count = TOKEN_COUNT,
    .external_token_count = EXTERNAL_TOKEN_COUNT,
    .state_count = STATE_COUNT,
    .large_state_count = LARGE_STATE_COUNT,
    .production_id_count = PRODUCTION_ID_COUNT,
    .field_count = FIELD_COUNT,
    .max_alias_sequence_length = MAX_ALIAS_SEQUENCE_LENGTH,
    .parse_table = &ts_parse_table[0][0],
    .small_parse_table = ts_small_parse_table,
    .small_parse_table_map = ts_small_parse_table_map,
    .parse_actions = ts_parse_actions,
    .symbol_names = ts_symbol_names,
    .field_names = ts_field_names,
    .field_map_slices = ts_field_map_slices,
    .field_map_entries = ts_field_map_entries,
    .symbol_metadata = ts_symbol_metadata,
    .public_symbol_map = ts_symbol_map,
    .alias_map = ts_non_terminal_alias_map,
    .alias_sequences = &ts_alias_sequences[0][0],
    .lex_modes = ts_lex_modes,
    .lex_fn = ts_lex,
    .primary_state_ids = ts_primary_state_ids,
  };
  return &language;
}
#ifdef __cplusplus
}
#endif
