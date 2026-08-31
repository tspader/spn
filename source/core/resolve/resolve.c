#include "spn/errors.h"
#include "git/types.h"
#include "index/types.h"
#include "pkg/types.h"
#include "resolve/types.h"
#include "semver/types.h"
#include "session/registry/types.h"
#include "sp.h"
#include "spn/core.h"

#include "index/cache.h"
#include "index/index.h"
#include "intern/intern.h"
#include "pkg/id.h"
#include "pkg/load.h"
#include "toml/issue.h"
#include "pkg/pkg.h"
#include "resolve/resolve.h"
#include "semver/compare.h"
#include "semver/convert.h"
#include "macro/macro.h"
#include "str/str.h"
#include "pkg/options.h"
#include "target/mutate.h"
#include "target/select.h"
#include "when/when.h"

#define try_union(expr) \
  do { \
    spn_err_union_t __err = (expr); \
    if (__err.kind) { \
      return __err; \
    } \
  } while (0)

#define ok_union() ((spn_err_union_t) sp_zero_initialize())

typedef struct {
  spn_pkg_id_t from;
  spn_dep_edge_t edge;
  spn_requested_dep_t req;
} spn_scope_boundary_t;

typedef struct {
  sp_intern_id_t name;
  spn_semver_t version;
} spn_pin_t;

typedef struct {
  sp_intern_id_t root;
  spn_pkg_id_t from;
  sp_da(spn_requested_dep_t) reqs;
  u64 cursor;
  sp_ht(sp_intern_id_t, spn_resolved_pkg_t) named;
  sp_da(spn_pin_t) pins;
  sp_da(u32) processes;
} spn_scope_t;

typedef struct {
  u32 from;
  u32 to;
  bool private;
} spn_scope_edge_t;

typedef struct {
  spn_resolve_query_t* query;
  sp_da(spn_scope_t) scopes;
  sp_da(spn_scope_boundary_t) boundaries;
  sp_da(spn_scope_edge_t) edges;
  sp_ht(sp_intern_id_t, u8) visited;
  u32 scope;
  u64 picks;
  u64 budget;
  bool fatal;
  spn_pin_t forced;
  bool has_forced;
  spn_pin_t retry[2];
  u32 retries;
} spn_resolve_run_t;

typedef struct {
  u32 scope;
  sp_intern_id_t name;
} spn_group_node_t;

typedef sp_ht(spn_group_node_t, u8) spn_group_states_t;

typedef struct {
  u8 state;
  sp_hash_t hash;
} spn_hash_state_t;

typedef sp_ht(spn_group_node_t, spn_hash_state_t) spn_group_hash_t;

typedef struct {
  sp_hash_t hash;
  u32 kind;
  u32 private;
} spn_edge_record_t;

typedef struct {
  u32 scope;
  sp_intern_id_t name;
  spn_resolved_pkg_t* node;
  spn_dep_kind_t kind;
  spn_dep_edge_t edge;
  bool private;
} spn_node_edge_t;

static spn_err_union_t resolve_dep(spn_resolver_t* resolver, spn_resolve_run_t* run, spn_resolved_pkg_t* node, spn_requested_dep_t* dep);
static spn_err_union_t resolve_deps(spn_resolver_t* resolver, spn_resolve_run_t* run, spn_resolved_pkg_t* node);
static spn_err_union_t solve_node(spn_resolver_t* resolver, spn_resolve_run_t* run, spn_scope_t* scope, sp_intern_id_t name, spn_resolved_pkg_t* node);

void spn_resolver_init(spn_resolver_t* r, sp_mem_t mem, sp_intern_t* intern, spn_index_cache_t* index, spn_pkg_registry_t* registry, spn_profile_info_t profile, sp_da(spn_pkg_config_entry_t) config, u64 budget) {
  *r = (spn_resolver_t){
    .mem = mem,
    .intern = intern,
    .index = index,
    .registry = registry,
    .profile = profile,
    .config = config,
    .budget = budget ? budget : SPN_RESOLVE_DEFAULT_BUDGET,
  };
}

void spn_resolve_query_init(sp_mem_t mem, spn_resolve_query_t* query) {
  *query = (spn_resolve_query_t) sp_zero_initialize();
  sp_da_init(mem, query->reqs);
  sp_da_init(mem, query->errors);
  sp_ht_init(mem, query->result);
}

void spn_resolve_query_add(spn_resolve_query_t* query, spn_requested_dep_t req) {
  sp_da_push(query->reqs, req);
}

static spn_dep_kind_t dep_kind_from_index(spn_index_dep_kind_t kind) {
  switch (kind) {
    case SPN_INDEX_DEP_NORMAL: return SPN_DEP_KIND_PACKAGE;
    case SPN_INDEX_DEP_BUILD:  return SPN_DEP_KIND_BUILD;
    case SPN_INDEX_DEP_TEST:   return SPN_DEP_KIND_TEST;
  }
  sp_unreachable_return(SPN_DEP_KIND_PACKAGE);
}

static spn_err_pkg_name_t err_pkg_name(spn_pkg_name_t id) {
  return (spn_err_pkg_name_t) { .name = id.name, .namespace = id.namespace };
}

static spn_err_union_t unsatisfiable_err(spn_resolver_t* resolver, spn_resolved_pkg_t* from, spn_requested_dep_t* request, spn_err_t kind, spn_semver_t selected) {
  return (spn_err_union_t) {
    .kind = kind,
    .unsatisfiable = {
      .qualified = request->qualified,
      .range = request->source == SPN_PKG_SOURCE_INDEX ? spn_semver_range_to_str(resolver->mem, request->index.range) : sp_str_lit(""),
      .requester = from ? sp_intern_str_from_id(resolver->intern, from->id.qualified) : sp_str_lit(""),
      .requester_version = from ? from->id.version : sp_zero_s(spn_semver_t),
      .selected = selected,
    }
  };
}

