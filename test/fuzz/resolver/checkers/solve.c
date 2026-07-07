#include "fuzz.h"

#include "pkg/id.h"
#include "semver/compare.h"

s32 fz_pkg_from_qualified(fz_universe_t* u, sp_str_t qualified) {
  sp_da_for(u->pkgs, it) {
    sp_str_t expected = spn_pkg_canonicalize_pair(sp_str_lit("spn"), sp_str_view(fz_names[it]));
    if (sp_str_equal(qualified, expected)) {
      return (s32)it;
    }
  }
  return -1;
}

fz_err_t fz_check_solve(fz_universe_t* u, spn_resolve_query_t* query) {
  s64 picks[FZ_MAX_PKGS];
  sp_carr_for(picks, it) {
    picks[it] = -1;
  }

  sp_ht_for_kv(query->result, it) {
    spn_resolved_pkg_t* node = it.val;
    if (sp_str_equal(node->qualified, sp_str_view(fz_root_qualified))) {
      continue;
    }

    s32 pkg = fz_pkg_from_qualified(u, node->qualified);
    if (pkg < 0) {
      return FZ_ERR_SOLVE_FOREIGN_PKG;
    }
    if (picks[pkg] >= 0) {
      return FZ_ERR_SOLVE_DUPLICATE_INSTANCE;
    }

    sp_da_for(u->pkgs[pkg].releases, rt) {
      if (spn_semver_eq(u->pkgs[pkg].releases[rt].version, node->version)) {
        picks[pkg] = (s64)rt;
        break;
      }
    }
    if (picks[pkg] < 0) {
      return FZ_ERR_SOLVE_FOREIGN_VERSION;
    }
  }

  sp_da_for(u->roots, it) {
    fz_dep_t dep = u->roots[it];
    if (picks[dep.pkg] < 0) {
      return FZ_ERR_ROOT_UNRESOLVED;
    }
    if (!fz_range_sat(dep, u->pkgs[dep.pkg].releases[picks[dep.pkg]].version)) {
      return FZ_ERR_ROOT_OUT_OF_RANGE;
    }
  }

  sp_da_for(u->pkgs, it) {
    if (picks[it] < 0) continue;
    fz_release_t* release = &u->pkgs[it].releases[picks[it]];
    sp_da_for(release->deps, dt) {
      fz_dep_t dep = release->deps[dt];
      if (picks[dep.pkg] < 0) {
        return FZ_ERR_DEP_UNRESOLVED;
      }
      if (!fz_range_sat(dep, u->pkgs[dep.pkg].releases[picks[dep.pkg]].version)) {
        return FZ_ERR_DEP_OUT_OF_RANGE;
      }
    }
  }

  return FZ_OK;
}

static s32 fz_sort_pick(const void* a, const void* b) {
  const fz_pick_t* lhs = (const fz_pick_t*)a;
  const fz_pick_t* rhs = (const fz_pick_t*)b;
  if (lhs->pkg != rhs->pkg) return lhs->pkg < rhs->pkg ? -1 : 1;
  s32 order = spn_semver_cmp(lhs->version, rhs->version);
  if (order) return order;
  if (lhs->hash != rhs->hash) return lhs->hash < rhs->hash ? -1 : 1;
  return 0;
}

fz_solution_t fz_solution(sp_mem_t mem, fz_universe_t* u, spn_resolve_query_t* query) {
  fz_solution_t solution = sp_da_new(mem, fz_pick_t);
  sp_ht_for_kv(query->result, it) {
    spn_resolved_pkg_t* node = it.val;
    if (sp_str_equal(node->qualified, sp_str_view(fz_root_qualified))) {
      continue;
    }
    sp_da_push(solution, ((fz_pick_t) {
      .pkg = fz_pkg_from_qualified(u, node->qualified),
      .version = node->version,
      .hash = node->id.hash,
    }));
  }
  sp_da_sort(solution, fz_sort_pick);
  return solution;
}

bool fz_solution_equal(fz_solution_t a, fz_solution_t b) {
  if (sp_da_size(a) != sp_da_size(b)) {
    return false;
  }
  sp_da_for(a, it) {
    if (a[it].pkg != b[it].pkg) return false;
    if (!spn_semver_eq(a[it].version, b[it].version)) return false;
    if (a[it].hash != b[it].hash) return false;
  }
  return true;
}
