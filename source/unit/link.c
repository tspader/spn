#include "unit/unit.h"

#include "forward/types.h"
#include "sp/sp_om.h"
#include "unit/types.h"
#include "target/types.h"

typedef sp_om(spn_pkg_unit_t*, u8) pkg_set_t;
typedef sp_om(spn_target_unit_t*, u8) target_set_t;

typedef struct {
  pkg_set_t visited;
  sp_da(spn_closure_entry_t) closure;
} search_t;

typedef struct {
  target_set_t visited;
  sp_da(spn_target_unit_t*) order;
} sibling_search_t;

static bool pkg_is_shared_boundary(spn_pkg_unit_t* pkg) {
  sp_da_for(pkg->libs, it) {
    if (pkg->libs[it]->lib_kind == SPN_LIB_KIND_SHARED) return true;
  }
  return false;
}

static bool pkg_links_code(spn_pkg_unit_t* pkg) {
  sp_da_for(pkg->libs, it) {
    spn_target_unit_t* lib = pkg->libs[it];
    if (lib->info->no_link) continue;
    if (lib->lib_kind == SPN_LIB_KIND_SHARED) continue;
    return true;
  }
  return sp_da_empty(pkg->libs);
}

bool spn_dep_kind_applies(spn_dep_kind_t dep, spn_target_kind_t target) {
  bool metaprogram = target == SPN_TARGET_CONFIGURE_METAPROGRAM || target == SPN_TARGET_BUILD_METAPROGRAM;
  switch (dep) {
    case SPN_DEP_KIND_PACKAGE: return !metaprogram;
    case SPN_DEP_KIND_TEST:    return target == SPN_TARGET_TEST;
    case SPN_DEP_KIND_BUILD:   return metaprogram;
  }
  sp_unreachable_return(false);
}

static bool edge_links(spn_pkg_dep_t* dep, bool tests, bool builds) {
  switch (dep->kind) {
    case SPN_DEP_KIND_PACKAGE: return !builds;
    case SPN_DEP_KIND_TEST:    return tests;
    case SPN_DEP_KIND_BUILD:   return builds;
  }
  sp_unreachable_return(false);
}

#define CLOSURE_SEARCH_PRIVATE true
#define CLOSURE_SEARCH_PUBLIC false

#define CLOSURE_SEARCH_TESTS true
#define CLOSURE_DO_NOT_SEARCH_TESTS false

#define CLOSURE_SEARCH_BUILDS true
#define CLOSURE_DO_NOT_SEARCH_BUILDS false

static void collect(search_t* s, spn_pkg_unit_t* pkg, bool private, bool tests, bool builds) {
  sp_da(spn_pkg_dep_t) deps = pkg->deps;
  sp_da_for(deps, it) {
    spn_pkg_dep_t* dep = &deps[it];
    if (!edge_links(dep, tests, builds)) {
      continue;
    }
    if (sp_om_has(s->visited, dep->unit)) {
      continue;
    }
    sp_om_insert(s->visited, dep->unit, (u8)true);

    // A shared lib is its own link unit; it already resolved its dependencies,
    // so the consumer stops here instead of inheriting its private closure.
    if (!pkg_is_shared_boundary(dep->unit)) {

      collect(s, dep->unit, private || dep->private, CLOSURE_DO_NOT_SEARCH_TESTS, CLOSURE_DO_NOT_SEARCH_BUILDS);
    }

    sp_da_push(s->closure, ((spn_closure_entry_t) {
      .pkg = dep->unit,
      .private = private || dep->private,
    }));
  }
}

static bool sibling_code_links(spn_target_unit_t* dep) {
  if (dep->info->no_link) {
    return false;
  }
  switch (dep->lib_kind) {
    case SPN_LIB_KIND_STATIC:
    case SPN_LIB_KIND_SOURCE:
    case SPN_LIB_KIND_OBJECT: {
      return true;
    }
    case SPN_LIB_KIND_SHARED:
    case SPN_LIB_KIND_NONE: {
      return false;
    }
  }
  sp_unreachable_return(false);
}

static void collect_siblings(sibling_search_t* s, spn_target_unit_t* target) {
  sp_da_for(target->deps, it) {
    spn_target_unit_t* dep = target->deps[it];
    if (sp_om_has(s->visited, dep)) {
      continue;
    }
    sp_om_insert(s->visited, dep, (u8)true);

    if (sibling_code_links(dep)) {
      collect_siblings(s, dep);
    }

    sp_da_push(s->order, dep);
  }
}

static sp_da(spn_target_unit_t*) sibling_link_targets(sp_mem_t mem, spn_target_unit_t* root) {
  sp_mem_arena_marker_t s = sp_mem_begin_scratch_for(mem);

  sibling_search_t search = {
    .order = sp_da_new(s.mem, spn_target_unit_t*),
  };
  sp_om_insert(search.visited, root, (u8)true);
  collect_siblings(&search, root);

  sp_da(spn_target_unit_t*) targets = sp_da_new(mem, spn_target_unit_t*);
  sp_da_rfor(search.order, it) {
    sp_da_push(targets, search.order[it]);
  }
  sp_om_free(search.visited);
  sp_mem_end_scratch(s);
  return targets;
}