static spn_err_union_t index_err(spn_resolver_t* resolver, spn_requested_dep_t* request, spn_err_t kind, spn_index_diag_t* diag) {
  switch (kind) {
    case SPN_ERR_MANIFEST_ISSUES: {
      return (spn_err_union_t) {
        .kind = kind,
        .manifest = { .name = request->qualified, .path = diag->path, .issues = spn_codegen_issues_to_err(resolver->mem, diag->issues) },
      };
    }
    case SPN_ERR_INDEX_CORRUPT: {
      return (spn_err_union_t) {
        .kind = kind,
        .index_corrupt = { .name = request->qualified, .path = diag->path },
      };
    }
    case SPN_ERR_INDEX_PATH_DEP: {
      return (spn_err_union_t) {
        .kind = kind,
        .pkg = { .name = request->qualified, .requested = diag->dep },
      };
    }
    default: {
      return (spn_err_union_t) { .kind = kind };
    }
  }
}

static spn_err_union_t load_file_pkg(spn_resolver_t* resolver, spn_requested_dep_t* request, spn_registry_pkg_t** pkg) {
  spn_pkg_id_t id = spn_pkg_id(resolver->intern, request->qualified);
  spn_registry_pkg_t* existing = sp_ht_getp(*resolver->registry, id);
  if (existing) {
    *pkg = existing;
    return ok_union();
  }

  *pkg = SP_NULLPTR;

  spn_pkg_info_t* info = sp_alloc_type(resolver->mem, spn_pkg_info_t);
  spn_codegen_issues_t issues = sp_zero;
  spn_err_t loaded = spn_pkg_load(resolver->mem, resolver->intern, request->file.path, SPN_MANIFEST_DEP, info, &issues);
  if (loaded == SPN_ERR_NO_MANIFEST) {
    return (spn_err_union_t) {
      .kind = SPN_ERR_NO_MANIFEST,
      .no_manifest = { .path = request->file.path },
    };
  }
  if (loaded) {
    return (spn_err_union_t) {
      .kind = SPN_ERR_MANIFEST_ISSUES,
      .manifest = { .name = request->qualified, .path = request->file.path, .issues = spn_codegen_issues_to_err(resolver->mem, issues) },
    };
  }

  // Option requests, config keys, and edge lookups all route by the name the
  // edge requested; a manifest declaring some other name would strand them
  if (!sp_str_equal(info->qualified, request->qualified)) {
    return (spn_err_union_t) {
      .kind = SPN_ERR_PKG_MISMATCH,
      .mismatch = { .path = request->file.path, .declared = info->qualified, .requested = request->qualified },
    };
  }

  sp_ht_insert(*resolver->registry, id, ((spn_registry_pkg_t) {
    .source = SPN_PKG_SOURCE_FILE,
    .info = info,
    .manifest = request->file.path,
  }));

  *pkg = sp_ht_getp(*resolver->registry, id);
  return ok_union();
}

static bool find_forced(spn_resolve_run_t* run, sp_intern_id_t name, spn_semver_t* version) {
  if (run->has_forced && run->forced.name == name) {
    *version = run->forced.version;
    return true;
  }
  return false;
}

static spn_kind_query_t kind_query(spn_resolver_t* resolver, sp_str_t pkg_name) {
  spn_kind_query_t query = { .linkage = resolver->profile.linkage };
  spn_pkg_config_t* config = spn_pkg_config_find(resolver->config, pkg_name);
  if (config && !sp_opt_is_null(config->kind)) {
    sp_opt_set(query.config, config->kind.value);
  }
  return query;
}

static bool target_selects_shared(spn_target_info_t* info, spn_kind_query_t query) {
  spn_linkage_t kind = SPN_LIB_KIND_NONE;
  if (spn_target_select_lib_kind(info, query, &kind)) {
    return false;
  }
  return kind == SPN_LIB_KIND_SHARED;
}

static void node_options_env(spn_resolver_t* resolver, spn_resolved_pkg_t* node, spn_when_env_t* env) {
  spn_option_requests_t* seeds = SP_NULLPTR;
  if (resolver->seeds) {
    seeds = sp_ht_getp(resolver->seeds, node->id.qualified);
  }
  spn_pkg_options_env(resolver->mem, node, &resolver->profile, resolver->config, seeds ? *seeds : SP_NULLPTR, env);
}

static bool node_is_shared(spn_resolver_t* resolver, spn_resolved_pkg_t* node) {
  spn_kind_query_t query = kind_query(resolver, node->name);

  switch (node->source) {
    case SPN_PKG_SOURCE_INDEX: {
      if (!node->origin.release) {
        return false;
      }

      sp_da_for(node->origin.release->targets, it) {
        spn_index_target_t* target = &node->origin.release->targets[it];
        if (sp_da_empty(target->linkages)) {
          continue;
        }

        spn_target_info_t info = sp_zero;
        sp_da_for(target->linkages, jt) {
          spn_linkage_set_add(&info.linkages, target->linkages[jt]);
        }
        if (target_selects_shared(&info, query)) {
          return true;
        }
      }
      return false;
    }
    case SPN_PKG_SOURCE_ROOT:
    case SPN_PKG_SOURCE_FILE: {
      spn_registry_pkg_t* pkg = sp_ht_getp(*resolver->registry, ((spn_pkg_id_t) { .qualified = node->id.qualified }));
      if (!pkg) {
        return false;
      }

      sp_str_om_for(pkg->info->libs, it) {
        if (target_selects_shared(sp_str_om_at(pkg->info->libs, it), query)) {
          return true;
        }
      }
      return false;
    }
  }
  sp_unreachable_return(false);
}

static spn_dep_edge_t classify_dep(spn_resolver_t* resolver, spn_resolved_pkg_t* node, spn_requested_dep_t* dep) {
  switch (dep->kind) {
    case SPN_DEP_KIND_BUILD: {
      return SPN_DEP_EDGE_PROCESS;
    }
    case SPN_DEP_KIND_TEST: {
      return node->source == SPN_PKG_SOURCE_ROOT ? SPN_DEP_EDGE_PROCESS : SPN_DEP_EDGE_PRUNED;
    }
    case SPN_DEP_KIND_PACKAGE: {
      break;
    }
  }

  if (dep->private && node_is_shared(resolver, node)) {
    return SPN_DEP_EDGE_PRIVATE;
  }
  return SPN_DEP_EDGE_SCOPE;
}

static sp_da(sp_intern_id_t) snapshot_named(sp_mem_t mem, spn_scope_t* scope) {
  sp_da(sp_intern_id_t) snapshot = sp_da_new(mem, sp_intern_id_t);
  sp_ht_for_kv(scope->named, it) {
    sp_da_push(snapshot, *it.key);
  }
  return snapshot;
}

