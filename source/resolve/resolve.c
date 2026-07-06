#include "sp.h"
#include "sp/macro.h"
#include "error/types.h"
#include "event/event.h"
#include "index/cache.h"
#include "intern/intern.h"
#include "pkg/id.h"
#include "pkg/types.h"
#include "resolve/resolve.h"
#include "resolve/types.h"
#include "semver/compare.h"
#include "semver/parser.h"
#include "sp/str.h"
#include "semver/types.h"
#include "session/registry/registry.h"
#include "session/registry/types.h"
#include "target/mutate.h"
#include "target/select.h"
#include "spn.h"

typedef struct {
  spn_pkg_id_t from;
  spn_dep_edge_t edge;
  spn_requested_pkg_t req;
} spn_scope_boundary_t;

typedef struct {
  sp_intern_id_t name;
  spn_semver_t version;
} spn_pin_t;

typedef struct {
  sp_intern_id_t root;
  spn_pkg_id_t from;
  sp_da(spn_requested_pkg_t) reqs;
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
  bool failed;
  spn_build_event_t failure;
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

typedef sp_ht(spn_pkg_id_t, spn_hash_state_t) spn_hash_memo_t;

typedef struct {
  sp_hash_t hash;
  u32 kind;
  u32 private;
} spn_edge_record_t;

static spn_err_t resolve_dep(spn_resolver_t* resolver, spn_resolve_run_t* run, spn_resolved_pkg_t* node, spn_requested_pkg_t* dep);
static spn_err_t resolve_deps(spn_resolver_t* resolver, spn_resolve_run_t* run, spn_resolved_pkg_t* node);

void spn_resolver_init(spn_resolver_t* r, sp_mem_t mem, sp_intern_t* intern, spn_index_cache_t* index, spn_pkg_registry_t* registry, spn_event_buffer_t* events, spn_linkage_t linkage, sp_da(spn_pkg_config_entry_t) config, u64 budget) {
  *r = (spn_resolver_t){
    .mem = mem,
    .intern = intern,
    .index = index,
    .events = events,
    .registry = registry,
    .linkage = linkage,
    .config = config,
    .budget = budget ? budget : SPN_RESOLVE_DEFAULT_BUDGET,
  };
}

void spn_resolve_query_init(sp_mem_t mem, spn_resolve_query_t* query) {
  *query = (spn_resolve_query_t) sp_zero_initialize();
  sp_da_init(mem, query->reqs);
  sp_ht_init(mem, query->result);
}

void spn_resolve_query_add(spn_resolve_query_t* query, spn_requested_pkg_t req) {
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

static bool version_in_range(spn_semver_t version, spn_semver_range_t range) {
  return
    spn_semver_satisfies(version, range.low.version, range.low.op) &&
    spn_semver_satisfies(version, range.high.version, range.high.op);
}

static void record_failure(spn_resolve_run_t* run, spn_build_event_t event) {
  if (run->failed) {
    return;
  }
  run->failed = true;
  run->failure = event;
}

static spn_kind_query_t kind_query(spn_resolver_t* resolver, sp_str_t pkg_name) {
  spn_kind_query_t query = { .linkage = resolver->linkage };
  sp_da_for(resolver->config, it) {
    spn_pkg_config_entry_t* entry = &resolver->config[it];
    if (sp_str_equal(entry->key, pkg_name) && !sp_opt_is_null(entry->value.kind)) {
      sp_opt_set(query.config, entry->value.kind.value);
    }
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

static bool node_is_shared(spn_resolver_t* resolver, spn_resolved_pkg_t* node) {
  spn_kind_query_t query = kind_query(resolver, spn_pkg_name_from_qualified(node->qualified).name);

  switch (node->source) {
    case SPN_PKG_SOURCE_INDEX: {
      if (!node->index.release) {
        return false;
      }

      sp_da_for(node->index.release->targets, it) {
        spn_index_rel_target_t* target = &node->index.release->targets[it];
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

static spn_dep_edge_t classify_dep(spn_resolver_t* resolver, spn_resolved_pkg_t* node, spn_requested_pkg_t* dep) {
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

static bool pkg_id_eq(spn_pkg_id_t a, spn_pkg_id_t b) {
  return a.qualified == b.qualified && spn_semver_eq(a.version, b.version);
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
    if (run->scopes[it].root == root && pkg_id_eq(run->scopes[it].from, from)) {
      return &run->scopes[it];
    }
  }
  return SP_NULLPTR;
}

static u32 find_or_create_scope(spn_resolver_t* resolver, spn_resolve_run_t* run, sp_intern_id_t root, spn_pkg_id_t from) {
  sp_da_for(run->scopes, it) {
    if (run->scopes[it].root == root && pkg_id_eq(run->scopes[it].from, from)) {
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

static spn_err_t resolve_local_package(spn_resolver_t* resolver, spn_resolve_run_t* run, spn_requested_pkg_t* request) {
  spn_scope_t* scope = &run->scopes[run->scope];
  sp_intern_id_t name = sp_intern_get_or_insert(resolver->intern, request->qualified);

  if (sp_ht_getp(run->visited, name)) {
    record_failure(run, (spn_build_event_t) {
      .kind = SPN_EVENT_ERR_CIRCULAR_DEP,
      .circular.id = spn_pkg_name_from_qualified(request->qualified),
    });
    return SPN_ERROR;
  }

  if (sp_ht_getp(scope->named, name)) {
    return SPN_OK;
  }

  spn_registry_pkg_t* pkg = sp_ht_getp(*resolver->registry, ((spn_pkg_id_t) { .qualified = name }));

  // If the package is local, just load it
  if (!pkg && request->source == SPN_PKG_SOURCE_FILE) {
    spn_registry_err_t err = sp_zero;
    pkg = spn_registry_load_file_pkg(resolver->registry, resolver->mem, resolver->intern, request->qualified, request->file.path, &err);
    if (!pkg) {
      record_failure(run, (spn_build_event_t) {
        .kind = SPN_EVENT_ERR_MANIFEST,
        .manifest_err = {
          .name = request->qualified,
          .path = err.manifest,
          .error = err.error,
          .issues = err.issues,
        }
      });
      return SPN_ERROR;
    }
  }

  if (!pkg) {
    record_failure(run, (spn_build_event_t) {
      .kind = SPN_EVENT_ERR_UNKNOWN_PKG,
      .unknown.request = *request
    });
    return SPN_ERROR;
  }

  spn_semver_t pinned = sp_zero;
  bool contradiction = false;
  if (find_pin(scope, name, &pinned, &contradiction)) {
    if (contradiction || !spn_semver_eq(pkg->info->version, pinned)) {
      record_failure(run, (spn_build_event_t) {
        .kind = SPN_EVENT_ERR_UNSATISFIABLE_VERSION,
        .unsatisfiable = {
          .low = *request,
          .high = *request
        }
      });
      return SPN_ERROR;
    }
  }

  spn_resolved_pkg_t node = {
    .id = {
      .qualified = sp_intern_get_or_insert(resolver->intern, pkg->info->qualified),
      .version = pkg->info->version,
    },
    .qualified = pkg->info->qualified,
    .source = pkg->source,
    .version = pkg->info->version,
  };

  if (pkg->source == SPN_PKG_SOURCE_FILE) {
    node.file.path = request->file.path;
  }

  sp_da_init(resolver->mem, node.deps);

  // Looks frivolous, but the point here is that the list of dependencies defined in the manifest is
  // not the same as the list of dependencies we're actually using for this build.
  //
  // Pretty much any conditional compilation feature will run into this, and this is the place to
  // filter dependencies (e.g. if pkg is only a dependency on aarch64, it would get cut here). The
  // reason it's done here is that:
  //   - It's the earliest place we *can* do it, because resolve is the first place where we have
  //     package dependencies rather than just a list of requested package names
  //   - We don't want to do it later, because then we'd fetch index data for packages that aren't
  //     even going to be included in the build
  sp_da_for(pkg->info->deps, it) {
    sp_da_push(node.deps, pkg->info->deps[it]);
  }

  node.priority = run->picks++;

  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  sp_da(sp_intern_id_t) snapshot = snapshot_named(scratch.mem, scope);
  u64 boundaries = sp_da_size(run->boundaries);
  sp_ht_insert(scope->named, name, node);

  if (!resolve_deps(resolver, run, &node)) {
    sp_mem_end_scratch(scratch);
    return SPN_OK;
  }

  restore_named(scratch.mem, scope, snapshot);
  sp_da_head(run->boundaries)->size = boundaries;
  sp_mem_end_scratch(scratch);
  return SPN_ERROR;
}

static spn_err_t try_candidate(spn_resolver_t* resolver, spn_resolve_run_t* run, spn_index_rel_t* release) {
  if (run->fatal) {
    return SPN_ERROR;
  }
  if (!run->budget) {
    run->fatal = true;
    run->failed = true;
    run->failure = (spn_build_event_t) {
      .kind = SPN_EVENT_ERR_RESOLUTION_TOO_COMPLEX,
      .too_complex.id = release->id,
    };
    return SPN_ERROR;
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
    .qualified = qualified,
    .source = SPN_PKG_SOURCE_INDEX,
    .version = release->version,
    .index = {
      .release = release,
    }
  };

  sp_da_init(resolver->mem, node.deps);
  sp_da_for(release->deps, it) {
    sp_da_push(node.deps, ((spn_requested_pkg_t) {
      .qualified = spn_pkg_name_to_qualified(release->deps[it].id),
      .source = SPN_PKG_SOURCE_INDEX,
      .kind = dep_kind_from_index(release->deps[it].kind),
      .private = release->deps[it].private,
      .index = {
        .range = spn_semver_parse_range(release->deps[it].version)
      }
    }));
  }

  node.priority = run->picks++;

  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  sp_da(sp_intern_id_t) snapshot = snapshot_named(scratch.mem, scope);
  u64 boundaries = sp_da_size(run->boundaries);
  sp_ht_insert(scope->named, name, node);

  if (!resolve_deps(resolver, run, &node)) {
    run->failed = false;
    sp_mem_end_scratch(scratch);
    return SPN_OK;
  }

  restore_named(scratch.mem, scope, snapshot);
  sp_da_head(run->boundaries)->size = boundaries;
  sp_mem_end_scratch(scratch);
  return SPN_ERROR;
}

static spn_err_t resolve_index_package(spn_resolver_t* resolver, spn_resolve_run_t* run, spn_requested_pkg_t* request) {
  spn_scope_t* scope = &run->scopes[run->scope];
  sp_intern_id_t name = sp_intern_get_or_insert(resolver->intern, request->qualified);

  if (sp_ht_getp(run->visited, name)) {
    record_failure(run, (spn_build_event_t) {
      .kind = SPN_EVENT_ERR_CIRCULAR_DEP,
      .circular.id = spn_pkg_name_from_qualified(request->qualified),
    });
    return SPN_ERROR;
  }

  // If this scope already resolved a version for this package, check if that version satisfies
  // this request, too. If not, the scope is unsatisfiable; a caller with alternatives backtracks.
  spn_resolved_pkg_t* existing = sp_ht_getp(scope->named, name);
  if (existing) {
    if (version_in_range(existing->version, request->index.range)) {
      return SPN_OK;
    }

    record_failure(run, (spn_build_event_t) {
      .kind = SPN_EVENT_ERR_UNSATISFIABLE_VERSION,
      .unsatisfiable = {
        .low = *request,
        .high = *request
      }
    });
    return SPN_ERROR;
  }

  spn_index_pkg_t* pkg = spn_index_cache_get_package(resolver->index, spn_pkg_name_from_qualified(request->qualified));
  if (!pkg) {
    record_failure(run, (spn_build_event_t) {
      .kind = SPN_EVENT_ERR_UNKNOWN_PKG,
      .unknown.request = *request
    });
    return SPN_ERROR;
  }

  // By definition, if a name is pinned and a candidatae version doesn't satisfy
  // the pin, this branch of resolution fails.
  spn_semver_t pinned = sp_zero;
  bool contradiction = false;
  if (find_pin(scope, name, &pinned, &contradiction)) {
    if (contradiction || !version_in_range(pinned, request->index.range)) {
      record_failure(run, (spn_build_event_t) {
        .kind = SPN_EVENT_ERR_UNSATISFIABLE_VERSION,
        .unsatisfiable = {
          .low = *request,
          .high = *request
        }
      });
      return SPN_ERROR;
    }

    sp_da_for(pkg->releases, it) {
      if (spn_semver_eq(pkg->releases[it].version, pinned)) {
        if (!try_candidate(resolver, run, &pkg->releases[it])) {
          return SPN_OK;
        }
        break;
      }
    }

    record_failure(run, (spn_build_event_t) {
      .kind = SPN_EVENT_ERR_UNSATISFIABLE_VERSION,
      .unsatisfiable = {
        .low = *request,
        .high = *request
      }
    });
    return SPN_ERROR;
  }

  // If the name is totally free, just pick the newest legal version. This
  // is just a simple, greedy heuristic, not for correctness.
  spn_err_t result = SPN_ERROR;
  sp_da_rfor(pkg->releases, it) {
    spn_index_rel_t* release = &pkg->releases[it];
    if (!version_in_range(release->version, request->index.range)) {
      continue;
    }
    if (!try_candidate(resolver, run, release)) {
      result = SPN_OK;
      break;
    }
    if (run->fatal) {
      break;
    }
  }

  if (!result) {
    return SPN_OK;
  }

  record_failure(run, (spn_build_event_t) {
    .kind = SPN_EVENT_ERR_UNSATISFIABLE_VERSION,
    .unsatisfiable = {
      .low = *request,
      .high = *request
    }
  });
  return SPN_ERROR;
}

static spn_err_t resolve_dep(spn_resolver_t* resolver, spn_resolve_run_t* run, spn_resolved_pkg_t* node, spn_requested_pkg_t* dep) {
  spn_dep_edge_t edge = classify_dep(resolver, node, dep);
  switch (edge) {
    case SPN_DEP_EDGE_PRUNED: {
      return SPN_OK;
    }
    case SPN_DEP_EDGE_PROCESS:
    case SPN_DEP_EDGE_PRIVATE: {
      sp_da_push(run->boundaries, ((spn_scope_boundary_t) {
        .from = node->id,
        .edge = edge,
        .req = *dep,
      }));
      return SPN_OK;
    }
    case SPN_DEP_EDGE_SCOPE: {
      break;
    }
  }

  switch (dep->source) {
    case SPN_PKG_SOURCE_INDEX: return resolve_index_package(resolver, run, dep);
    case SPN_PKG_SOURCE_ROOT:
    case SPN_PKG_SOURCE_FILE:  return resolve_local_package(resolver, run, dep);
  }

  sp_unreachable_return(SPN_ERROR);
}

static spn_err_t resolve_deps(spn_resolver_t* resolver, spn_resolve_run_t* run, spn_resolved_pkg_t* node) {
  sp_ht_insert(run->visited, node->id.qualified, (u8)true);

  spn_err_t result = SPN_OK;
  sp_da_for(node->deps, it) {
    result = resolve_dep(resolver, run, node, &node->deps[it]);
    if (result) break;
  }

  sp_ht_erase(run->visited, node->id.qualified);
  return result;
}

static spn_err_t solve_reqs(spn_resolver_t* resolver, spn_resolve_run_t* run, u64 from, u64 to) {
  spn_resolved_pkg_t root = {
    .id = spn_pkg_id(resolver->intern, sp_str_lit("")),
    .qualified = sp_str_lit(""),
    .source = SPN_PKG_SOURCE_ROOT,
  };

  for (u64 it = from; it < to; it++) {
    spn_requested_pkg_t req = run->scopes[run->scope].reqs[it];
    spn_try(resolve_dep(resolver, run, &root, &req));
  }

  return SPN_OK;
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

static spn_err_t attempt_reqs(spn_resolver_t* resolver, spn_resolve_run_t* run, u64 from, u64 to, bool keep, spn_witness_t* witness) {
  spn_scope_t* scope = &run->scopes[run->scope];

  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  sp_da(sp_intern_id_t) snapshot = snapshot_named(scratch.mem, scope);
  u64 boundaries = sp_da_size(run->boundaries);

  spn_err_t result = solve_reqs(resolver, run, from, to);
  if (!result && witness) {
    capture_witness(resolver->mem, run, scope, snapshot, boundaries, witness);
  }
  if (result || !keep) {
    restore_named(scratch.mem, scope, snapshot);
    sp_da_head(run->boundaries)->size = boundaries;
  }

  sp_mem_end_scratch(scratch);
  return result;
}

static spn_err_t resolve_scope(spn_resolver_t* resolver, spn_resolve_run_t* run) {
  spn_scope_t* scope = &run->scopes[run->scope];
  u64 from = scope->cursor;
  u64 to = sp_da_size(scope->reqs);
  scope->cursor = to;
  run->budget = resolver->budget;

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
      .version = picks[it]->version,
    }));
  }

  // Try to solve, given what we have pinned. If it succeeds, we're done.
  sp_da_for(pins, it) {
    sp_da_push(scope->pins, pins[it]);
  }
  if (!attempt_reqs(resolver, run, from, to, true, SP_NULLPTR)) {
    sp_mem_end_scratch(scratch);
    return SPN_OK;
  }
  sp_da_clear(scope->pins);

  if (run->fatal) {
    sp_mem_end_scratch(scratch);
    return SPN_ERROR;
  }

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
    run->failed = false;
    sp_da_push(scope->pins, pins[it]);
    if (attempt_reqs(resolver, run, from, to, false, &witness)) {
      if (run->fatal) {
        sp_mem_end_scratch(scratch);
        return SPN_ERROR;
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
    run->failed = false;
    return SPN_OK;
  }

  sp_mem_end_scratch(scratch);
  run->failed = false;
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

    spn_requested_pkg_t req = boundary.req;
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

static spn_resolved_pkg_t* find_dep_instance(spn_resolver_t* resolver, spn_resolve_run_t* run, spn_scope_t* scope, spn_resolved_pkg_t* node, spn_requested_pkg_t* dep, spn_dep_edge_t edge) {
  sp_intern_id_t name = sp_intern_get_or_insert(resolver->intern, dep->qualified);

  switch (edge) {
    case SPN_DEP_EDGE_SCOPE: {
      return sp_ht_getp(scope->named, name);
    }
    case SPN_DEP_EDGE_PROCESS:
    case SPN_DEP_EDGE_PRIVATE: {
      spn_scope_t* target = find_scope(run, edge == SPN_DEP_EDGE_PRIVATE ? node->id.qualified : name, node->id);
      return target ? sp_ht_getp(target->named, name) : SP_NULLPTR;
    }
    case SPN_DEP_EDGE_PRUNED: {
      return SP_NULLPTR;
    }
  }

  sp_unreachable_return(SP_NULLPTR);
}

static void materialize_edges(spn_resolver_t* resolver, spn_resolve_run_t* run) {
  sp_ht_for_kv(run->query->result, it) {
    sp_da_init(resolver->mem, it.val->edges);
  }

  sp_da_for(run->scopes, s) {
    spn_scope_t* scope = &run->scopes[s];
    sp_ht_for_kv(scope->named, it) {
      spn_resolved_pkg_t* node = it.val;
      spn_resolved_pkg_t* instance = sp_ht_getp(run->query->result, node->id);

      sp_da_for(node->deps, jt) {
        spn_requested_pkg_t* dep = &node->deps[jt];
        spn_dep_edge_t edge = classify_dep(resolver, node, dep);
        if (edge == SPN_DEP_EDGE_PRUNED) {
          continue;
        }

        spn_resolved_pkg_t* target = find_dep_instance(resolver, run, scope, node, dep, edge);
        if (!target) {
          continue;
        }

        bool present = false;
        sp_da_for(instance->edges, kt) {
          if (pkg_id_eq(instance->edges[kt].id, target->id) && instance->edges[kt].kind == dep->kind) {
            present = true;
            break;
          }
        }
        if (!present) {
          sp_da_push(instance->edges, ((spn_resolved_dep_t) {
            .id = target->id,
            .kind = dep->kind,
            .edge = edge,
            .private = dep->private,
          }));
        }
      }
    }
  }
}

static spn_err_t check_group_cycle(spn_resolver_t* resolver, spn_resolve_run_t* run, spn_group_states_t states, u32 scope_index, sp_intern_id_t name) {
  spn_group_node_t key = { .scope = scope_index, .name = name };
  u8* state = sp_ht_getp(states, key);
  if (state && *state == 2) {
    return SPN_OK;
  }

  spn_scope_t* scope = &run->scopes[scope_index];
  spn_resolved_pkg_t* node = sp_ht_getp(scope->named, name);
  if (!node) {
    return SPN_OK;
  }

  if (state && *state == 1) {
    spn_event_buffer_push(resolver->events, (spn_build_event_t) {
      .kind = SPN_EVENT_ERR_UNIT_CYCLE,
      .unit_cycle = {
        .id = spn_pkg_name_from_qualified(node->qualified),
        .version = node->version,
      }
    });
    return SPN_ERROR;
  }

  sp_ht_insert(states, key, (u8)1);

  sp_da_for(node->deps, it) {
    spn_requested_pkg_t* dep = &node->deps[it];
    spn_dep_edge_t edge = classify_dep(resolver, node, dep);
    if (edge == SPN_DEP_EDGE_PRUNED) {
      continue;
    }

    sp_intern_id_t dep_name = sp_intern_get_or_insert(resolver->intern, dep->qualified);
    u32 target = scope_index;
    if (edge != SPN_DEP_EDGE_SCOPE) {
      spn_scope_t* found = find_scope(run, edge == SPN_DEP_EDGE_PRIVATE ? node->id.qualified : dep_name, node->id);
      if (!found) {
        continue;
      }
      target = (u32)(found - run->scopes);
    }

    spn_try(check_group_cycle(resolver, run, states, target, dep_name));
  }

  *sp_ht_getp(states, key) = 2;
  return SPN_OK;
}

static spn_err_t check_group_cycles(spn_resolver_t* resolver, spn_resolve_run_t* run) {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();

  spn_group_states_t states = SP_NULLPTR;
  sp_ht_init(scratch.mem, states);

  spn_err_t result = SPN_OK;
  sp_da_for(run->scopes, it) {
    spn_scope_t* scope = &run->scopes[it];
    sp_ht_for_kv(scope->named, jt) {
      result = check_group_cycle(resolver, run, states, (u32)it, *jt.key);
      if (result) break;
    }
    if (result) break;
  }

  sp_mem_end_scratch(scratch);
  return result;
}

static sp_hash_t leaf_hash(spn_resolved_pkg_t* instance) {
  sp_hash_t hash = sp_hash_str(instance->qualified);
  return sp_hash_bytes(&instance->version, sizeof(spn_semver_t), hash);
}

static s32 sort_edge_record(const void* a, const void* b) {
  const spn_edge_record_t* lhs = (const spn_edge_record_t*)a;
  const spn_edge_record_t* rhs = (const spn_edge_record_t*)b;
  if (lhs->hash != rhs->hash) return lhs->hash < rhs->hash ? -1 : 1;
  if (lhs->kind != rhs->kind) return lhs->kind < rhs->kind ? -1 : 1;
  if (lhs->private != rhs->private) return lhs->private < rhs->private ? -1 : 1;
  return 0;
}

static sp_hash_t hash_instance(spn_resolve_run_t* run, spn_hash_memo_t memo, spn_resolved_pkg_t* instance) {
  spn_hash_state_t* state = sp_ht_getp(memo, instance->id);
  if (state && state->state == 2) {
    return state->hash;
  }
  if (state && state->state == 1) {
    return leaf_hash(instance);
  }

  sp_ht_insert(memo, instance->id, ((spn_hash_state_t) { .state = 1 }));

  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  sp_da(spn_edge_record_t) records = sp_da_new(scratch.mem, spn_edge_record_t);
  sp_da_for(instance->edges, it) {
    spn_resolved_pkg_t* child = sp_ht_getp(run->query->result, instance->edges[it].id);
    sp_assert(child);
    sp_da_push(records, ((spn_edge_record_t) {
      .hash = hash_instance(run, memo, child),
      .kind = (u32)instance->edges[it].kind,
      .private = (u32)instance->edges[it].private,
    }));
  }
  sp_da_sort(records, sort_edge_record);

  sp_hash_t hash = leaf_hash(instance);
  if (!sp_da_empty(records)) {
    hash = sp_hash_bytes(records, sp_da_size(records) * sizeof(spn_edge_record_t), hash);
  }
  sp_mem_end_scratch(scratch);

  spn_hash_state_t* slot = sp_ht_getp(memo, instance->id);
  slot->state = 2;
  slot->hash = hash;
  return hash;
}

static void assign_identity(spn_resolver_t* resolver, spn_resolve_run_t* run) {
  spn_hash_memo_t memo = SP_NULLPTR;
  sp_ht_init(resolver->mem, memo);

  sp_ht_for_kv(run->query->result, it) {
    hash_instance(run, memo, it.val);
  }

  spn_resolve_t rekeyed = SP_NULLPTR;
  sp_ht_init(resolver->mem, rekeyed);
  sp_ht_for_kv(run->query->result, it) {
    spn_resolved_pkg_t instance = *it.val;
    instance.id.hash = sp_ht_getp(memo, instance.id)->hash;
    sp_da_for(instance.edges, jt) {
      spn_hash_state_t* child = sp_ht_getp(memo, instance.edges[jt].id);
      sp_assert(child);
      instance.edges[jt].id.hash = child->hash;
    }
    sp_ht_insert(rekeyed, instance.id, instance);
  }

  run->query->result = rekeyed;
}

static spn_err_t check_dynamic_duplicates(spn_resolver_t* resolver, spn_resolve_run_t* run) {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  spn_err_t result = SPN_OK;

  sp_ht(spn_pkg_id_t, sp_hash_t) identities = SP_NULLPTR;
  sp_ht_init(scratch.mem, identities);
  sp_ht_for_kv(run->query->result, it) {
    spn_pkg_id_t key = it.val->id;
    key.hash = 0;
    sp_ht_insert(identities, key, it.val->id.hash);
  }

  sp_da_for(run->scopes, process) {
    sp_ht(sp_intern_id_t, spn_resolved_pkg_t*) shared = SP_NULLPTR;
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

        spn_resolved_pkg_t** prior = sp_ht_getp(shared, node->id.qualified);
        if (!prior) {
          sp_ht_insert(shared, node->id.qualified, node);
          continue;
        }

        sp_hash_t* prior_identity = sp_ht_getp(identities, (*prior)->id);
        sp_hash_t* node_identity = sp_ht_getp(identities, node->id);
        sp_assert(prior_identity && node_identity);
        if (*prior_identity == *node_identity) {
          continue;
        }

        bool ordered = spn_semver_le((*prior)->version, node->version);
        spn_event_buffer_push(resolver->events, (spn_build_event_t) {
          .kind = SPN_EVENT_ERR_DYNAMIC_DUPLICATE,
          .dynamic_dup = {
            .id = spn_pkg_name_from_qualified(node->qualified),
            .low = ordered ? (*prior)->version : node->version,
            .high = ordered ? node->version : (*prior)->version,
          }
        });
        result = SPN_ERROR;
        goto done;
      }
    }
  }

done:
  sp_mem_end_scratch(scratch);
  return result;
}

spn_err_t spn_resolve_from_lock_file(spn_resolver_t* resolver, spn_lock_file_t* lock) {
  return SPN_OK;
}

spn_err_t spn_resolve_from_solver(spn_resolver_t* resolver, spn_resolve_query_t* query) {
  sp_tm_timer_t timer = sp_tm_start_timer();

  spn_resolve_run_t run = {
    .query = query,
  };
  sp_da_init(resolver->mem, run.scopes);
  sp_da_init(resolver->mem, run.boundaries);
  sp_da_init(resolver->mem, run.edges);
  sp_ht_init(resolver->mem, run.visited);

  u32 root = find_or_create_scope(resolver, &run, 0, sp_zero_s(spn_pkg_id_t));
  sp_da_for(query->reqs, it) {
    sp_da_push(run.scopes[root].reqs, query->reqs[it]);
  }

  bool progress = true;
  while (progress) {
    progress = false;

    sp_da_for(run.scopes, it) {
      spn_scope_t* scope = &run.scopes[it];
      if (scope->cursor == sp_da_size(scope->reqs)) {
        continue;
      }

      progress = true;
      run.scope = (u32)it;

      if (resolve_scope(resolver, &run)) {
        if (run.failed) {
          spn_event_buffer_push(resolver->events, run.failure);
        }
        query->time = sp_tm_read_timer(&timer);
        return SPN_ERROR;
      }

      commit_scope(resolver, &run);
      process_boundaries(resolver, &run);
      run.failed = false;
    }
  }

  compute_processes(&run);
  materialize_edges(resolver, &run);
  spn_err_t err = check_group_cycles(resolver, &run);
  if (!err) {
    assign_identity(resolver, &run);
    err = check_dynamic_duplicates(resolver, &run);
  }

  query->time = sp_tm_read_timer(&timer);
  return err;
}
