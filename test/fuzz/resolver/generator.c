#include "fuzz.h"

#include "semver/compare.h"
#include "semver/convert.h"

const c8* fz_names[FZ_MAX_PKGS] = { "a", "b", "c", "d", "e", "f" };

// A universe is a self-contained resolution problem over a tiny version
// lattice: an index of packages whose deps form a DAG over pkg indices, plus
// the root manifest's requests. Pure scope edges only, so the whole problem
// is one scope and the exhaustive oracle is exact.
bool fz_range_sat(fz_dep_t dep, spn_semver_t version) {
  spn_semver_t low = dep.version;
  switch (dep.shape) {
    case FZ_RANGE_EXACT: {
      return spn_semver_cmp(version, low) == 0;
    }
    case FZ_RANGE_CARET: {
      spn_semver_t high = { .major = low.major + 1 };
      return spn_semver_cmp(version, low) >= 0 && spn_semver_cmp(version, high) < 0;
    }
    case FZ_RANGE_TILDE: {
      spn_semver_t high = { .major = low.major, .minor = low.minor + 1 };
      return spn_semver_cmp(version, low) >= 0 && spn_semver_cmp(version, high) < 0;
    }
    case FZ_RANGE_GEQ: {
      return spn_semver_cmp(version, low) >= 0;
    }
    case FZ_RANGE_LEQ: {
      return spn_semver_cmp(version, low) <= 0;
    }
    case FZ_RANGE_LT: {
      return spn_semver_cmp(version, low) < 0;
    }
    case FZ_RANGE_ANY: {
      return true;
    }
    case FZ_RANGE_COUNT: {
      break;
    }
  }
  sp_unreachable_return(false);
}

sp_str_t fz_range_render(sp_mem_t mem, fz_dep_t dep) {
  sp_str_t version = spn_semver_to_str(mem, dep.version);
  switch (dep.shape) {
    case FZ_RANGE_EXACT: return sp_fmt(mem, "={}", sp_fmt_str(version)).value;
    case FZ_RANGE_CARET: return sp_fmt(mem, "^{}", sp_fmt_str(version)).value;
    case FZ_RANGE_TILDE: return sp_fmt(mem, "~{}", sp_fmt_str(version)).value;
    case FZ_RANGE_GEQ:   return sp_fmt(mem, ">={}", sp_fmt_str(version)).value;
    case FZ_RANGE_LEQ:   return sp_fmt(mem, "<={}", sp_fmt_str(version)).value;
    case FZ_RANGE_LT:    return sp_fmt(mem, "<{}", sp_fmt_str(version)).value;
    case FZ_RANGE_ANY:   return sp_str_lit("*");
    case FZ_RANGE_COUNT: break;
  }
  sp_unreachable_return(sp_str_lit(""));
}

static spn_semver_t fz_sample_version(sp_fuzz_prng_t* prng) {
  return (spn_semver_t) {
    .major = 1 + (u32)sp_fuzz_below(prng, 3),
    .minor = (u32)sp_fuzz_below(prng, 4),
    .patch = (u32)sp_fuzz_below(prng, 3),
  };
}

static s32 fz_sort_release(const void* a, const void* b) {
  const fz_release_t* lhs = (const fz_release_t*)a;
  const fz_release_t* rhs = (const fz_release_t*)b;
  return spn_semver_cmp(lhs->version, rhs->version);
}

static fz_dep_t fz_free_range(sp_fuzz_prng_t* prng, fz_universe_t* u, u32 target) {
  fz_pkg_t* pkg = &u->pkgs[target];
  spn_semver_t anchor = sp_fuzz_chance(prng, 3, 4) ?
    pkg->releases[sp_fuzz_below(prng, sp_da_size(pkg->releases))].version :
    fz_sample_version(prng);
  return (fz_dep_t) {
    .pkg = target,
    .shape = (fz_range_shape_t)sp_fuzz_below(prng, FZ_RANGE_COUNT),
    .version = anchor,
  };
}