static void restore_named(sp_mem_t mem, spn_scope_t* scope, sp_da(sp_intern_id_t) snapshot) {
  sp_da(sp_intern_id_t) added = sp_da_new(mem, sp_intern_id_t);
  sp_ht_for_kv(scope->named, it) {
    bool held = false;
    sp_da_for(snapshot, jt) {
      if (snapshot[jt] == *it.key) {
        held = true;
        break;
      }
    }
    if (!held) {
      sp_da_push(added, *it.key);
    }
  }

  sp_da_for(added, it) {
    sp_ht_erase(scope->named, added[it]);
  }
}

static s32 sort_pick_by_priority(const void* a, const void* b) {
  const spn_resolved_pkg_t* lhs = *(const spn_resolved_pkg_t* const*)a;
  const spn_resolved_pkg_t* rhs = *(const spn_resolved_pkg_t* const*)b;
  return lhs->priority < rhs->priority ? -1 : 1;
}

static s32 sort_req_canonical(const void* a, const void* b) {
  const spn_requested_dep_t* lhs = (const spn_requested_dep_t*)a;
  const spn_requested_dep_t* rhs = (const spn_requested_dep_t*)b;

  s32 order = sp_str_compare_alphabetical(lhs->qualified, rhs->qualified);
  if (order) return order;
  if (lhs->kind != rhs->kind) return lhs->kind < rhs->kind ? -1 : 1;
  if (lhs->private != rhs->private) return lhs->private < rhs->private ? -1 : 1;
  if (lhs->source != rhs->source) return lhs->source < rhs->source ? -1 : 1;

  switch (lhs->source) {
    case SPN_PKG_SOURCE_INDEX: {
      order = spn_semver_cmp(lhs->index.range.low.version, rhs->index.range.low.version);
      if (order) return order;
      if (lhs->index.range.low.op != rhs->index.range.low.op) return lhs->index.range.low.op < rhs->index.range.low.op ? -1 : 1;
      order = spn_semver_cmp(lhs->index.range.high.version, rhs->index.range.high.version);
      if (order) return order;
      if (lhs->index.range.high.op != rhs->index.range.high.op) return lhs->index.range.high.op < rhs->index.range.high.op ? -1 : 1;
      if (lhs->index.range.mod != rhs->index.range.mod) return lhs->index.range.mod < rhs->index.range.mod ? -1 : 1;
      return 0;
    }
    case SPN_PKG_SOURCE_FILE: {
      return sp_str_compare_alphabetical(lhs->file.path, rhs->file.path);
    }
    case SPN_PKG_SOURCE_ROOT: {
      return 0;
    }
  }
  sp_unreachable_return(0);
}

static bool find_pin(spn_scope_t* scope, sp_intern_id_t name, spn_semver_t* version, bool* contradiction) {
  bool found = false;
  sp_da_for(scope->pins, it) {
    if (scope->pins[it].name != name) {
      continue;
    }
    if (found && !spn_semver_eq(*version, scope->pins[it].version)) {
      *contradiction = true;
      return true;
    }
    *version = scope->pins[it].version;
    found = true;
  }
  return found;
}

static spn_scope_t* find_scope(spn_resolve_run_t* run, sp_intern_id_t root, spn_pkg_id_t from) {
  sp_da_for(run->scopes, it) {
    if (run->scopes[it].root == root && spn_pkg_id_eq(run->scopes[it].from, from)) {
      return &run->scopes[it];
    }
  }
  return SP_NULLPTR;
}

static u32 find_or_create_scope(spn_resolver_t* resolver, spn_resolve_run_t* run, sp_intern_id_t root, spn_pkg_id_t from) {
  sp_da_for(run->scopes, it) {
    if (run->scopes[it].root == root && spn_pkg_id_eq(run->scopes[it].from, from)) {
      return (u32)it;
    }
  }

  spn_scope_t scope = {
    .root = root,
    .from = from,
  };
  sp_da_init(resolver->mem, scope.reqs);
  sp_da_init(resolver->mem, scope.pins);
  sp_da_init(resolver->mem, scope.processes);
  sp_ht_init(resolver->mem, scope.named);
  sp_da_push(run->scopes, scope);
  return (u32)(sp_da_size(run->scopes) - 1);
}

static spn_err_union_t resolve_local_package(spn_resolver_t* resolver, spn_resolve_run_t* run, spn_resolved_pkg_t* from, spn_requested_dep_t* request) {
  spn_scope_t* scope = &run->scopes[run->scope];
  sp_intern_id_t name = sp_intern_get_or_insert(resolver->intern, request->qualified);

  if (sp_ht_getp(run->visited, name)) {
    return (spn_err_union_t) {
      .kind = SPN_ERR_DEP_CYCLE,
      .circular.id = err_pkg_name(spn_pkg_name_from_qualified(request->qualified)),
    };
  }

  if (sp_ht_getp(scope->named, name)) {
    return ok_union();
  }

  spn_registry_pkg_t* pkg = sp_ht_getp(*resolver->registry, ((spn_pkg_id_t) { .qualified = name }));

  // If the package is local, just load it
  if (!pkg && request->source == SPN_PKG_SOURCE_FILE) {
    try_union(load_file_pkg(resolver, request, &pkg));
  }

  if (!pkg) {
    return (spn_err_union_t) {
      .kind = SPN_ERR_PKG_UNKNOWN,
      .unknown.qualified = request->qualified
    };
  }

  spn_semver_t pinned = sp_zero;
  bool contradiction = false;
  bool held = find_forced(run, name, &pinned) || find_pin(scope, name, &pinned, &contradiction);
  if (held) {
    if (contradiction || !spn_semver_eq(pkg->info->version, pinned)) {
      return unsatisfiable_err(resolver, from, request, SPN_ERR_PKG_CONFLICT_EXACT, pinned);
    }
  }

  spn_resolved_pkg_t node = {
    .id = {
      .qualified = sp_intern_get_or_insert(resolver->intern, pkg->info->qualified),
      .version = pkg->info->version,
    },
    .name = pkg->info->name,
    .options = pkg->info->options,
    .source = pkg->source,
    .origin = {
      .recipe = {
        .kind = SPN_PKG_ROOT_LOCAL,
        .local = sp_fs_parent_path(pkg->manifest)
      },
      .source = spn_pkg_upstream(pkg->info),
      .paths = {
        .manifest = sp_fs_get_name(pkg->manifest),
        .script = sp_str_lit("spn.c")
      },
      .info = pkg->info,
    },
  };

  sp_da_init(resolver->mem, node.deps);

  spn_when_env_t env;
  node_options_env(resolver, &node, &env);
  sp_da_for(pkg->info->deps, it) {
    if (!spn_when_eval(&pkg->info->deps[it].when, &env)) {
      continue;
    }
    sp_da_push(node.deps, pkg->info->deps[it]);
  }
  sp_da_sort(node.deps, sort_req_canonical);

  return solve_node(resolver, run, scope, name, &node);
}

