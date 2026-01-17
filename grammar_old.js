/**
 * @file Parser for the yap language
 * @author Filip Król <amightypancake@gmail.com>
 * @license MIT
 */

/// <reference types="tree-sitter-cli/dsl" />
// @ts-nocheck

const removePrefix = (str, prefix) => {
  if (str.startsWith(prefix)){
    return str.slice(prefix.length);
  }
  return str;
}

const comma_sep = (rule) => {
  return seq(
    rule,
    optional(
      repeat(
        seq(
          field("comma", ','),
          rule
        )
      ),
    )
  )
}

const fielded = ($, ruleName) => {
  // Use bracket notation to access the rule dynamically: $[ruleName]
  if (!ruleName) throw new Error("Missing rule name in 'fielded' call!");
  let name = removePrefix(ruleName, "_")
  if (!$[ruleName]) throw new Error(`fielded error for rule ${ruleName}`);
  return field(name, $[ruleName]);
};

const PREC = {
  ASSIGN: 0,            // =, +=, -=, *=, /=, %=, ?= 
  TERNARY: 1,           // cond ? val_if_true : val_if_false
  EXPR_STATEMENT: 1,    // expr being a statement
  COMPARISON: 2,    // expr == expr, etc.
  ADD: 3,               // +
  SUB: 3,               // -
  DIV: 4,               // /
  MOD: 4,               // %
  MUL: 4,               // *
  INCR: 5,              // expr++, expr-- etc.
  PAREN: 13,            // ()
  CALL: 15,             // func()
  IF: 16,               // if
  IF_ELSE: 17,          // if-else
  FIELD: 18,            // x.field
  CAST: 18,             // x.(type)
};

