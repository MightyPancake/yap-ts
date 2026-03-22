#include "parse.h"
#include "error.h"
#include "node.h"
#include "tree_sitter/api.h"
#include "ts_yap.h"
#include "utils/utils.h"
#include <stdint.h>

static void yap_push_parse_error(yap_source* src, TSNode node, const char* msg){
    yap_log("%s [type=%s%s]", msg, ts_node_type(node), ts_node_has_error(node) ? ", has_error" : "");
    yap_ctx_push_error(src->ctx, yap_node_error(src, node, (char*)msg));
}

static void yap_log_node(yap_source* src, const char* prefix, TSNode node){
    char* node_val = yap_node_get_val_ctx(src, node);
    yap_log("%s type='%s' value='%s'%s", prefix, ts_node_type(node), node_val, ts_node_has_error(node) ? ", has error" : "");
}

yap_ctx* yap_parse(yap_args args){
    yap_parser* parser = yap_new_parser();
    if (darr_len(args.extra) == 0){
        printf("No source file provided\n");
        exit(1);
    }
    yap_log("Parsing entry file '%s'", darr_first(args.extra));
    yap_parser_parse_file(parser, darr_first(args.extra));
    yap_ctx* ret = parser->ctx;
    yap_free_parser(parser);
    return ret;
}

yap_source_code yap_parse_source_file(yap_source* src, TSNode node){
    yap_ctx* ctx = src->ctx;
    if (ts_node_null_or_error(node)){
        yap_push_parse_error(src, node, "Expected source file root");
        return (yap_source_code){};
    }
    const char* typ = ts_node_type(node);
    yap_log("Parsing source root of type '%s'", typ);
    if (strus_eq(typ, "source")){
        uint32_t decl_count = 0;
        for_ts_named_children(node, n) {
            decl_count++;
        }
        yap_log("Source file has %u top-level declarations", decl_count);
        darr(yap_decl) defs = yap_ctx_darr_new(ctx, yap_decl, .cap=decl_count, .len=0);
        for_ts_named_children(node, n){
            darr_push(defs, yap_parse_decl(src, n));
        }
        return (yap_source_code){
            .declarations=defs
        };
    }else{
        yap_push_parse_error(src, node, "Expected source file root");
        return (yap_source_code){};
    }
}

yap_decl yap_parse_decl(yap_source *src, TSNode node){
    yap_node_guard(node, yap_decl, "Invalid declaration", src);
    const char* typ = ts_node_type(node);
    yap_log("Parsing declaration: %s", typ);
    strus_switch(typ, "function_declaration"){
        return yap_parse_fn_decl(src, node);
    }else{
        yap_log_node(src, "Unhandled declaration", node);
        yap_push_parse_error(src, node, "Unhandled declaration");
        return yap_error_result(yap_decl, "Unhandled declaration");
    }
}

yap_decl yap_parse_fn_decl(yap_source *src, TSNode node){
    yap_node_guard(node, yap_decl, "Invalid function declaration", src);
    yap_node_field_by_name_var_check_push(node, name, yap_decl, "Missing function name", src);
    yap_node_field_by_name_var_check_push(node, body, yap_decl, "Missing function body", src);
    yap_node_field_by_name_var_check_push(node, args, yap_decl, "Missing function arguments", src);
    // LOGGING
    yap_node_val_ctx(name_node);
    yap_node_start_point(node);
    char* pos_str = yap_pos_string(*src, node_start_point.row, node_start_point.column);
    yap_log("Parsing function named %s at %s", name_node_val, pos_str);
    free(pos_str);
    // END LOGGING
    yap_node_field_by_name_var(node, return_type);
    yap_log("return_type_node: %s", ts_node_is_null(return_type_node) ? "valid" : "NULL");
    if (ts_node_null_or_error(return_type_node)){
        yap_log_node(src, "Function is missing return type annotation, trying to get it from the return statements of block", node);
    }
    darr(yap_func_arg) args = yap_parse_fn_args(src, args_node);
    //TODO: Check if arguments are valid
    yap_block body = yap_parse_block(src, body_node);
    //TODO: Get argument list and return type from the node
    //TODO: Register function in scope
    return (yap_decl){
        .kind=yap_decl_func,
        .func_decl=(yap_func_decl){
            .args=args,
            //TODO: FIX! Get argument types and names from the parameter list
            .ret_typ=0,
            .body=body
        }
    };
}

