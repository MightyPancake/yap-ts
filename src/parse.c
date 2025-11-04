#include "parse.h"
#include "error.h"
#include "node.h"
#include "tree_sitter/api.h"
#include "ts_yap.h"
#include "utils/utils.h"

yap_state* yap_parse(yap_args args){
    yap_parser* parser = yap_new_parser();
    if (darr_empty(args.extra)){
        printf("No sources!\n");
        exit(1);
    }
    yap_parser_parse_file(parser, darr_first(char*, args.extra));
    yap_state* ret = parser->state;
    yap_free_parser(parser);
    return ret;
}

yap_source_code yap_parse_source_file(yap_source* src, TSNode node){
    const char* typ = ts_node_type(node);
    darr defs = darr_new(yap_def, 64);
    if (strus_eq(typ, "source")){
     for_ts_children(node, n){
         darr_push(yap_def, defs, yap_parse_def(src, n));
     }
    }else{
        yap_log("Error! Not valid source");
    }
    //TODO: Remove this; just for testing
    // yap_ts_print_error((yap_error){
    //     .kind=yap_error_pos,
    //     .src=src,
    //     .pos=(yap_code_pos){
    //         .line=3,
    //         .column=2,
    //         .offset=20
    //     },
    //     .msg="Missing programmer's brain"
    // });

    return (yap_source_code){
        .definitions=defs
    };
}

yap_def yap_parse_def(yap_source *src, TSNode node){
    const char* typ = ts_node_type(node);
    yap_log("Parsing definition: %s", typ);
    strus_switch(typ, "function_definition"){
        return yap_parse_fn_def(src, node);
    }strus_case(typ, "ERROR"){
        return yap_error_result(yap_def, "Invalid definition");
    }else{
        return yap_error_result(yap_def, "Unhandled definition");
    }
}

yap_def yap_parse_fn_def(yap_source *src, TSNode node){
    TSNode name_node = ts_node_child_by_field_name(node, "name", strlen("name"));
    TSNode body_node = ts_node_child_by_field_name(node, "body", strlen("body"));
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
            .args=darr_new(int, 0),
            .ret_typ=(yap_type){},
            .body=body
        }
    };
}

yap_block yap_parse_block(yap_source* src, TSNode node){
    yap_log("Parsing block");
    TSNode opening_bracket = ts_node_child_by_field_name(node, "opening_bracket", strlen("opening_bracket"));
    TSNode closing_bracket = ts_node_child_by_field_name(node, "closing_bracket", strlen("closing_bracket"));
    if (ts_node_error_or_null(opening_bracket)){
        yap_log("No opening brackets!");
    }
    darr statements = darr_new(yap_statement, ts_node_child_count(node)-2);
    int i = 0;
    for (TSNode n = ts_node_next_sibling(opening_bracket); !ts_node_eq(n, closing_bracket); n = ts_node_next_sibling(n)){
        //print?
        char* node_val = yap_node_get_val(src, n);
        yap_log("Statement #%d: %s%s", i, node_val, ts_node_has_error(n) ? ", has error":"");
        free(node_val);
        yap_statement st = yap_parse_statement(src, n);
        if (st.kind == yap_statement_error){
            yap_ts_print_error(yap_node_error(src, n, "Invalid statement"));
        }
        darr_push(yap_statement, statements, st);
        i++;
    }
    return (yap_block){
        .kind=yap_block_valid,
        .statements=statements
    };
}

yap_statement yap_parse_statement(yap_source* src, TSNode p_node){
    TSNode node = ts_node_child(p_node, 0);
    yap_node_val(node);
    yap_statement ret = yap_error_result(yap_statement, "Invalid statement");
    const char* typ = ts_node_type(node);
    strus_switch(typ, "expr_statement"){
        ret = yap_parse_expr_statement(src, node);
    }strus_case(typ, "other_shit"){
        //ret = yap_parse_other_shit_statement();
    }else{
        ret = yap_ts_error_result_node(yap_statement, "Invalid statement in block", src, node);
    }
    free(node_val);
    return ret;
}

