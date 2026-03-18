#include "parse.h"
#include "error.h"
#include "node.h"
#include "tree_sitter/api.h"
#include "ts_yap.h"
#include "utils/utils.h"

static void yap_push_parse_error(yap_source* src, TSNode node, const char* msg){
    yap_log("%s [type=%s%s]", msg, ts_node_type(node), ts_node_has_error(node) ? ", has_error" : "");
    yap_ctx_push_error(src->ctx, yap_node_error(src, node, (char*)msg));
}

static void yap_log_node(yap_source* src, const char* prefix, TSNode node){
    char* node_val = yap_node_get_val(src, node);
    yap_log("%s type='%s' value='%s'%s", prefix, ts_node_type(node), node_val, ts_node_has_error(node) ? ", has error" : "");
    free(node_val);
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
    darr(yap_def) defs = darr_new(yap_def);
    if (ts_node_error_or_null(node)){
        yap_push_parse_error(src, node, "Expected source file root");
        return (yap_source_code){
            .definitions=defs
        };
    }
    const char* typ = ts_node_type(node);
    yap_log("Parsing source root of type '%s'", typ);
    if (strus_eq(typ, "source")){
        for_ts_named_children(node, n){
            darr_push(defs, yap_parse_decl(src, n));
        }
    }else{
        yap_push_parse_error(src, node, "Expected source file root");
    }
    return (yap_source_code){
        .definitions=defs
    };
}

yap_def yap_parse_decl(yap_source *src, TSNode node){
    yap_node_guard(node, yap_def, "Invalid declaration", src);
    const char* typ = ts_node_type(node);
    yap_log("Parsing declaration: %s", typ);
    strus_switch(typ, "function_declaration"){
        return yap_parse_fn_def(src, node);
    }else{
        yap_log_node(src, "Unhandled declaration", node);
        yap_push_parse_error(src, node, "Unhandled declaration");
        return yap_error_result(yap_def, "Unhandled declaration");
    }
}

yap_def yap_parse_fn_def(yap_source *src, TSNode node){
    yap_node_guard(node, yap_def, "Invalid function declaration", src);
    yap_node_field_by_name_var_check_push(node, name, yap_def, "Missing function name", src);
    yap_node_field_by_name_var_check_push(node, body, yap_def, "Missing function body", src);
    yap_node_val(body_node);

    yap_node_val(name_node);
    yap_node_start_point(node);
    char* pos_str = yap_pos_string(*src, node_start_point.row, node_start_point.column);
    yap_log("Parsing function\n\t\t%s\n\t\tat %s", name_node_val, pos_str);
    free(pos_str);
    // const char* typ = ts_node_type(node);
    // yap_log("fn def type: %s", typ);
    yap_log(
        "name:%s\nbody:%s\n",
        name_node_val, body_node_val
    );
    yap_block body = yap_parse_block(src, body_node);
    free(body_node_val);
    free(name_node_val);
    return (yap_def){
        .kind=yap_def_func,
        .func_def=(yap_func_def){
            .args=darr_new(int),
            //TODO: FIX! Get argument types and names from the parameter list
            .ret_typ=0,
            .body=body
        }
    };
}

yap_block yap_parse_block(yap_source* src, TSNode node){
    yap_node_guard(node, yap_block, "Invalid block", src);
    yap_log("Parsing block");
    yap_node_field_by_name_var_check_push(node, opening_bracket, yap_block, "Missing opening bracket in block", src);
    yap_node_field_by_name_var_check_push(node, closing_bracket, yap_block, "Missing closing bracket in block", src);
    darr(yap_statement) statements = darr_new(yap_statement);
    int i = 0;
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
    yap_node_val(node);
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
    free(node_val);
    return ret;
}