darr(yap_func_arg) yap_parse_fn_args(yap_source* src, TSNode node){
    yap_ctx* ctx = src->ctx;
    uint32_t param_count = ts_node_named_child_count(node);
    yap_log("Parsing %u function arguments", param_count);
    darr(yap_func_arg) args = yap_ctx_darr_new(ctx, yap_func_arg, .cap=param_count, .len=0);
    for_ts_named_children(node, n){
        if (!strus_eq(ts_node_type(n), "func_decl_arg")){
            yap_log_node(src, "Expected parameter in function parameter list", n);
            yap_push_parse_error(src, n, "Expected parameter in function parameter list");
            continue;
        }
        darr_push(args, yap_parse_fn_arg(src, n));
    }
    return args;
}

yap_func_arg yap_parse_fn_arg(yap_source* src, TSNode node){
    yap_node_guard(node, yap_func_arg, "Invalid function argument", src);
    yap_node_field_by_name_var_check_push(node, name, yap_func_arg, "Missing argument name", src);
    yap_node_field_by_name_var_check_push(node, type, yap_func_arg, "Missing argument type", src);
    yap_node_val_ctx(name_node);
    char* type_str = yap_node_get_val_ctx(src, type_node);
    yap_log("Parsing function argument: %s of type %s", name_node_val, type_str);
    return (yap_func_arg){
        .kind=yap_func_arg_valid,
        .name=name_node_val,
        .type=yap_parse_type(src, type_node),
        .default_value=(yap_expr){0}
    };
}

yap_type_id yap_parse_type(yap_source* src, TSNode p_node){
    yap_type_id res = -1;
    TSNode node = ts_node_child(p_node, 0);
    if (ts_node_null_or_error(node)){
        yap_push_parse_error(src, p_node, "Missing type");
        return 0;
    }
    yap_ctx* ctx = src->ctx;
    (void)ctx;
    const char* typ = ts_node_type(node);
    // yap_node_guard(node, yap_type, "Invalid type", src);
    yap_node_val_ctx(node);
    //TODO: We can not allocate this probably, here for dev purposes only
    char* type_str = node_val;
    yap_log("Parsing type: %s of type %s", type_str, typ);
    //TODO: Implement type parsing logic
    strus_switch(typ, "identifier"){
        res = yap_ctx_get_type_id_by_name(ctx, node_val);
        if (res == -1){
            yap_log("Unknown type '%s'", node_val);
            yap_push_parse_error(src, node, "Invalid type");
            return -1;
        }
        yap_log("Parsing identifier as type '%s', got id %u", node_val, res);
        return res;
    }strus_case(typ, "pointer_type"){
        yap_log("Parsing pointer type");
        return yap_parse_pointer_type(src, node);
    }strus_case(typ, "function_type"){
        yap_log("Parsing function type");
        return yap_parse_function_type(src, node);
    }else{
        yap_log_node(src, "Unhandled type", node);
        yap_push_parse_error(src, node, "Unhandled type");
    }
    return 0; //Invalid type
}

yap_type_id yap_parse_function_type(yap_source* src, TSNode node){
    yap_ctx* ctx = src->ctx;
    yap_type_id return_type_id = ctx->void_type_id; //Default return type is void
    yap_node_field_by_name_var(node, return_type);
    darr(yap_type_id) arg_types = NULL;
    if (ts_node_null_or_error(return_type_node)){
        yap_log_node(src, "Function is missing return type annotation, defaulting to void", node);
    }else{
        return_type_id = yap_parse_type(src, return_type_node);
        if (!return_type_id){
            yap_log("Invalid return type in function type");
            yap_push_parse_error(src, return_type_node, "Invalid return type in function type");
            return 0;
        }
    }
    yap_node_field_by_name_var(node, func_type_params);
    if (ts_node_null_or_error(func_type_params_node)){
        yap_log_node(src, "Function is missing parameters, defaulting to none", node);
        arg_types = yap_ctx_darr_new(ctx, yap_type_id, .cap=0, .len=0);
    }else{
        uint32_t param_count = ts_node_named_child_count(func_type_params_node);
        yap_log("Parsing function type with %u parameters", param_count);
        arg_types = yap_ctx_darr_new(ctx, yap_type_id, .cap=param_count, .len=0);
        for_ts_named_children(func_type_params_node, n){
            yap_log_node(src, "Function type parameter", n);
            yap_type_id arg_type_id = yap_parse_type(src, n);
            if (!arg_type_id){
                yap_log("Invalid parameter type in function type");
                yap_push_parse_error(src, n, "Invalid parameter type in function type");
                return 0;
            }
            darr_push(arg_types, arg_type_id);
        }
    }
    yap_type_id res_type_id = yap_ctx_insert_type_if_not_exists(ctx, (yap_type){
        .kind=yap_type_func,
        .func=(yap_fn_type){
            .args=arg_types,
            .return_type=return_type_id
        }
    });
    if (!res_type_id) yap_push_parse_error(src, node, "Failed to create function type");
    return 0;
}

