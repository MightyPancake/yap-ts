#include "parse.h"
#include "error.h"
#include "node.h"
#include "tree_sitter/api.h"
#include "ts_yap.h"
#include "utils/utils.h"
#include <stdint.h>
#include <string.h>

static void yap_push_parse_error(yap_source* src, TSNode node, const char* fmt, ...){
    if (!src || !src->ctx || !fmt) return;

    va_list ap;
    va_start(ap, fmt);
    char* msg = NULL;
    int fmt_res = vasprintf(&msg, fmt, ap);
    va_end(ap);

    if (fmt_res < 0 || !msg){
        msg = strus_copy("(failed to format parse error)");
    }

    yap_log("%s [type=%s%s]", msg, ts_node_type(node), ts_node_has_error(node) ? ", has_error" : "");
    yap_ctx_push_error(src->ctx, yap_node_error(src, node, msg));
    free(msg);
}

static void yap_log_node(yap_source* src, const char* prefix, TSNode node){
    char* node_val = yap_node_get_val_ctx(src, node);
    yap_log("%s type='%s' value='%s'%s", prefix, ts_node_type(node), node_val, ts_node_has_error(node) ? ", has error" : "");
}

static uint32_t yap_collect_ts_syntax_errors(yap_source* src, TSNode node){
    uint32_t errors_found = 0;

    if (ts_node_is_missing(node)){
        const char* expected = ts_node_type(node);
        char* msg = strus_newf("Syntax error: expected '%s'", expected);
        yap_ctx_push_error(src->ctx, yap_node_error(src, node, msg));
        free(msg);
        errors_found++;
    }else if (ts_node_is_error(node)){
        char* found = yap_node_get_val_ctx(src, node);
        if (found && found[0] != '\0'){
            char* msg = strus_newf("Syntax error near '%s'", found);
            yap_ctx_push_error(src->ctx, yap_node_error(src, node, msg));
            free(msg);
        }else{
            yap_ctx_push_error(src->ctx, yap_node_error(src, node, "Syntax error"));
        }
        errors_found++;
    }

    for_ts_children(node, child){
        errors_found += yap_collect_ts_syntax_errors(src, child);
    }

    return errors_found;
}

