#include "target/closure.h"

#include "session/session.h"
#include "unit/types.h"

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

static void collect(spn_session_t* session, spn_pkg_unit_t* pkg, bool private, bool tests, sp_da(spn_pkg_unit_t*)* visited, sp_da(spn_closure_entry_t)* post) {
  sp_da(spn_pkg_dep_t) deps = spn_session_pkg_deps(session, pkg);
  sp_da_for(deps, it) {
    spn_pkg_dep_t* dep = &deps[it];
    if (!edge_links(dep, tests)) {
      continue;
    }
    if (!dep->unit || dep->unit == pkg) {
      continue;
    }
    if (closure_has_pkg(*visited, dep->unit)) {
      continue;
    }
    sp_da_push(*visited, dep->unit);

    // A shared lib is its own link unit; it already resolved its dependencies,
    // so the consumer stops here instead of inheriting its private closure.
    if (!pkg_is_shared_boundary(dep->unit)) {
      collect(session, dep->unit, private || dep->private, false, visited, post);
    }

    sp_da_push(*post, ((spn_closure_entry_t) {
      .pkg = dep->unit,
      .private = private || dep->private,
    }));
  }
}

sp_da(spn_closure_entry_t) spn_target_link_closure(sp_mem_t mem, spn_target_unit_t* root) {
  sp_da(spn_pkg_unit_t*) visited = sp_da_new(mem, spn_pkg_unit_t*);
  sp_da(spn_closure_entry_t) post = sp_da_new(mem, spn_closure_entry_t);
  collect(root->session, root->pkg, false, root->info->kind == SPN_TARGET_TEST, &visited, &post);

  // Reverse post-order is a topological sort: every depender lands before its
  // dependees, so the linker resolves symbols left-to-right through the chain.
  sp_da(spn_closure_entry_t) out = sp_da_new(mem, spn_closure_entry_t);
  u32 n = sp_da_size(post);
  for (u32 i = 0; i < n; i++) {
    sp_da_push(out, post[n - 1 - i]);
  }
  return out;
}