yap_type_id yap_parse_pointer_type(yap_source* src, TSNode node){
    yap_ctx* ctx = src->ctx;
    TSNode subtype_node = ts_node_child(node, 0);
    yap_type_id subtype_id = yap_parse_type(src, subtype_node);
    yap_type_id res_type_id = yap_ctx_insert_type_if_not_exists(ctx, (yap_type){
        .kind=yap_type_ptr,
        .pointer_type=subtype_id
    });
    if (!res_type_id) yap_push_parse_error(src, node, "Failed to create pointer type");
    return res_type_id;
}

yap_block yap_parse_block(yap_source* src, TSNode node){
    yap_ctx* ctx = src->ctx;
    yap_node_guard(node, yap_block, "Invalid block", src);
    yap_node_field_by_name_var_check_push(node, opening_bracket, yap_block, "Missing opening bracket in block", src);
    yap_node_field_by_name_var_check_push(node, closing_bracket, yap_block, "Missing closing bracket in block", src);
    int i = 0;
    uint32_t statement_count = ts_node_named_child_count(node); //Only count named children as statements
    yap_log("Parsing block: %u statements, type: %s, errors: %s", statement_count, ts_node_type(node), ts_node_null_or_error(node) ? "yes" : "no");
    darr(yap_statement) statements = yap_ctx_darr_new(ctx, yap_statement, .cap=statement_count, .len=0);
    for_ts_named_children(node, n){
        yap_log_node(src, "Statement", n);
        yap_statement st = yap_parse_statement(src, n);
        darr_push(statements, st);
        i++;
    }
    yap_log("Parsed %d statements in block", i);
    return (yap_block){
        .kind=yap_block_valid,
        .statements=statements
    };
}

yap_statement yap_parse_statement(yap_source* src, TSNode node){
    yap_node_guard(node, yap_statement, "Invalid statement", src);
    yap_statement ret = yap_error_result(yap_statement, "Invalid statement");
    const char* typ = ts_node_type(node);
    yap_log("Parsing statement of type '%s'", typ);
    strus_switch(typ, "expr_statement"){
        ret = yap_parse_expr_statement(src, node);
    }strus_case(typ, "empty_statement"){
        ret = (yap_statement){
            .kind=yap_statement_empty
        };
    }strus_case(typ, "var_decl"){
        ret = yap_parse_var_decl(src, node);
    }else{
        yap_log_node(src, "Unhandled statement", node);
        yap_push_parse_error(src, node, "Unhandled statement in block");
        ret = yap_ts_error_result_node(yap_statement, "Unhandled statement in block", src, node);
    }
    return ret;
}

yap_statement yap_parse_var_decl(yap_source* src, TSNode node){
    yap_node_guard(node, yap_statement, "Invalid variable declaration", src);
    yap_node_field_by_name_var_check_push(node, name, yap_statement, "Missing variable name in declaration", src);
    yap_node_field_by_name_var_check_push(node, value, yap_statement, "Missing variable value in declaration", src);
    yap_node_val_ctx(name_node);
    yap_node_val_ctx(value_node);
    yap_log("Parsing variable declaration: %s := %s", name_node_val, value_node_val);
    //Get assignment value expression
    yap_expr value_expr = yap_parse_expr(src, value_node);
    if (value_expr.kind == yap_expr_error){
        yap_log("Invalid variable initializer expression");
        return yap_error_result(yap_statement, "Invalid variable initializer expression");
    }
    //Get type from the initializer expression.
    yap_type_id var_type_id = yap_ctx_coerce_type_id_to_id(src->ctx, value_expr.type);
    char* name = yap_ctx_strus_cpy(src->ctx, name_node_val);
    yap_statement res = (yap_statement){
        .kind=yap_statement_var_decl,
        .var_decl=(yap_var_decl){
            .var=(yap_var){
                .name=name,
                .type=var_type_id
            },
            .expr=value_expr
        }
    };
    yap_log("Declared variable '%s' of type id %d", name, var_type_id);
    yap_scope* scope = yap_ctx_current_scope(src->ctx);
    yap_scope_set_var(scope, res.var_decl.var);
    return res;
}

