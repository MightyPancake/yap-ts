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
          ',',
          rule
        )
      ),
    ),
    optional(',')
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
  '+': 3,               // +
  '-': 3,               // -
  '/': 4,               // /
  '*': 4,               // *
  '%': 4,               // %
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
    [$.function_declaration, $._expr],
    [$.macro_declaration, $.macro_statement, $._expr],
    [$.macro_statement, $._expr],
    [$.typ, $._expr],
    [$.macro_type, $._expr],
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
      $.macro_declaration,
      // $.struct_declaration,
      $._statement,
      //enum 
    ),
    macro_declaration: $ => $._macro_call,
    //def function_declaration
    function_declaration: $ => seq(
      field("return_type", $.typ),
      field("fn", "fn"),
      optional(
        field("subject", seq(
        field("type", $.typ),
        ":"
        )
      )),
      field("name", $.identifier),
      "(",
      field("args", optional($.func_decl_args)),
      ")",
      field("body", $.block)
    ),
    //def func_decl_args
    func_decl_args: $ => comma_sep($.func_decl_arg),
    //def func_decl_arg
    func_decl_arg: $ => seq(
      field("type", $.typ),
      field("name", $.identifier),
      optional(
        seq(
          field("default_op", ':='),
          field("default_value", $._expr)
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
      $.pointer_type,
      $.identifier,
      $.module_access,
      $.function_type,
      $.paren_type,
      $.macro_type,
    ),
    paren_type: $ => seq(
      field("open_paren", '('),
      field("inner", $.typ),
      field("close_paren", ')'),
    ),
    pointer_type: $ => prec.right(seq(
      field("subtyp", $.typ),
      field("ptr_of", '@'),
      repeat('@'),
    )),
    function_type: $ => seq(
      field("open_paren", '('),
      optional(field("return_type", $.typ)),
      field("fn", "fn"),
      optional(field("func_type_params", $.func_type_params)),
      field("close_paren", ')'),
    ),
    func_type_params: $ => comma_sep($.typ),
    macro_type: $ => $._macro_call,
    // typ: $ => choice(
    //   // field("primary", $.identifier),
    //   field("pointer", seq(
    //       field("ptr_of", '@'),
      //       field("subtyp", $.typ),
    //     )
    //   ),
    //   field("array", seq(
    //       field("open_bracket", '['),
    //       field("close_bracket", ']'),
    //       field("subtyp", $.typ),
    //     )
    //   ),
    // ),
    //def block
    block: $ => seq(
      field("opening_bracket", '{'),
      repeat($._statement),
      field("closing_bracket", '}'),
    ),
    //def _statement
    _statement: $ => choice(
      $.macro_statement, //TODO
      $.var_decl,
      $.expr_statement, 
      $.if_statement,
      $.if_else_statement,
      $.empty_statement,
      $.while_loop, //TODO
      $.for_loop, //TODO
      $.return_statement,
      $.break_statement, //TODO
      $.continue_statement //TODO
    ),
    macro_statement: $ => $._macro_call,
    //def break_statement
    break_statement: $ => "break",
    continue_statement: $ => "continue",
    return_statement: $ => seq(
      field("return", "ret"),
      choice(
        field("value", $._expr),
        field("empty", ";")
      )
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
      field("then_branch", $._statement)
    )),
    //def if_else_statement
    if_else_statement: $ => prec.right(PREC.IF_ELSE, seq(
      field("if", "if"),
      field("condition", $._expr),
      field("then_branch", $._statement),
      field("else", "else"),
      field("else_branch", $._statement),
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
      field("func", $._expr),
      field("open_bracket", '('),
      field("params", optional($.call_params)),
      field("close_bracket", ')')
    )),
    //def call_params
    call_params: $ => comma_sep($._param),
    //def _param
    _param: $ => choice(
      $.unnamed_param,
      $.named_param
    ),
    //def unnamed_param
    unnamed_param: $ => $._expr,
    //def named_param
    named_param: $ => seq(
      field("name", $.identifier),
      field("assign", ":="),
      field("value", $._expr)
    ),
    //def bin_expr
    // bin_expr: $ => prec.left(PREC, seq(
    //   field("left", $._expr),
    //   field("operator", $.binary_operator),
    //   field("right", $._expr)
    // )),
    bin_expr: $ => {
      const bin_ops = ['+', '-', '*', '/', '%'];
      return choice(...bin_ops.map((op) => {
        return prec.left(PREC[op], seq(
          field('left', $._expr),
          // @ts-ignore
          field('operator', op),
          field('right', $._expr),
        ));
      }));
    },
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
      $.bin_expr,
      $.identifier,
      $.assignment,
      $.at_op,
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
      $._macro_call,
      $.comptime_context,
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
    //def at_op
    at_op: $ => seq(
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
    //def literal
    literal: $ => choice(
      $.num_literal,
      $.string_literal,
      $.bool_literal,
      $.blob_literal, //blob literals can be cast to structs
      //TODO:
      // char
      // hex
      // binary
    ),
    //def blob_literal
    blob_literal: $ => seq(
      '[',
      comma_sep($._param),
      ']'
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
    //def _macro_call
    _macro_call: $ => choice(
      $.paramless_macro_call,
      $.param_macro_call
    ),
    //def paramless_macro_call
    paramless_macro_call: $ => seq(
      field("caller", $.macro_caller),
      field("macro_op", '#')
    ),
    //def param_macro_call
    param_macro_call: $ => seq(
      field("caller", $.macro_caller),
      field("macro_op", "#("),
      repeat($.macro_param),
      field("close_paren", ')')
    ),
    //def macro_param
    macro_param: $ => choice(
      $._param,
      ','
    ),
    //def macro_caller
    macro_caller: $ => choice(
      $.identifier,
      $.module_access,
      $.method_access,
    ),
    //comptime_context
    comptime_context: $ => '(#)',
  }
});

//TODO:
// C ABI
// struct unpacking
// ?? and ?.field
// call chaining
// macros