sp_da(spn_closure_entry_t) spn_target_link_closure(sp_mem_t mem, spn_target_unit_t* root) {
  sp_da(spn_closure_entry_t) closure = sp_da_new(mem, spn_closure_entry_t);
  sp_da_push(closure, ((spn_closure_entry_t) {
    .pkg = root->pkg,
    .targets = sibling_link_targets(mem, root),
    .links_code = true,
  }));

  if (root->info->kind == SPN_TARGET_CONFIGURE_METAPROGRAM) {
    return closure;
  }

  sp_mem_arena_marker_t s = sp_mem_begin_scratch_for(mem);

  search_t search = {
    .closure = sp_da_new(s.mem, spn_closure_entry_t),
  };
  sp_om_insert(search.visited, root->pkg, (u8)true);
  collect(&search, root->pkg, CLOSURE_SEARCH_PUBLIC, root->info->kind == SPN_TARGET_TEST, root->info->kind == SPN_TARGET_BUILD_METAPROGRAM);

  // The result is in post-order. Reversing us gives us a topological sort.
  sp_da_rfor(search.closure, it) {
    sp_assert(search.closure[it].pkg);
    sp_da_push(closure, ((spn_closure_entry_t) {
      .pkg = search.closure[it].pkg,
      .targets = search.closure[it].pkg->libs,
      .private = search.closure[it].private,
      .links_code = pkg_links_code(search.closure[it].pkg),
    }));
  }
  sp_om_free(search.visited);
  sp_mem_end_scratch(s);
  return closure;
}

static void collect_runtime(search_t* s, spn_pkg_unit_t* pkg, bool tests, bool builds) {
  sp_da(spn_pkg_dep_t) deps = pkg->deps;
  sp_da_for(deps, it) {
    spn_pkg_dep_t* dep = &deps[it];
    if (!edge_links(dep, tests, builds)) {
      continue;
    }
    if (sp_om_has(s->visited, dep->unit)) {
      continue;
    }
    sp_om_insert(s->visited, dep->unit, (u8)true);

    collect_runtime(s, dep->unit, CLOSURE_DO_NOT_SEARCH_TESTS, CLOSURE_DO_NOT_SEARCH_BUILDS);

    sp_da_push(s->closure, ((spn_closure_entry_t) {
      .pkg = dep->unit,
    }));
  }
}

static void push_unique_runtime_lib(target_set_t* unique, sp_da(spn_target_unit_t*)* libs, spn_target_unit_t* lib) {
  if (lib->info->no_link) return;
  if (lib->lib_kind != SPN_LIB_KIND_SHARED) return;
  if (sp_om_has(*unique, lib)) return;
  sp_om_insert(*unique, lib, (u8)true);
  sp_da_push(*libs, lib);
}

static void collect_runtime_siblings(target_set_t* seen, target_set_t* unique, sp_da(spn_target_unit_t*)* libs, spn_target_unit_t* target) {
  sp_da_for(target->deps, it) {
    spn_target_unit_t* dep = target->deps[it];
    if (sp_om_has(*seen, dep)) continue;
    sp_om_insert(*seen, dep, (u8)true);

    push_unique_runtime_lib(unique, libs, dep);
    collect_runtime_siblings(seen, unique, libs, dep);
  }
}

sp_da(spn_target_unit_t*) spn_target_runtime_libs(sp_mem_t mem, spn_target_unit_t* root) {
  sp_mem_arena_marker_t s = sp_mem_begin_scratch_for(mem);

  search_t search = {
    .closure = sp_da_new(s.mem, spn_closure_entry_t),
  };
  sp_om_insert(search.visited, root->pkg, (u8)true);
  collect_runtime(&search, root->pkg, root->info->kind == SPN_TARGET_TEST, root->info->kind == SPN_TARGET_BUILD_METAPROGRAM);

  sp_da(spn_target_unit_t*) libs = sp_da_new(mem, spn_target_unit_t*);
  target_set_t seen = SP_NULLPTR;
  target_set_t unique = SP_NULLPTR;
  collect_runtime_siblings(&seen, &unique, &libs, root);
  sp_da_for(search.closure, it) {
    sp_da_for(search.closure[it].pkg->libs, lt) {
      push_unique_runtime_lib(&unique, &libs, search.closure[it].pkg->libs[lt]);
    }
  }
  sp_om_free(seen);
  sp_om_free(unique);
  sp_om_free(search.visited);
  sp_mem_end_scratch(s);
  return libs;
}

sp_da(spn_link_lib_t) spn_closure_link_libs(sp_mem_t mem, sp_da(spn_closure_entry_t) closure) {
  sp_da(spn_link_lib_t) libs = sp_da_new(mem, spn_link_lib_t);

  sp_da_for(closure, it) {
    spn_closure_entry_t* entry = &closure[it];
    sp_da_for(entry->targets, lt) {
      spn_target_unit_t* lib = entry->targets[lt];
      if (lib->info->no_link) {
        continue;
      }

      switch (lib->lib_kind) {
        case SPN_LIB_KIND_STATIC:
        case SPN_LIB_KIND_SHARED: {
          sp_da_push(libs, ((spn_link_lib_t) {
            .pkg = entry->pkg,
            .lib = lib,
            .private = entry->private,
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