static fz_dep_t fz_sat_range(sp_fuzz_prng_t* prng, fz_universe_t* u, u32 target, spn_semver_t assigned) {
  for (u32 it = 0; it < 8; it++) {
    fz_dep_t dep = fz_free_range(prng, u, target);
    if (fz_range_sat(dep, assigned)) {
      return dep;
    }
  }
  return (fz_dep_t) { .pkg = target, .shape = FZ_RANGE_EXACT, .version = assigned };
}

// Half of all universes plant a known assignment: every range generated on
// the assigned release of each package (and at the root) is built to admit
// the assigned version of its target, so the universe is satisfiable by
// construction and any resolver error is a completeness bug. Decoy releases
// get unconstrained ranges; a decoy newer than the assigned version is
// exactly the bait a greedy searcher takes.
fz_universe_t fz_gen_universe(sp_mem_t mem, sp_fuzz_prng_t* prng) {
  fz_universe_t u = sp_zero;
  sp_da_init(mem, u.pkgs);
  sp_da_init(mem, u.roots);

  u64 count = 2 + sp_fuzz_below(prng, FZ_MAX_PKGS - 1);
  for (u64 it = 0; it < count; it++) {
    fz_pkg_t pkg = sp_zero;
    sp_da_init(mem, pkg.releases);

    u64 releases = 1 + sp_fuzz_below(prng, FZ_MAX_RELEASES);
    for (u64 attempt = 0; attempt < 32 && sp_da_size(pkg.releases) < releases; attempt++) {
      spn_semver_t version = fz_sample_version(prng);
      bool held = false;
      sp_da_for(pkg.releases, jt) {
        if (spn_semver_eq(pkg.releases[jt].version, version)) {
          held = true;
          break;
        }
      }
      if (held) continue;

      fz_release_t release = { .version = version };
      sp_da_init(mem, release.deps);
      sp_da_push(pkg.releases, release);
    }
    sp_da_sort(pkg.releases, fz_sort_release);
    sp_da_push(u.pkgs, pkg);
  }

  u.planted = sp_fuzz_chance(prng, 1, 2);
  u64 assigned[FZ_MAX_PKGS] = sp_zero;
  sp_da_for(u.pkgs, it) {
    assigned[it] = sp_fuzz_below(prng, sp_da_size(u.pkgs[it].releases));
  }

  u64 density = sp_fuzz_below(prng, 4);
  sp_da_for(u.pkgs, it) {
    fz_pkg_t* pkg = &u.pkgs[it];
    sp_da_for(pkg->releases, rt) {
      fz_release_t* release = &pkg->releases[rt];
      bool on_plan = u.planted && rt == assigned[it];
      for (u64 jt = it + 1; jt < sp_da_size(u.pkgs); jt++) {
        if (!sp_fuzz_chance(prng, (u32)density, 4)) continue;
        spn_semver_t target = u.pkgs[jt].releases[assigned[jt]].version;
        sp_da_push(release->deps, on_plan ?
          fz_sat_range(prng, &u, (u32)jt, target) :
          fz_free_range(prng, &u, (u32)jt));
      }
    }
  }

  sp_da_for(u.pkgs, it) {
    if (!sp_fuzz_chance(prng, 1, 2)) continue;
    spn_semver_t target = u.pkgs[it].releases[assigned[it]].version;
    sp_da_push(u.roots, u.planted ?
      fz_sat_range(prng, &u, (u32)it, target) :
      fz_free_range(prng, &u, (u32)it));
  }
  if (sp_da_empty(u.roots)) {
    spn_semver_t target = u.pkgs[0].releases[assigned[0]].version;
    sp_da_push(u.roots, u.planted ?
      fz_sat_range(prng, &u, 0, target) :
      fz_free_range(prng, &u, 0));
  }

  return u;
}
