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
    $.single_line_comment,
    $.multi_line_comment
  ],
  //Rules (aka the meat)
  rules: {
    //comments
    //def single_line_comment
    single_line_comment: $ => token(seq('//', /.*/)),
    //def multi_line_comment
    multi_line_comment: $ => token(seq('/*', /[^*]*\*+([^/*][^*]*\*+)*\//)),
    //def source_file
    source_file: $ => repeat($._definition),
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
      field("opening_bracket", '}'),
    ),
    //def _statement
    statement: $ => choice(
      $.expr_statement,
      $.block,
    ),
    //def binary_expression
    binary_expression: $ => choice(
      // add/sub
      prec.left(PREC.MUL, seq(
        field("left", $.expr),
        $.binary_operator,
        field("right", $.expr)
      )),
      $._assignment
    ),
    //_assignment
    _assignment: $ => prec.right(PREC.ASSIGN, seq(
      $.lvalue,
      '=',
      field("right", $.literal))),
    binary_operator: $ => choice(
      field("addition", "+"),
      field("substraction", "-"),
    ),
    //def lvalue
    lvalue: $ => choice(
      $.identifier
    ),
    //def expr_statement
    expr_statement: $ => prec.right(seq(
      field("expression", $.expr),
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
      $.binary_expression,
    ),
    //def number
    num_literal: $ => /\d+/,
    //def identifier
    identifier : $ => /[a-zA-Z_]\w*/
  }
});