static spn_err_union_t try_candidate(spn_resolver_t* resolver, spn_resolve_run_t* run, spn_index_release_t* release) {
  if (run->fatal || !run->budget) {
    run->fatal = true;
    return (spn_err_union_t) {
      .kind = SPN_ERR_RESOLVE_TOO_COMPLEX,
      .too_complex.id = err_pkg_name(release->id),
    };
  }
  run->budget--;

  spn_scope_t* scope = &run->scopes[run->scope];
  sp_str_t qualified = spn_pkg_name_to_qualified(release->id);
  sp_intern_id_t name = sp_intern_get_or_insert(resolver->intern, qualified);

  spn_resolved_pkg_t node = {
    .id = {
      .qualified = name,
      .version = release->version,
    },
    .name = release->id.name,
    .options = release->options,
    .source = SPN_PKG_SOURCE_INDEX,
    .origin = {
      .release = release,
      .paths = release->paths,
      .recipe = release->manifest.kind ? release->manifest : release->source,
      .source = release->source,
    }
  };

  spn_when_env_t env;
  node_options_env(resolver, &node, &env);

  sp_da_init(resolver->mem, node.deps);
  sp_da_for(release->deps, it) {
    if (!spn_when_eval(&release->deps[it].when, &env)) {
      continue;
    }

    sp_da_push(node.deps, ((spn_requested_dep_t) {
      .qualified = spn_pkg_name_to_qualified(release->deps[it].id),
      .source = SPN_PKG_SOURCE_INDEX,
      .kind = dep_kind_from_index(release->deps[it].kind),
      .private = release->deps[it].private,
      .when = release->deps[it].when,
      .options = release->deps[it].options,
      .index = {
        .range = release->deps[it].range
      }
    }));
  }
  sp_da_sort(node.deps, sort_req_canonical);

  return solve_node(resolver, run, scope, name, &node);
}

static spn_err_union_t resolve_index_package(spn_resolver_t* resolver, spn_resolve_run_t* run, spn_resolved_pkg_t* from, spn_requested_dep_t* request) {
  spn_scope_t* scope = &run->scopes[run->scope];
  sp_intern_id_t name = sp_intern_get_or_insert(resolver->intern, request->qualified);

  if (sp_ht_getp(run->visited, name)) {
    return (spn_err_union_t) {
      .kind = SPN_ERR_DEP_CYCLE,
      .circular.id = err_pkg_name(spn_pkg_name_from_qualified(request->qualified)),
    };
  }

  // If this scope already resolved a version for this package, check if that version satisfies
  // this request, too. If not, the scope is unsatisfiable; a caller with alternatives backtracks.
  spn_resolved_pkg_t* existing = sp_ht_getp(scope->named, name);
  if (existing) {
    if (spn_semver_in_range(existing->id.version, request->index.range)) {
      return ok_union();
    }

    return unsatisfiable_err(resolver, from, request, SPN_ERR_PKG_CONFLICT, existing->id.version);
  }

  spn_index_pkg_t* pkg = SP_NULLPTR;
  spn_index_diag_t diag = sp_zero;
  spn_err_t got = spn_index_cache_get_package(resolver->index, spn_pkg_name_from_qualified(request->qualified), &pkg, &diag);
  if (got) {
    return index_err(resolver, request, got, &diag);
  }
  if (!pkg) {
    return (spn_err_union_t) {
      .kind = SPN_ERR_PKG_UNKNOWN,
      .unknown = {
        .qualified = request->qualified,
        .range = spn_semver_range_to_str(resolver->mem, request->index.range),
      },
    };
  }

  // By definition, if a name is pinned and a candidatae version doesn't satisfy
  // the pin, this branch of resolution fails.
  spn_semver_t pinned = sp_zero;
  bool contradiction = false;
  bool held = find_forced(run, name, &pinned) || find_pin(scope, name, &pinned, &contradiction);
  if (held) {
    if (contradiction || !spn_semver_in_range(pinned, request->index.range)) {
      return unsatisfiable_err(resolver, from, request, SPN_ERR_PKG_CONFLICT, pinned);
    }

    sp_da_for(pkg->releases, it) {
      if (spn_semver_eq(pkg->releases[it].version, pinned)) {
        return try_candidate(resolver, run, &pkg->releases[it]);
      }
    }

    return unsatisfiable_err(resolver, from, request, SPN_ERR_PKG_CONFLICT, pinned);
  }

  // If the name is totally free, just pick the newest legal version. This
  // is just a simple, greedy heuristic, not for correctness.
  spn_err_union_t first = sp_zero;
  sp_da_rfor(pkg->releases, it) {
    spn_index_release_t* release = &pkg->releases[it];
    if (release->yanked) {
      continue;
    }
    if (!spn_semver_in_range(release->version, request->index.range)) {
      continue;
    }

    spn_err_union_t err = try_candidate(resolver, run, release);
    if (!err.kind || run->fatal) {
      return err;
    }
    if (!first.kind) {
      first = err;
    }
  }

  if (first.kind) {
    return first;
  }
  return unsatisfiable_err(resolver, from, request, SPN_ERR_PKG_NO_MATCH, sp_zero_s(spn_semver_t));
}

