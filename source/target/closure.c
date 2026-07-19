#include "target/closure.h"

#include "forward/types.h"
#include "unit/types.h"
#include "target/types.h"

typedef struct {
  sp_da(spn_pkg_unit_t*) visited;
  sp_da(spn_closure_entry_t) closure;
} search_t;

static bool closure_has_pkg(sp_da(spn_pkg_unit_t*) seen, spn_pkg_unit_t* pkg) {
  sp_da_for(seen, it) {
    if (seen[it] == pkg) return true;
  }
  return false;
}

static bool pkg_is_shared_boundary(spn_pkg_unit_t* pkg) {
  sp_da_for(pkg->libs, it) {
    if (pkg->libs[it]->lib_kind == SPN_LIB_KIND_SHARED) return true;
  }
  return false;
}

static bool edge_links(spn_pkg_dep_t* dep, bool tests) {
  switch (dep->kind) {
    case SPN_DEP_KIND_PACKAGE: return true;
    case SPN_DEP_KIND_TEST:    return tests;
    case SPN_DEP_KIND_BUILD:   return false;
  }
  sp_unreachable_return(false);
}

#define CLOSURE_SEARCH_PRIVATE true
#define CLOSURE_SEARCH_PUBLIC false

#define CLOSURE_SEARCH_TESTS true
#define CLOSURE_DO_NOT_SEARCH_TESTS false

static void collect(search_t* s, spn_pkg_unit_t* pkg, bool private, bool tests) {
  sp_da(spn_pkg_dep_t) deps = pkg->deps;
  sp_da_for(deps, it) {
    spn_pkg_dep_t* dep = &deps[it];
    if (!edge_links(dep, tests)) {
      continue;
    }
    if (!dep->unit || dep->unit == pkg) {
      continue;
    }
    if (closure_has_pkg(s->visited, dep->unit)) {
      continue;
    }
    sp_da_push(s->visited, dep->unit);

    // A shared lib is its own link unit; it already resolved its dependencies,
    // so the consumer stops here instead of inheriting its private closure.
    if (!pkg_is_shared_boundary(dep->unit)) {

      collect(s, dep->unit, private || dep->private, CLOSURE_DO_NOT_SEARCH_TESTS);
    }

    sp_da_push(s->closure, ((spn_closure_entry_t) {
      .pkg = dep->unit,
      .private = private || dep->private,
    }));
  }
}

sp_da(spn_closure_entry_t) spn_target_link_closure(sp_mem_t mem, spn_target_unit_t* root) {
  if (root->info->kind == SPN_TARGET_CONFIGURE_METAPROGRAM) {
    return sp_da_new(mem, spn_closure_entry_t);
  }

  sp_mem_arena_marker_t s = sp_mem_begin_scratch_for(mem);

  search_t search = {
    .visited = sp_da_new(s.mem, spn_pkg_unit_t*),
    .closure = sp_da_new(s.mem, spn_closure_entry_t),
  };
  collect(&search, root->pkg, CLOSURE_SEARCH_PUBLIC, root->info->kind == SPN_TARGET_TEST);

  // The result is in post-order. Reversing us gives us a topological sort.
  sp_da(spn_closure_entry_t) closure = sp_da_new(mem, spn_closure_entry_t);
  sp_da_rfor(search.closure, it) {
    sp_da_push(closure, search.closure[it]);
  }
  sp_mem_end_scratch(s);
  return closure;
}

static void collect_runtime(search_t* s, spn_pkg_unit_t* pkg, bool tests) {
  sp_da(spn_pkg_dep_t) deps = pkg->deps;
  sp_da_for(deps, it) {
    spn_pkg_dep_t* dep = &deps[it];
    if (!edge_links(dep, tests)) {
      continue;
    }
    if (!dep->unit || dep->unit == pkg) {
      continue;
    }
    if (closure_has_pkg(s->visited, dep->unit)) {
      continue;
    }
    sp_da_push(s->visited, dep->unit);

    collect_runtime(s, dep->unit, CLOSURE_DO_NOT_SEARCH_TESTS);

    sp_da_push(s->closure, ((spn_closure_entry_t) {
      .pkg = dep->unit,
    }));
  }
}

static void push_unique_runtime_lib(sp_da(spn_target_unit_t*)* libs, spn_target_unit_t* lib) {
  if (lib->info->no_link) return;
  if (lib->lib_kind != SPN_LIB_KIND_SHARED) return;
  sp_da_for(*libs, it) {
    if ((*libs)[it] == lib) return;
  }
  sp_da_push(*libs, lib);
}

static bool runtime_target_seen(sp_da(spn_target_unit_t*) seen, spn_target_unit_t* target) {
  sp_da_for(seen, it) {
    if (seen[it] == target) return true;
  }
  return false;
}

static void collect_runtime_siblings(sp_da(spn_target_unit_t*)* seen, sp_da(spn_target_unit_t*)* libs, spn_target_unit_t* target) {
  sp_da_for(target->deps.target, it) {
    spn_target_unit_t* dep = target->deps.target[it];
    if (runtime_target_seen(*seen, dep)) continue;
    sp_da_push(*seen, dep);

    push_unique_runtime_lib(libs, dep);
    collect_runtime_siblings(seen, libs, dep);
  }
}

sp_da(spn_target_unit_t*) spn_target_runtime_libs(sp_mem_t mem, spn_target_unit_t* root) {
  sp_mem_arena_marker_t s = sp_mem_begin_scratch_for(mem);

  search_t search = {
    .visited = sp_da_new(s.mem, spn_pkg_unit_t*),
    .closure = sp_da_new(s.mem, spn_closure_entry_t),
  };
  collect_runtime(&search, root->pkg, root->info->kind == SPN_TARGET_TEST);

  sp_da(spn_target_unit_t*) libs = sp_da_new(mem, spn_target_unit_t*);
  sp_da(spn_target_unit_t*) seen = sp_da_new(s.mem, spn_target_unit_t*);
  collect_runtime_siblings(&seen, &libs, root);
  sp_da_for(search.closure, it) {
    spn_pkg_unit_t* pkg = search.closure[it].pkg;
    if (!pkg || pkg == root->pkg) {
      continue;
    }
    sp_da_for(pkg->libs, lt) {
      push_unique_runtime_lib(&libs, pkg->libs[lt]);
    }
  }
  sp_mem_end_scratch(s);
  return libs;
}

sp_da(spn_link_lib_t) spn_closure_link_libs(sp_mem_t mem, sp_da(spn_closure_entry_t) closure, spn_pkg_unit_t* self) {
  sp_da(spn_link_lib_t) libs = sp_da_new(mem, spn_link_lib_t);

  sp_da_for(closure, it) {
    spn_pkg_unit_t* pkg = closure[it].pkg;
    if (!pkg || pkg == self) {
      continue;
    }

    sp_da_for(pkg->libs, lt) {
      spn_target_unit_t* lib = pkg->libs[lt];
      if (lib->info->no_link) {
        continue;
      }

      switch (lib->lib_kind) {
        case SPN_LIB_KIND_STATIC:
        case SPN_LIB_KIND_SHARED: {
          sp_da_push(libs, ((spn_link_lib_t) {
            .pkg = pkg,
            .lib = lib,
            .private = closure[it].private,
          }));
          break;
        }
        case SPN_LIB_KIND_SOURCE:
        case SPN_LIB_KIND_OBJECT:
        case SPN_LIB_KIND_NONE: {
          break;
        }
      }
    }
  }

  return libs;
}
