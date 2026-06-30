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
    // Precedence for variable declarations (one higher than EXPR_STATEMENT)
  VAR_DECL: 2,
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
    [$._func_decl_core, $._expr],
    [$.macro_declaration, $.macro_expr, $.macro_type],
    [$.macro_expr, $.macro_type],
    [$.typ, $._expr],
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
    //def non_empty_source
    non_empty_source: $ => repeat1($._declaration),
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
      $.module_declaration,
      $.module_import_declaration,
      $.file_import_declaration,
      $.function_declaration,
      $.function_definition,
      $.type_declaration,
      $._statement, //Emits into current module.
    ),
    //def module_declaration
    module_declaration: $ => seq(
      field("module", "module"),
      field("name", $.identifier),
      field("opening_bracket", '{'),
      optional(field("module_info", $.module_info)),
      field("closing_bracket", '}'),
    ),
    //def module_info
    module_info: $ => comma_sep($.module_info_item),
    //def module_info_item
    module_info_item: $ => seq(
      field("key", $.identifier),
      field("assign", ":"),
      field("value", choice(
        $.string_literal,
        $.num_literal,
        $.bool_literal,
      ))
    ),
    //def module_import_declaration
    module_import_declaration: $ => seq(
      field("import", "import"),
      field("name", $.identifier)
    ),
    //def file_import_declaration
    file_import_declaration: $ => seq(
      field("import", "import"),
      field("path", $.string_literal)
    ),
    //def type_declaration
    type_declaration: $ => choice(
      $.struct_declaration,
      $.enum_declaration,
      $.union_declaration,
      $.forward_type_declaration,
    ),
    forward_type_declaration: $ => seq(
      field("type", "type"),
      field("name", $.identifier),
    ),
    //def struct_declaration
    //TODO: Rename to struct_definition
    struct_declaration: $ => seq(
      field("struct", "struct"),
      field("name", $.identifier),
      field("opening_bracket", '{'),
      field("fields", $.struct_fields),
      field("closing_bracket", '}'),
    ),
    //def anon_struct_type
    anon_struct_type: $ => seq(
      field("struct", "struct"),
      field("opening_bracket", '{'),
      field("fields", $.struct_fields),
      field("closing_bracket", '}'),
    ),
    //def struct_fields
    struct_fields: $ => seq(
      comma_sep($.struct_field),
    ),
    //def struct_field
    struct_field: $ => choice(
      $.var_decl,
      seq(
        field("type", choice($.anon_struct_type, $.anon_enum_type, $.anon_union_type)),
        optional(field("name", $.identifier)),
        optional(seq(
          field("assign", "="),
          field("default_value", $._expr)
        ))
      )
    ),
    _type_annotation: $ => choice(
      $.typ,
      $.anon_struct_type,
      $.anon_enum_type,
      $.anon_union_type,
    ),
    enum_declaration: $ => seq(
      field("enum", "enum"),
      field("name", $.identifier),
      field("opening_bracket", '{'),
      field("variants", $.enum_variants),
      field("closing_bracket", '}'),
    ),
    enum_variants: $ => comma_sep($.enum_variant),
    enum_variant: $ => seq(
      field("name", $.identifier),
      field("value_node", 
        optional(
          seq(
            field("assign", '='),
            field("value", $._expr)
          )
        )
      )
    ),
    anon_enum_type: $ => seq(
      field("enum", "enum"),
      field("opening_bracket", '{'),
      field("variants", $.enum_variants),
      field("closing_bracket", '}'),
    ),
    union_declaration: $ => seq(
      field("union", "union"),
      field("name", $.identifier),
      field("opening_bracket", '{'),
      field("variants", $.union_variants),
      field("closing_bracket", '}'),
    ),
    union_variants: $ => comma_sep($.union_variant),
    union_variant: $ => seq(
      field("type", $._type_annotation),
      optional(field("name", $.identifier)),
    ),
    anon_union_type: $ => seq(
      field("union", "union"),
      field("opening_bracket", '{'),
      field("variants", $.union_variants),
      field("closing_bracket", '}'),
    ),
    macro_declaration: $ => $._macro_call,
    //def function_declaration
    function_declaration: $ => seq(
      field("func_decl", $._func_decl_core),
      field("semicolon", ';')
    ),
    //def function_definition
    function_definition: $ => seq(
      field("func_decl", $._func_decl_core),
      field("body", $.block)
    ),
    //def _func_decl_core
    _func_decl_core: $ => seq(
      field("return_type", optional($.typ)),
      field("fn", "fn"),
      optional(
        field("subject", seq(
          field("type", $.typ),
          ":"
          )
        )
      ),
      field("name", $.identifier),
      "(",
      field("args", optional($.func_decl_args)),
      ")",
    ),
    //def func_decl_args
    func_decl_args: $ => comma_sep($.var_decl),
    //def typ
    typ: $ => choice(
      $.pointer_type,
      $.identifier,
      $.module_access,
      $.function_type,
      $.macro_type,
      $.paren_type,
      $.const_type,
      $.slice_type,
      $.array_type,
    ),
    array_type: $ => seq(
      field("inner", $.typ),
      field("open_bracket", '['),
      field("size", $._expr),
      field("close_bracket", ']'),
    ),
    slice_type: $ => seq(
      field("inner", $.typ),
      field("open_bracket", '['),
      field("close_bracket", ']'),
    ),
    const: $ => "const",
    const_type: $ => seq(
      field("inner", $.typ),
      field("const", $.const),
    ),
    paren_type: $ => seq(
      field("open_paren", '('),
      field("inner", $.typ),
      field("close_paren", ')'),
    ),
    pointer_type: $ => prec.right(seq(
      field("subtyp", $.typ),
      field("ptr_of", '@')
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
    //def block
    block: $ => seq(
      field("opening_bracket", '{'),
      repeat($._statement),
      field("closing_bracket", '}'),
    ),
    //def _statement
    //def statement
    statement: $ => $._statement,
    _statement: $ => choice(
      $.block,
      $._var_decl_statement,
      $.expr_statement,
      $.if_statement,
      $.if_else_statement,
      $.empty_statement,
      $.while_loop,
      $.for_loop,
      $.return_statement,
      $.break_statement,
      $.continue_statement
    ),
    _var_decl_statement: $ => seq(
      field("var_decl", $.var_decl),
      field("semicolon", ';')
    ),
    macro_statement: $ => $._macro_call,
    //def break_statement
    break_statement: $ => "break",
    continue_statement: $ => "continue",
    return_statement: $ => seq(
      field("return", "ret"),
      field("value", optional($._expr)),
      ";"
    ),
    //def var_decl
    var_decl: $ => prec.right(PREC.VAR_DECL, seq(
      choice(
        $.infered_type_var_decl,
        $.explicit_type_var_decl
      )
    )),
    infered_type_var_decl: $ => seq(
      field("no_type", "_"),
      field("name", $.identifier),
      field("assign", "="),
      field("value", $._expr)
    ),
    explicit_type_var_decl: $ => seq(
      field("type", $.typ),
      field("name", $.identifier),
      optional(
        seq(
           field("assign", "="),
           field("value", $._expr)
        )
      )
    ),
    //def for_loop
    //The condition is parenthesized (like C) so there's a hard boundary
    //between it and the body - without one, a brace-less body leaves the
    //GLR parser to guess where the condition ends (see e.g. the deref_expr
    //vs member_access ambiguity this resolves: "for (...; p.field; ...) body"
    //has no ambiguity, whereas a bare "for (...; p.field; ...) body" without
    //parens would).
    for_loop: $ => seq(
      field("for", "for"),
      field("open_paren", '('),
      field("init", $._statement), //start (carries its own trailing ';')
      field("condition", $._expr), //condition
      field("semicolon", ';'),
      field("update", $._expr), //step (this should be a statement, but C forces expressions)
      field("close_paren", ')'),
      field("body", $._statement)
    ),
    //def while_loop
    while_loop: $ => seq(
      field("while", "while"),
      field("open_paren", '('),
      field("condition", $._expr),
      field("close_paren", ')'),
      field("body", $._statement)
    ),
    //def if_statement
    if_statement: $ => prec.right(PREC.IF, seq(
      field("if", "if"),
      field("open_paren", '('),
      field("condition", $._expr),
      field("close_paren", ')'),
      field("then_branch", $._statement)
    )),
    //def if_else_statement
    if_else_statement: $ => prec.right(PREC.IF_ELSE, seq(
      field("if", "if"),
      field("open_paren", '('),
      field("condition", $._expr),
      field("close_paren", ')'),
      field("then_branch", $._statement),
      field("else", "else"),
      field("else_branch", $._statement),
    )),
    //ternary_op
    ternary_expr: $ => prec.right(PREC.TERNARY, seq(
      field("condition", $._expr),
      field("if", "?"),
      field("then_expr", $._expr),
      // field("else", ":"),
      field("else", "else"),
      field("else_expr", $._expr)
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
      field("dot", '.'),
      field("name", $.identifier),
      field("assign", "="),
      field("value", $._expr)
    ),
    //def bin_expr
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
    expr_statement: $ => prec.right(PREC.EXPR_STATEMENT, seq(
      field("expr", $._expr),
      field("semicolon", ';')
    )),
    //def empty_statement
    empty_statement: $ => ';',
    //def expr
    //def _expr
    _expr: $ => choice(
      $.literal, //TODO: Check for errors, finish literals
      $.bin_expr, //TODO: Finish/checks
      $.identifier,
      $.assignment, //TODO: Finish/checks
      $.at_op, //TODO: Checks
      $.ternary_expr,
      $.func_call,
      $.block_expr, //NOT IMPLEMENTED: parses, but yap_build_expr (build.c) has no case for yap_expr_block yet
      $.paren_expr,
      $.cast_expr,
      $.member_access,
      $.optional_member_access,
      $.deref_expr,
      $.index_access,
      $.incr_expr,
      $.method_access, //method dispatch (parse.c, build.c) when called as 'obj:name(args)'; bare 'Type:name;' statements are still reinterpreted as an uninitialized var-decl shorthand
      $.module_access,
      $.comp_op,
      $.ast_blueprint, //NOT IMPLEMENTED: parses but no semantic/codegen handling — see blueprint_literal
      $.macro_expr,
    ),
    //def macro_expr
    macro_expr: $ => $._macro_call,
    //def  block_expr
    block_expr: $ => prec.right(PREC.PAREN, seq(
      field("open_bracket", '('),
      field("block", $.block),
      field("close_bracket", ')'),
    )),
    //def paren_op
    paren_expr: $ => prec.right(PREC.PAREN, seq(
      field("open_bracket", '('),
      field("expr", $._expr),
      field("close_bracket", ')'),
    )),
    //def cast_op
    cast_expr: $ =>
    prec.left(PREC.CAST,
      seq(
        field("expr", $._expr),
        field("cast", ".("),
        field("type", $.typ),
        field("close_bracket", ')'),
      ),
    ),
    //def member_access
    member_access: $ => prec.left(PREC.FIELD, seq(
      field("object", $._expr),
      field("dot", '.'),
      field("member", $.identifier),
    )),
    //def deref_expr
    deref_expr: $ => prec.left(PREC.FIELD, seq(
      field("expr", $._expr),
      field("dot", '.'),
    )),
    //def optional_member_access
    optional_member_access: $ => prec.left(PREC.FIELD, seq(
      field("object", $._expr),
      field("dot", '?.'),
      field("member", $.identifier),
    )),
    //def index_access
    index_access: $ => prec.left(PREC.FIELD, seq(
      field("expr", $._expr),
      field("open_bracket", ':['),
      field("index", $._expr),
      field("close_bracket", ']'),
    )),
    //def at_op
    at_op: $ => seq(
      field("expr", $._expr),
      field("at_op", '@')
    ),
    //def incr_expr
    incr_expr: $ => seq(
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
      $.byte_literal,
      $.bool_literal,
      $.null_literal,
      $.blob_literal, //blob literals can be cast to structs and arrays alike
      $.func_literal,
      //TODO:
      // hex
      // binary
    ),
    //def byte_literal
    byte_literal: $ => seq(
      "'",
      field("content", choice(
        alias(token.immediate(prec(1, /[^\\'\n]/)), $.byte_content),
        $.esc_seq,
      )),
      "'"
    ),
    //def func_literal
    func_literal: $ => seq(
      field("open_paren", '('),
      optional(field("return_type", $.typ)),
      field("fn", "fn"),
      optional(field("func_literal_params", $.func_literal_params)),
      field("close_paren", ')'),
      field("body", $.block)
    ),
    //def func_literal_params
    func_literal_params: $ => comma_sep($.func_literal_param),
    //def func_literal_named_type
    func_literal_param: $ => seq(
      field("type", $.typ),
      field("name", $.identifier)
    ),
    //def blob_literal
    blob_literal: $ => seq(
      '[',
      comma_sep($._param), //TODO: Rework
      ']'
    ),
    //def num_literal
    num_literal: $ => /\d+(\.\d+)?/,
    //def bool_literal
    bool_literal: $ => choice("true", "false"),
    //def null_literal
    null_literal: $ => "null",
    //def string_literal
    string_literal: $ => seq(
      field("start", choice('L"', 'u"', 'U"', 'u8"', 'c"', '"')),
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
    //def _macro_call — comptime call syntax: func:(args)
    _macro_call: $ => prec.right(seq(
      field("macro_call", $.comptime_call),
    )),
    //def comptime_call
    comptime_call: $ => seq(
      field("caller", $.macro_caller),
      field("comptime_call_op", ":("),
      field("params", optional($.macro_params)),
      field("close_paren", ')')
    ),
    //def macro_params
    macro_params: $ => comma_sep($.macro_param),
    //def macro_param
    macro_param: $ => choice(
      $.named_param,
      $.unnamed_param,
      $.ast_param,
      $._statement,
      $.identifier_adding_param,
      $.macro_mut_param,
    ),
    //def ast_param — pass expression as yExpr AST node: #expr
    ast_param: $ => seq(
      field("ast_op", "#"),
      field("expr", $._expr),
    ),
    //def identifier_adding_param
    identifier_adding_param: $ => seq(
      field("identifier_adding_op", "+"),
      field("name", $.identifier),
    ),
    //def macro_mut_param
    macro_mut_param: $ => seq(
      field("expr", $._expr),
      field("mut_op", "=")
    ),
    //def macro_caller
    macro_caller: $ => choice(
      $.identifier,
      $.module_access,
      $.method_access,
    ),
    //def expr_blueprint — NOT IMPLEMENTED: grammar only, no parser/semantic handling anywhere
    expr_blueprint: $ => seq(
      field("expr_start", "$("),
      optional(field("expr_content", $.non_empty_source)),
      field("expr_end", ')'),
    ),
    //def statement_blueprint — NOT IMPLEMENTED: grammar only, no parser/semantic handling anywhere
    statement_blueprint: $ => seq(
      field("statement_start", "${"),
      optional(field("statement_content", $.non_empty_source)),
      field("statement_end", '}'),
    ),
    //def declaration_blueprint — NOT IMPLEMENTED, unreachable (not wired into blueprint_literal)
    declaration_blueprint: $ => seq(
      field("declaration_start", "$["),
      optional(field("declaration_content", $.non_empty_source)),
      field("declaration_end", ']'),
    ),
    //def ast_blueprint — NOT IMPLEMENTED: reachable from _expr but no parser/semantic handling
    ast_blueprint: $ => seq(
      field("ast_start", "$<"),
      optional(field("ast_content", $.non_empty_source)),
      field("ast_end", '>'),
    ),
    //blueprint_literal — unused: not referenced by `literal` or any other rule
    blueprint_literal: $ => choice(
      $.expr_blueprint,
      $.statement_blueprint,
      //$.declaration_blueprint,
      //$.ast_blueprint,
    ),
  }
});

//TODO:
// C ABI (bindgen.c/libclang header import exists as a standalone tool — not wired into the main compile pipeline)
// struct unpacking
// ?? (null coalescing) — ?.field (optional chaining) is implemented
// blueprints ($(), ${}, $<>)
