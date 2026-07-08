#include "target/closure.h"

#include "forward/types.h"
#include "session/session.h"
#include "target/types.h"

typedef struct {
  spn_session_t* session;
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
  sp_da(spn_pkg_dep_t) deps = spn_session_pkg_deps(s->session, pkg);
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
  sp_mem_arena_marker_t s = sp_mem_begin_scratch_for(mem);

  search_t search = {
    .session = root->session,
    .visited = sp_da_new(s.mem, spn_pkg_unit_t*),
    .closure = sp_da_new(s.mem, spn_closure_entry_t),
  };
  collect(&search, root->pkg, CLOSURE_SEARCH_PUBLIC, root->info->kind == SPN_TARGET_TEST);

  // The result is in post-order. Reversing us gives us a topological sort.
  sp_da(spn_closure_entry_t) closure = sp_da_new(mem, spn_closure_entry_t);
  sp_da_rfor(search.closure, it) {
    sp_da_push(closure, search.closure[it]);
  }
  return closure;
}