static spn_err_union_t solve_node(spn_resolver_t* resolver, spn_resolve_run_t* run, spn_scope_t* scope, sp_intern_id_t name, spn_resolved_pkg_t* node) {
  node->priority = run->picks++;

  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  sp_da(sp_intern_id_t) snapshot = snapshot_named(scratch.mem, scope);
  u64 boundaries = sp_da_size(run->boundaries);
  sp_ht_insert(scope->named, name, *node);

  spn_err_union_t err = resolve_deps(resolver, run, node);
  if (err.kind) {
    restore_named(scratch.mem, scope, snapshot);
    sp_da_head(run->boundaries)->size = boundaries;
  }

  sp_mem_end_scratch(scratch);
  return err;
}

static spn_err_union_t resolve_dep(spn_resolver_t* resolver, spn_resolve_run_t* run, spn_resolved_pkg_t* node, spn_requested_dep_t* dep) {
  spn_dep_edge_t edge = classify_dep(resolver, node, dep);
  switch (edge) {
    case SPN_DEP_EDGE_PRUNED: {
      return ok_union();
    }
    case SPN_DEP_EDGE_PROCESS:
    case SPN_DEP_EDGE_PRIVATE: {
      sp_da_push(run->boundaries, ((spn_scope_boundary_t) {
        .from = node->id,
        .edge = edge,
        .req = *dep,
      }));
      return ok_union();
    }
    case SPN_DEP_EDGE_SCOPE: {
      break;
    }
  }

  switch (dep->source) {
    case SPN_PKG_SOURCE_INDEX: return resolve_index_package(resolver, run, node, dep);
    case SPN_PKG_SOURCE_ROOT:
    case SPN_PKG_SOURCE_FILE:  return resolve_local_package(resolver, run, node, dep);
  }

  sp_unreachable_return(ok_union());
}

static spn_err_union_t resolve_deps(spn_resolver_t* resolver, spn_resolve_run_t* run, spn_resolved_pkg_t* node) {
  sp_ht_insert(run->visited, node->id.qualified, (u8)true);

  spn_err_union_t err = sp_zero;
  sp_da_for(node->deps, it) {
    err = resolve_dep(resolver, run, node, &node->deps[it]);
    if (err.kind) break;
  }

  sp_ht_erase(run->visited, node->id.qualified);
  return err;
}

static spn_err_union_t solve_reqs(spn_resolver_t* resolver, spn_resolve_run_t* run, u64 from, u64 to) {
  spn_resolved_pkg_t root = {
    .id = spn_pkg_id(resolver->intern, sp_str_lit("")),
    .source = SPN_PKG_SOURCE_ROOT,
  };

  for (u64 it = from; it < to; it++) {
    spn_requested_dep_t req = run->scopes[run->scope].reqs[it];
    try_union(resolve_dep(resolver, run, &root, &req));
  }

  return ok_union();
}

// The full assignment for some feasibility check.
typedef struct {
  sp_da(sp_intern_id_t) keys;
  sp_da(spn_resolved_pkg_t) vals;
  sp_da(spn_scope_boundary_t) boundaries;
  bool held;
} spn_witness_t;

static void capture_witness(sp_mem_t mem, spn_resolve_run_t* run, spn_scope_t* scope, sp_da(sp_intern_id_t) snapshot, u64 boundaries, spn_witness_t* witness) {
  witness->keys = sp_da_new(mem, sp_intern_id_t);
  witness->vals = sp_da_new(mem, spn_resolved_pkg_t);
  witness->boundaries = sp_da_new(mem, spn_scope_boundary_t);

  sp_ht_for_kv(scope->named, it) {
    bool held = false;
    sp_da_for(snapshot, jt) {
      if (snapshot[jt] == *it.key) {
        held = true;
        break;
      }
    }
    if (!held) {
      sp_da_push(witness->keys, *it.key);
      sp_da_push(witness->vals, *it.val);
    }
  }

  for (u64 it = boundaries; it < sp_da_size(run->boundaries); it++) {
    sp_da_push(witness->boundaries, run->boundaries[it]);
  }

  witness->held = true;
}

static spn_err_union_t attempt_reqs(spn_resolver_t* resolver, spn_resolve_run_t* run, u64 from, u64 to, bool keep, spn_witness_t* witness) {
  spn_scope_t* scope = &run->scopes[run->scope];

  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  sp_da(sp_intern_id_t) snapshot = snapshot_named(scratch.mem, scope);
  u64 boundaries = sp_da_size(run->boundaries);

  spn_err_union_t err = solve_reqs(resolver, run, from, to);
  if (!err.kind && witness) {
    capture_witness(resolver->mem, run, scope, snapshot, boundaries, witness);
  }
  if (err.kind || !keep) {
    restore_named(scratch.mem, scope, snapshot);
    sp_da_head(run->boundaries)->size = boundaries;
  }

  sp_mem_end_scratch(scratch);
  return err;
}