yap_statement yap_parse_expr_statement(yap_source* src, TSNode node){
    yap_node_guard(node, yap_statement, "Invalid expression statement", src);
    TSNode expr_node = ts_node_child(node, 0);
    yap_log("Parsing expression statement");
    if (ts_node_null_or_error(expr_node)){
        yap_push_parse_error(src, node, "Missing expression in statement");
        return yap_error_result(yap_statement, "Missing expression");
    }
    yap_expr expr = yap_parse_expr(src, expr_node);
    if (expr.kind == yap_expr_error){
        yap_log("Expression statement contains invalid expression");
        return yap_error_result(yap_statement, "Invalid expression statement");
    }
    return (yap_statement){
        .kind=yap_statement_expr,
        .expr=expr
    };
}

yap_expr yap_parse_expr(yap_source* src, TSNode node){
    yap_node_guard(node, yap_expr, "Invalid expression", src);
    const char* typ = ts_node_type(node);
    yap_log("Parsing expression of type '%s'", typ);
    // yap_node_val_ctx(node);
    yap_expr ret = yap_error_result(yap_expr, "Invalid expression");
    strus_switch(typ, "literal"){
        ret = yap_parse_literal(src, node);
    }strus_case(typ, "bin_expr"){
        ret = yap_parse_bin_expr(src, node);
    }strus_case(typ, "assignment"){
        ret = (yap_expr){
            .kind=yap_expr_assignment,
            .assignment=yap_parse_assignment(src, node)
        };
    }strus_case(typ, "identifier"){ //variable access
        ret = yap_parse_var_access(src, node);
    }else{
        yap_log_node(src, "Unhandled expression", node);
        yap_push_parse_error(src, node, "Invalid expression");
        ret = yap_ts_error_result_node(yap_expr, "Invalid expression", src, node);
    }
    if (ret.kind == yap_expr_assignment && ret.assignment.kind == yap_assignment_error){
        return yap_error_result(yap_expr, "Invalid assignment expression");
    }
    return ret;
}

yap_expr yap_parse_var_access(yap_source* src, TSNode node){
    yap_ctx *ctx = src->ctx;
    yap_node_guard(node, yap_expr, "Invalid variable access", src);
    yap_node_val_ctx(node);
    const char* typ = ts_node_type(node);
    yap_log("Parsing variable access of type '%s'", typ);
    if (!strus_eq(typ, "identifier")){
        yap_log_node(src, "Expected identifier for variable access", node);
        yap_push_parse_error(src, node, "Expected identifier for variable access");
        return yap_error_result(yap_expr, "Expected identifier for variable access");
    }
    yap_scope* scope = darr_last(ctx->scopes);
    const yap_var* var = yap_scope_get_var_recursive(scope, node_val);
    yap_log("Variable '%s': %s", node_val, var ? var->name : "not found");
    if (!var){
        yap_push_parse_error(src, node, "Undefined variable");
        return yap_error_result(yap_expr, "Undefined variable");
    }
    // (void)var;
    return (yap_expr){
        .kind=yap_expr_var,
        .type=var->type,
        .is_lvalue=true,
        //TODO: Determine if variable is comptime or not, currently all variables are non-comptime
        .is_comptime=false,
    };
}

yap_expr yap_parse_literal(yap_source* src, TSNode p_node){
    //TODO: Handle different literal types, currently only numerical literals are supported and treated as untyped ints
    yap_node_guard(p_node, yap_expr, "Invalid literal", src);
    TSNode node = ts_node_child(p_node, 0);
    if (ts_node_null_or_error(node)){
        yap_push_parse_error(src, p_node, "Missing literal value");
        return yap_error_result(yap_expr, "Missing literal value");
    }
    const char* typ = ts_node_type(node);
    yap_node_val_ctx(node);
    yap_log("Parsing literal of type '%s'", typ);

    int kind = yap_literal_error;
    yap_expr res = (yap_expr){.kind = yap_expr_error};
    yap_ctx* ctx = src->ctx;
    
    strus_switch(typ, "num_literal"){
        kind = yap_literal_numerical;
        res.type = ctx->untyped_int_type_id;
    }else{
        yap_log_node(src, "Unhandled literal", node);
        yap_push_parse_error(src, node, "Unhandled literal type");
        return yap_error_result(yap_expr, "Unhandled literal type");
    }
    yap_literal lit = (yap_literal){
      .kind = kind,
      .text = yap_ctx_strus_cpy(ctx, node_val)
    };
    return (yap_expr){
        .kind=yap_expr_literal,
        .literal=lit,
        .type=res.type,
        .is_comptime=true,
        .is_lvalue=false
    };
}