yap_statement yap_parse_expr_statement(yap_source* src, TSNode node){
    TSNode semicolon_node = yap_node_get_field(node, "semicolon");
    TSNode expr_node = yap_node_get_field(node, "expr");
    yap_log("Parsing expression statement");
    if (ts_node_is_null(semicolon_node)){
        return yap_error_result(yap_statement, "Missing semicolon");
    }
    //Redundant?
    if (ts_node_is_null(expr_node)){
        return yap_error_result(yap_statement, "Missing expression");
    }
    yap_expr expr = yap_parse_expr(src, expr_node);
    return (yap_statement){
        .kind=yap_statement_expr,
        .expr=expr
    };
}


yap_expr yap_parse_expr(yap_source* src, TSNode p_node){
    yap_log("Parsing expression");
    TSNode node = ts_node_child(p_node, 0);
    const char* typ = ts_node_type(node);
    yap_log("of type %s", typ);
    yap_node_val(node);
    yap_expr ret = yap_error_result(yap_expr, "Invalid expression");
    strus_switch(typ, "literal"){
        ret = (yap_expr){
            .kind=yap_expr_literal,
            .literal=yap_parse_literal(src, node)
        };
    }strus_case(typ, "bin_op"){
        ret = (yap_expr){
            .kind=yap_expr_bin_op,
            .bin_op=yap_parse_bin_op(src, node)
        };
    }strus_case(typ, "assignment"){
        ret = (yap_expr){
            .kind=yap_expr_assignment,
            .assignment=yap_parse_assignment(src, node)
        };
    }else{
        ret = yap_ts_error_result_node(yap_expr, "Invalid expression", src, node);
    }
    free(node_val);
    return ret;
}

yap_literal yap_parse_literal(yap_source* src, TSNode p_node){
    yap_log("Parsing literal");
    //TODO: Implement
    return (yap_literal){};
}

yap_bin_op yap_parse_bin_op(yap_source* src, TSNode node){
    yap_log("Parsing bin op");
    yap_node_field_by_name_var_check(node, left, yap_bin_op, "Missing left side expression of a binary operation");
    yap_node_field_by_name_var_check(node, right, yap_bin_op, "Missing right side expression of a binary operation");
    yap_node_field_by_name_var_check(node, operator, yap_bin_op, "Missing binary operator");
    char op = *yap_node_val_start(src, operator_node);
    yap_expr left_expr = yap_parse_expr(src, left_node);
    yap_expr right_expr = yap_parse_expr(src, right_node);
    //TODO: Remove this   
    // yap_node_error(src, node, "This is an error message!");
    return (yap_bin_op){
        .kind=op,
        .left=mem_one_cpy(left_expr),
        .right=mem_one_cpy(right_expr)
    };
}

yap_assignment yap_parse_assignment(yap_source* src, TSNode node){
    yap_node_field_by_name_var_check(node, lvalue, yap_assignment, "Missing l-value in assignment");
    yap_node_field_by_name_var_check(node, assign, yap_assignment, "Missing '=' in assignment");
    yap_node_field_by_name_var_check(node, expr, yap_assignment, "Missing expression in assignment");
    yap_expr expr = yap_parse_expr(src, expr_node);
    yap_lvalue lvalue = yap_parse_lvalue(src, lvalue_node);
    return (yap_assignment){
        .kind=yap_assignment_valid,
        .lvalue=lvalue,
        .expr=mem_one_cpy(expr)
    };
}


yap_lvalue yap_parse_lvalue(yap_source* src, TSNode p_node){
    yap_log("Parsing lvalue");
    TSNode node = ts_node_child(p_node, 0);
    const char* typ = ts_node_type(node);
    strus_switch(typ, "var"){
        yap_log("Got var!");
    }else{
        yap_log("Didn't get var!");
    }
    return (yap_lvalue){
        .kind=yap_lvalue_error,
    };
}