static spn_err_union_t resolve_scope(spn_resolver_t* resolver, spn_resolve_run_t* run) {
  spn_scope_t* scope = &run->scopes[run->scope];
  u64 from = scope->cursor;
  u64 to = sp_da_size(scope->reqs);
  scope->cursor = to;
  run->budget = resolver->budget;
  sp_os_qsort(scope->reqs + from, to - from, sizeof(spn_requested_dep_t), sort_req_canonical);

  // Re-entered scopes solve under their kept pins; re-pushed requests are a
  // function of manifests the scope already resolved, so they only duplicate
  // what it holds
  if (from || !sp_ht_size(run->query->result)) {
    return solve_reqs(resolver, run, from, to);
  }

  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();

  // # PICKS
  //
  // Picks are outputs. A pick says that some scope resolved name X to
  // version Y in a way that cannot be changed by anything that happens by
  // another scope.
  sp_da(spn_resolved_pkg_t*) picks = sp_da_new(scratch.mem, spn_resolved_pkg_t*);
  sp_ht_for_kv(run->query->result, it) {
    sp_da_push(picks, it.val);
  }
  sp_da_sort(picks, sort_pick_by_priority);

  // # PINS
  //
  // Pins are constraints. A pin says that if you're resolving name X, try
  // version Y. But if version Y doesn't work (either by itself or its
  // subtrees, exhaustively), it can be unpinned.
  //
  // But, and this is important, "try a different version" does not mean
  // "override the pick in the parent scope". Picks are final decisions. They
  // can't be changed by a child scope. What happens instead is that two
  // versions of X now exist
  //
  // How that gets processed just depends on what X actually is. If X is, say,
  // linked dynamically such that both copies would be visible, that's an
  // error.
  //
  // # EXAMPLE
  //
  // If A is pinned to 1.9.0 and we have a simple dependency on B@1.0.0
  //
  // PIN: A@1.9.0
  // NEED: B@1.0.0
  //   TRY C@1.1.0 (The solver's free choice)
  //     NEED: A =2.0.0
  //       FAIL (Only candidate is A@1.9.0, from the pin)
  //   KILL C@1.1.0 (The pin for A is not dropped yet)
  //   TRY C@1.0.0 (This is just the solver trying the next version of C)
  //     NEED A ^1.0.0
  //       OK (The pinned A@1.9.0 works)
  //   SUCCESS
  sp_da(spn_pin_t) pins = sp_da_new(scratch.mem, spn_pin_t);
  sp_da_for(picks, it) {
    sp_da_push(pins, ((spn_pin_t) {
      .name = picks[it]->id.qualified,
      .version = picks[it]->id.version,
    }));
  }

  // Try to solve, given what we have pinned. If it succeeds, we're done.
  sp_da_for(pins, it) {
    sp_da_push(scope->pins, pins[it]);
  }
  spn_err_union_t err = attempt_reqs(resolver, run, from, to, true, SP_NULLPTR);
  if (!err.kind || run->fatal) {
    sp_mem_end_scratch(scratch);
    return err;
  }
  sp_da_clear(scope->pins);

  // At this point, we know that this set of pins can't work, but we don't
  // know which pin is guilty.
  //
  // Walk the pins, which are transitively sorted by priority, and keep
  // a pin iff a complete assignment exists satisfying the set. If we drop
  // a pin, we'll have to split.
  //
  // Keeping a pin, on the other hand, means that X@Y is proven and that
  // we have an assignment that satisfies the entire set of pins at the
  // point the pin in question was run.
  //
  // Say that we have [A, B, C]
  // - A's check succeeds. A is kept; the assignment for {A} is saved as the witness.
  // - B's check fails. B is dropped; the witness is still valid for {A}.
  // - C's check succeeds. C is kept, and the witness is now an assignment for {A, C}
  spn_witness_t witness = sp_zero;
  sp_da_for(pins, it) {
    sp_da_push(scope->pins, pins[it]);
    spn_err_union_t attempt = attempt_reqs(resolver, run, from, to, false, &witness);
    if (attempt.kind) {
      if (run->fatal) {
        sp_mem_end_scratch(scratch);
        return attempt;
      }
      sp_da_head(scope->pins)->size -= 1;
    }
  }

  // At this point, the pins that were kept form a set that is proven to be
  // satisfiable. In our example, we know that we can satisfy {A, C}. Mark
  // it down and keep going.
  if (witness.held) {
    sp_da_for(witness.keys, it) {
      sp_ht_insert(scope->named, witness.keys[it], witness.vals[it]);
    }
    sp_da_for(witness.boundaries, it) {
      sp_da_push(run->boundaries, witness.boundaries[it]);
    }
    sp_mem_end_scratch(scratch);
    return ok_union();
  }

  sp_mem_end_scratch(scratch);
  return solve_reqs(resolver, run, from, to);
}

static void commit_scope(spn_resolver_t* resolver, spn_resolve_run_t* run) {
  spn_scope_t* scope = &run->scopes[run->scope];

  sp_ht_for_kv(scope->named, it) {
    if (!sp_ht_getp(run->query->result, it.val->id)) {
      sp_ht_insert(run->query->result, it.val->id, *it.val);
    }
  }
}

static void process_boundaries(spn_resolver_t* resolver, spn_resolve_run_t* run) {
  u32 from = run->scope;

  sp_da_for(run->boundaries, it) {
    spn_scope_boundary_t boundary = run->boundaries[it];

    sp_intern_id_t root = boundary.edge == SPN_DEP_EDGE_PRIVATE ?
      boundary.from.qualified :
      sp_intern_get_or_insert(resolver->intern, boundary.req.qualified);
    u32 target = find_or_create_scope(resolver, run, root, boundary.from);

    sp_da_push(run->edges, ((spn_scope_edge_t) {
      .from = from,
      .to = target,
      .private = boundary.edge == SPN_DEP_EDGE_PRIVATE,
    }));

    spn_requested_dep_t req = boundary.req;
    req.kind = SPN_DEP_KIND_PACKAGE;
    req.private = false;
    sp_da_push(run->scopes[target].reqs, req);
  }

  sp_da_clear(run->boundaries);
}

static void scope_add_process(spn_scope_t* scope, u32 process) {
  sp_da_for(scope->processes, it) {
    if (scope->processes[it] == process) {
      return;
    }
  }
  sp_da_push(scope->processes, process);
}

static bool scope_has_process(spn_scope_t* scope, u32 process) {
  sp_da_for(scope->processes, it) {
    if (scope->processes[it] == process) {
      return true;
    }
  }
  return false;
}

static void compute_processes(spn_resolve_run_t* run) {
  scope_add_process(&run->scopes[0], 0);
  sp_da_for(run->edges, it) {
    if (!run->edges[it].private) {
      scope_add_process(&run->scopes[run->edges[it].to], run->edges[it].to);
    }
  }

  bool changed = true;
  while (changed) {
    changed = false;
    sp_da_for(run->edges, it) {
      if (!run->edges[it].private) {
        continue;
      }

      spn_scope_t* from = &run->scopes[run->edges[it].from];
      spn_scope_t* to = &run->scopes[run->edges[it].to];
      sp_da_for(from->processes, jt) {
        if (!scope_has_process(to, from->processes[jt])) {
          scope_add_process(to, from->processes[jt]);
          changed = true;
        }
      }
    }
  }
}

