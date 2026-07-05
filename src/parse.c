#include "ts_yap.h"
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>

yap_decl_node yap_parse_forward_type_decl(yap_source* src, TSNode node);
yap_statement_node yap_parse_statement(yap_source* src, TSNode node);
yap_macro_call_node yap_parse_macro_call(yap_source* src, TSNode node);

static bool yap_error_already_reported(yap_ctx* ctx, yap_source* src, TSNode node){
    uint32_t start = ts_node_start_byte(node);
    uint32_t end = ts_node_end_byte(node);
    for_darr(i, err, ctx->errors){
        if (err.src == src && (uint32_t)err.range.start.offset == start && (uint32_t)err.range.end.offset == end)
            return true;
    }
    return false;
}

static void yap_push_parse_error(yap_source* src, TSNode node, const char* fmt, ...){
    if (!src || !src->ctx || !fmt) return;

    // yap_check_tree_errors() already walks the whole tree once up front and
    // reports every genuine tree-sitter ERROR node; AST-building code below
    // re-visits the same broken node while building decls/statements/exprs,
    // so without this check the same syntax error gets reported twice.
    if (ts_node_is_error(node) && yap_error_already_reported(src->ctx, src, node))
        return;

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

static void yap_log_node(yap_source* src, TSNode node, const char* fmt, ...){
    if (!src) return;
    
    char* node_val = yap_node_get_val_ctx(src, node);
    char val_display[14] __attribute__((unused)) = {0};
    if (node_val){
        size_t len = 0;
        for (size_t i = 0; i < 10 && node_val[i]; i++){
            if (node_val[i] == '\n') break;
            val_display[len++] = node_val[i];
        }
        bool needs_ellipsis = (node_val[len] && node_val[len] != '\n');
        if (needs_ellipsis){
            val_display[len++] = '.';
            val_display[len++] = '.';
            val_display[len++] = '.';
            val_display[len] = '\0';
        }
    }
    
    va_list ap;
    va_start(ap, fmt);
    char* msg = NULL;
    int fmt_res = vasprintf(&msg, fmt, ap);
    va_end(ap);
    
    if (fmt_res < 0 || !msg) msg = strus_copy("(format error)");
    yap_log("%s[type=%s%s] val='%s'", msg ? msg : "", ts_node_type(node), ts_node_has_error(node) ? ", error" : "", val_display);
    free(msg);
}

yap_ctx* yap_parse(yap_ctx* ctx, yap_args args){
    yap_log("\n\n Parsing phase\n");
    yap_ctx* res = yap_ctx_init_ts_parser(ctx);
    if (darr_len(args.extra) == 0){
        printf("No source file provided\n");
        exit(1);
    }
    char* resolved_path = yap_resolve_path(darr_first(args.extra));
    char* identity = yap_ctx_strus_cpy(ctx, resolved_path);
    free(resolved_path);
    yap_parse_file(res, darr_first(args.extra), identity, (yap_loc){0});
    yap_free_parser(res->parser_ctx);
    return res;
}

TSNode yap_parse_first_child(TSNode node){
    TSNode child = ts_node_child(node, 0);
    if (ts_node_null_or_error(child)) child = ts_node_named_child(node, 0);
    return child;
}

darr(yap_var_decl_node) yap_parse_struct_fields(yap_source* src, TSNode fields_node){
    yap_ctx* ctx = src->ctx;
    if (ts_node_null_or_error(fields_node)) return yap_ctx_darr_new(ctx, yap_var_decl_node, .cap=0, .len=0);
    uint32_t count = ts_node_named_child_count(fields_node);
    darr(yap_var_decl_node) fields = yap_ctx_darr_new(ctx, yap_var_decl_node, .cap=count, .len=0);
    for_ts_named_children(fields_node, field_node){
        /* struct_field wraps its alternatives; peek inside to find the actual type */
        TSNode field_inner = yap_parse_first_child(field_node);
        const char* _ft = ts_node_type(field_inner);
        if (strus_eq(_ft, "var_decl")){
            darr_push(fields, yap_parse_var_decl(src, field_inner));
            continue;
        }
        /* typed-field form: type [name] [ := default ] */
        yap_node_field_by_name_var(field_node, type);
        yap_node_field_by_name_var(field_node, name);
        yap_node_field_by_name_var(field_node, default_value);
        if (ts_node_null_or_error(type_node)){
            yap_push_parse_error(src, field_node, "Missing type in struct field");
            continue;
        }
        yap_var_decl_node v = {0};
        if (!ts_node_null_or_error(name_node)){
            v.name = yap_parse_identifier(src, name_node);
        }
        v.has_type = true;
        v.type_node = yap_ctx_one_cpy(ctx, yap_parse_type_node(src, type_node));
        v.has_init = !ts_node_null_or_error(default_value_node);
        v.init = v.has_init ? yap_parse_expr(src, default_value_node) : (yap_expr_node){0};
        v.loc = yap_ts_node_loc(field_node, src);
        darr_push(fields, v);
    }
    return fields;
}

darr(yap_enum_variant_node) yap_parse_enum_variants(yap_source* src, TSNode variants_node){
    yap_ctx* ctx = src->ctx;
    if (ts_node_null_or_error(variants_node)) return yap_ctx_darr_new(ctx, yap_enum_variant_node, .cap=0, .len=0);
    uint32_t count = ts_node_named_child_count(variants_node);
    darr(yap_enum_variant_node) variants = yap_ctx_darr_new(ctx, yap_enum_variant_node, .cap=count, .len=0);
    for_ts_named_children(variants_node, variant_node){
        yap_node_field_by_name_var(variant_node, value);
        yap_node_field_by_name_var(variant_node, name);
        yap_enum_variant_node ev = {0};
        if (ts_node_null_or_error(name_node)){
            yap_push_parse_error(src, variant_node, "Missing enum variant name");
            continue;
        }
        ev.name = yap_parse_identifier(src, name_node);
        if (!ts_node_null_or_error(value_node)){
            ev.has_value = true;
            ev.value = yap_parse_expr(src, value_node);
        }else{
            ev.has_value = false;
            ev.value = (yap_expr_node){0};
        }
        ev.loc = yap_ts_node_loc(variant_node, src);
        darr_push(variants, ev);
    }
    return variants;
}

darr(yap_var_decl_node) yap_parse_union_variants(yap_source* src, TSNode variants_node){
    yap_ctx* ctx = src->ctx;
    if (ts_node_null_or_error(variants_node)) return yap_ctx_darr_new(ctx, yap_var_decl_node, .cap=0, .len=0);
    uint32_t count = ts_node_named_child_count(variants_node);
    darr(yap_var_decl_node) variants = yap_ctx_darr_new(ctx, yap_var_decl_node, .cap=count, .len=0);
    for_ts_named_children(variants_node, variant_node){
        yap_node_field_by_name_var(variant_node, type);
        yap_node_field_by_name_var(variant_node, name);
        if (ts_node_null_or_error(type_node)){
            yap_push_parse_error(src, variant_node, "Missing type in union variant");
            continue;
        }
        yap_var_decl_node v = {0};
        if (!ts_node_null_or_error(name_node)){
            v.name = yap_parse_identifier(src, name_node);
        }
        v.has_type = true;
        v.type_node = yap_ctx_one_cpy(ctx, yap_parse_type_node(src, type_node));
        v.has_init = false;
        v.init = (yap_expr_node){0};
        v.loc = yap_ts_node_loc(variant_node, src);
        darr_push(variants, v);
    }
    return variants;
}

static void yap_check_tree_errors_recursive(yap_source* src, TSNode node, int depth){
    if (depth > 20) return;
    uint32_t count = ts_node_child_count(node);
    for (uint32_t i = 0; i < count; i++){
        TSNode child = ts_node_child(node, i);
        if (ts_node_is_missing(child)){
            const char* typ = ts_node_type(child);
            yap_push_parse_error(src, child, "Missing expected '%s'", typ ? typ : "token");
            continue;
        }
        if (ts_node_is_error(child)){
            uint32_t err_bytes = ts_node_end_byte(child) - ts_node_start_byte(child);
            if (err_bytes <= 1) continue;
            char* text = yap_node_get_val_ctx(src, child);
            char preview[40] = {0};
            if (text){
                size_t len = 0;
                for (size_t j = 0; j < 36 && text[j] && text[j] != '\n'; j++)
                    preview[len++] = text[j];
                if (text[len] && text[len] != '\n'){
                    preview[len++] = '.'; preview[len++] = '.'; preview[len++] = '.';
                }
                preview[len] = '\0';
            }
            yap_push_parse_error(src, child, "Syntax error: '%s'", preview[0] ? preview : "?");
            continue;
        }
        if (ts_node_has_error(child))
            yap_check_tree_errors_recursive(src, child, depth + 1);
    }
}

static void yap_check_tree_errors(yap_source* src, TSNode node){
    yap_check_tree_errors_recursive(src, node, 0);
}

void yap_parse_source(yap_source* src){
    yap_ctx* ctx = src->ctx;
    yap_parser* parser = ctx->parser_ctx;
    TSParser* ts_parser = parser->parser;
    //Tree-sitter parsing
    TSTree* tree = ts_parser_parse_string(ts_parser, NULL, src->content, src->sz);
    //Get root node
    TSNode node = ts_tree_root_node(tree);
    uint32_t count = ts_node_named_child_count(node);
    yap_log("Parsing source '%s', %u declarations", src->label, count);

    yap_print_tree(node, 0);

    if (ts_node_has_error(node)){
        yap_check_tree_errors(src, node);
    }

    darr(yap_decl_node) decls = yap_ctx_darr_new(ctx, yap_decl_node, .cap=count, .len=0);
    for_ts_named_children(node, n){
        darr_push(decls, yap_parse_decl(src, n));
    }
    yap_source_node source_node = {
        .declarations = decls,
        .loc = (yap_loc){
            .src = src,
            .range = yap_node_get_range(node)
        }
    };
    src->source_node = yap_ctx_one_cpy(ctx, source_node);
    //Free tree
    ts_tree_delete(tree);
}

yap_decl_node yap_parse_decl(yap_source* src, TSNode node){
    yap_decl_node res = {.kind=yap_decl_error};
    yap_log_node(src, node, "Parsing declaration");
    if (ts_node_null_or_error(node)){
        res.err = yap_node_error(src, node, "Invalid declaration: node is null or error");
        yap_push_parse_error(src, node, "Invalid declaration: node is null or error");
        return res;
    }

    const char* typ = ts_node_type(node);
    strus_switch(typ, "function_declaration"){
        res = yap_parse_func_decl(src, node);
        res.kind = yap_decl_func_decl;
    }strus_case(typ, "function_definition"){
        res = yap_parse_func_decl(src, node);
    }strus_case(typ, "struct_declaration"){
        res = yap_parse_struct_decl(src, node);
    }strus_case(typ, "enum_declaration"){
        res = yap_parse_enum_decl(src, node);
    }strus_case(typ, "union_declaration"){
        res = yap_parse_union_decl(src, node);
    }strus_case(typ, "type_declaration"){
        res = yap_parse_type_declaration(src, node);
    }strus_case(typ, "forward_type_declaration"){
        res = yap_parse_forward_type_decl(src, node);
    }strus_case(typ, "module_import_declaration"){
        res = yap_parse_module_import_decl(src, node);
    }strus_case(typ, "file_import_declaration"){
        res = yap_parse_file_import_decl(src, node);
    }strus_case(typ, "module_declaration"){
        res = yap_parse_module_decl(src, node);
    }strus_case(typ, "macro_declaration"){
        yap_log_node(src, node, "Parsing macro declaration");
        res.kind = yap_decl_macro;
        res.macro_call = yap_parse_macro_call(src, node);
    }else{
        yap_log_node(src, node, "Unhandled declaration");
        yap_push_parse_error(src, node, "Unhandled declaration");
        res.err = yap_node_error(src, node, "Unhandled declaration");
    }
    res.loc.src = src;
    res.loc.range = yap_node_get_range(node);
    return res;
}

yap_decl_node yap_parse_module_import_decl(yap_source* src, TSNode node){
    yap_ctx* ctx = src->ctx;
    yap_decl_node res = {.kind=yap_decl_error};
    if (ts_node_null_or_error(node)){
        yap_push_parse_error(src, node, "Invalid module import declaration");
        res.err = yap_node_error(src, node, "Invalid module import declaration");
        return res;
    }
    yap_log_node(src, node, "Parsing module import declaration");

    yap_node_field_by_name_var(node, name);
    if (ts_node_null_or_error(name_node)){
        yap_push_parse_error(src, node, "Missing module name in module import declaration");
        res.err = yap_node_error(src, node, "Missing module name in module import declaration");
        return res;
    }
    yap_identifier_node module_name = yap_parse_identifier(src, name_node);
    yap_log("Module import declaration: module='%s'", module_name.value ? module_name.value : "(anon)");

    char* mod_path = NULL;
    for_darr(i, lookup_path, ctx->module_lookup_paths){
        char* candidate = strus_newf("%s/%s/mod.yap", lookup_path, module_name.value);
        if (access(candidate, R_OK) == 0){
            mod_path = candidate;
            break;
        }
        free(candidate);
    }

    if (mod_path){
        yap_log("Found module '%s' at '%s'", module_name.value, mod_path);
        char* resolved = yap_resolve_path(mod_path);
        char* identity = yap_ctx_strus_cpy(ctx, resolved);
        char* display = yap_ctx_strus_newf(ctx, "%s/mod.yap", module_name.value);
        free(mod_path);

        yap_source* parent = yap_ctx_top_source(ctx);
        if (parent){
            yap_import imp = {
                .kind = yap_import_module,
                .module_name = yap_ctx_strus_cpy(ctx, module_name.value),
                .loc = yap_ts_node_loc(node, src)
            };
            darr_push(parent->imports, imp);
        }

        size_t sources_before = darr_len(ctx->sources);
        yap_parse_file(ctx, display, identity, yap_ts_node_loc(node, src));
        free(resolved);

        char* mod_tag = yap_ctx_strus_cpy(ctx, module_name.value);
        for (size_t si = sources_before; si < darr_len(ctx->sources); si++){
            if (ctx->sources[si] && !ctx->sources[si]->from_module_import)
                ctx->sources[si]->from_module_import = mod_tag;
        }
    } else {
        yap_push_parse_error(src, node, "Module '%s' not found in any lookup path", module_name.value);
    }

    return (yap_decl_node){
        .kind=yap_decl_module_import,
        .module_import={
            .module_name=module_name,
            .loc=yap_ts_node_loc(node, src),
        }
    };
}

yap_decl_node yap_parse_module_decl(yap_source* src, TSNode node){
    yap_decl_node res = {.kind=yap_decl_error};
    if (ts_node_null_or_error(node)){
        yap_push_parse_error(src, node, "Invalid module declaration");
        res.err = yap_node_error(src, node, "Invalid module declaration");
        return res;
    }
    yap_log_node(src, node, "Parsing module declaration");

    yap_node_field_by_name_var(node, name);
    if (ts_node_null_or_error(name_node)){
        yap_push_parse_error(src, node, "Missing module name in module declaration");
        res.err = yap_node_error(src, node, "Missing module name in module declaration");
        return res;
    }
    yap_identifier_node module_name = yap_parse_identifier(src, name_node);
    yap_log("Module declaration: name='%s'", module_name.value ? module_name.value : "(anon)");
    return (yap_decl_node){
        .kind=yap_decl_module_decl,
        .module_decl={
            .name=module_name,
            .loc=yap_ts_node_loc(node, src),
        }
    };
}

yap_decl_node yap_parse_file_import_decl(yap_source* src, TSNode node){
    yap_ctx* ctx = src->ctx;
    yap_decl_node res = {.kind=yap_decl_error};
    if (ts_node_null_or_error(node)){
        yap_push_parse_error(src, node, "Invalid file import declaration");
        return res;
    }
    yap_log_node(src, node, "Parsing file import declaration");

    yap_node_field_by_name_var(node, path);
    if (ts_node_null_or_error(path_node)){
        yap_push_parse_error(src, node, "Missing file path in file import declaration");
        return res;
    }
    yap_literal_node path_lit = yap_parse_string_literal(src, path_node);
    char* display_path = path_lit.string.value ? path_lit.string.value : "(null)";
    yap_log("File import declaration: path='%s'", display_path);
    //Actually import the file and parse it
    char* parent_path = yap_get_parent_dir(src->origin);
    yap_log("Resolving path '%s' relative to '%s'", display_path, parent_path ? parent_path : "(null)");
    char* resolved_path = yap_resolve_path_relative_to(parent_path, path_lit.string.value);
    char* identity = yap_ctx_strus_cpy(ctx, resolved_path);
    yap_log("Resolved path: '%s'", resolved_path ? resolved_path : "(null)");
    free(parent_path);

    if (!resolved_path || access(resolved_path, R_OK) != 0){
        yap_push_parse_error(src, node, "Imported file '%s' not found", display_path);
        free(resolved_path);
        return res;
    }

    yap_log("\t Importing and parsing file '%s'", display_path);
    yap_parse_file(ctx, display_path, identity, yap_ts_node_loc(node, src));
    yap_log("\t Finished importing and parsing file '%s', back to %s", display_path, src->label);
    free(resolved_path);
    //end of import+parsing
    return (yap_decl_node){
        .kind=yap_decl_file_import,
        .file_import={
            .path=path_lit.string,
            .loc=yap_ts_node_loc(node, src),
        }
    };
}

yap_decl_node yap_parse_type_declaration(yap_source* src, TSNode node){
    if (ts_node_null_or_error(node)){
        yap_push_parse_error(src, node, "Invalid type declaration");
        return (yap_decl_node){ .kind=yap_decl_error, .err=yap_node_error(src, node, "Invalid type declaration") };
    }

    TSNode inner = yap_parse_first_child(node);
    yap_log_node(src, inner, "Parsing type declaration");
    if (ts_node_null_or_error(inner)){
        yap_push_parse_error(src, node, "Invalid type declaration");
        return (yap_decl_node){ .kind=yap_decl_error, .err=yap_node_error(src, node, "Invalid type declaration") };
    }

    const char* typ = ts_node_type(inner);
    strus_switch(typ, "struct_declaration"){
        return yap_parse_struct_decl(src, inner);
    }strus_case(typ, "enum_declaration"){
        return yap_parse_enum_decl(src, inner);
    }strus_case(typ, "union_declaration"){
        return yap_parse_union_decl(src, inner);
    }strus_case(typ, "forward_type_declaration"){
        return yap_parse_forward_type_decl(src, inner);
    }else{
        yap_push_parse_error(src, inner, "Unhandled type declaration");
        return (yap_decl_node){ .kind=yap_decl_error, .err=yap_node_error(src, inner, "Unhandled type declaration") };
    }
}

yap_decl_node yap_parse_func_decl(yap_source* src, TSNode node){
    if (ts_node_null_or_error(node)){
        yap_push_parse_error(src, node, "Invalid function declaration");
        return (yap_decl_node){ .kind=yap_decl_error, .err=yap_node_error(src, node, "Invalid function declaration") };
    }

    yap_log_node(src, node, "Parsing function declaration");

    yap_node_field_by_name_var(node, name);
    yap_identifier_node parsed_name = yap_parse_identifier(src, name_node);
    if (!yap_identifier_node_is_valid(parsed_name)){
        yap_push_parse_error(src, node, "Missing or invalid function name");
        return (yap_decl_node){ .kind=yap_decl_error, .err=yap_node_error(src, node, "Missing or invalid function name") };
    }

    yap_node_field_var(subject_node, node, "subject");
    bool has_subject = !ts_node_null_or_error(subject_node);
    yap_type_node* subject_type_node = NULL;
    yap_identifier_node subject_name = {0};
    if (has_subject){
        yap_node_field_by_name_var(subject_node, type);
        yap_node_field_by_name_var(subject_node, name);
        if (ts_node_null_or_error(type_node) || ts_node_null_or_error(name_node)){
            yap_push_parse_error(src, subject_node, "Missing subject type or name");
        }else{
            yap_ctx* _ctx = src->ctx;
            yap_type_node parsed_subject_type = yap_parse_type_node(src, type_node);
            subject_type_node = yap_ctx_one_cpy(_ctx, parsed_subject_type);
            subject_name = yap_parse_identifier(src, name_node);
        }
    }

    yap_node_field_var(args_node, node, "args");
    darr(yap_func_arg_node) args = yap_parse_func_args(src, args_node);

    yap_log("Function declaration: name='%s' args=%u", parsed_name.value ? parsed_name.value : "(anon)", (unsigned)darr_len(args));

    yap_node_field_var(body_node, node, "body");
    yap_node_field_var(return_type_node, node, "return_type");

    bool has_return_type = false;
    yap_type_node* rt_node = NULL;
    if (!ts_node_null_or_error(return_type_node)){
        has_return_type = true;
        yap_type_node parsed_rt = yap_parse_type_node(src, return_type_node);
        yap_ctx* _ctx = src->ctx;
        rt_node = yap_ctx_one_cpy(_ctx, parsed_rt);
    }

    return (yap_decl_node){
        .kind=yap_decl_func_def,
        .func_decl={
            .name=parsed_name,
            .args=args,
            .has_return_type=has_return_type,
            .return_type_node=rt_node,
            .has_subject=has_subject,
            .subject_type_node=subject_type_node,
            .subject_name=subject_name,
            .body=ts_node_null_or_error(body_node) ? (yap_block_node){0} : yap_parse_block(src, body_node),
            .loc=yap_ts_node_loc(node, src),
        }
    };
}

yap_decl_node yap_parse_struct_decl(yap_source* src, TSNode node){
    yap_node_field_by_name_var(node, name);
    yap_node_field_by_name_var(node, fields);
    yap_log_node(src, node, "Parsing struct declaration");
    if (ts_node_null_or_error(name_node) || ts_node_null_or_error(fields_node)){
        yap_push_parse_error(src, node, "Missing name or fields in named struct declaration");
        return (yap_decl_node){ .kind=yap_decl_error, .err=yap_node_error(src, node, "Missing name or fields in named struct declaration") };
    }

    yap_identifier_node name = yap_parse_identifier(src, name_node);
    yap_log("Struct declaration: name='%s'", name.value ? name.value : "(anon)");
    darr(yap_var_decl_node) fields = yap_parse_struct_fields(src, fields_node);
    return (yap_decl_node){
        .kind=yap_decl_named_type,
        .named_type_decl={
            .kind=yap_named_type_decl_struct,
            .name=name,
            .as_struct={ .fields = fields },
        }
    };
}

yap_decl_node yap_parse_enum_decl(yap_source* src, TSNode node){
    yap_node_field_by_name_var(node, name);
    yap_node_field_by_name_var(node, variants);
    yap_log_node(src, node, "Parsing enum declaration");
    if (ts_node_null_or_error(name_node) || ts_node_null_or_error(variants_node)){
        yap_push_parse_error(src, node, "Missing name or variants in named enum declaration");
        return (yap_decl_node){ .kind=yap_decl_error, .err=yap_node_error(src, node, "Missing name or variants in named enum declaration") };
    }

    yap_identifier_node name = yap_parse_identifier(src, name_node);
    yap_log("Enum declaration: name='%s'", name.value ? name.value : "(anon)");
    darr(yap_enum_variant_node) variants = yap_parse_enum_variants(src, variants_node);
    return (yap_decl_node){
        .kind=yap_decl_named_type,
        .named_type_decl={
            .kind=yap_named_type_decl_enum,
            .name=name,
            .as_enum={ .variants = variants },
        }
    };
}

yap_decl_node yap_parse_union_decl(yap_source* src, TSNode node){
    yap_node_field_by_name_var(node, name);
    yap_node_field_by_name_var(node, variants);
    yap_log_node(src, node, "Parsing union declaration");
    if (ts_node_null_or_error(name_node) || ts_node_null_or_error(variants_node)){
        yap_push_parse_error(src, node, "Missing name or variants in named union declaration");
        return (yap_decl_node){ .kind=yap_decl_error, .err=yap_node_error(src, node, "Missing name or variants in named union declaration") };
    }

    yap_identifier_node name = yap_parse_identifier(src, name_node);
    yap_log("Union declaration: name='%s'", name.value ? name.value : "(anon)");
    darr(yap_var_decl_node) variants = yap_parse_union_variants(src, variants_node);
    return (yap_decl_node){
        .kind=yap_decl_named_type,
        .named_type_decl={
            .kind=yap_named_type_decl_union,
            .name=name,
            .as_union={ .variants = variants },
        }
    };
}

yap_decl_node yap_parse_forward_type_decl(yap_source* src, TSNode node){
    yap_node_field_by_name_var(node, name);
    yap_log_node(src, node, "Parsing forward type declaration");
    if (ts_node_null_or_error(name_node)){
        yap_push_parse_error(src, node, "Missing name in forward type declaration");
        return (yap_decl_node){ .kind=yap_decl_error, .err=yap_node_error(src, node, "Missing name in forward type declaration") };
    }

    yap_identifier_node name = yap_parse_identifier(src, name_node);
    yap_log("Forward type declaration: name='%s'", name.value ? name.value : "(anon)");
    return (yap_decl_node){
        .kind=yap_decl_named_type,
        .named_type_decl={
            .kind=yap_named_type_decl_alias,
            .name=name,
        }
    };
}

yap_block_node yap_parse_block(yap_source* src, TSNode node){
    yap_ctx* ctx = src->ctx;
    yap_log_node(src, node, "Parsing block");
    if (ts_node_null_or_error(node)){
        yap_push_parse_error(src, node, "Invalid block");
        return (yap_block_node){
            .statements=yap_ctx_darr_new(ctx, yap_statement_node, .cap=0, .len=0),
            .loc=yap_ts_node_loc(node, src)
        };
    }
    const char* _blk_typ = ts_node_type(node);
    strus_switch(_blk_typ, "block"){
    }else{
        yap_push_parse_error(src, node, "Invalid block");
        return (yap_block_node){
            .statements=yap_ctx_darr_new(ctx, yap_statement_node, .cap=0, .len=0),
            .loc=yap_ts_node_loc(node, src)
        };
    }

    uint32_t stmt_count = ts_node_named_child_count(node);
    darr(yap_statement_node) statements = yap_ctx_darr_new(ctx, yap_statement_node, .cap=stmt_count, .len=0);
    for_ts_named_children(node, n){
        darr_push(statements, yap_parse_statement(src, n));
    }
    return (yap_block_node){
        .statements=statements,
        .loc=yap_ts_node_loc(node, src)
    };
}

yap_func_arg_node yap_parse_func_arg(yap_source* src, TSNode node){
    if (ts_node_null_or_error(node)){
        yap_push_parse_error(src, node, "Invalid function argument");
        return (yap_func_arg_node){
            .err=yap_node_error(src, node, "Invalid function argument"),
            .is_valid=false,
            .loc=(yap_loc){ .src=src, .range=yap_node_get_range(node) }
        };
    }

    TSNode inner = node;
    {
        const char* _node_typ = ts_node_type(node);
        strus_switch(_node_typ, "var_decl"){
            inner = yap_parse_first_child(node);
        }
    }
    if (ts_node_null_or_error(inner)){
        yap_push_parse_error(src, node, "Invalid function argument");
        return (yap_func_arg_node){
            .err=yap_node_error(src, node, "Invalid function argument"),
            .is_valid=false,
            .loc=yap_ts_node_loc(node, src)
        };
    }

    const char* typ = ts_node_type(inner);
    strus_switch(typ, "explicit_type_var_decl"){
        yap_node_field_by_name_var(inner, type);
        yap_node_field_by_name_var(inner, name);
        yap_node_field_by_name_var(inner, value);
        if (ts_node_null_or_error(type_node) || ts_node_null_or_error(name_node)){
            yap_push_parse_error(src, inner, "Missing argument name or type");
            return (yap_func_arg_node){
                .err=yap_node_error(src, inner, "Missing argument name or type"),
                .is_valid=false,
                .loc=yap_ts_node_loc(inner, src)
            };
        }
        yap_identifier_node arg_name = yap_parse_identifier(src, name_node);
        yap_type_node arg_type = yap_parse_type_node(src, type_node);
        yap_log("Function arg: name='%s'", arg_name.value ? arg_name.value : "(anon)");
        yap_ctx* _ctx = src->ctx;
        return (yap_func_arg_node){
            .name=arg_name,
            .has_type=true,
            .type_node=yap_ctx_one_cpy(_ctx, arg_type),
            .has_default=!ts_node_null_or_error(value_node),
            .default_value=ts_node_null_or_error(value_node) ? (yap_expr_node){0} : yap_parse_expr(src, value_node),
            .is_valid=true,
            .loc=yap_ts_node_loc(inner, src)
        };
    }strus_case(typ, "infered_type_var_decl"){
        yap_node_field_by_name_var(inner, name);
        yap_node_field_by_name_var(inner, value);
        if (ts_node_null_or_error(name_node) || ts_node_null_or_error(value_node)){
            yap_push_parse_error(src, inner, "Missing argument name or default value");
            return (yap_func_arg_node){
                .err=yap_node_error(src, inner, "Missing argument name or default value"),
                .is_valid=false,
                .loc=yap_ts_node_loc(inner, src)
            };
        }
        yap_identifier_node arg_name = yap_parse_identifier(src, name_node);
        yap_log("Function arg: name='%s' (inferred)", arg_name.value ? arg_name.value : "(anon)");
        return (yap_func_arg_node){
            .name=arg_name,
            .has_type=false,
            .type_node=NULL,
            .has_default=true,
            .default_value=yap_parse_expr(src, value_node),
            .is_valid=true,
            .loc=yap_ts_node_loc(inner, src)
        };
    }else{
        yap_push_parse_error(src, inner, "Unsupported function argument form");
        return (yap_func_arg_node){
            .err=yap_node_error(src, inner, "Unsupported function argument form"),
            .is_valid=false,
            .loc=yap_ts_node_loc(inner, src)
        };
    }
}

darr(yap_func_arg_node) yap_parse_func_args(yap_source* src, TSNode node){
    yap_ctx* ctx = src->ctx;
    if (ts_node_is_null(node) || ts_node_has_error(node)){
        return yap_ctx_darr_new(ctx, yap_func_arg_node, .cap=0, .len=0);
    }

    uint32_t count = ts_node_named_child_count(node);
    darr(yap_func_arg_node) args = yap_ctx_darr_new(ctx, yap_func_arg_node, .cap=count, .len=0);
    for_ts_named_children(node, n){
        darr_push(args, yap_parse_func_arg(src, n));
    }
    return args;
}

bool yap_identifier_node_is_valid(yap_identifier_node node){
    return node.value != NULL;
}

yap_type_node yap_parse_type_node(yap_source* src, TSNode type_node){
    yap_ctx* ctx = src->ctx;
    if (ts_node_null_or_error(type_node)){
        return (yap_type_node){ .kind = yap_type_node_error, .err = yap_node_error(src, type_node, "Invalid type node"), .loc = yap_ts_node_loc(type_node, src) };
    }

    /* Unwrap layers */
    while (true){
        const char* tt = ts_node_type(type_node);
        if (strus_eq(tt, "typ")){
            type_node = yap_parse_first_child(type_node);
            if (ts_node_null_or_error(type_node)){
                return (yap_type_node){ .kind = yap_type_node_error, .err = yap_node_error(src, type_node, "Empty typ wrapper"), .loc = yap_ts_node_loc(type_node, src) };
            }
            continue;
        }
        if (strus_eq(tt, "paren_type")){
            yap_node_field_by_name_var(type_node, inner);
            if (!ts_node_null_or_error(inner_node)){
                type_node = inner_node;
                continue;
            }
        }
        break;
    }

    const char* tt = ts_node_type(type_node);
    yap_loc loc = yap_ts_node_loc(type_node, src);

    if (strus_eq(tt, "identifier")){
        return (yap_type_node){ .kind = yap_type_node_identifier, .identifier = yap_parse_identifier(src, type_node), .loc = loc };
    }
    if (strus_eq(tt, "blueprint_hole")){
        // $name in type position: eager splice (in type${ }/fn$) or a lazy type-hole.
        char* text = yap_node_get_val_ctx(src, type_node);
        char* name = (text && text[0] == '$') ? text + 1 : text;
        return (yap_type_node){ .kind = yap_type_node_blueprint_hole, .identifier = { .value = name, .loc = loc }, .loc = loc };
    }
    if (strus_eq(tt, "pointer_type")){
        yap_node_field_by_name_var(type_node, subtyp);
        yap_type_node* subtype = yap_ctx_one(ctx, yap_type_node);
        *subtype = yap_parse_type_node(src, subtyp_node);
        return (yap_type_node){ .kind = yap_type_node_pointer, .pointer_subtype = subtype, .loc = loc };
    }
    if (strus_eq(tt, "const_type")){
        yap_node_field_by_name_var(type_node, inner);
        yap_type_node* subtype = yap_ctx_one(ctx, yap_type_node);
        *subtype = yap_parse_type_node(src, inner_node);
        return (yap_type_node){ .kind = yap_type_node_const, .const_subtype = subtype, .loc = loc };
    }
    if (strus_eq(tt, "paren_type")){
        yap_node_field_by_name_var(type_node, inner);
        yap_type_node* subtype = yap_ctx_one(ctx, yap_type_node);
        *subtype = yap_parse_type_node(src, inner_node);
        return (yap_type_node){ .kind = yap_type_node_paren, .paren_subtype = subtype, .loc = loc };
    }
    if (strus_eq(tt, "function_type")){
        yap_node_field_var(return_type_node, type_node, "return_type");
        yap_node_field_by_name_var(type_node, func_type_params);
        yap_type_node* rt = NULL;
        if (!ts_node_null_or_error(return_type_node)){
            rt = yap_ctx_one(ctx, yap_type_node);
            *rt = yap_parse_type_node(src, return_type_node);
        }
        uint32_t param_cnt = ts_node_null_or_error(func_type_params_node) ? 0 : ts_node_named_child_count(func_type_params_node);
        darr(yap_type_node) params = yap_ctx_darr_new(ctx, yap_type_node, .cap = param_cnt, .len = 0);
        if (!ts_node_null_or_error(func_type_params_node)){
            for_ts_named_children(func_type_params_node, p){
                darr_push(params, yap_parse_type_node(src, p));
            }
        }
        return (yap_type_node){ .kind = yap_type_node_function, .func_type = { .return_type = rt, .params = params }, .loc = loc };
    }
    if (strus_eq(tt, "anon_struct_type")){
        yap_node_field_by_name_var(type_node, fields);
        darr(yap_var_decl_node) f = yap_parse_struct_fields(src, fields_node);
        return (yap_type_node){ .kind = yap_type_node_anon_struct, .anon_struct = { .fields = f }, .loc = loc };
    }
    if (strus_eq(tt, "anon_enum_type")){
        yap_node_field_by_name_var(type_node, variants);
        darr(yap_enum_variant_node) v = yap_parse_enum_variants(src, variants_node);
        return (yap_type_node){ .kind = yap_type_node_anon_enum, .anon_enum = { .variants = v }, .loc = loc };
    }
    if (strus_eq(tt, "anon_union_type")){
        yap_node_field_by_name_var(type_node, variants);
        darr(yap_var_decl_node) v = yap_parse_union_variants(src, variants_node);
        return (yap_type_node){ .kind = yap_type_node_anon_union, .anon_union = { .variants = v }, .loc = loc };
    }

    if (strus_eq(tt, "array_type")){
        yap_node_field_by_name_var(type_node, inner);
        yap_node_field_by_name_var(type_node, size);
        yap_type_node* elem = yap_ctx_one(ctx, yap_type_node);
        *elem = yap_parse_type_node(src, inner_node);
        yap_expr_node* sz = yap_ctx_one(ctx, yap_expr_node);
        *sz = yap_parse_expr(src, size_node);
        return (yap_type_node){ .kind = yap_type_node_array, .array_type = { .element_type = elem, .size_expr = sz }, .loc = loc };
    }
    if (strus_eq(tt, "slice_type")){
        yap_node_field_by_name_var(type_node, inner);
        yap_type_node* elem = yap_ctx_one(ctx, yap_type_node);
        *elem = yap_parse_type_node(src, inner_node);
        return (yap_type_node){ .kind = yap_type_node_slice, .slice_subtype = elem, .loc = loc };
    }

    if (strus_eq(tt, "macro_type")){
        yap_log_node(src, type_node, "Parsing macro type");
        return (yap_type_node){ .kind = yap_type_node_macro, .macro_call = yap_parse_macro_call(src, type_node), .loc = loc };
    }

    /* Fallback: treat as opaque identifier */
    return (yap_type_node){ .kind = yap_type_node_identifier, .identifier = yap_parse_identifier(src, type_node), .loc = loc };
}

yap_identifier_node yap_parse_identifier(yap_source* src, TSNode node){
    if (ts_node_null_or_error(node)){
        yap_push_parse_error(src, node, "Invalid identifier");
        return (yap_identifier_node){ .value=NULL, .loc=yap_ts_node_loc(node, src) };
    }

    const char* _id_typ = ts_node_type(node);
    if (strus_eq(_id_typ, "identifier")){
        return (yap_identifier_node){ .value=yap_node_get_val_ctx(src, node), .loc=yap_ts_node_loc(node, src) };
    }
    if (strus_eq(_id_typ, "blueprint_hole")){
        // $name in identifier position (currently: a var_decl's name inside stmt${ }).
        char* text = yap_node_get_val_ctx(src, node);
        char* name = (text && text[0] == '$') ? text + 1 : text;
        return (yap_identifier_node){ .value=name, .is_hole=true, .loc=yap_ts_node_loc(node, src) };
    }

    /* If we got a wrapper node (typ, pointer_type, function_type, etc.),
       try to descend to the first named child that is an identifier. If
       none exists, fall back to returning the raw text of the whole node
       as the identifier value (this covers function types, pointer
       annotations, etc.). */
    TSNode child = yap_parse_first_child(node);
    while (!ts_node_null_or_error(child)){
        if (strus_eq(ts_node_type(child), "identifier")){
            return (yap_identifier_node){ .value=yap_node_get_val_ctx(src, child), .loc=yap_ts_node_loc(child, src) };
        }
        /* descend further */
        child = yap_parse_first_child(child);
    }

    /* Fallback: use the raw text for complex type nodes instead of erroring. */
    return (yap_identifier_node){ .value=yap_node_get_val_ctx(src, node), .loc=yap_ts_node_loc(node, src) };
}

static yap_func_arg_node yap_parse_func_literal_param(yap_source* src, TSNode node){
    yap_ctx* ctx = src->ctx;
    yap_node_field_by_name_var(node, type);
    yap_node_field_by_name_var(node, name);
    if (ts_node_null_or_error(type_node) || ts_node_null_or_error(name_node)){
        yap_push_parse_error(src, node, "Missing parameter name or type in function literal");
        return (yap_func_arg_node){
            .err=yap_node_error(src, node, "Missing parameter name or type in function literal"),
            .is_valid=false,
            .loc=yap_ts_node_loc(node, src)
        };
    }
    yap_identifier_node param_name = yap_parse_identifier(src, name_node);
    yap_type_node param_type = yap_parse_type_node(src, type_node);
    yap_log("Function literal param: name='%s'", param_name.value ? param_name.value : "(anon)");
    return (yap_func_arg_node){
        .name=param_name,
        .has_type=true,
        .type_node=yap_ctx_one_cpy(ctx, param_type),
        .has_default=false,
        .is_valid=true,
        .loc=yap_ts_node_loc(node, src)
    };
}

static darr(yap_func_arg_node) yap_parse_func_literal_params(yap_source* src, TSNode node){
    yap_ctx* ctx = src->ctx;
    if (ts_node_is_null(node) || ts_node_has_error(node)){
        return yap_ctx_darr_new(ctx, yap_func_arg_node, .cap=0, .len=0);
    }
    uint32_t count = ts_node_named_child_count(node);
    darr(yap_func_arg_node) params = yap_ctx_darr_new(ctx, yap_func_arg_node, .cap=count, .len=0);
    for_ts_named_children(node, n){
        darr_push(params, yap_parse_func_literal_param(src, n));
    }
    return params;
}

yap_literal_node yap_parse_func_literal(yap_source* src, TSNode node){
    yap_ctx* ctx = src->ctx;
    yap_node_field_by_name_var(node, return_type);
    yap_node_field_by_name_var(node, func_literal_params);
    yap_node_field_by_name_var(node, body);

    if (ts_node_null_or_error(body_node)){
        yap_push_parse_error(src, node, "Missing body in function literal");
        return (yap_literal_node){ .kind=yap_literal_error, .err=yap_node_error(src, node, "Missing body in function literal"), .loc=yap_ts_node_loc(node, src) };
    }

    bool has_return_type = !ts_node_null_or_error(return_type_node);
    yap_type_node* return_type = NULL;
    if (has_return_type){
        yap_type_node rt = yap_parse_type_node(src, return_type_node);
        return_type = yap_ctx_one_cpy(ctx, rt);
    }

    darr(yap_func_arg_node) args = yap_parse_func_literal_params(src, func_literal_params_node);
    yap_block_node body = yap_parse_block(src, body_node);
    yap_log("Parsed function literal: %u params, return type %s",
        (unsigned)darr_len(args), has_return_type ? "present" : "none");
    return (yap_literal_node){
        .kind=yap_literal_func,
        .func_literal=(yap_func_literal_node){
            .args=args,
            .has_return_type=has_return_type,
            .return_type_node=return_type,
            .body=body,
            .loc=yap_ts_node_loc(node, src)
        },
        .loc=yap_ts_node_loc(node, src)
    };
}

yap_literal_node yap_parse_literal(yap_source* src, TSNode node){
    if (ts_node_null_or_error(node)){
        yap_push_parse_error(src, node, "Invalid literal");
        return (yap_literal_node){
            .kind=yap_literal_error,
            .err=yap_node_error(src, node, "Invalid literal"),
            .loc=yap_ts_node_loc(node, src)
        };
    }

    const char* typ = ts_node_type(node);
    if (strus_eq(typ, "literal")){
        node = yap_parse_first_child(node);
        if (ts_node_null_or_error(node)){
            yap_push_parse_error(src, node, "Missing literal value");
            return (yap_literal_node){ .kind=yap_literal_error, .err=yap_node_error(src, node, "Missing literal value"), .loc=yap_ts_node_loc(node, src) };
        }
        typ = ts_node_type(node);
    }

    strus_switch(typ, "blob_literal"){
        return yap_parse_blob_literal(src, node);
    }
    strus_case(typ, "num_literal"){
        return yap_parse_num_literal(src, node);
    }
    strus_case(typ, "string_literal"){
        return yap_parse_string_literal(src, node);
    }
    strus_case(typ, "bool_literal"){
        return yap_parse_bool_literal(src, node);
    }
    strus_case(typ, "null_literal"){
        return yap_parse_null_literal(src, node);
    }
    strus_case(typ, "byte_literal"){
        return yap_parse_byte_literal(src, node);
    }
    strus_case(typ, "func_literal"){
        return yap_parse_func_literal(src, node);
    }

    yap_push_parse_error(src, node, "Unhandled literal type");
    return (yap_literal_node){ .kind=yap_literal_error, .err=yap_node_error(src, node, "Unhandled literal type"), .loc=yap_ts_node_loc(node, src) };
}

static int yap_hex_digit_val(char c){
    if (c >= '0' && c <= '9') return c - '0';
    return tolower((unsigned char)c) - 'a' + 10;
}

static size_t yap_utf8_encode(uint32_t cp, char* out){
    if (cp <= 0x7F){
        out[0] = (char)cp;
        return 1;
    } else if (cp <= 0x7FF){
        out[0] = (char)(0xC0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    } else if (cp <= 0xFFFF){
        out[0] = (char)(0xE0 | (cp >> 12));
        out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (char)(0x80 | (cp & 0x3F));
        return 3;
    } else {
        out[0] = (char)(0xF0 | (cp >> 18));
        out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
        out[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[3] = (char)(0x80 | (cp & 0x3F));
        return 4;
    }
}

// Decodes backslash escapes in place (output is never longer than input, so no realloc needed)
static void yap_decode_string_escapes(char* text){
    size_t len = strlen(text);
    size_t r = 0, w = 0;
    while (r < len){
        if (text[r] != '\\'){
            text[w++] = text[r++];
            continue;
        }
        r++; // consume backslash
        if (r >= len){ text[w++] = '\\'; break; }
        char c = text[r];
        if (c >= '0' && c <= '9'){
            // 1-3 octal digits
            size_t start = r;
            unsigned v = 0;
            while (r < len && r - start < 3 && text[r] >= '0' && text[r] <= '9'){
                v = v * 8 + (unsigned)(text[r] - '0');
                r++;
            }
            text[w++] = (char)(v & 0xFF);
        } else if (c == 'x'){
            r++;
            size_t start = r;
            unsigned v = 0;
            while (r < len && r - start < 4 && isxdigit((unsigned char)text[r])){
                v = v * 16 + (unsigned)yap_hex_digit_val(text[r]);
                r++;
            }
            text[w++] = (char)(v & 0xFF);
        } else if (c == 'u' || c == 'U'){
            size_t n = c == 'U' ? 8 : 4;
            r++;
            uint32_t v = 0;
            for (size_t k = 0; k < n && r < len && isxdigit((unsigned char)text[r]); k++, r++)
                v = v * 16 + (uint32_t)yap_hex_digit_val(text[r]);
            char buf[4];
            size_t n_bytes = yap_utf8_encode(v, buf);
            for (size_t k = 0; k < n_bytes; k++) text[w++] = buf[k];
        } else {
            switch (c){
                case 'n': text[w++] = '\n'; break;
                case 't': text[w++] = '\t'; break;
                case 'r': text[w++] = '\r'; break;
                case 'a': text[w++] = '\a'; break;
                case 'b': text[w++] = '\b'; break;
                case 'f': text[w++] = '\f'; break;
                case 'v': text[w++] = '\v'; break;
                default:  text[w++] = c; break; // \\ \' \" \? and unknown escapes pass the char through
            }
            r++;
        }
    }
    text[w] = '\0';
}

yap_literal_node yap_parse_string_literal(yap_source* src, TSNode node){
    if (ts_node_null_or_error(node)){
        yap_push_parse_error(src, node, "Invalid string literal");
        return (yap_literal_node){
            .kind=yap_literal_error,
            .err=yap_node_error(src, node, "Invalid string literal"),
            .loc=yap_ts_node_loc(node, src)
        };
    }
    char* text = yap_node_get_val_ctx(src, node);
    size_t offset = 0;
    yap_string_literal_node str_lit = {0};
    bool is_cstring = false;
    switch (text[0]){
        case 'L':
            offset = 2;
            break;
        case 'u':
            offset = text[1] == '8' ? 3 : 2;
            break;
        case 'U':
            offset = 2;
            break;
        case 'c':
            offset = 2;
            is_cstring = true;
            break;
        default:
            offset = 1;
            break;
    }
    strncpy(str_lit.prefix, text, offset);
    str_lit.prefix[offset] = '\0';
    str_lit.value = text + offset; //skip prefix
    str_lit.value[strlen(str_lit.value)-1] = '\0'; //remove closing quote
    yap_decode_string_escapes(str_lit.value);
    yap_log("Parsed string literal: prefix='%s' value='%s'", str_lit.prefix, str_lit.value);
    return (yap_literal_node){
        .kind = is_cstring ? yap_literal_cstring : yap_literal_string,
        .string=str_lit,
        .loc=yap_ts_node_loc(node, src)
    };
}

yap_literal_node yap_parse_blob_literal(yap_source* src, TSNode node){
    yap_ctx* ctx = src->ctx;
    if (ts_node_null_or_error(node)){
        yap_push_parse_error(src, node, "Invalid blob literal");
        return (yap_literal_node){
            .kind=yap_literal_error,
            .err=yap_node_error(src, node, "Invalid blob literal"),
            .loc=yap_ts_node_loc(node, src)
        };
    }
    uint32_t count = ts_node_named_child_count(node);
    darr(yap_blob_element_node) elements = yap_ctx_darr_new(ctx, yap_blob_element_node, .cap=count, .len=0);
    for_ts_named_children(node, p){
        const char* _pt = ts_node_type(p);
        strus_switch(_pt, "named_param"){
            yap_node_field_by_name_var(p, name);
            yap_node_field_by_name_var(p, value);
            if (!ts_node_null_or_error(name_node) && !ts_node_null_or_error(value_node)){
                darr_push(elements, ((yap_blob_element_node){
                    .is_named = true,
                    .name = yap_parse_identifier(src, name_node),
                    .value = yap_ctx_one_cpy(ctx, yap_parse_expr(src, value_node)),
                    .loc = yap_ts_node_loc(p, src)
                }));
            }
        }strus_case(_pt, "unnamed_param"){
            TSNode expr_node = yap_parse_first_child(p);
            darr_push(elements, ((yap_blob_element_node){
                .is_named = false,
                .name = (yap_identifier_node){0},
                .value = yap_ctx_one_cpy(ctx, yap_parse_expr(src, expr_node)),
                .loc = yap_ts_node_loc(p, src)
            }));
        }else{
            darr_push(elements, ((yap_blob_element_node){
                .is_named = false,
                .name = (yap_identifier_node){0},
                .value = yap_ctx_one_cpy(ctx, yap_parse_expr(src, p)),
                .loc = yap_ts_node_loc(p, src)
            }));
        }
    }
    return (yap_literal_node){ .kind=yap_literal_blob, .blob_elements=elements, .loc=yap_ts_node_loc(node, src) };
}

// Decodes hex/octal/binary to canonical decimal text (yap's '0o'/'0b' aren't valid C) and range-checks the value
static char* yap_normalize_num_literal(yap_source* src, TSNode node, char* text){
    yap_ctx* ctx = src->ctx;
    int base = 10;
    char* digits = text;
    if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) { base = 16; digits = text + 2; }
    else if (text[0] == '0' && (text[1] == 'o' || text[1] == 'O')) { base = 8; digits = text + 2; }
    else if (text[0] == '0' && (text[1] == 'b' || text[1] == 'B')) { base = 2; digits = text + 2; }

    if (base != 10){
        errno = 0;
        char* endptr = NULL;
        unsigned long long v = strtoull(digits, &endptr, base);
        if (errno == ERANGE || *endptr != '\0'){
            yap_push_parse_error(src, node, "Numeric literal '%s' does not fit in 64 bits", text);
            return text;
        }
        return yap_ctx_strus_newf(ctx, "%llu", v);
    }

    bool is_float = strchr(text, '.') != NULL || strchr(text, 'e') != NULL || strchr(text, 'E') != NULL;
    errno = 0;
    char* endptr = NULL;
    if (is_float)
        strtod(text, &endptr);
    else
        strtoull(text, &endptr, 10);
    if (errno == ERANGE || *endptr != '\0'){
        yap_push_parse_error(src, node, is_float
            ? "Numeric literal '%s' is not representable as a 64-bit float"
            : "Numeric literal '%s' does not fit in 64 bits", text);
    }
    return text;
}

yap_literal_node yap_parse_num_literal(yap_source* src, TSNode node){
    if (ts_node_null_or_error(node)){
        yap_push_parse_error(src, node, "Invalid numeric literal");
        return (yap_literal_node){
            .kind=yap_literal_error,
            .err=yap_node_error(src, node, "Invalid numeric literal"),
            .loc=yap_ts_node_loc(node, src)
        };
    }
    char* text = yap_node_get_val_ctx(src, node);
    text = yap_normalize_num_literal(src, node, text);
    return (yap_literal_node){ .kind=yap_literal_numerical, .numerical=text, .loc=yap_ts_node_loc(node, src) };
}

yap_literal_node yap_parse_bool_literal(yap_source* src, TSNode node){
    if (ts_node_null_or_error(node)){
        yap_push_parse_error(src, node, "Invalid bool literal");
        return (yap_literal_node){
            .kind=yap_literal_error,
            .err=yap_node_error(src, node, "Invalid bool literal"),
            .loc=yap_ts_node_loc(node, src)
        };
    }
    char* text = yap_node_get_val_ctx(src, node);
    return (yap_literal_node){ .kind=yap_literal_bool, .numerical=text, .loc=yap_ts_node_loc(node, src) };
}

yap_literal_node yap_parse_null_literal(yap_source* src, TSNode node){
    return (yap_literal_node){ .kind=yap_literal_null, .numerical="0", .loc=yap_ts_node_loc(node, src) };
}

yap_literal_node yap_parse_byte_literal(yap_source* src, TSNode node){
    if (ts_node_null_or_error(node)){
        yap_push_parse_error(src, node, "Invalid byte literal");
        return (yap_literal_node){ .kind=yap_literal_error, .err=yap_node_error(src, node, "Invalid byte literal"), .loc=yap_ts_node_loc(node, src) };
    }
    char* text = yap_node_get_val_ctx(src, node);
    yap_log("Parsed byte literal: '%s'", text);
    return (yap_literal_node){ .kind=yap_literal_byte, .numerical=text, .loc=yap_ts_node_loc(node, src) };
}

yap_expr_node yap_parse_expr_literal(yap_source* src, TSNode node){
    return (yap_expr_node){
        .kind=yap_expr_literal,
        .literal=yap_parse_literal(src, node),
        .loc=yap_ts_node_loc(node, src)
    };
}

yap_expr_node yap_parse_expr_identifier(yap_source* src, TSNode node){
    return (yap_expr_node){
        .kind=yap_expr_var,
        .var=yap_parse_identifier(src, node),
        .loc=yap_ts_node_loc(node, src)
    };
}

yap_expr_node yap_parse_expr_bin(yap_source* src, TSNode node){
    yap_ctx* ctx = (yap_ctx*)src->ctx;
    yap_node_field_by_name_var(node, left);
    yap_node_field_by_name_var(node, right);
    yap_node_field_by_name_var(node, operator);
    if (ts_node_null_or_error(left_node) || ts_node_null_or_error(right_node) || ts_node_null_or_error(operator_node)){
        yap_push_parse_error(src, node, "Missing operand or operator in binary expression");
        return (yap_expr_node){ .kind=yap_expr_error, .err=yap_node_error(src, node, "Missing operand or operator in binary expression"), .loc=yap_ts_node_loc(node, src) };
    }
    return (yap_expr_node){
        .kind=yap_expr_bin,
        .bin={
            .op=*yap_node_val_start(src, operator_node),
            .left=yap_ctx_one_cpy(ctx, yap_parse_expr(src, left_node)),
            .right=yap_ctx_one_cpy(ctx, yap_parse_expr(src, right_node)),
            .loc=yap_ts_node_loc(node, src)
        },
        .loc=yap_ts_node_loc(node, src)
    };
}

yap_expr_node yap_parse_expr_comp(yap_source* src, TSNode node){
    yap_ctx* ctx = (yap_ctx*)src->ctx;
    yap_node_field_by_name_var(node, left);
    yap_node_field_by_name_var(node, right);
    yap_node_field_by_name_var(node, op);
    if (ts_node_null_or_error(left_node) || ts_node_null_or_error(right_node) || ts_node_null_or_error(op_node)){
        yap_push_parse_error(src, node, "Missing operand or operator in comparison");
        return (yap_expr_node){ .kind=yap_expr_error, .err=yap_node_error(src, node, "Missing operand or operator in comparison"), .loc=yap_ts_node_loc(node, src) };
    }
    char* op_text = yap_node_get_val_ctx(src, op_node);
    char op_char = op_text[0];
    if (op_text[0] == '=' && op_text[1] == '=') op_char = 'e';
    else if (op_text[0] == '!' && op_text[1] == '=') op_char = 'n';
    else if (op_text[0] == '<' && op_text[1] == '=') op_char = 'l';
    else if (op_text[0] == '>' && op_text[1] == '=') op_char = 'g';
    return (yap_expr_node){
        .kind=yap_expr_bin,
        .bin={
            .op=op_char,
            .left=yap_ctx_one_cpy(ctx, yap_parse_expr(src, left_node)),
            .right=yap_ctx_one_cpy(ctx, yap_parse_expr(src, right_node)),
            .loc=yap_ts_node_loc(node, src)
        },
        .loc=yap_ts_node_loc(node, src)
    };
}

yap_expr_node yap_parse_expr_logic(yap_source* src, TSNode node){
    yap_ctx* ctx = (yap_ctx*)src->ctx;
    yap_node_field_by_name_var(node, left);
    yap_node_field_by_name_var(node, right);
    yap_node_field_by_name_var(node, op);
    if (ts_node_null_or_error(left_node) || ts_node_null_or_error(right_node) || ts_node_null_or_error(op_node)){
        yap_push_parse_error(src, node, "Missing operand or operator in logical expression");
        return (yap_expr_node){ .kind=yap_expr_error, .err=yap_node_error(src, node, "Missing operand or operator in logical expression"), .loc=yap_ts_node_loc(node, src) };
    }
    char* op_text = yap_node_get_val_ctx(src, op_node);
    // Sentinel chars ('a'/'o'), distinct from bin_expr/comp_op's codes -- see yap_bin_expr in semtree.h.
    char op_char = (op_text[0] == '&') ? 'a' : 'o'; // && -> and, || -> or
    return (yap_expr_node){
        .kind=yap_expr_bin,
        .bin={
            .op=op_char,
            .left=yap_ctx_one_cpy(ctx, yap_parse_expr(src, left_node)),
            .right=yap_ctx_one_cpy(ctx, yap_parse_expr(src, right_node)),
            .loc=yap_ts_node_loc(node, src)
        },
        .loc=yap_ts_node_loc(node, src)
    };
}

yap_expr_node yap_parse_expr_shift(yap_source* src, TSNode node){
    yap_ctx* ctx = (yap_ctx*)src->ctx;
    yap_node_field_by_name_var(node, left);
    yap_node_field_by_name_var(node, right);
    yap_node_field_by_name_var(node, op);
    if (ts_node_null_or_error(left_node) || ts_node_null_or_error(right_node) || ts_node_null_or_error(op_node)){
        yap_push_parse_error(src, node, "Missing operand or operator in shift expression");
        return (yap_expr_node){ .kind=yap_expr_error, .err=yap_node_error(src, node, "Missing operand or operator in shift expression"), .loc=yap_ts_node_loc(node, src) };
    }
    char* op_text = yap_node_get_val_ctx(src, op_node);
    // Sentinel chars ('L'/'R'), distinct from comp_op's '<'/'>' (less/greater-than).
    char op_char = (op_text[0] == '<') ? 'L' : 'R'; // << -> shl, >> -> shr
    return (yap_expr_node){
        .kind=yap_expr_bin,
        .bin={
            .op=op_char,
            .left=yap_ctx_one_cpy(ctx, yap_parse_expr(src, left_node)),
            .right=yap_ctx_one_cpy(ctx, yap_parse_expr(src, right_node)),
            .loc=yap_ts_node_loc(node, src)
        },
        .loc=yap_ts_node_loc(node, src)
    };
}

yap_expr_node yap_parse_expr_coalesce(yap_source* src, TSNode node){
    yap_ctx* ctx = (yap_ctx*)src->ctx;
    yap_node_field_by_name_var(node, left);
    yap_node_field_by_name_var(node, right);
    yap_node_field_by_name_var(node, op);
    if (ts_node_null_or_error(left_node) || ts_node_null_or_error(right_node) || ts_node_null_or_error(op_node)){
        yap_push_parse_error(src, node, "Missing operand or operator in coalesce expression");
        return (yap_expr_node){ .kind=yap_expr_error, .err=yap_node_error(src, node, "Missing operand or operator in coalesce expression"), .loc=yap_ts_node_loc(node, src) };
    }
    // Sentinel char ('c'), distinct from every other bin_expr/comp_op/logic_op/shift_op
    // code; never reaches semtree/codegen - build.c desugars this into a ternary.
    return (yap_expr_node){
        .kind=yap_expr_bin,
        .bin={
            .op='c',
            .left=yap_ctx_one_cpy(ctx, yap_parse_expr(src, left_node)),
            .right=yap_ctx_one_cpy(ctx, yap_parse_expr(src, right_node)),
            .loc=yap_ts_node_loc(node, src)
        },
        .loc=yap_ts_node_loc(node, src)
    };
}

yap_expr_node yap_parse_expr_assignment(yap_source* src, TSNode node){
    yap_ctx* ctx = (yap_ctx*)src->ctx;
    yap_node_field_by_name_var(node, left);
    yap_node_field_by_name_var(node, right);
    yap_node_field_by_name_var(node, operator);
    if (ts_node_null_or_error(left_node) || ts_node_null_or_error(right_node) || ts_node_null_or_error(operator_node)){
        yap_push_parse_error(src, node, "Missing operand or operator in assignment");
        return (yap_expr_node){ .kind=yap_expr_error, .err=yap_node_error(src, node, "Missing operand or operator in assignment"), .loc=yap_ts_node_loc(node, src) };
    }
    char* op_text = yap_node_get_val_ctx(src, operator_node);
    yap_expr_node res = (yap_expr_node){
        .kind=yap_expr_assignment,
        .assignment={
            .left=yap_ctx_one_cpy(ctx, yap_parse_expr(src, left_node)),
            .right=yap_ctx_one_cpy(ctx, yap_parse_expr(src, right_node)),
            .loc=yap_ts_node_loc(node, src)
        },
        .loc=yap_ts_node_loc(node, src)
    };
    snprintf(res.assignment.op, sizeof(res.assignment.op), "%s", op_text ? op_text : "");
    return res;
}

yap_expr_node yap_parse_expr_func_call(yap_source* src, TSNode node){
    yap_ctx* ctx = (yap_ctx*)src->ctx;
    yap_node_field_by_name_var(node, func);
    yap_node_field_var(params_node, node, "params");
    if (ts_node_null_or_error(func_node)){
        yap_push_parse_error(src, node, "Missing callee in function call");
        return (yap_expr_node){ .kind=yap_expr_error, .err=yap_node_error(src, node, "Missing callee in function call"), .loc=yap_ts_node_loc(node, src) };
    }

    uint32_t argc = ts_node_is_null(params_node) ? 0 : ts_node_named_child_count(params_node);
    darr(yap_call_arg_node) args = yap_ctx_darr_new(ctx, yap_call_arg_node, .cap=argc, .len=0);
    if (!ts_node_is_null(params_node)){
        for_ts_named_children(params_node, p){
            const char* _pt = ts_node_type(p);
            strus_switch(_pt, "named_param"){
                yap_node_field_by_name_var(p, name);
                yap_node_field_by_name_var(p, value);
                if (!ts_node_null_or_error(name_node) && !ts_node_null_or_error(value_node)){
                    darr_push(args, ((yap_call_arg_node){
                        .is_named = true,
                        .name = yap_parse_identifier(src, name_node),
                        .value = yap_ctx_one_cpy(ctx, yap_parse_expr(src, value_node)),
                        .loc = yap_ts_node_loc(p, src)
                    }));
                }
            }strus_case(_pt, "unnamed_param"){
                TSNode expr_node = yap_parse_first_child(p);
                darr_push(args, ((yap_call_arg_node){
                    .is_named = false,
                    .value = yap_ctx_one_cpy(ctx, yap_parse_expr(src, expr_node)),
                    .loc = yap_ts_node_loc(p, src)
                }));
            }else{
                darr_push(args, ((yap_call_arg_node){
                    .is_named = false,
                    .value = yap_ctx_one_cpy(ctx, yap_parse_expr(src, p)),
                    .loc = yap_ts_node_loc(p, src)
                }));
            }
        }
    }
    return (yap_expr_node){
        .kind=yap_expr_func_call,
        .func_call={
            .func=yap_ctx_one_cpy(ctx, yap_parse_expr(src, func_node)),
            .args=args,
            .loc=yap_ts_node_loc(node, src)
        },
        .loc=yap_ts_node_loc(node, src)
    };
}

yap_expr_node yap_parse_expr_cast(yap_source* src, TSNode node){
    yap_ctx* ctx = (yap_ctx*)src->ctx;
    yap_node_field_by_name_var(node, expr);
    yap_node_field_var(type_node, node, "type");
    if (ts_node_null_or_error(expr_node) || ts_node_null_or_error(type_node)){
        yap_push_parse_error(src, node, "Missing expression or type in cast expression");
        return (yap_expr_node){ .kind=yap_expr_error, .err=yap_node_error(src, node, "Missing expression or type in cast expression"), .loc=yap_ts_node_loc(node, src) };
    }
    return (yap_expr_node){
        .kind=yap_expr_cast,
        .cast={
            .type_node=yap_ctx_one_cpy(ctx, yap_parse_type_node(src, type_node)),
            .expr=yap_ctx_one_cpy(ctx, yap_parse_expr(src, expr_node)),
            .loc=yap_ts_node_loc(node, src)
        },
        .loc=yap_ts_node_loc(node, src)
    };
}

yap_expr_node yap_parse_expr_at_op(yap_source* src, TSNode node){
    yap_ctx* ctx = (yap_ctx*)src->ctx;
    yap_node_field_by_name_var(node, expr);
    if (ts_node_null_or_error(expr_node)){
        yap_push_parse_error(src, node, "Missing expression in address-of operator");
        return (yap_expr_node){ .kind=yap_expr_error, .err=yap_node_error(src, node, "Missing expression in address-of operator"), .loc=yap_ts_node_loc(node, src) };
    }
    return (yap_expr_node){
        .kind=yap_expr_at_op,
        .at_op={
            .name=(yap_identifier_node){0},
            .expr=yap_ctx_one_cpy(ctx, yap_parse_expr(src, expr_node)),
            .loc=yap_ts_node_loc(node, src)
        },
        .loc=yap_ts_node_loc(node, src)
    };
}

yap_expr_node yap_parse_expr_paren(yap_source* src, TSNode node){
    yap_ctx* ctx = (yap_ctx*)src->ctx;
    yap_node_field_by_name_var(node, expr);
    if (ts_node_null_or_error(expr_node)){
        yap_push_parse_error(src, node, "Missing expression in paren expression");
        return (yap_expr_node){ .kind=yap_expr_error, .err=yap_node_error(src, node, "Missing expression in paren expression"), .loc=yap_ts_node_loc(node, src) };
    }
    return (yap_expr_node){
        .kind=yap_expr_paren,
        .paren={
            .expr=yap_ctx_one_cpy(ctx, yap_parse_expr(src, expr_node)),
            .loc=yap_ts_node_loc(node, src)
        },
        .loc=yap_ts_node_loc(node, src)
    };
}

// $( template ) -> blueprint node wrapping the parsed template expression.
yap_expr_node yap_parse_expr_blueprint(yap_source* src, TSNode node){
    yap_ctx* ctx = (yap_ctx*)src->ctx;
    yap_node_field_by_name_var(node, expr);
    if (ts_node_null_or_error(expr_node)){
        yap_push_parse_error(src, node, "Missing expression in blueprint");
        return (yap_expr_node){ .kind=yap_expr_error, .err=yap_node_error(src, node, "Missing expression in blueprint"), .loc=yap_ts_node_loc(node, src) };
    }
    return (yap_expr_node){
        .kind=yap_expr_blueprint,
        .blueprint={
            .template=yap_ctx_one_cpy(ctx, yap_parse_expr(src, expr_node)),
            .loc=yap_ts_node_loc(node, src)
        },
        .loc=yap_ts_node_loc(node, src)
    };
}

// type${ <anon struct/enum/union> } -> type_blueprint node wrapping the parsed body type.
yap_expr_node yap_parse_type_blueprint(yap_source* src, TSNode node){
    yap_ctx* ctx = (yap_ctx*)src->ctx;
    yap_node_field_by_name_var(node, body);
    if (ts_node_null_or_error(body_node)){
        yap_push_parse_error(src, node, "Missing body in type blueprint");
        return (yap_expr_node){ .kind=yap_expr_error, .err=yap_node_error(src, node, "Missing body in type blueprint"), .loc=yap_ts_node_loc(node, src) };
    }
    yap_type_node* body = yap_ctx_one(ctx, yap_type_node);
    *body = yap_parse_type_node(src, body_node);
    return (yap_expr_node){
        .kind=yap_expr_type_blueprint,
        .type_blueprint={ .body=body, .loc=yap_ts_node_loc(node, src) },
        .loc=yap_ts_node_loc(node, src)
    };
}

// (RET fn$ params){body} -> fn_blueprint node. Same shape as a func literal
// (return type, params, block body); build.c desugars it to a yFnT builder chain.
yap_expr_node yap_parse_fn_blueprint(yap_source* src, TSNode node){
    yap_ctx* ctx = (yap_ctx*)src->ctx;
    yap_node_field_by_name_var(node, return_type);
    yap_node_field_by_name_var(node, func_literal_params);
    yap_node_field_by_name_var(node, body);
    if (ts_node_null_or_error(body_node)){
        yap_push_parse_error(src, node, "Missing body in function blueprint");
        return (yap_expr_node){ .kind=yap_expr_error, .err=yap_node_error(src, node, "Missing body in function blueprint"), .loc=yap_ts_node_loc(node, src) };
    }
    bool has_return_type = !ts_node_null_or_error(return_type_node);
    yap_type_node* return_type = NULL;
    if (has_return_type){
        yap_type_node rt = yap_parse_type_node(src, return_type_node);
        return_type = yap_ctx_one_cpy(ctx, rt);
    }
    darr(yap_func_arg_node) args = yap_parse_func_literal_params(src, func_literal_params_node);
    yap_block_node body = yap_parse_block(src, body_node);
    return (yap_expr_node){
        .kind=yap_expr_fn_blueprint,
        .fn_blueprint=(yap_func_literal_node){
            .args=args,
            .has_return_type=has_return_type,
            .return_type_node=return_type,
            .body=body,
            .loc=yap_ts_node_loc(node, src)
        },
        .loc=yap_ts_node_loc(node, src)
    };
}

// stmt${ ...statements... } -> stmt_blueprint node. Named children are the
// statements (opener/closer are anonymous tokens), parsed like a block body.
yap_expr_node yap_parse_stmt_blueprint(yap_source* src, TSNode node){
    yap_ctx* ctx = (yap_ctx*)src->ctx;
    uint32_t cnt = ts_node_named_child_count(node);
    darr(yap_statement_node) body = yap_ctx_darr_new(ctx, yap_statement_node, .cap=cnt, .len=0);
    for_ts_named_children(node, n){
        darr_push(body, yap_parse_statement(src, n));
    }
    return (yap_expr_node){
        .kind=yap_expr_stmt_blueprint,
        .stmt_blueprint={ .body=body, .loc=yap_ts_node_loc(node, src) },
        .loc=yap_ts_node_loc(node, src)
    };
}

// $name placeholder token. The token text includes the leading '$'; strip it
// so the hole carries just the bare name.
yap_expr_node yap_parse_expr_blueprint_hole(yap_source* src, TSNode node){
    char* text = yap_node_get_val_ctx(src, node);
    char* name = (text && text[0] == '$') ? text + 1 : text;
    return (yap_expr_node){
        .kind=yap_expr_blueprint_hole,
        .blueprint_hole={
            .name={ .value=name, .loc=yap_ts_node_loc(node, src) },
            .loc=yap_ts_node_loc(node, src)
        },
        .loc=yap_ts_node_loc(node, src)
    };
}

yap_expr_node yap_parse_expr_unary(yap_source* src, TSNode node){
    yap_ctx* ctx = (yap_ctx*)src->ctx;
    yap_node_field_by_name_var(node, expr);
    yap_node_field_by_name_var(node, op);
    if (ts_node_null_or_error(expr_node) || ts_node_null_or_error(op_node)){
        yap_push_parse_error(src, node, "Missing operand or operator in unary expression");
        return (yap_expr_node){ .kind=yap_expr_error, .err=yap_node_error(src, node, "Missing operand or operator in unary expression"), .loc=yap_ts_node_loc(node, src) };
    }
    char* op_text = yap_node_get_val_ctx(src, op_node);
    return (yap_expr_node){
        .kind=yap_expr_unary,
        .unary={
            .expr=yap_ctx_one_cpy(ctx, yap_parse_expr(src, expr_node)),
            .op=op_text[0],
            .loc=yap_ts_node_loc(node, src)
        },
        .loc=yap_ts_node_loc(node, src)
    };
}

yap_expr_node yap_parse_expr_incr(yap_source* src, TSNode node){
    yap_ctx* ctx = (yap_ctx*)src->ctx;
    yap_node_field_by_name_var(node, expr);
    if (ts_node_null_or_error(expr_node)){
        yap_push_parse_error(src, node, "Missing expression in increment/decrement expression");
        return (yap_expr_node){ .kind=yap_expr_error, .err=yap_node_error(src, node, "Missing expression in increment/decrement expression"), .loc=yap_ts_node_loc(node, src) };
    }
    yap_node_field_by_name_var(node, op);
    bool is_increment = true;
    if (!ts_node_null_or_error(op_node)){
        is_increment = strus_eq(yap_node_get_val_ctx(src, op_node), "++");
    }
    return (yap_expr_node){
        .kind=is_increment ? yap_expr_increment : yap_expr_decrement,
        .increment={
            .expr=yap_ctx_one_cpy(ctx, yap_parse_expr(src, expr_node)),
            .prefix=false,
            .loc=yap_ts_node_loc(node, src)
        },
        .loc=yap_ts_node_loc(node, src)
    };
}

yap_expr_node yap_parse_expr_prefix_incr(yap_source* src, TSNode node){
    yap_ctx* ctx = (yap_ctx*)src->ctx;
    yap_node_field_by_name_var(node, expr);
    if (ts_node_null_or_error(expr_node)){
        yap_push_parse_error(src, node, "Missing expression in increment/decrement expression");
        return (yap_expr_node){ .kind=yap_expr_error, .err=yap_node_error(src, node, "Missing expression in increment/decrement expression"), .loc=yap_ts_node_loc(node, src) };
    }
    yap_node_field_by_name_var(node, op);
    bool is_increment = true;
    if (!ts_node_null_or_error(op_node)){
        is_increment = strus_eq(yap_node_get_val_ctx(src, op_node), "++");
    }
    return (yap_expr_node){
        .kind=is_increment ? yap_expr_increment : yap_expr_decrement,
        .increment={
            .expr=yap_ctx_one_cpy(ctx, yap_parse_expr(src, expr_node)),
            .prefix=true,
            .loc=yap_ts_node_loc(node, src)
        },
        .loc=yap_ts_node_loc(node, src)
    };
}

yap_expr_node yap_parse_expr_ternary(yap_source* src, TSNode node){
    yap_ctx* ctx = (yap_ctx*)src->ctx;
    yap_node_field_by_name_var(node, condition);
    yap_node_field_by_name_var(node, then_expr);
    yap_node_field_by_name_var(node, else_expr);
    if (ts_node_null_or_error(condition_node) || ts_node_null_or_error(then_expr_node) || ts_node_null_or_error(else_expr_node)){
        yap_push_parse_error(src, node, "Missing branch in ternary expression");
        return (yap_expr_node){ .kind=yap_expr_error, .err=yap_node_error(src, node, "Missing branch in ternary expression"), .loc=yap_ts_node_loc(node, src) };
    }
    return (yap_expr_node){
        .kind=yap_expr_ternary,
        .ternary={
            .condition=yap_ctx_one_cpy(ctx, yap_parse_expr(src, condition_node)),
            .then_expr=yap_ctx_one_cpy(ctx, yap_parse_expr(src, then_expr_node)),
            .else_expr=yap_ctx_one_cpy(ctx, yap_parse_expr(src, else_expr_node)),
            .loc=yap_ts_node_loc(node, src)
        },
        .loc=yap_ts_node_loc(node, src)
    };
}

yap_expr_node yap_parse_expr_member_access(yap_source* src, TSNode node){
    yap_ctx* ctx = (yap_ctx*)src->ctx;
    yap_node_field_by_name_var(node, object);
    yap_node_field_by_name_var(node, member);
    if (ts_node_null_or_error(object_node) || ts_node_null_or_error(member_node)){
        yap_push_parse_error(src, node, "Missing object or member in member access");
        return (yap_expr_node){ .kind=yap_expr_error, .err=yap_node_error(src, node, "Missing object or member in member access"), .loc=yap_ts_node_loc(node, src) };
    }
    return (yap_expr_node){
        .kind=yap_expr_member_access,
        .member_access={
            .object=yap_ctx_one_cpy(ctx, yap_parse_expr(src, object_node)),
            .member=yap_parse_identifier(src, member_node),
            .loc=yap_ts_node_loc(node, src)
        },
        .loc=yap_ts_node_loc(node, src)
    };
}

yap_expr_node yap_parse_expr_method_access(yap_source* src, TSNode node){
    yap_ctx* ctx = (yap_ctx*)src->ctx;
    yap_node_field_by_name_var(node, caller);
    yap_node_field_by_name_var(node, name);
    if (ts_node_null_or_error(caller_node) || ts_node_null_or_error(name_node)){
        yap_push_parse_error(src, node, "Missing caller or name in method access");
        return (yap_expr_node){ .kind=yap_expr_error, .err=yap_node_error(src, node, "Missing caller or name in method access"), .loc=yap_ts_node_loc(node, src) };
    }
    return (yap_expr_node){
        .kind=yap_expr_method_access,
        .method_access={
            .caller=yap_ctx_one_cpy(ctx, yap_parse_expr(src, caller_node)),
            .name=yap_parse_identifier(src, name_node),
            .loc=yap_ts_node_loc(node, src)
        },
        .loc=yap_ts_node_loc(node, src)
    };
}

yap_expr_node yap_parse_expr_optional_member_access(yap_source* src, TSNode node){
    yap_ctx* ctx = (yap_ctx*)src->ctx;
    yap_node_field_by_name_var(node, object);
    yap_node_field_by_name_var(node, member);
    if (ts_node_null_or_error(object_node) || ts_node_null_or_error(member_node)){
        yap_push_parse_error(src, node, "Missing object or member in optional member access");
        return (yap_expr_node){ .kind=yap_expr_error, .err=yap_node_error(src, node, "Missing object or member in optional member access"), .loc=yap_ts_node_loc(node, src) };
    }
    return (yap_expr_node){
        .kind=yap_expr_optional_member_access,
        .member_access={
            .object=yap_ctx_one_cpy(ctx, yap_parse_expr(src, object_node)),
            .member=yap_parse_identifier(src, member_node),
            .loc=yap_ts_node_loc(node, src)
        },
        .loc=yap_ts_node_loc(node, src)
    };
}

yap_expr_node yap_parse_expr_deref(yap_source* src, TSNode node){
    yap_ctx* ctx = (yap_ctx*)src->ctx;
    yap_node_field_by_name_var(node, expr);
    if (ts_node_null_or_error(expr_node)){
        yap_push_parse_error(src, node, "Missing operand in dereference");
        return (yap_expr_node){ .kind=yap_expr_error, .err=yap_node_error(src, node, "Missing operand in dereference"), .loc=yap_ts_node_loc(node, src) };
    }
    return (yap_expr_node){
        .kind=yap_expr_deref,
        .deref={
            .expr=yap_ctx_one_cpy(ctx, yap_parse_expr(src, expr_node)),
            .loc=yap_ts_node_loc(node, src)
        },
        .loc=yap_ts_node_loc(node, src)
    };
}

yap_expr_node yap_parse_expr_index_access(yap_source* src, TSNode node){
    yap_ctx* ctx = (yap_ctx*)src->ctx;
    yap_node_field_by_name_var(node, expr);
    yap_node_field_by_name_var(node, index);
    if (ts_node_null_or_error(expr_node) || ts_node_null_or_error(index_node)){
        yap_push_parse_error(src, node, "Missing object or index in index access");
        return (yap_expr_node){ .kind=yap_expr_error, .err=yap_node_error(src, node, "Missing object or index in index access"), .loc=yap_ts_node_loc(node, src) };
    }
    return (yap_expr_node){
        .kind=yap_expr_index_access,
        .index_access={
            .object=yap_ctx_one_cpy(ctx, yap_parse_expr(src, expr_node)),
            .index=yap_ctx_one_cpy(ctx, yap_parse_expr(src, index_node)),
            .loc=yap_ts_node_loc(node, src)
        },
        .loc=yap_ts_node_loc(node, src)
    };
}

yap_macro_call_node yap_parse_macro_call(yap_source* src, TSNode node){
    yap_ctx* ctx = (yap_ctx*)src->ctx;
    yap_node_field_by_name_var(node, macro_call);
    if (ts_node_null_or_error(macro_call_node)){
        yap_push_parse_error(src, node, "Invalid comptime call");
        return (yap_macro_call_node){ .loc=yap_ts_node_loc(node, src) };
    }

    yap_node_field_by_name_var(macro_call_node, caller);
    if (ts_node_null_or_error(caller_node)){
        yap_push_parse_error(src, node, "Missing caller in comptime call");
        return (yap_macro_call_node){ .loc=yap_ts_node_loc(node, src) };
    }

    TSNode caller_inner = yap_parse_first_child(caller_node);
    if (ts_node_null_or_error(caller_inner))
        caller_inner = caller_node;
    yap_expr_node* caller_expr = yap_ctx_one_cpy(ctx, yap_parse_expr(src, caller_inner));

    darr(yap_macro_param_node) params = yap_ctx_darr_new(ctx, yap_macro_param_node, .cap=0, .len=0);

    yap_node_field_by_name_var(macro_call_node, params);
    if (!ts_node_null_or_error(params_node)){
        uint32_t count = ts_node_named_child_count(params_node);
        params = yap_ctx_darr_new(ctx, yap_macro_param_node, .cap=count, .len=0);
        for_ts_named_children(params_node, p){
            TSNode inner_p = p;
            if (strus_eq(ts_node_type(p), "macro_param")){
                inner_p = yap_parse_first_child(p);
                if (ts_node_null_or_error(inner_p)) inner_p = p;
            }
            const char* pt = ts_node_type(inner_p);
            strus_switch(pt, "named_param"){
                yap_node_field_by_name_var(inner_p, name);
                yap_node_field_by_name_var(inner_p, value);
                darr_push(params, ((yap_macro_param_node){
                    .kind=yap_macro_param_named,
                    .named={
                        .name=yap_parse_identifier(src, name_node),
                        .value=ts_node_null_or_error(value_node) ? NULL : yap_ctx_one_cpy(ctx, yap_parse_expr(src, value_node)),
                    },
                    .loc=yap_ts_node_loc(inner_p, src),
                }));
            }strus_case(pt, "unnamed_param"){
                TSNode expr_inner = yap_parse_first_child(inner_p);
                darr_push(params, ((yap_macro_param_node){
                    .kind=yap_macro_param_unnamed,
                    .expr=yap_ctx_one_cpy(ctx, yap_parse_expr(src, expr_inner)),
                    .loc=yap_ts_node_loc(inner_p, src),
                }));
            }strus_case(pt, "ast_param"){
                yap_node_field_by_name_var(inner_p, expr);
                darr_push(params, ((yap_macro_param_node){
                    .kind=yap_macro_param_ast,
                    .expr=yap_ctx_one_cpy(ctx, yap_parse_expr(src, expr_node)),
                    .loc=yap_ts_node_loc(inner_p, src),
                }));
            }strus_case(pt, "identifier_adding_param"){
                yap_node_field_by_name_var(inner_p, name);
                darr_push(params, ((yap_macro_param_node){
                    .kind=yap_macro_param_ident_add,
                    .ident_add=yap_parse_identifier(src, name_node),
                    .loc=yap_ts_node_loc(inner_p, src),
                }));
            }strus_case(pt, "macro_mut_param"){
                yap_node_field_by_name_var(inner_p, expr);
                darr_push(params, ((yap_macro_param_node){
                    .kind=yap_macro_param_mut,
                    .mut_expr=yap_ctx_one_cpy(ctx, yap_parse_expr(src, expr_node)),
                    .loc=yap_ts_node_loc(inner_p, src),
                }));
            }else{
                darr_push(params, ((yap_macro_param_node){
                    .kind=yap_macro_param_statement,
                    .statement=yap_ctx_one_cpy(ctx, yap_parse_statement(src, inner_p)),
                    .loc=yap_ts_node_loc(inner_p, src),
                }));
            }
        }
    }

    return (yap_macro_call_node){
        .caller=caller_expr,
        .params=params,
        .loc=yap_ts_node_loc(node, src),
    };
}

yap_expr_node yap_parse_expr(yap_source* src, TSNode node){
    if (ts_node_null_or_error(node)){
        yap_push_parse_error(src, node, "Invalid expression");
        return (yap_expr_node){
            .kind=yap_expr_error,
            .err=yap_node_error(src, node, "Invalid expression"),
            .loc=yap_ts_node_loc(node, src)
        };
    }

    const char* typ = ts_node_type(node);
    yap_log_node(src, node, "Parsing expression");
    strus_switch(typ, "literal"){
        return yap_parse_expr_literal(src, node);
    }strus_case(typ, "identifier"){
        return yap_parse_expr_identifier(src, node);
    }strus_case(typ, "bin_expr"){
        return yap_parse_expr_bin(src, node);
    }strus_case(typ, "unary_expr"){
        return yap_parse_expr_unary(src, node);
    }strus_case(typ, "assignment"){
        return yap_parse_expr_assignment(src, node);
    }strus_case(typ, "func_call"){
        return yap_parse_expr_func_call(src, node);
    }strus_case(typ, "cast_expr"){
        return yap_parse_expr_cast(src, node);
    }strus_case(typ, "at_op"){
        return yap_parse_expr_at_op(src, node);
    }strus_case(typ, "expr_blueprint"){
        return yap_parse_expr_blueprint(src, node);
    }strus_case(typ, "type_blueprint"){
        return yap_parse_type_blueprint(src, node);
    }strus_case(typ, "stmt_blueprint"){
        return yap_parse_stmt_blueprint(src, node);
    }strus_case(typ, "fn_blueprint"){
        return yap_parse_fn_blueprint(src, node);
    }strus_case(typ, "blueprint_hole"){
        return yap_parse_expr_blueprint_hole(src, node);
    }strus_case(typ, "paren_expr"){
        return yap_parse_expr_paren(src, node);
    }strus_case(typ, "incr_expr"){
        return yap_parse_expr_incr(src, node);
    }strus_case(typ, "prefix_incr_expr"){
        return yap_parse_expr_prefix_incr(src, node);
    }strus_case(typ, "ternary_expr"){
        return yap_parse_expr_ternary(src, node);
    }strus_case(typ, "member_access"){
        return yap_parse_expr_member_access(src, node);
    }strus_case(typ, "optional_member_access"){
        return yap_parse_expr_optional_member_access(src, node);
    }strus_case(typ, "deref_expr"){
        return yap_parse_expr_deref(src, node);
    }strus_case(typ, "index_access"){
        return yap_parse_expr_index_access(src, node);
    }strus_case(typ, "module_access"){
        yap_log_node(src, node, "Parsing module access expression");
        yap_node_field_by_name_var(node, module);
        yap_node_field_by_name_var(node, field);
        if (ts_node_null_or_error(module_node) || ts_node_null_or_error(field_node)){
            yap_push_parse_error(src, node, "Invalid module access expression");
            return (yap_expr_node){ .kind=yap_expr_error, .err=yap_node_error(src, node, "Invalid module access"), .loc=yap_ts_node_loc(node, src) };
        }
        return (yap_expr_node){
            .kind=yap_expr_module_access,
            .module_access={
                .module=yap_parse_identifier(src, module_node),
                .field=yap_parse_identifier(src, field_node),
                .loc=yap_ts_node_loc(node, src),
            },
            .loc=yap_ts_node_loc(node, src)
        };
    }strus_case(typ, "method_access"){
        return yap_parse_expr_method_access(src, node);
    }strus_case(typ, "comp_op"){
        return yap_parse_expr_comp(src, node);
    }strus_case(typ, "logic_op"){
        return yap_parse_expr_logic(src, node);
    }strus_case(typ, "shift_op"){
        return yap_parse_expr_shift(src, node);
    }strus_case(typ, "coalesce_op"){
        return yap_parse_expr_coalesce(src, node);
    }strus_case(typ, "block_expr"){
        /* block_expr is ( block ) ; extract the block field */
        yap_node_field_var(block_node, node, "block");
        if (ts_node_null_or_error(block_node)){
            yap_push_parse_error(src, node, "Missing block in block expression");
            return (yap_expr_node){ .kind=yap_expr_error, .err=yap_node_error(src, node, "Missing block in block expression"), .loc=yap_ts_node_loc(node, src) };
        }
        yap_log_node(src, node, "Parsing block expression");
        return (yap_expr_node){
            .kind=yap_expr_block,
            .block=yap_parse_block(src, block_node),
            .loc=yap_ts_node_loc(node, src)
        };
    }strus_case(typ, "macro_expr"){
        yap_log_node(src, node, "Parsing macro expression");
        return (yap_expr_node){
            .kind=yap_expr_macro,
            .macro_call=yap_parse_macro_call(src, node),
            .loc=yap_ts_node_loc(node, src),
        };
    }strus_case(typ, "macro_statement"){
        yap_log_node(src, node, "Parsing macro expression (from macro_statement)");
        return (yap_expr_node){
            .kind=yap_expr_macro,
            .macro_call=yap_parse_macro_call(src, node),
            .loc=yap_ts_node_loc(node, src),
        };
    }strus_case(typ, "macro_declaration"){
        yap_log_node(src, node, "Parsing macro expression (from macro_declaration)");
        return (yap_expr_node){
            .kind=yap_expr_macro,
            .macro_call=yap_parse_macro_call(src, node),
            .loc=yap_ts_node_loc(node, src),
        };
    }else{
        yap_log_node(src, node, "Unhandled expression");
        yap_push_parse_error(src, node, "Unhandled expression");
        return (yap_expr_node){ .kind=yap_expr_error, .err=yap_node_error(src, node, "Unhandled expression"), .loc=yap_ts_node_loc(node, src) };
    }
}

yap_statement_node yap_parse_statement(yap_source* src, TSNode node){
    if (ts_node_null_or_error(node)){
        yap_push_parse_error(src, node, "Invalid statement");
        return (yap_statement_node){ .kind=yap_statement_error, .err=yap_node_error(src, node, "Invalid statement"), .loc=yap_ts_node_loc(node, src) };
    }


    yap_ctx* ctx = (yap_ctx*)src->ctx;
    yap_log_node(src, node, "Parsing statement");
    const char* typ = ts_node_type(node);
    strus_switch(typ, "block"){
        return (yap_statement_node){ .kind=yap_statement_block, .block=yap_parse_block(src, node), .loc=yap_ts_node_loc(node, src) };
    }strus_case(typ, "expr_statement"){
        TSNode expr_node = yap_parse_first_child(node);
        if (!ts_node_null_or_error(expr_node)){
            const char* _et = ts_node_type(expr_node);
            strus_switch(_et, "method_access"){
                yap_node_field_by_name_var(expr_node, caller);
                yap_node_field_by_name_var(expr_node, name);
                if (!ts_node_null_or_error(caller_node) && !ts_node_null_or_error(name_node)){
                    const char* _ct = ts_node_type(caller_node);
                    strus_switch(_ct, "identifier"){
                        yap_node_val_ctx(caller_node);
                        yap_node_val_ctx(name_node);
                        yap_log("Reinterpreting expression statement as uninitialized variable declaration: %s:%s", caller_node_val, name_node_val);
                        return (yap_statement_node){
                            .kind=yap_statement_var_decl,
                            .var_decl=(yap_var_decl_node){
                                .name=yap_parse_identifier(src, name_node),
                                .has_type=true,
                                .type_node=yap_ctx_one_cpy(ctx, yap_parse_type_node(src, caller_node)),
                                .has_init=false,
                                .init=(yap_expr_node){0},
                                .loc=yap_ts_node_loc(node, src)
                            },
                            .loc=yap_ts_node_loc(node, src)
                        };
                    }
                }
            }
        }
        return (yap_statement_node){ .kind=yap_statement_expr, .expr=yap_parse_expr(src, expr_node), .loc=yap_ts_node_loc(node, src) };
    }strus_case(typ, "empty_statement"){
        return (yap_statement_node){ .kind=yap_statement_empty, .loc=yap_ts_node_loc(node, src) };
    }strus_case(typ, "var_decl"){
        return (yap_statement_node){ .kind=yap_statement_var_decl, .var_decl=yap_parse_var_decl(src, node), .loc=yap_ts_node_loc(node, src) };
    }strus_case(typ, "return_statement"){
        yap_node_field_by_name_var(node, value);
        bool has_value = !ts_node_null_or_error(value_node);
        return (yap_statement_node){
            .kind=yap_statement_return,
            .return_stmt={ .has_value=has_value, .value=has_value ? yap_parse_expr(src, value_node) : (yap_expr_node){0}, .loc=yap_ts_node_loc(node, src) },
            .loc=yap_ts_node_loc(node, src)
        };
    }strus_case(typ, "if_statement"){
        yap_node_field_by_name_var(node, condition);
        yap_node_field_by_name_var(node, then_branch);
        if (ts_node_null_or_error(condition_node) || ts_node_null_or_error(then_branch_node)){
            yap_push_parse_error(src, node, "Missing condition or then branch in if statement");
            return (yap_statement_node){ .kind=yap_statement_error, .err=yap_node_error(src, node, "Missing condition or then branch in if statement"), .loc=yap_ts_node_loc(node, src) };
        }
        return (yap_statement_node){
            .kind=yap_statement_if,
            .if_stmt={ .condition=yap_parse_expr(src, condition_node), .then_branch=yap_ctx_one_cpy(ctx, yap_parse_statement(src, then_branch_node)), .loc=yap_ts_node_loc(node, src) },
            .loc=yap_ts_node_loc(node, src)
        };
    }strus_case(typ, "if_else_statement"){
        yap_node_field_by_name_var(node, condition);
        yap_node_field_by_name_var(node, then_branch);
        yap_node_field_by_name_var(node, else_branch);
        if (ts_node_null_or_error(condition_node) || ts_node_null_or_error(then_branch_node) || ts_node_null_or_error(else_branch_node)){
            yap_push_parse_error(src, node, "Missing branch in if-else statement");
            return (yap_statement_node){ .kind=yap_statement_error, .err=yap_node_error(src, node, "Missing branch in if-else statement"), .loc=yap_ts_node_loc(node, src) };
        }
        return (yap_statement_node){
            .kind=yap_statement_if_else,
            .if_else_stmt={
                .condition=yap_parse_expr(src, condition_node),
                .then_branch=yap_ctx_one_cpy(ctx, yap_parse_statement(src, then_branch_node)),
                .else_branch=yap_ctx_one_cpy(ctx, yap_parse_statement(src, else_branch_node)),
                .loc=yap_ts_node_loc(node, src)
            },
            .loc=yap_ts_node_loc(node, src)
        };
    }strus_case(typ, "while_loop"){
        yap_node_field_by_name_var(node, condition);
        yap_node_field_by_name_var(node, body);
        if (ts_node_null_or_error(condition_node) || ts_node_null_or_error(body_node)){
            yap_push_parse_error(src, node, "Missing condition or body in while loop");
            return (yap_statement_node){ .kind=yap_statement_error, .err=yap_node_error(src, node, "Missing condition or body in while loop"), .loc=yap_ts_node_loc(node, src) };
        }
        return (yap_statement_node){
            .kind=yap_statement_while,
            .while_stmt={
                .condition=yap_parse_expr(src, condition_node),
                .body=yap_ctx_one_cpy(ctx, yap_parse_statement(src, body_node)),
                .loc=yap_ts_node_loc(node, src)
            },
            .loc=yap_ts_node_loc(node, src)
        };
    }strus_case(typ, "for_loop"){
        yap_node_field_by_name_var(node, init);
        yap_node_field_by_name_var(node, condition);
        yap_node_field_by_name_var(node, update);
        yap_node_field_by_name_var(node, body);
        if (ts_node_null_or_error(init_node) || ts_node_null_or_error(condition_node) || ts_node_null_or_error(update_node) || ts_node_null_or_error(body_node)){
            yap_push_parse_error(src, node, "Missing branch in for loop");
            return (yap_statement_node){ .kind=yap_statement_error, .err=yap_node_error(src, node, "Missing branch in for loop"), .loc=yap_ts_node_loc(node, src) };
        }
        return (yap_statement_node){
            .kind=yap_statement_for,
            .for_stmt={
                .init=yap_ctx_one_cpy(ctx, yap_parse_statement(src, init_node)),
                .condition=yap_parse_expr(src, condition_node),
                .update=yap_parse_expr(src, update_node),
                .body=yap_ctx_one_cpy(ctx, yap_parse_statement(src, body_node)),
                .loc=yap_ts_node_loc(node, src)
            },
            .loc=yap_ts_node_loc(node, src)
        };
    }strus_case(typ, "break_statement"){
        return (yap_statement_node){ .kind=yap_statement_break, .loc=yap_ts_node_loc(node, src) };
    }strus_case(typ, "continue_statement"){
        return (yap_statement_node){ .kind=yap_statement_continue, .loc=yap_ts_node_loc(node, src) };
    }strus_case(typ, "macro_statement"){
        yap_log_node(src, node, "Parsing macro statement");
        return (yap_statement_node){
            .kind=yap_statement_macro,
            .macro_call=yap_parse_macro_call(src, node),
            .loc=yap_ts_node_loc(node, src),
        };
    }

    yap_log_node(src, node, "Unhandled statement");
    yap_push_parse_error(src, node, "Unhandled statement");
    return (yap_statement_node){ .kind=yap_statement_error, .err=yap_node_error(src, node, "Unhandled statement"), .loc=yap_ts_node_loc(node, src) };
}

yap_var_decl_node yap_parse_var_decl(yap_source* src, TSNode node){
    if (ts_node_null_or_error(node)){
        yap_push_parse_error(src, node, "Invalid variable declaration");
        return (yap_var_decl_node){ .loc=yap_ts_node_loc(node, src) };
    }

    yap_log_node(src, node, "Parsing variable declaration");

    TSNode inner = yap_parse_first_child(node);
    if (ts_node_null_or_error(inner)){
        yap_push_parse_error(src, node, "Missing variable declaration payload");
        return (yap_var_decl_node){ .loc=yap_ts_node_loc(node, src) };
    }

    const char* typ = ts_node_type(inner);
    strus_switch(typ, "infered_type_var_decl"){
        yap_node_field_by_name_var(inner, name);
        yap_node_field_by_name_var(inner, value);
        yap_identifier_node var_name = yap_parse_identifier(src, name_node);
        if (!yap_identifier_node_is_valid(var_name)){
            yap_push_parse_error(src, inner, "Invalid variable name");
            return (yap_var_decl_node){ .loc=yap_ts_node_loc(inner, src) };
        }
        yap_log("Variable declaration (inferred): name='%s'", var_name.value ? var_name.value : "(anon)");
        return (yap_var_decl_node){
            .name=var_name,
            .has_type=false,
            .type_node=NULL,
            .has_init=!ts_node_null_or_error(value_node),
            .init=ts_node_null_or_error(value_node) ? (yap_expr_node){0} : yap_parse_expr(src, value_node),
            .loc=yap_ts_node_loc(inner, src)
        };
    }strus_case(typ, "explicit_type_var_decl"){
        yap_node_field_by_name_var(inner, type);
        yap_node_field_by_name_var(inner, name);
        yap_node_field_by_name_var(inner, value);
        if (ts_node_null_or_error(type_node) || ts_node_null_or_error(name_node)){
            yap_push_parse_error(src, inner, "Missing declared variable type or name in variable declaration");
            return (yap_var_decl_node){ .loc=yap_ts_node_loc(inner, src) };
        }
        yap_identifier_node var_name = yap_parse_identifier(src, name_node);
        yap_type_node type_node_parsed = yap_parse_type_node(src, type_node);
        yap_log("Variable declaration (explicit): name='%s'", var_name.value ? var_name.value : "(anon)");
        yap_ctx* _ctx2 = src->ctx;
        return (yap_var_decl_node){
            .name=var_name,
            .has_type=true,
            .type_node=yap_ctx_one_cpy(_ctx2, type_node_parsed),
            .has_init=!ts_node_null_or_error(value_node),
            .init=ts_node_null_or_error(value_node) ? (yap_expr_node){0} : yap_parse_expr(src, value_node),
            .loc=yap_ts_node_loc(inner, src)
        };
    }

    yap_push_parse_error(src, inner, "Unsupported variable declaration form");
    return (yap_var_decl_node){ .loc=yap_ts_node_loc(inner, src) };
}