#include "parser.h"
#include "ts_yap.h"
#include "types.h"
#include <dirent.h>
#include <unistd.h>

typedef struct {
    char* name;
    darr(yap_dep_node) constraints;
    yap_version chosen;
    bool resolved;
} yap_resolve_entry;

/* Git deps are fetched into the project's own lookup path, so by build time they resolve
 * like any other module. The sources with no fetcher yet are refused here rather than
 * silently resolving to something else of the same name. */
static bool yap_dep_source_unsupported(yap_ctx* ctx, yap_dep_node dep){
    const char* kind = dep.registry ? "registry" : NULL;
    if (!kind) return false;

    yap_ctx_push_error(ctx, (yap_error){
        .kind = yap_error_no_pos,
        .msg  = strus_newf("Dependency '%s' uses a %s source, which is not yet supported", dep.name, kind)
    });
    return true;
}

static darr(yap_version) yap_module_candidates(yap_ctx* ctx, char* name){
    darr(yap_version) out = darr_new(yap_version);

    for_darr(i, lookup_path, ctx->module_lookup_paths){
        char* mod_dir = strus_newf("%s/%s", lookup_path, name);
        DIR* dir = opendir(mod_dir);
        if (dir){
            struct dirent* ent;
            while ((ent = readdir(dir)) != NULL){
                yap_version v;
                if (!yap_version_parse(ent->d_name, &v)) continue;
                char* mod_yp = strus_newf("%s/%s/mod.yp", mod_dir, ent->d_name);
                if (access(mod_yp, R_OK) == 0) darr_push(out, v);
                free(mod_yp);
            }
            closedir(dir);
        }
        free(mod_dir);

        char* flat = strus_newf("%s/%s/mod.yp", lookup_path, name);
        if (access(flat, R_OK) == 0){
            yap_module_decl_node manifest = {0};
            yap_version v = {0};
            if (yap_read_manifest(ctx, flat, &manifest) && manifest.version)
                yap_version_parse(manifest.version, &v);
            darr_push(out, v);
        }
        free(flat);
    }
    return out;
}

static char* yap_module_path_for(yap_ctx* ctx, char* name, yap_version v){
    for_darr(i, lookup_path, ctx->module_lookup_paths){
        char* versioned = strus_newf("%s/%s/%u.%u.%u/mod.yp", lookup_path, name, v.major, v.minor, v.patch);
        if (access(versioned, R_OK) == 0) return versioned;
        free(versioned);

        char* flat = strus_newf("%s/%s/mod.yp", lookup_path, name);
        if (access(flat, R_OK) == 0) return flat;
        free(flat);
    }
    return NULL;
}

static yap_resolve_entry* yap_resolve_find(darr(yap_resolve_entry)* entries, char* name){
    for (size_t i = 0; i < darr_len(*entries); i++)
        if (strcmp((*entries)[i].name, name) == 0) return &(*entries)[i];
    return NULL;
}

static bool yap_resolve_add_constraint(darr(yap_resolve_entry)* entries, yap_dep_node dep){
    yap_resolve_entry* e = yap_resolve_find(entries, dep.name);
    if (!e){
        yap_resolve_entry fresh = { .name = dep.name, .constraints = darr_new(yap_dep_node) };
        darr_push(fresh.constraints, dep);
        darr_push(*entries, fresh);
        return true;
    }
    for_darr(i, existing, e->constraints){
        if (existing.kind == dep.kind && yap_version_cmp(existing.version, dep.version) == 0) return false;
    }
    darr_push(e->constraints, dep);
    return true;
}

static void yap_resolve_report_unsatisfiable(yap_ctx* ctx, yap_resolve_entry* e){
    char* specs = NULL;
    for_darr(i, c, e->constraints){
        char* one = yap_dep_spec_string(ctx, c);
        char* next = specs ? strus_newf("%s, %s", specs, one) : strus_copy(one);
        free(specs);
        specs = next;
    }
    bool from_git = false;
    for_darr(i, c, e->constraints) if (c.git) from_git = true;

    yap_ctx_push_error(ctx, (yap_error){
        .kind = yap_error_no_pos,
        .msg  = from_git
            ? strus_newf("No version of '%s' satisfies %s; run 'yap --fetch' to clone it",
                         e->name, specs ? specs : "its constraints")
            : strus_newf("No version of '%s' satisfies %s", e->name, specs ? specs : "its constraints")
    });
    free(specs);
}

/* Fixpoint over the dependency graph: choose the newest version satisfying every
 * constraint gathered so far, read what that choice itself depends on, repeat. Reading
 * manifests rather than building modules is what allows a choice to be revised. */
void yap_resolve_module_versions(yap_ctx* ctx, darr(yap_dep_node) root_deps){
    yap_parser* parser = ctx->parser_ctx;
    if (!parser || !root_deps) return;

    darr(yap_resolve_entry) entries = darr_new(yap_resolve_entry);
    for_darr(i, dep, root_deps){
        if (!dep.name) continue;
        if (yap_dep_source_unsupported(ctx, dep)) continue;
        yap_resolve_add_constraint(&entries, dep);
    }

    bool changed = true;
    while (changed){
        changed = false;

        for (size_t i = 0; i < darr_len(entries); i++){
            yap_resolve_entry* e = &entries[i];

            darr(yap_version) candidates = yap_module_candidates(ctx, e->name);
            bool have_best = false;
            yap_version best = {0};
            for_darr(ci, cand, candidates){
                bool ok = true;
                for_darr(k, c, e->constraints){
                    if (!yap_dep_satisfied_by(c, cand)){ ok = false; break; }
                }
                if (ok && (!have_best || yap_version_cmp(cand, best) > 0)){ best = cand; have_best = true; }
            }
            darr_free(candidates);

            if (!have_best){
                if (!e->resolved){
                    yap_resolve_report_unsatisfiable(ctx, e);
                    e->resolved = true;
                }
                continue;
            }
            if (e->resolved && yap_version_cmp(best, e->chosen) == 0) continue;

            e->chosen = best;
            e->resolved = true;
            changed = true;
            yap_log("Resolved '%s' to %u.%u.%u", e->name, best.major, best.minor, best.patch);

            char* path = yap_module_path_for(ctx, e->name, best);
            if (!path) continue;
            yap_module_decl_node manifest = {0};
            if (yap_read_manifest(ctx, path, &manifest) && manifest.deps){
                for_darr(di, dep, manifest.deps){
                    /* entries can relocate here, so e is not reused past this point. */
                    if (!dep.name || yap_dep_source_unsupported(ctx, dep)) continue;
                    yap_resolve_add_constraint(&entries, dep);
                }
            }
            free(path);
        }
    }

    for_darr(i, e, entries){
        if (!e.resolved) continue;
        darr_push(parser->pins, ((yap_module_pin){ .name = e.name, .version = e.chosen }));
    }
    for_darr(i, e, entries) darr_free(e.constraints);
    darr_free(entries);
}