static sp_da(spn_node_edge_t) collect_node_edges(spn_resolver_t* resolver, spn_resolve_run_t* run, sp_mem_t mem, u32 scope_index, spn_resolved_pkg_t* node) {
  sp_da(spn_node_edge_t) edges = sp_da_new(mem, spn_node_edge_t);

  sp_da_for(node->deps, it) {
    spn_requested_dep_t* dep = &node->deps[it];
    spn_dep_edge_t edge = classify_dep(resolver, node, dep);
    if (edge == SPN_DEP_EDGE_PRUNED) {
      continue;
    }

    sp_intern_id_t name = sp_intern_get_or_insert(resolver->intern, dep->qualified);
    u32 target_scope = scope_index;
    if (edge != SPN_DEP_EDGE_SCOPE) {
      spn_scope_t* found = find_scope(run, edge == SPN_DEP_EDGE_PRIVATE ? node->id.qualified : name, node->id);
      if (!found) {
        continue;
      }
      target_scope = (u32)(found - run->scopes);
    }

    spn_resolved_pkg_t* target = sp_ht_getp(run->scopes[target_scope].named, name);
    if (!target) {
      continue;
    }

    bool present = false;
    sp_da_for(edges, jt) {
      if (edges[jt].scope == target_scope && edges[jt].name == name && edges[jt].kind == dep->kind && edges[jt].private == dep->private) {
        present = true;
        break;
      }
    }
    if (!present) {
      sp_da_push(edges, ((spn_node_edge_t) {
        .scope = target_scope,
        .name = name,
        .node = target,
        .kind = dep->kind,
        .edge = edge,
        .private = dep->private,
      }));
    }
  }

  return edges;
}

static spn_err_union_t check_group_cycle(spn_resolver_t* resolver, spn_resolve_run_t* run, spn_group_states_t states, u32 scope_index, sp_intern_id_t name) {
  spn_group_node_t key = { .scope = scope_index, .name = name };
  u8* state = sp_ht_getp(states, key);
  if (state && *state == 2) {
    return ok_union();
  }

  spn_scope_t* scope = &run->scopes[scope_index];
  spn_resolved_pkg_t* node = sp_ht_getp(scope->named, name);
  if (!node) {
    return ok_union();
  }

  if (state && *state == 1) {
    return (spn_err_union_t) {
      .kind = SPN_ERR_UNIT_CYCLE,
      .unit_cycle = {
        .id = err_pkg_name(spn_pkg_name_from_qualified(sp_intern_str_from_id(resolver->intern, node->id.qualified))),
        .version = node->id.version,
      }
    };
  }

  sp_ht_insert(states, key, (u8)1);

  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  sp_da(spn_node_edge_t) edges = collect_node_edges(resolver, run, scratch.mem, scope_index, node);

  spn_err_union_t err = sp_zero;
  sp_da_for(edges, it) {
    err = check_group_cycle(resolver, run, states, edges[it].scope, edges[it].name);
    if (err.kind) break;
  }

  sp_mem_end_scratch(scratch);
  try_union(err);

  *sp_ht_getp(states, key) = 2;
  return ok_union();
}

static spn_err_union_t check_group_cycles(spn_resolver_t* resolver, spn_resolve_run_t* run) {
  // The walk opens scratch frames per node, so the states table must not
  // grow from the scratch arena
  spn_group_states_t states = SP_NULLPTR;
  sp_ht_init(resolver->mem, states);

  spn_err_union_t err = sp_zero;
  sp_da_for(run->scopes, it) {
    spn_scope_t* scope = &run->scopes[it];
    sp_ht_for_kv(scope->named, jt) {
      err = check_group_cycle(resolver, run, states, (u32)it, *jt.key);
      if (err.kind) break;
    }
    if (err.kind) break;
  }

  return err;
}

static sp_hash_t leaf_hash(spn_resolver_t* resolver, spn_resolved_pkg_t* instance) {
  sp_hash_t hash = sp_hash_str(sp_intern_str_from_id(resolver->intern, instance->id.qualified));
  return sp_hash_bytes(&instance->id.version, sizeof(spn_semver_t), hash);
}

static s32 sort_edge_record(const void* a, const void* b) {
  const spn_edge_record_t* lhs = (const spn_edge_record_t*)a;
  const spn_edge_record_t* rhs = (const spn_edge_record_t*)b;
  if (lhs->hash != rhs->hash) return lhs->hash < rhs->hash ? -1 : 1;
  if (lhs->kind != rhs->kind) return lhs->kind < rhs->kind ? -1 : 1;
  if (lhs->private != rhs->private) return lhs->private < rhs->private ? -1 : 1;
  return 0;
}

static sp_hash_t hash_group_node(spn_resolver_t* resolver, spn_resolve_run_t* run, spn_group_hash_t memo, u32 scope_index, sp_intern_id_t name) {
  spn_group_node_t key = { .scope = scope_index, .name = name };
  spn_resolved_pkg_t* node = sp_ht_getp(run->scopes[scope_index].named, name);

  spn_hash_state_t* state = sp_ht_getp(memo, key);
  if (state && state->state == 2) {
    return state->hash;
  }
  if (state && state->state == 1) {
    return leaf_hash(resolver, node);
  }

  sp_ht_insert(memo, key, ((spn_hash_state_t) { .state = 1 }));

  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  sp_da(spn_node_edge_t) edges = collect_node_edges(resolver, run, scratch.mem, scope_index, node);
  sp_da(spn_edge_record_t) records = sp_da_new(scratch.mem, spn_edge_record_t);
  sp_da_for(edges, it) {
    sp_da_push(records, ((spn_edge_record_t) {
      .hash = hash_group_node(resolver, run, memo, edges[it].scope, edges[it].name),
      .kind = (u32)edges[it].kind,
      .private = (u32)edges[it].private,
    }));
  }
  sp_da_sort(records, sort_edge_record);

  sp_hash_t hash = leaf_hash(resolver, node);
  if (!sp_da_empty(records)) {
    hash = sp_hash_bytes(records, sp_da_size(records) * sizeof(spn_edge_record_t), hash);
  }
  sp_mem_end_scratch(scratch);

  spn_hash_state_t* slot = sp_ht_getp(memo, key);
  slot->state = 2;
  slot->hash = hash;
  return hash;
}

