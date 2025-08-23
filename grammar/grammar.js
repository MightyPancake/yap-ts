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
    //def source_file
    source_file: $ => repeat($._definition),
    //def definition
    definition: $ => $._definition,
    //def _defintion
    _definition: $ => choice(
      $.function_definition
    ),
    //def function definition
    function_definition: $ => seq(
      "fn",
      field("ret_type", optional($.type)),
      field("name", $.identifier),
      "(",
      field("args", $.variable_list),
      ")",
      field("body", $.block)
    ),
    //def variable
    variable: $ => seq(
      field("type", $.type),
      field("name", $.identifier),
    ),
    //def variable_list
    variable_list: $ => choice(
      '*',
      seq($.variable, repeat(seq(",", $.variable)))
    ),
    //def scope
    block: $ => seq('{', repeat($.statement), '}'),
    //def single_line_comment
    single_line_comment: $ => token(seq('//', /.*/)),
    multi_line_comment: $ => token(seq('/*', /[^*]*\*+([^/*][^*]*\*+)*\//)),
    //def _statement
    statement: $ => choice(
      $.expr_statement,
      $.empty_statement,
      $.block,
      $.return_statement,
      $.if_statement
    ),
    if_statement: $ => seq("if", $.expr, $.statement),
    if_else_statement: $ => seq("if", $.expr, $.statement, "else", $.statement),
    return_statement: $ => seq("ret", $.expr, ";"),
    //def binary_expression
    binary_expression: $ => choice(
      // add/sub
      prec.left(PREC.ADD, seq($.expr, '+', $.expr)),
      prec.left(PREC.SUB, seq($.expr, '-', $.expr)),
      // mul/div/mod
      prec.left(PREC.MUL, seq($.expr, '*', $.expr)),
      prec.left(PREC.DIV, seq($.expr, '/', $.expr)),
      prec.left(PREC.MOD, seq($.expr, '%', $.expr)),
      //assigns
      prec.left(PREC.ASSIGN, seq($.expr, '=', $.expr)),
      prec.left(PREC.ASSIGN, seq($.expr, '+=', $.expr)),
      prec.left(PREC.ASSIGN, seq($.expr, '-=', $.expr)),
      prec.left(PREC.ASSIGN, seq($.expr, '*=', $.expr)),
      prec.left(PREC.ASSIGN, seq($.expr, '/=', $.expr)),
      prec.left(PREC.ASSIGN, seq($.expr, '%=', $.expr))
    ),
    //def expr_list
    expr_list: $ => seq($.expr, repeat(seq(",", $.expr))),
    //def call
    call: $ => seq($.expr, "(", optional($.expr_list), ")"),
    //def type
    type: $ => choice(
      field("primitive", $.identifier),
      seq("@", $.type),
      seq("[]", $.type)
    ),
    //def expr_statement
    expr_statement: $ => seq($.expr, ";"),
    //def empty_statement
    empty_statement: $ => ";",
    literal: $ => choice(
      $.num_literal
    ),
    //def expr
    expr: $ => choice(
      $.literal,
      $.identifier,
      $.binary_expression,
      $.call,
      $.field_access
    ),
    //def field_access
    field_access: $ => seq($.expr, ".", $.identifier),
    //def number
    num_literal: $ => /\d+/,
    //def identifier
    identifier : $ => /[a-zA-Z_]\w*/
  }
});