yap_statement yap_parse_var_decl(yap_source* src, TSNode node){
    yap_node_guard(node, yap_statement, "Invalid variable declaration", src);
    yap_node_field_by_name_var_check_push(node, name, yap_statement, "Missing variable name in declaration", src);
    yap_node_field_by_name_var_check_push(node, value, yap_statement, "Missing variable value in declaration", src);
    yap_node_val(name_node);
    yap_node_val(value_node);
    yap_log("Parsing variable declaration: %s := %s", name_node_val, value_node_val);
    //Get assignment value expression
    yap_expr value_expr = yap_parse_expr(src, value_node);
    if (value_expr.kind == yap_expr_error){
        yap_log("Invalid variable initializer expression");
        free(name_node_val);
        free(value_node_val);
        return yap_error_result(yap_statement, "Invalid variable initializer expression");
    }
    //Get type from the initializer expression.
    yap_type_id var_type_id = yap_ctx_coerce_type_id_to_id(src->ctx, value_expr.type);
    yap_statement res = (yap_statement){
        .kind=yap_statement_var_decl,
        .var_decl=(yap_var_decl){
            .var=(yap_var){
                .name=name_node_val,
                .type=var_type_id
            },
            .expr=value_expr
        }
    };
    yap_log("Declared variable '%s' of type id %d", name_node_val, var_type_id);
    yap_scope* scope = yap_ctx_current_scope(src->ctx);
    yap_scope_set_var(scope, res.var_decl.var);
    free(value_node_val);
    return res;
}

yap_statement yap_parse_expr_statement(yap_source* src, TSNode node){
    yap_node_guard(node, yap_statement, "Invalid expression statement", src);
    TSNode expr_node = ts_node_child(node, 0);
    yap_log("Parsing expression statement");
    if (ts_node_error_or_null(expr_node)){
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
    yap_node_val(node);
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
    free(node_val);
    if (ret.kind == yap_expr_assignment && ret.assignment.kind == yap_assignment_error){
        return yap_error_result(yap_expr, "Invalid assignment expression");
    }
    return ret;
}

yap_expr yap_parse_var_access(yap_source* src, TSNode node){
    yap_ctx *ctx = src->ctx;
    yap_node_guard(node, yap_expr, "Invalid variable access", src);
    yap_node_val(node);
    const char* typ = ts_node_type(node);
    yap_log("Parsing variable access of type '%s'", typ);
    if (!strus_eq(typ, "identifier")){
        yap_log_node(src, "Expected identifier for variable access", node);
        yap_push_parse_error(src, node, "Expected identifier for variable access");
        free(node_val);
        return yap_error_result(yap_expr, "Expected identifier for variable access");
    }
    yap_scope* scope = darr_last(ctx->scopes);
    const yap_var* var = yap_scope_get_var_recursive(scope, node_val);
    yap_log("Variable '%s': %s", node_val, var ? var->name : "not found");
    if (!var){
        yap_push_parse_error(src, node, "Undefined variable");
        free(node_val);
        return yap_error_result(yap_expr, "Undefined variable");
    }
    // (void)var;
    free(node_val);
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
    if (ts_node_error_or_null(node)){
        yap_push_parse_error(src, p_node, "Missing literal value");
        return yap_error_result(yap_expr, "Missing literal value");
    }
    const char* typ = ts_node_type(node);
    yap_node_val(node);
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
        free(node_val);
        return yap_error_result(yap_expr, "Unhandled literal type");
    }
    yap_literal lit = (yap_literal){
      .kind = kind,
      .text = node_val
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
            .left=mem_one_cpy(left_expr),
            .right=mem_one_cpy(right_expr)
        },
        .is_comptime=left_expr.is_comptime && right_expr.is_comptime,
        .is_lvalue=false,
        .type=result_type
    };
}

yap_assignment yap_parse_assignment(yap_source* src, TSNode node){
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
    char* op_str = yap_node_get_val(src, operator_node);
    char op = op_str[0];
    yap_log("Assignment operator: %s", op_str);
    free(op_str);
    return (yap_assignment){
        .kind=yap_assignment_valid,
        .left=mem_one_cpy(left),
        .right=mem_one_cpy(right),
        .op=op,
    };
}