static void commit_instances(spn_resolver_t* resolver, spn_resolve_run_t* run, spn_group_hash_t memo) {
  spn_resolve_t rekeyed = SP_NULLPTR;
  sp_ht_init(resolver->mem, rekeyed);

  sp_da_for(run->scopes, s) {
    spn_scope_t* scope = &run->scopes[s];
    sp_ht_for_kv(scope->named, it) {
      spn_resolved_pkg_t* node = it.val;
      spn_pkg_id_t id = node->id;
      id.hash = sp_ht_getp(memo, ((spn_group_node_t) { .scope = (u32)s, .name = *it.key }))->hash;
      if (sp_ht_getp(rekeyed, id)) {
        continue;
      }

      spn_resolved_pkg_t instance = *node;
      instance.id = id;
      sp_da_init(resolver->mem, instance.edges);

      sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
      sp_da(spn_node_edge_t) edges = collect_node_edges(resolver, run, scratch.mem, (u32)s, node);
      sp_da_for(edges, jt) {
        spn_pkg_id_t target = edges[jt].node->id;
        target.hash = sp_ht_getp(memo, ((spn_group_node_t) { .scope = edges[jt].scope, .name = edges[jt].name }))->hash;
        sp_da_push(instance.edges, ((spn_resolved_dep_t) {
          .id = target,
          .kind = edges[jt].kind,
          .edge = edges[jt].edge,
          .private = edges[jt].private,
        }));
      }
      sp_mem_end_scratch(scratch);

      sp_ht_insert(rekeyed, id, instance);
    }
  }

  run->query->result = rekeyed;
}

typedef struct {
  spn_resolved_pkg_t* node;
  sp_hash_t hash;
} spn_shared_seen_t;

static spn_err_union_t check_dynamic_duplicates(spn_resolver_t* resolver, spn_resolve_run_t* run, spn_group_hash_t memo) {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  spn_err_union_t result = sp_zero;

  sp_da_for(run->scopes, process) {
    sp_ht(sp_intern_id_t, spn_shared_seen_t) shared = SP_NULLPTR;
    sp_ht_init(scratch.mem, shared);

    sp_da_for(run->scopes, s) {
      spn_scope_t* scope = &run->scopes[s];
      if (!scope_has_process(scope, (u32)process)) {
        continue;
      }

      sp_ht_for_kv(scope->named, it) {
        spn_resolved_pkg_t* node = it.val;
        if (!node_is_shared(resolver, node)) {
          continue;
        }

        sp_hash_t hash = sp_ht_getp(memo, ((spn_group_node_t) { .scope = (u32)s, .name = *it.key }))->hash;
        spn_shared_seen_t* prior = sp_ht_getp(shared, node->id.qualified);
        if (!prior) {
          sp_ht_insert(shared, node->id.qualified, ((spn_shared_seen_t) { .node = node, .hash = hash }));
          continue;
        }

        if (prior->hash == hash) {
          continue;
        }

        bool ordered = spn_semver_le(prior->node->id.version, node->id.version);
        spn_semver_t low = ordered ? prior->node->id.version : node->id.version;
        spn_semver_t high = ordered ? node->id.version : prior->node->id.version;
        result = (spn_err_union_t) {
          .kind = SPN_ERR_DYNAMIC_DUPLICATE,
          .dynamic_dup = {
            .id = err_pkg_name(spn_pkg_name_from_qualified(sp_intern_str_from_id(resolver->intern, node->id.qualified))),
            .low = low,
            .high = high,
          }
        };

        // A duplicate between two different versions may be an artifact of a
        // greedy pick: some assignment holding the name to one of the duped
        // versions everywhere might satisfy every scope. The caller re-solves
        // under each candidate and keeps the first full success.
        if (!run->has_forced && !spn_semver_eq(low, high)) {
          run->retry[0] = (spn_pin_t) { .name = node->id.qualified, .version = low };
          run->retry[1] = (spn_pin_t) { .name = node->id.qualified, .version = high };
          run->retries = 2;
        }
        goto done;
      }
    }
  }

done:
  sp_mem_end_scratch(scratch);
  return result;
}

static spn_err_union_t solve_once(spn_resolver_t* resolver, spn_resolve_query_t* query, spn_pin_t* forced, spn_resolve_run_t* run) {
  *run = (spn_resolve_run_t) {
    .query = query,
  };
  if (forced) {
    run->forced = *forced;
    run->has_forced = true;
  }
  sp_da_init(resolver->mem, run->scopes);
  sp_da_init(resolver->mem, run->boundaries);
  sp_da_init(resolver->mem, run->edges);
  sp_ht_init(resolver->mem, run->visited);

  u32 root = find_or_create_scope(resolver, run, 0, sp_zero_s(spn_pkg_id_t));
  sp_da_for(query->reqs, it) {
    sp_da_push(run->scopes[root].reqs, query->reqs[it]);
  }

  bool progress = true;
  while (progress) {
    progress = false;

    sp_da_for(run->scopes, it) {
      spn_scope_t* scope = &run->scopes[it];
      if (scope->cursor == sp_da_size(scope->reqs)) {
        continue;
      }

      progress = true;
      run->scope = (u32)it;

      try_union(resolve_scope(resolver, run));

      commit_scope(resolver, run);
      process_boundaries(resolver, run);
    }
  }

  compute_processes(run);
  try_union(check_group_cycles(resolver, run));

  spn_group_hash_t memo = SP_NULLPTR;
  sp_ht_init(resolver->mem, memo);
  sp_da_for(run->scopes, it) {
    spn_scope_t* scope = &run->scopes[it];
    sp_ht_for_kv(scope->named, jt) {
      hash_group_node(resolver, run, memo, (u32)it, *jt.key);
    }
  }

  commit_instances(resolver, run, memo);
  return check_dynamic_duplicates(resolver, run, memo);
}

spn_err_t spn_resolve_from_solver(spn_resolver_t* resolver, spn_resolve_query_t* query) {
  sp_tm_timer_t timer = sp_tm_start_timer();

  spn_resolve_run_t run = sp_zero;
  spn_err_union_t err = solve_once(resolver, query, SP_NULLPTR, &run);

  for (u32 it = 0; err.kind && it < run.retries; it++) {
    query->result = SP_NULLPTR;
    sp_ht_init(resolver->mem, query->result);

    spn_resolve_run_t retry = sp_zero;
    if (!solve_once(resolver, query, &run.retry[it], &retry).kind) {
      err = ok_union();
    }
  }

  if (err.kind) {
    sp_da_push(query->errors, err);
  }

  query->time = sp_tm_read_timer(&timer);
  return sp_da_empty(query->errors) ? SPN_OK : query->errors[0].kind;
}
