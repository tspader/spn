#include "fuzz.h"

#include "pkg/id.h"
#include "semver/compare.h"

static s32 fz_instance_pkg(fz_universe_t* u, sp_str_t qualified) {
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

    s32 pkg = fz_instance_pkg(u, node->qualified);
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