yap_ctx* yap_parse(yap_ctx* ctx, yap_args args){
    yap_parser* parser = yap_new_parser(ctx);
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
    uint32_t syntax_errors = yap_collect_ts_syntax_errors(src, node);
    if (syntax_errors > 0){
        yap_log("Collected %u tree-sitter syntax error(s)", syntax_errors);
    }
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
        //Pass 1: Collect all declarations and register them in the context, this allows for recursive declarations and mutual recursion. We only collect function declarations, other declarations are not registered in the context and can not be mutually recursive.
        for_ts_named_children(node, n){
            yap_parse_top_level_declaration(src, n);
        }
        //Pass 2: Do the actual parsing
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

void yap_parse_top_level_declaration(yap_source* src, TSNode node){
    const char* typ = ts_node_type(node);
    yap_log("Parsing top-level declaration of type '%s'", typ);
    strus_switch(typ, "function_declaration"){
        yap_parse_top_level_func_decl(src, node);
    }
}

void yap_parse_top_level_func_decl(yap_source *src, TSNode node){
    yap_ctx* ctx = src->ctx;
    yap_node_untyped_guard(node, "Invalid function declaration", src);
    yap_node_field_by_name_var_untyped_check(node, name, "Missing function name", src);
    yap_node_field_by_name_var_untyped_check(node, body, "Missing function body", src);
    yap_node_field_by_name_var_untyped_check(node, return_type, "Missing return type", src);
    yap_node_field_var(args_node, node, "args");
    // yap_node_field_by_name_var_untyped_check(node, args, "Missing function arguments", src);
    // LOGGING
    yap_node_val_ctx(name_node);
    yap_node_start_point(node);
    char* pos_str = yap_pos_string(*src, node_start_point.row, node_start_point.column);
    yap_log("Parsing function named %s at %s", name_node_val, pos_str);
    free(pos_str);
    // END LOGGING
    yap_type_id return_type = yap_parse_type(src, return_type_node);
    if (return_type == 0){
        yap_push_parse_error(src, return_type_node, "Invalid return type in function declaration");
        return;
    }
    darr(yap_func_arg) args = yap_parse_fn_args(src, args_node);
    //TODO: Check if arguments are valid
    //Register a new scope for the function body
    darr(yap_type_id) arg_type_ids = yap_ctx_darr_new(ctx, yap_type_id, .cap=darr_len(args), .len=0);
    darr(char*) arg_names = yap_ctx_darr_new(ctx, char*, .cap=darr_len(args), .len=0);
    for_darr(i, arg, args){
        darr_push(arg_type_ids, arg.type);
        darr_push(arg_names, arg.name);
    }
    yap_type func_type = {
        .kind=yap_type_func,
        .func=(yap_fn_type){
            .args=arg_type_ids,
            .return_type=return_type
        }
    };
    //TODO: Register arg names/defaults in the global context!
    //Register the function variable in the current scope
    yap_log("Registering function '%s' in global scope", name_node_val);
    yap_var func_var = {
        .name=name_node_val,
        .type=yap_ctx_insert_type_if_not_exists(ctx, func_type)
    };
    yap_ctx_push_var(ctx, func_var);
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
    yap_ctx* ctx = src->ctx;
    yap_node_guard(node, yap_decl, "Invalid function declaration", src);
    yap_node_field_by_name_var_check_push(node, name, yap_decl, "Missing function name", src);
    // yap_node_field_by_name_var_check_push(node, args, yap_decl, "Missing function arguments", src);
    yap_node_field_var(args_node, node, "args");
    yap_node_field_by_name_var_check_push(node, body, yap_decl, "Missing function body", src);
    
    yap_node_val_ctx(name_node);
    yap_log("Parsing function body for '%s'", name_node_val);
    
    // Get function var from global scope (signature already registered in pass 1)
    const yap_var *func_var = yap_scope_get_var_recursive(yap_ctx_current_scope(ctx), name_node_val);
    if (!func_var){
        if (darr_len(ctx->errors) == 0){
            yap_push_parse_error(src, node, "Internal parser error: function '%s' was not registered in pass 1", name_node_val);
        }else{
            yap_log("Skipping function '%s' in pass 2 because declaration pass already reported errors", name_node_val);
        }
        return yap_error_result(yap_decl, "Skipped function declaration after previous errors");
    }
    
    // Get function type (already parsed in pass 1)
    yap_type* func_type_ptr = yap_ctx_get_type(ctx, func_var->type);
    
    yap_fn_type fn_type = func_type_ptr->func;
    
    // Parse arguments (needed for populating local function scope aka body)
    darr(yap_func_arg) args = yap_parse_fn_args(src, args_node);
    
    // Push a new function scope and populate it with arguments
    yap_scope* func_scope = yap_ctx_push_new_scope(ctx);
    for_darr(i, arg, args){
        yap_var var = (yap_var){
            .name=arg.name,
            .type=arg.type
        };
        yap_scope_set_var(func_scope, var);
    }
    
    // Parse body
    yap_block body = yap_parse_block(src, body_node);
    
    // Pop function scope
    yap_ctx_pop_scope(ctx);
    
    return (yap_decl){
        .kind=yap_decl_func,
        .func_decl=(yap_func_decl){
            .name=name_node_val,
            .args=args,
            .ret_typ=fn_type.return_type,
            .body=body
        }
    };
}

darr(yap_func_arg) yap_parse_fn_args(yap_source* src, TSNode node){
    yap_ctx* ctx = src->ctx;
    if (ts_node_is_null(node)){
        return yap_ctx_darr_new(ctx, yap_func_arg, .cap=0, .len=0);
    }
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
        if (!res){
            yap_log("Unknown type '%s'", node_val);
            yap_push_parse_error(src, node, "Invalid type");
            return 0;
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
    return res_type_id;
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
    yap_ctx_push_new_scope(ctx); //Push a new scope for the block, this allows for variable declarations inside the block that do not affect the outer scope
    // Parse statements
    for_ts_named_children(node, n){
        yap_log_node(src, "Statement", n);
        yap_statement st = yap_parse_statement(src, n);
        if (st.kind == yap_statement_error){
            yap_log("Error parsing statement in block, skipping");
            return (yap_block){
                .kind=yap_block_error,
                .statements=NULL
            };
        }
        darr_push(statements, st);
        i++;
    }
    yap_ctx_pop_scope(ctx); //Pop the block scope after parsing
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
        ret = yap_parse_empty_statement(src, node);
    }strus_case(typ, "var_decl"){
        ret = yap_parse_var_decl(src, node);
    }strus_case(typ, "return_statement"){
        ret = yap_parse_return_statement(src, node);
    }strus_case(typ, "if_statement"){
        ret = yap_parse_if_statement(src, node);
    }strus_case(typ, "if_else_statement"){
        ret = yap_parse_if_else_statement(src, node);
    }strus_case(typ, "while_loop"){
        ret = yap_parse_while_loop(src, node);
    }strus_case(typ, "for_loop"){
        ret = yap_parse_for_loop(src, node);
    }strus_case(typ, "block"){
        ret = yap_parse_block_statement(src, node);
    }strus_case(typ, "break_statement"){
        ret = yap_parse_break_statement(src, node);
    }strus_case(typ, "continue_statement"){
        ret = yap_parse_continue_statement(src, node);
    }else{
        yap_log_node(src, "Unhandled statement", node);
        yap_push_parse_error(src, node, "Unhandled statement");
        ret = yap_ts_error_result_node(yap_statement, "Unhandled statement", src, node);
    }
    return ret;
}

yap_statement yap_parse_continue_statement(yap_source* src, TSNode node){
    yap_ctx* ctx = src->ctx;
    if (!yap_scope_in_loop(yap_ctx_current_scope(ctx))){
        yap_push_parse_error(src, node, "Continue statement not inside a loop");
        return yap_error_result(yap_statement, "Continue statement not inside a loop");
    }
    return (yap_statement){
        .kind=yap_statement_continue
    };
}

yap_statement yap_parse_break_statement(yap_source* src, TSNode node){
    yap_ctx* ctx = src->ctx;
    if (!yap_scope_in_loop(yap_ctx_current_scope(ctx))){
        yap_push_parse_error(src, node, "Break statement not inside a loop");
        return yap_error_result(yap_statement, "Break statement not inside a loop");
    }
    return (yap_statement){
        .kind=yap_statement_break
    };
}

yap_statement yap_parse_block_statement(yap_source* src, TSNode node){
    yap_block block = yap_parse_block(src, node);
    if (block.kind == yap_block_error){
        return yap_error_result(yap_statement, "Invalid block statement");
    }
    return (yap_statement){
        .kind=yap_statement_block,
        .block=block
    };
}

yap_statement yap_parse_for_loop(yap_source* src, TSNode node){
    yap_ctx* ctx = src->ctx;
    yap_log("Parsing for loop");
    yap_node_guard(node, yap_statement, "Invalid for loop", src);
    yap_node_field_by_name_var_check_push(node, init, yap_statement, "Missing initializer in for loop", src);
    yap_node_field_by_name_var_check_push(node, condition, yap_statement, "Missing condition in for loop", src);
    yap_node_field_by_name_var_check_push(node, update, yap_statement, "Missing update in for loop", src);
    yap_node_field_by_name_var_check_push(node, body, yap_statement, "Missing body in for loop", src);
    yap_ctx_push_new_loop_scope(ctx); //Push a new loop scope for the for loop, this allows for break/continue statements inside the loop body to work correctly
    yap_statement init = yap_parse_statement(src, init_node);
    yap_return_if_error_kind(yap_statement, yap_statement, init, "Invalid initializer statement in for loop");
    yap_expr condition = yap_parse_expr(src, condition_node);
    yap_return_if_error_kind(yap_statement, yap_expr, condition, "Invalid condition expression in for loop");
    yap_expr update = yap_parse_expr(src, update_node);
    yap_return_if_error_kind(yap_statement, yap_expr, update, "Invalid update expression in for loop");
    yap_statement body = yap_parse_statement(src, body_node);
    yap_return_if_error_kind(yap_statement, yap_statement, body, "Invalid body statement in for loop");
    yap_ctx_pop_scope(ctx); //Pop the loop scope after parsing the for loop
    return (yap_statement){
        .kind=yap_statement_for,
        .for_stmt=(yap_for){
            .init=yap_ctx_one_cpy(ctx, init),
            .condition=condition,
            .update=yap_ctx_one_cpy(ctx, update),
            .body=yap_ctx_one_cpy(ctx, body)
        }
    };
}

yap_statement yap_parse_while_loop(yap_source* src, TSNode node){
    yap_ctx* ctx = src->ctx;
    yap_log("Parsing while loop");
    yap_node_guard(node, yap_statement, "Invalid while loop", src);
    yap_node_field_by_name_var_check_push(node, condition, yap_statement, "Missing condition in while loop", src);
    yap_node_field_by_name_var_check_push(node, body, yap_statement, "Missing body in while loop", src);
    yap_ctx_push_new_loop_scope(ctx);
    yap_expr condition = yap_parse_expr(src, condition_node);
    yap_return_if_error_kind(yap_statement, yap_expr, condition, "Invalid condition expression in while loop");
    yap_statement body = yap_parse_statement(src, body_node);
    yap_return_if_error_kind(yap_statement, yap_statement, body, "Invalid body statement in while loop");
    yap_ctx_pop_scope(ctx);
    return (yap_statement){
        .kind=yap_statement_while,
        .while_stmt=(yap_while){
            .condition=condition,
            .body=yap_ctx_one_cpy(ctx, body)
        }
    };
}

yap_statement yap_parse_if_statement(yap_source* src, TSNode node){
    yap_ctx* ctx = src->ctx;
    yap_node_guard(node, yap_statement, "Invalid if statement", src);
    yap_node_field_by_name_var_check_push(node, condition, yap_statement, "Missing condition in if statement", src);
    yap_node_field_by_name_var_check_push(node, then_branch, yap_statement, "Missing then branch in if statement", src);
    yap_expr condition = yap_parse_expr(src, condition_node);
    yap_return_if_error_kind(yap_statement, yap_expr, condition, "Invalid condition expression in if statement");
    yap_statement then_branch = yap_parse_statement(src, then_branch_node);
    yap_return_if_error_kind(yap_statement, yap_statement, then_branch, "Invalid then branch statement in if statement");
    return (yap_statement){
        .kind=yap_statement_if,
        .if_stmt=(yap_if){
            .condition=condition,
            .then_branch=yap_ctx_one_cpy(ctx, then_branch)
        }
    };
}

yap_statement yap_parse_if_else_statement(yap_source* src, TSNode node){
    yap_ctx* ctx = src->ctx;
    yap_node_guard(node, yap_statement, "Invalid if-else statement", src);
    yap_node_field_by_name_var_check_push(node, condition, yap_statement, "Missing condition in if-else statement", src);
    yap_node_field_by_name_var_check_push(node, then_branch, yap_statement, "Missing then branch in if-else statement", src);
    yap_node_field_by_name_var_check_push(node, else_branch, yap_statement, "Missing else branch in if-else statement", src);
    yap_expr condition = yap_parse_expr(src, condition_node);
    yap_return_if_error_kind(yap_statement, yap_expr, condition, "Invalid condition expression in if-else statement");
    yap_statement then_branch = yap_parse_statement(src, then_branch_node);
    yap_return_if_error_kind(yap_statement, yap_statement, then_branch, "Invalid then branch statement in if-else statement");
    yap_statement else_branch = yap_parse_statement(src, else_branch_node);
    yap_return_if_error_kind(yap_statement, yap_statement, else_branch, "Invalid else branch statement in if-else statement");
    return (yap_statement){
        .kind=yap_statement_if_else,
        .if_else_stmt=(yap_if_else){
            .condition=condition,
            .then_branch=yap_ctx_one_cpy(ctx, then_branch),
            .else_branch=yap_ctx_one_cpy(ctx, else_branch)
        }
    };
}

yap_statement yap_parse_empty_statement(yap_source* src, TSNode node){
    yap_log("Parsing empty statement");
    return (yap_statement){
        .kind=yap_statement_empty
    };
}

yap_statement yap_parse_return_statement(yap_source* src, TSNode node){
    yap_ctx* ctx = src->ctx;
    yap_node_guard(node, yap_statement, "Invalid return statement", src);
    yap_node_field_var(return_value_node, node, "value");
    if (ts_node_null_or_error(return_value_node)){
        yap_log("Parsing return statement with no return value");
        return (yap_statement){
            .kind=yap_statement_return,
            .return_stmt=(yap_return_statement){
                .value=(yap_expr){
                    .type=ctx->void_type_id
                }
            }
        };
    }
    yap_log("Parsing return statement with return value");
    yap_expr return_value = yap_parse_expr(src, return_value_node);
    yap_return_if_error_kind(yap_statement, yap_expr, return_value, "Invalid return value expression in return statement");
    return (yap_statement){
        .kind=yap_statement_return,
        .return_stmt=(yap_return_statement){
            .value=return_value
        }
    };
}

yap_statement yap_parse_var_decl(yap_source* src, TSNode node){
    yap_ctx* ctx = src->ctx;
    yap_node_guard(node, yap_statement, "Invalid variable declaration", src);
    yap_node_field_by_name_var_check_push(node, name, yap_statement, "Missing variable name in declaration", src);
    yap_node_field_by_name_var_check_push(node, value, yap_statement, "Missing variable value in declaration", src);
    yap_node_val_ctx(name_node);
    yap_node_val_ctx(value_node);
    yap_log("Parsing variable declaration: %s := %s", name_node_val, value_node_val);
    //Get assignment value expression
    yap_expr value_expr = yap_parse_expr(src, value_node);
    yap_return_if_error_kind(yap_statement, yap_expr, value_expr, "Invalid variable initializer expression");
    //Get type from the initializer expression.
    yap_type_id var_type_id = yap_ctx_coerce_type_id_to_id(ctx, value_expr.type);
    char* name = yap_ctx_strus_cpy(ctx, name_node_val);
    yap_var var = (yap_var){
        .name=name,
        .type=var_type_id
    };
    yap_statement res = (yap_statement){
        .kind=yap_statement_var_decl,
        .var_decl=(yap_var_decl){
            .var=var,
            .init=value_expr
        }
    };
    yap_log("Declared variable '%s' of type id %d", name, var_type_id);
    yap_ctx_push_var(ctx, var);
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
    yap_return_if_error_kind(yap_statement, yap_expr, expr, "Invalid expression statement");
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
    }strus_case(typ, "func_call"){
        ret = yap_parse_func_call(src, node);
    }else{
        yap_log_node(src, "Unhandled expression", node);
        yap_push_parse_error(src, node, "Invalid expression");
        ret = yap_ts_error_result_node(yap_expr, "Invalid expression", src, node);
    }
    // Do additional checks
    if (ret.kind == yap_expr_assignment && ret.assignment.kind == yap_assignment_error){
        ret = yap_error_result(yap_expr, "Invalid assignment expression");
    }
    ret.range = yap_node_get_range(node);
    return ret;
}