module.exports = grammar({
  name: "yap",
  conflicts: $ => [
    // [$.lvalue, $.expr]
  ],
  //Things to ignore
  extras: $ => [
    // whitespace in general
    /\s+/,
    //def single_line_comment
    // token(/\/\/[^\r\n]*(\n|\r)/),
    // token(seq("//", /[^\n]*/)),
    // //def multi_line_comment
    // token(seq('/*', /[^*]*\*+([^/*][^*]*\*+)*\//)),
    $._comment,
  ],
  //Rules (aka the meat)
  rules: {
    //def source_file
    source: $ => repeat($._declaration),
    //def comment
    _comment: $ => token(choice(
      seq('//', /(\\+(.|\r?\n)|[^\\\n])*/),
      seq(
        '/*',
        /[^*]*\*+([^/*][^*]*\*+)*/,
        '/',
      ),
    )),
    comment: $ => $._comment,
    //def _defintion
    _declaration: $ => choice(
      $.function_declaration,
      $.struct_declaration,
      $._statement,
      //enum 
    ),
    //def function_declaration
    function_declaration: $ => seq(
      field("fn", "fn"),
      field("name", $.identifier),
      "(",
      optional($.params),
      ")",
      field("body", $.block)
    ),
    //def struct_definition
    struct_declaration: $ => seq(
      field("struct", "struct"),
      field("name", $.identifier),
      field("open_bracket", "{"),
      field("members", $.struct_members),
      field("close_bracket", "}"),
    ),
    //def struct_members
    struct_members: $ => comma_sep($.struct_member),
    //def struct_member
    struct_member: $ => choice(
      seq(
        field("typ", $.typ),
        field("name", $.identifier),
        optional(
          seq(
            field("equals", "="),
            field("default", $._expr)
          )
        )
      )
    ),
    //def _param
    _param: $ => choice(
      $.param,
      $.default_param,
    ),
    //def named_param
    param: $ => seq(
      field("type", $.typ),
      field("name", $.identifier),
    ),
    //def param_with_default
    default_param: $ => seq(
      field("param", $.param),
      field("equals", "="),
      field("value", $._expr)
    ),
    //def params
    params: $ => comma_sep(
      $._param
    ),
    //def typ
    typ: $ => choice(
      field("primary", $.identifier),
      field("pointer", seq(
          field("ptr_of", '@'),
          field("subtyp", $.typ),
        )
      ),
      field("array", seq(
          field("open_bracket", '['),
          field("close_bracket", ']'),
          field("subtyp", $.typ),
        )
      ),
    ),
    //def scope
    block: $ => seq(
      field("opening_bracket", '{'),
      repeat($._statement),
      field("closing_bracket", '}'),
    ),
    //def _statement
    _statement: $ => choice(
      $.expr_statement,
      $.if_statement,
      $.if_else_statement,
      $.empty_statement,
      $.while_loop,
      $.for_loop,
      $.var_decl,
      $.return_statement,
      $.break_statement,
      $.continue_statement
    ),
    //def break_statement
    break_statement: $ => "break",
    continue_statement: $ => "continue",
    return_statement: $ => seq(
      field("return", "ret"),
      field("value", $._expr),
    ),
    //def var_decl
    var_decl: $ => seq(
      field("name", $.identifier),
      field("decl_op", ":="),
      field("value", $._expr)
    ),
    //def for_loop
    for_loop: $ => seq(
      field("for", "for"),
      field("start", $._statement),
      field("comma1", ','),
      field("condition", $._expr),
      field("comma2", ','),
      field("step", $._statement), //TODO: C forces expression here... why?
      field("body", $._statement)
    ),
    //def while_loop
    while_loop: $ => seq(
      field("while", "while"),
      field("condition", $._expr),
      field("body", $._statement)
    ),
    //def if_statement
    if_statement: $ => prec.right(PREC.IF, seq(
      field("if", "if"),
      field("condition", $._expr),
      field("statement", $._statement)
    )),
    //def if_else_statement
    if_else_statement: $ => prec.right(PREC.IF_ELSE, seq(
      field("if", "if"),
      field("condition", $._expr),
      field("statement", $._statement),
      field("else", "else"),
      field("else_statement", $._statement),
    )),
    //ternary_op
    ternary_expr: $ => prec.right(PREC.TERNARY, seq(
      field("condition", $._expr),
      field("if", "?"),
      field("expr1", $._expr),
      // field("else", ":"),
      field("else", "else"),
      field("expr2", $._expr)
    )),
    //def func_call
    func_call: $ => prec.right(PREC.CALL, seq(
      field("caller", $._expr),
      field("open_bracket", '('),
      field("params", optional($.call_params)),
      field("close_bracket", ')')
    )),
    //def call_params
    call_params: $ => comma_sep($._call_param),
    //def _call_param
    _call_param: $ => choice(
      $.unnamed_call_param,
      $.named_call_param
    ),
    //def unnamed_call_param
    unnamed_call_param: $ => $._expr,
    //def named_call_param
    named_call_param: $ => seq(
      field("name", $.identifier),
      field("assign", ":="),
      field("value", $._expr)
    ),
    //def bin_expr
    bin_expr: $ => choice(
      // add/sub
      prec.left(PREC.ADD, seq(
        field("left", $._expr),
        field("operator", $.binary_operator),
        field("right", $._expr)
      )),
    ),
    //_assignment
    assignment: $ => prec.right(PREC.ASSIGN,
      seq(
        field("left", $._expr),
        field("operator", choice(
          "=",
          "+=",
          "-=",
          "*=",
          "/=",
          "%=",
          "<<=",
          ">>=",
          "&=",
          "^=",
          "|=",
          "?=",
        )),
        field("right", $._expr),
      )
    ),
    binary_operator: $ => choice(
      field("add", "+"),
      field("sub", "-"),
      field("mul", "*"),
      field("div", "/"),
      field("mod", "%"),
    ),
    //def comp_op
    comp_op: $ => prec.right(PREC.COMPARISON, seq(
      field("left", $._expr),
      field("op", choice(
        "<",
        ">",
        "<=",
        ">=",
        "==",
        "!=",
      )),
      field("right", $._expr),
    )),
    //def expr_statement
    expr_statement: $ => prec.right(PREC.EXPR_STATEMENT, $._expr),
    //def empty_statement
    empty_statement: $ => ';',
    //def _expr
    _expr: $ => choice(
      $.literal,
      $.identifier,
      $.bin_expr,
      $.assignment,
      $.addr_expr,
      $.ternary_expr,
      $.func_call,
      $.block,
      $.paren_expr,
      $.cast_expr,
      $.field_expr,
      $._incr_expr,
      $.method_access,
      $.module_access,
      $.comp_op,
      $.macro,
    ),
    //def paren_op
    paren_expr: $ => prec.right(PREC.PAREN, seq(
      field("open_bracket", '('),
      fielded($, "_expr"),
      field("close_bracket", ')'),
    )),
    //def cast_op
    cast_expr: $ =>
    prec.left(PREC.CAST,
      seq(
        fielded($, "_expr"),
        field("cast", ".("),
        fielded($, "typ"),
        field("close_bracket", ')'),
      ),
    ),
    //def field_expr
    field_expr: $ => prec.left(PREC.FIELD, seq(
      fielded($, "_expr"),
      field("dot", '.'),
      field("field", $.identifier),
    )),
    //def addr_op
    addr_expr: $ => seq(
      fielded($, "_expr"),
      field("at_op", '@')
    ),
    //def _incr_expr
    _incr_expr: $ => choice(
      $.postfix_incr_expr,
    ),
    //def postfix_incr_expr
    postfix_incr_expr: $ => seq(
      field("expr", $._expr),
      field("op", choice(
        "++",
        "--"
      ))
    ),
    //def module_access
    module_access: $ => seq(
      field("module", $.identifier),
      field("module_access_op", "->"),
      field("field", $.identifier)
    ),
    //def method
    method_access: $ => seq(
      field("caller", $._expr),
      field("method_access_op", ":"),
      field("name", $.identifier)
    ),
    //def 
    //def literal
    literal: $ => choice(
      $.num_literal,
      $.string_literal,
      $.bool_literal,
      $.struct_literal,
      //TODO:
      // char
      // hex
      // binary
    ),
    //def struct_literal
    struct_literal: $ => seq(
      field("name", $.identifier),
      field("struct_literal_op", "-{"),
      // field("open", "{"),
      optional(field("fields", $.struct_fields)),
      field("close", "}"),
    ),
    struct_fields: $ => comma_sep($._struct_field),
    //def struct_field
    _struct_field: $ => choice(
      $.unnamed_struct_field,
      $.named_struct_field
    ),
    //def unnamed_struct_field
    unnamed_struct_field: $ => $._expr,
    //def named_struct_field
    named_struct_field: $ => seq(
      field("name", $.identifier),
      field("assign_field_op", ":="),
      field("value", $._expr),
    ),
    //def num_literal
    num_literal: $ => /\d+/,
    //def bool_literal
    bool_literal: $ => choice("true", "false"),
    //def string_literal
    string_literal: $ => seq(
      field("start", choice('L"', 'u"', 'U"', 'u8"', '"')),
      field("content", repeat(choice(
        alias(token.immediate(prec(1, /[^\\"\n]+/)), $.string_content),
        $.esc_seq,
      ))),
      field("end", '"'),
    ),
    // def esc_seq
    esc_seq: _ => token(prec(1, seq(
      '\\',
      choice(
        /[^xuU]/,
        /\d{2,3}/,
        /x[0-9a-fA-F]{1,4}/,
        /u[0-9a-fA-F]{4}/,
        /U[0-9a-fA-F]{8}/,
      ),
    ))),
    //def identifier
    identifier : $ => /[a-zA-Z_]\w*/ ,
    //def macro
    macro: $ => seq(
      field("expr", $._expr),
      field("macro_op", "#")
    ),
  }
});

//TODO:
// struct literals
// enums definitions
// enum literals
// finish function calls
// methods
// modules
// C ABI
// struct unpacking
// ?? and ?.field
// call chaining
// macros
