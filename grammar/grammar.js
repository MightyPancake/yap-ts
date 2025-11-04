/**
 * @file Parser for the yap language
 * @author Filip Król <amightypancake@gmail.com>
 * @license MIT
 */

/// <reference types="tree-sitter-cli/dsl" />
// @ts-nocheck

const PREC = {
  ASSIGN: 0,
  CALL: 11,
  ADD: 13,
  SUB: 13,
  DIV: 14,
  MOD: 14,
  MUL: 14,
};

module.exports = grammar({
  name: "yap",
  //Things to ignore
  extras: $ => [
    /\s+/,
    token(/\/\/[^\r\n]*(\n|\r)/),
    token(seq('/*', /[^*]*\*+([^/*][^*]*\*+)*\//)),
  ],
  //Rules (aka the meat)
  rules: {
    //def source_file
    source: $ => repeat($._definition),
    //comments
    //def single_line_comment
    //def multi_line_comment
    //def _defintion
    _definition: $ => choice(
      $.function_definition
    ),
    //def function definition
    function_definition: $ => seq(
      field("fn", "fn"),
      field("name", $.identifier),
      "(",
      ")",
      field("body", $.block)
    ),
    //def scope
    block: $ => seq(
      field("opening_bracket", '{'),
      repeat($.statement),
      field("closing_bracket", '}'),
    ),
    //def _statement
    statement: $ => choice(
      $.expr_statement,
      $.block,
    ),
    //def bin_op
    bin_op: $ => choice(
      // add/sub
      prec.left(PREC.ADD, seq(
        field("left", $.expr),
        field("operator", $.binary_operator),
        field("right", $.expr)
      )),
    ),
    //_assignment
    assignment: $ => prec.right(PREC.ASSIGN, seq(
      field("lvalue", $.lvalue),
      field("assign", '='),
      field("expr", $.expr))
    ),
    binary_operator: $ => choice(
      field("add", "+"),
      field("sub", "-"),
      field("mul", "*"),
      field("div", "/"),
      field("mod", "%"),
    ),
    //def lvalue
    lvalue: $ => choice(
      field("var", $.identifier)
    ),
    //def expr_statement
    expr_statement: $ => prec.right(seq(
      field("expr", $.expr),
      field("semicolon", ";")
    )),
    //def empty_statement
    literal: $ => choice(
      $.num_literal
    ),
    //def expr
    expr: $ => choice(
      $.literal,
      $.identifier,
      $.bin_op,
      $.assignment
    ),
    //def number
    num_literal: $ => /\d+/,
    //def identifier
    identifier : $ => /[a-zA-Z_]\w*/
  }
});