yap_expr yap_parse_bin_expr(yap_source* src, TSNode node){
    yap_ctx* ctx = src->ctx;
    yap_node_guard(node, yap_expr, "Invalid binary expression", src);
    yap_log("Parsing binary expression");
    yap_node_field_by_name_var_check_push(node, left, yap_expr, "Missing left side expression of a binary operation", src);
    yap_node_field_by_name_var_check_push(node, right, yap_expr, "Missing right side expression of a binary operation", src);
    yap_node_field_by_name_var_check_push(node, operator, yap_expr, "Missing binary operator", src);
    char op = *yap_node_val_start(src, operator_node);
    yap_expr left_expr = yap_parse_expr(src, left_node);
    yap_expr right_expr = yap_parse_expr(src, right_node);
    if (left_expr.kind == yap_expr_error || right_expr.kind == yap_expr_error){
        yap_log("Binary expression contains invalid operand(s)");
        return yap_error_result(yap_expr, "Invalid binary expression");
    }
    if (!strchr("+-*/%", op)){
        yap_push_parse_error(src, operator_node, "Unsupported binary operator");
        return yap_error_result(yap_expr, "Unsupported binary operator");
    }
    //TODO: Check if both types are compatible and figure out the resulting type
    bool types_compatible = yap_ctx_type_ids_eq(src->ctx, left_expr.type, right_expr.type);
    if (!types_compatible){
        yap_push_parse_error(src, node, "Incompatible types in binary expression");
        return yap_error_result(yap_expr, "Incompatible types in binary expression");
    }
    yap_type_id result_type = yap_ctx_coerce_type_id_to_id(src->ctx, left_expr.type);
    return (yap_expr){
        .kind=yap_expr_bin,
        .bin_expr=(yap_bin_expr){
            .kind=op,
            .left=yap_ctx_one_cpy(ctx, left_expr),
            .right=yap_ctx_one_cpy(ctx, right_expr)
        },
        .is_comptime=left_expr.is_comptime && right_expr.is_comptime,
        .is_lvalue=false,
        .type=result_type
    };
}

yap_assignment yap_parse_assignment(yap_source* src, TSNode node){
    yap_ctx* ctx = src->ctx;
    yap_node_guard(node, yap_assignment, "Invalid assignment", src);
    yap_log("Parsing assignment expression");
    yap_node_field_by_name_var_check_push(node, left, yap_assignment, "Missing left side of assignment", src);
    yap_node_field_by_name_var_check_push(node, operator, yap_assignment, "Missing operator in assignment", src);
    yap_node_field_by_name_var_check_push(node, right, yap_assignment, "Missing expression in assignment", src);
    yap_expr left = yap_parse_expr(src, left_node);
    yap_expr right = yap_parse_expr(src, right_node);
    if (left.kind == yap_expr_error || right.kind == yap_expr_error){
        yap_log("Assignment expression contains invalid side(s)");
        return yap_error_result(yap_assignment, "Invalid assignment");
    }
    if (!left.is_lvalue){
        yap_push_parse_error(src, left_node, "Left side of assignment must be an lvalue");
        return yap_error_result(yap_assignment, "Left side of assignment must be an lvalue");
    }
    //TODO: Check if operator is supported and figure out the resulting type (e.g. for += operator)
    //TODO: Check if types are compatible for the assignment
    char* op_str = yap_node_get_val_ctx(src, operator_node);
    char op = op_str[0];
    yap_log("Assignment operator: %s", op_str);
    return (yap_assignment){
        .kind=yap_assignment_valid,
        .left=yap_ctx_one_cpy(ctx, left),
        .right=yap_ctx_one_cpy(ctx, right),
        .op=op,
    };
}