yap_expr yap_parse_func_call(yap_source* src, TSNode node){
    //TODO: Rework this parsing; gather params first, then decide what to do with them and emit errors!
    yap_ctx* ctx = src->ctx;
    yap_node_guard(node, yap_expr, "Invalid function call", src);
    // TODO: Parse type of function call expression
    // This *has* to be done to get default and named parameters to work.
    // Right now the compiler just rejects them!
    yap_node_field_by_name_var_check_push(node, func, yap_expr, "Missing function to call", src);
    // yap_node_field_by_name_var_check_push(node, params, yap_expr, "Missing arguments in function call", src);
    yap_node_field_var(params_node, node, "params");
    yap_expr func_expr = yap_parse_expr(src, func_node);
    yap_type* func_type = yap_ctx_get_type(ctx, func_expr.type);
    yap_type_id return_type_id = 0;
    if (func_type && func_type->kind == yap_type_func){
        return_type_id = func_type->func.return_type;
    }else{
        yap_log("Trying to call a non-function type");
        char* func_type_str = yap_ctx_type_id_to_string(ctx, func_expr.type);
        yap_push_parse_error(src, func_node, "Cannot call a non-function type "aesc_reverse("%s"), func_type_str);
        free(func_type_str);
        return yap_error_result(yap_expr, "Cannot call a non-function type");
    }
    darr(yap_type_id) args = func_type->func.args;
    darr(yap_expr) params = NULL;
    if (ts_node_null_or_error(params_node)){
        yap_log_node(src, "Function call is missing arguments, defaulting to none", node);
        params = yap_ctx_darr_new(ctx, yap_expr, .cap=0, .len=0);
    }else{
        uint32_t params_count = ts_node_named_child_count(params_node);
        yap_log("Parsing function call with %u arguments", params_count);
        params = yap_ctx_darr_new(ctx, yap_expr, .cap=darr_len(args), .len=0);
        bool too_many_args = false;
        uint32_t unnamed_params = 0;
        for_ts_named_children(params_node, n){
            yap_log_node(src, "Function call argument", n);
            const char* param_kind = ts_node_type(n);
            strus_switch(param_kind, "unnamed_param"){
                unnamed_params++;
                if (too_many_args) continue;
                TSNode param_expr_node = ts_node_child(n, 0);
                yap_expr param_expr = yap_parse_expr(src, param_expr_node);
                yap_return_if_error_kind(yap_expr, yap_expr, param_expr, "Invalid argument expression in function call");
                //Check if original function type has enough parameters
                if (darr_len(args) <= darr_len(params)){
                    too_many_args = true;
                    continue;
                }
                //Check type missmatch between argument and parameter
                if (!yap_ctx_type_id_compatible(ctx, param_expr.type, args[darr_len(params)])){
                    char* expected_type_str = yap_ctx_type_id_to_string(ctx, args[darr_len(params)]);
                    char* actual_type_str = yap_ctx_type_id_to_string(ctx, param_expr.type);
                    yap_log("Type mismatch in function call argument: expected %s but got %s", expected_type_str, actual_type_str);
                    yap_push_parse_error(src, param_expr_node, "Expected parameter of type "aesc_reverse("%s")" but got "aesc_reverse("%s"), expected_type_str, actual_type_str);
                    free(expected_type_str);
                    free(actual_type_str);
                    return yap_error_result(yap_expr, "Type mismatch in function call argument");
                }
                darr_push(params, param_expr);
            }strus_case(param_kind, "named_param"){
                //TODO: Support named params
                return yap_error_result(yap_expr, "Named parameters are not supported yet");
            }
        }
        if (too_many_args){
            yap_log("Too many arguments in function call, expected at most %u but got %u", darr_len(args), darr_len(params) + 1);
            yap_push_parse_error(src, node, "Expected at most "aesc_reverse("%u")" parameters, but got "aesc_reverse("%u"), darr_len(args), unnamed_params);
            return yap_error_result(yap_expr, "Too many arguments in function call");
        }
    }
    return (yap_expr){
        .kind=yap_expr_func_call,
        .func_call=(yap_func_call){
            .func_expr=yap_ctx_one_cpy(ctx, func_expr),
            .params=params
        },
        .type = return_type_id,
        .is_comptime=false,
        .is_lvalue=false
    };
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
    return (yap_expr){
        .var_name=var->name,
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
            .op=op,
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
