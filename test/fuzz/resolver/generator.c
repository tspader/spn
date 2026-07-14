#include "fuzz.h"

#include "semver/compare.h"
#include "semver/convert.h"
#include "semver/parser.h"
#include "target/select.h"

static c8 fz_name_chars[FZ_MAX_PKGS][3];

sp_str_t fz_pkg_name(u32 pkg) {
  sp_assert(pkg < FZ_MAX_PKGS);
  c8* chars = fz_name_chars[pkg];
  if (!chars[0]) {
    if (pkg < 26) {
      chars[0] = (c8)('a' + pkg);
    }
    else {
      chars[0] = (c8)('a' + pkg / 26 - 1);
      chars[1] = (c8)('a' + pkg % 26);
    }
  }
  return sp_str_view(chars);
}

bool fz_pkg_shared(fz_universe_t* u, s32 pkg) {
  if (pkg < 0) {
    return false;
  }

  fz_pkg_t* it = &u->pkgs[pkg];
  if (!it->linkages.source && !it->linkages.static_lib && !it->linkages.shared) {
    return false;
  }

  spn_target_info_t info = sp_zero_s(spn_target_info_t);
  info.linkages = it->linkages;

  spn_kind_query_t query = { .linkage = u->profile.linkage };
  if (it->has_config) {
    sp_opt_set(query.config, it->config);
  }

  spn_linkage_t kind = SPN_LIB_KIND_NONE;
  if (spn_target_select_lib_kind(&info, query, &kind)) {
    return false;
  }
  return kind == SPN_LIB_KIND_SHARED;
}

static bool fz_caret_sat(spn_semver_t low, spn_semver_t version) {
  spn_semver_t high = sp_zero_s(spn_semver_t);
  if (low.major) {
    high.major = low.major + 1;
  }
  else if (low.minor) {
    high.minor = low.minor + 1;
  }
  else {
    high.patch = low.patch + 1;
  }
  return spn_semver_cmp(version, low) >= 0 && spn_semver_cmp(version, high) < 0;
}

bool fz_range_sat(fz_dep_t dep, spn_semver_t version) {
  spn_semver_t low = dep.version;
  switch (dep.shape) {
    case FZ_RANGE_EXACT: {
      return spn_semver_cmp(version, low) == 0;
    }
    case FZ_RANGE_BARE:
    case FZ_RANGE_CARET: {
      return fz_caret_sat(low, version);
    }
    case FZ_RANGE_TILDE: {
      spn_semver_t high = { .major = low.major, .minor = low.minor + 1 };
      return spn_semver_cmp(version, low) >= 0 && spn_semver_cmp(version, high) < 0;
    }
    case FZ_RANGE_TILDE_MAJOR: {
      return version.major == low.major;
    }
    case FZ_RANGE_GT: {
      return spn_semver_cmp(version, low) > 0;
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
    case FZ_RANGE_STAR_MINOR: {
      return version.major == low.major;
    }
    case FZ_RANGE_STAR_PATCH: {
      return version.major == low.major && version.minor == low.minor;
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
    case FZ_RANGE_EXACT:       return sp_fmt(mem, "={}", sp_fmt_str(version)).value;
    case FZ_RANGE_BARE:        return version;
    case FZ_RANGE_CARET:       return sp_fmt(mem, "^{}", sp_fmt_str(version)).value;
    case FZ_RANGE_TILDE:       return sp_fmt(mem, "~{}", sp_fmt_str(version)).value;
    case FZ_RANGE_TILDE_MAJOR: return sp_fmt(mem, "~{}", sp_fmt_uint(dep.version.major)).value;
    case FZ_RANGE_GT:          return sp_fmt(mem, ">{}", sp_fmt_str(version)).value;
    case FZ_RANGE_GEQ:         return sp_fmt(mem, ">={}", sp_fmt_str(version)).value;
    case FZ_RANGE_LEQ:         return sp_fmt(mem, "<={}", sp_fmt_str(version)).value;
    case FZ_RANGE_LT:          return sp_fmt(mem, "<{}", sp_fmt_str(version)).value;
    case FZ_RANGE_STAR_MINOR:  return sp_fmt(mem, "{}.*", sp_fmt_uint(dep.version.major)).value;
    case FZ_RANGE_STAR_PATCH:  return sp_fmt(mem, "{}.{}.*", sp_fmt_uint(dep.version.major), sp_fmt_uint(dep.version.minor)).value;
    case FZ_RANGE_ANY:         return sp_str_lit("*");
    case FZ_RANGE_COUNT:       break;
  }
  sp_unreachable_return(sp_str_lit(""));
}

#define FZ_LATTICE_SIZE (4 * 4 * 3)

static u64 fz_lattice(spn_semver_t* out) {
  u64 count = 0;
  for (u32 major = 0; major <= 3; major++) {
    for (u32 minor = 0; minor <= 3; minor++) {
      for (u32 patch = 0; patch <= 2; patch++) {
        out[count++] = (spn_semver_t) { .major = major, .minor = minor, .patch = patch };
      }
    }
  }
  return count;
}

static spn_semver_t fz_sample_version(sp_fuzz_prng_t* prng) {
  spn_semver_t lattice[FZ_LATTICE_SIZE];
  u64 count = fz_lattice(lattice);
  return lattice[sp_fuzz_below(prng, count)];
}

bool fz_ranges_agree(void) {
  sp_mem_arena_t* arena = sp_mem_arena_new(sp_mem_os_new());
  sp_mem_t mem = sp_mem_arena_as_allocator(arena);

  spn_semver_t lattice[FZ_LATTICE_SIZE];
  u64 count = fz_lattice(lattice);

  bool agree = true;
  for (u32 shape = 0; agree && shape < FZ_RANGE_COUNT; shape++) {
    for (u64 at = 0; agree && at < count; at++) {
      fz_dep_t dep = { .shape = (fz_range_shape_t)shape, .version = lattice[at] };
      spn_semver_range_t range = sp_zero;
      SP_ASSERT(!spn_semver_parse_range(fz_range_render(mem, dep), &range));
      for (u64 ct = 0; ct < count; ct++) {
        bool in_range =
          spn_semver_satisfies(lattice[ct], range.low.version, range.low.op) &&
          spn_semver_satisfies(lattice[ct], range.high.version, range.high.op);
        if (fz_range_sat(dep, lattice[ct]) != in_range) {
          agree = false;
          break;
        }
      }
    }
  }

  sp_mem_arena_destroy(arena);
  return agree;
}

static s32 fz_sort_release(const void* a, const void* b) {
  const fz_release_t* lhs = (const fz_release_t*)a;
  const fz_release_t* rhs = (const fz_release_t*)b;
  return spn_semver_cmp(lhs->version, rhs->version);
}

static spn_index_dep_kind_t fz_sample_kind(sp_fuzz_prng_t* prng, fz_profile_t* profile) {
  if (!profile->features) {
    return SPN_INDEX_DEP_NORMAL;
  }
  if (sp_fuzz_chance(prng, (u32)profile->build_pct, 16)) {
    return SPN_INDEX_DEP_BUILD;
  }
  if (sp_fuzz_chance(prng, (u32)profile->test_pct, 16)) {
    return SPN_INDEX_DEP_TEST;
  }
  return SPN_INDEX_DEP_NORMAL;
}

static fz_dep_t fz_free_range(sp_fuzz_prng_t* prng, fz_universe_t* u, u32 target) {
  fz_pkg_t* pkg = &u->pkgs[target];
  if (pkg->local) {
    return (fz_dep_t) {
      .pkg = target,
      .shape = FZ_RANGE_ANY,
      .kind = fz_sample_kind(prng, &u->profile),
      .private = sp_fuzz_chance(prng, (u32)u->profile.private_pct, 16),
    };
  }

  spn_semver_t anchor = sp_fuzz_chance(prng, 3, 4) ?
    pkg->releases[sp_fuzz_below(prng, sp_da_size(pkg->releases))].version :
    fz_sample_version(prng);
  return (fz_dep_t) {
    .pkg = target,
    .shape = (fz_range_shape_t)sp_fuzz_weighted(prng, u->profile.shapes, FZ_RANGE_COUNT),
    .version = anchor,
    .kind = fz_sample_kind(prng, &u->profile),
    .private = sp_fuzz_chance(prng, (u32)u->profile.private_pct, 16),
  };
}

static fz_dep_t fz_sat_range(sp_fuzz_prng_t* prng, fz_universe_t* u, u32 target, spn_semver_t assigned) {
  if (u->pkgs[target].local) {
    return (fz_dep_t) { .pkg = target, .shape = FZ_RANGE_ANY, .kind = SPN_INDEX_DEP_NORMAL };
  }

  for (u32 it = 0; it < 8; it++) {
    fz_dep_t dep = fz_free_range(prng, u, target);
    dep.kind = SPN_INDEX_DEP_NORMAL;
    dep.private = false;
    if (fz_range_sat(dep, assigned)) {
      return dep;
    }
  }
  return (fz_dep_t) { .pkg = target, .shape = FZ_RANGE_EXACT, .version = assigned };
}

static bool fz_can_dep(fz_universe_t* u, u64 owner, u64 target) {
  return u->pkgs[owner].local || !u->pkgs[target].local;
}

fz_profile_t fz_gen_profile(sp_fuzz_prng_t* prng) {
  fz_profile_t profile = sp_zero;
  sp_fuzz_swarm(prng, profile.shapes, FZ_RANGE_COUNT);
  profile.big = sp_fuzz_chance(prng, 1, 8);
  if (profile.big) {
    profile.pkg_count = sp_fuzz_range(prng, FZ_SMALL_PKGS + 1, FZ_MAX_PKGS);
    profile.out_degree = sp_fuzz_range(prng, 1, 4);
  }
  else {
    profile.pkg_count = 2 + sp_fuzz_below(prng, FZ_SMALL_PKGS - 1);
  }
  profile.release_count = 1 + sp_fuzz_below(prng, FZ_MAX_RELEASES);
  profile.density = sp_fuzz_below(prng, 4);
  profile.back_density = sp_fuzz_chance(prng, 1, 4) ? sp_fuzz_range(prng, 1, 2) : 0;
  profile.local_pct = sp_fuzz_chance(prng, 1, 3) ? sp_fuzz_range(prng, 1, 6) : 0;
  profile.features = sp_fuzz_chance(prng, 2, 5);
  if (profile.features) {
    profile.build_pct = sp_fuzz_below(prng, 6);
    profile.test_pct = sp_fuzz_below(prng, 4);
    profile.private_pct = sp_fuzz_below(prng, 8);
    profile.shared_pct = sp_fuzz_range(prng, 2, 12);
    profile.static_pct = sp_fuzz_below(prng, 8);
    profile.config_pct = sp_fuzz_below(prng, 4);
    static const spn_linkage_t linkages[] = { SPN_LIB_KIND_NONE, SPN_LIB_KIND_STATIC, SPN_LIB_KIND_SHARED, SPN_LIB_KIND_SOURCE };
    profile.linkage = linkages[sp_fuzz_below(prng, SP_CARR_LEN(linkages))];
  }
  profile.planted = !profile.features && sp_fuzz_chance(prng, 1, 2);
  if (!profile.big && sp_fuzz_chance(prng, 1, 16)) {
    profile.budget = sp_fuzz_range(prng, 1, 64);
  }
  return profile;
}

fz_universe_t fz_gen_universe(sp_mem_t mem, sp_fuzz_prng_t* prng, fz_profile_t profile) {
  fz_universe_t u = sp_zero;
  u.profile = profile;
  sp_da_init(mem, u.pkgs);
  sp_da_init(mem, u.roots);
  sp_da_init(mem, u.plan);

  u64 count = profile.pkg_count;
  for (u64 it = 0; it < count; it++) {
    fz_pkg_t pkg = sp_zero;
    pkg.local = profile.local_pct && sp_fuzz_chance(prng, (u32)profile.local_pct, 16);
    if (profile.features) {
      pkg.linkages.shared = sp_fuzz_chance(prng, (u32)profile.shared_pct, 16);
      pkg.linkages.static_lib = sp_fuzz_chance(prng, (u32)profile.static_pct, 16);
      pkg.linkages.source = sp_fuzz_chance(prng, (u32)profile.static_pct, 16);
      if (sp_fuzz_chance(prng, (u32)profile.config_pct, 16)) {
        static const spn_linkage_t kinds[] = { SPN_LIB_KIND_SHARED, SPN_LIB_KIND_STATIC, SPN_LIB_KIND_SOURCE };
        pkg.has_config = true;
        pkg.config = kinds[sp_fuzz_below(prng, SP_CARR_LEN(kinds))];
      }
    }
    sp_da_init(mem, pkg.releases);

    u64 releases = pkg.local ? 1 : 1 + sp_fuzz_below(prng, profile.release_count);
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

  u.planted = profile.planted;
  sp_da_for(u.pkgs, it) {
    sp_da_push(u.plan, (u32)sp_fuzz_below(prng, sp_da_size(u.pkgs[it].releases)));
  }

  sp_da_for(u.pkgs, it) {
    fz_pkg_t* pkg = &u.pkgs[it];
    sp_da_for(pkg->releases, rt) {
      fz_release_t* release = &pkg->releases[rt];
      bool on_plan = u.planted && rt == u.plan[it];
      if (profile.big) {
        u64 forward = count - it - 1;
        u64 degree = forward ? sp_fuzz_below(prng, profile.out_degree + 1) : 0;
        for (u64 dt = 0; dt < degree; dt++) {
          u64 jt = it + 1 + sp_fuzz_below(prng, forward);
          if (!fz_can_dep(&u, it, jt)) continue;
          spn_semver_t target = u.pkgs[jt].releases[u.plan[jt]].version;
          sp_da_push(release->deps, on_plan ?
            fz_sat_range(prng, &u, (u32)jt, target) :
            fz_free_range(prng, &u, (u32)jt));
        }
      }
      else {
        for (u64 jt = it + 1; jt < count; jt++) {
          if (!sp_fuzz_chance(prng, (u32)u.profile.density, 4)) continue;
          if (!fz_can_dep(&u, it, jt)) continue;
          spn_semver_t target = u.pkgs[jt].releases[u.plan[jt]].version;
          sp_da_push(release->deps, on_plan ?
            fz_sat_range(prng, &u, (u32)jt, target) :
            fz_free_range(prng, &u, (u32)jt));
        }
      }
      if (!on_plan) {
        if (profile.big) {
          if (sp_fuzz_chance(prng, (u32)u.profile.back_density, 8)) {
            u64 jt = sp_fuzz_below(prng, it + 1);
            if (fz_can_dep(&u, it, jt)) {
              sp_da_push(release->deps, fz_free_range(prng, &u, (u32)jt));
            }
          }
        }
        else {
          for (u64 jt = 0; jt <= it; jt++) {
            if (!sp_fuzz_chance(prng, (u32)u.profile.back_density, 8)) continue;
            if (!fz_can_dep(&u, it, jt)) continue;
            sp_da_push(release->deps, fz_free_range(prng, &u, (u32)jt));
          }
        }
      }
      if (!sp_da_empty(release->deps) && sp_fuzz_chance(prng, 1, 8)) {
        u32 dup = release->deps[sp_fuzz_below(prng, sp_da_size(release->deps))].pkg;
        spn_semver_t target = u.pkgs[dup].releases[u.plan[dup]].version;
        sp_da_push(release->deps, on_plan ?
          fz_sat_range(prng, &u, dup, target) :
          fz_free_range(prng, &u, dup));
      }
    }
  }

  sp_da_for(u.pkgs, it) {
    if (!sp_fuzz_chance(prng, 1, 2)) continue;
    spn_semver_t target = u.pkgs[it].releases[u.plan[it]].version;
    sp_da_push(u.roots, u.planted ?
      fz_sat_range(prng, &u, (u32)it, target) :
      fz_free_range(prng, &u, (u32)it));
  }
  if (sp_da_empty(u.roots)) {
    spn_semver_t target = u.pkgs[0].releases[u.plan[0]].version;
    sp_da_push(u.roots, u.planted ?
      fz_sat_range(prng, &u, 0, target) :
      fz_free_range(prng, &u, 0));
  }
  if (sp_fuzz_chance(prng, 1, 8)) {
    u32 dup = u.roots[sp_fuzz_below(prng, sp_da_size(u.roots))].pkg;
    spn_semver_t target = u.pkgs[dup].releases[u.plan[dup]].version;
    sp_da_push(u.roots, u.planted ?
      fz_sat_range(prng, &u, dup, target) :
      fz_free_range(prng, &u, dup));
  }

  return u;
}

static fz_universe_t fz_copy_universe(sp_mem_t mem, fz_universe_t* u) {
  fz_universe_t copy = *u;
  sp_da_init(mem, copy.pkgs);
  sp_da_init(mem, copy.roots);
  sp_da_init(mem, copy.plan);

  sp_da_for(u->pkgs, it) {
    fz_pkg_t pkg = u->pkgs[it];
    sp_da_init(mem, pkg.releases);
    sp_da_for(u->pkgs[it].releases, rt) {
      fz_release_t release = u->pkgs[it].releases[rt];
      sp_da_init(mem, release.deps);
      sp_da_for(u->pkgs[it].releases[rt].deps, dt) {
        sp_da_push(release.deps, u->pkgs[it].releases[rt].deps[dt]);
      }
      sp_da_push(pkg.releases, release);
    }
    sp_da_push(copy.pkgs, pkg);
  }

  sp_da_for(u->roots, it) {
    sp_da_push(copy.roots, u->roots[it]);
  }

  sp_da_for(u->plan, it) {
    sp_da_push(copy.plan, u->plan[it]);
  }

  return copy;
}

fz_universe_t fz_shuffle_universe(sp_mem_t mem, sp_fuzz_prng_t* prng, fz_universe_t* u) {
  fz_universe_t copy = fz_copy_universe(mem, u);
  sp_fuzz_shuffle(prng, copy.roots, sp_da_size(copy.roots), sizeof(fz_dep_t));
  sp_da_for(copy.pkgs, it) {
    sp_da_for(copy.pkgs[it].releases, rt) {
      fz_release_t* release = &copy.pkgs[it].releases[rt];
      sp_fuzz_shuffle(prng, release->deps, sp_da_size(release->deps), sizeof(fz_dep_t));
    }
  }
  return copy;
}

fz_universe_t fz_rename_universe(sp_mem_t mem, sp_fuzz_prng_t* prng, fz_universe_t* u) {
  u32 perm[FZ_MAX_PKGS];
  u64 count = sp_da_size(u->pkgs);
  for (u64 it = 0; it < count; it++) {
    perm[it] = (u32)it;
  }
  sp_fuzz_shuffle(prng, perm, count, sizeof(u32));

  fz_universe_t copy = fz_copy_universe(mem, u);
  sp_da_for(u->pkgs, it) {
    copy.pkgs[perm[it]] = u->pkgs[it];
    copy.plan[perm[it]] = u->plan[it];
  }
  sp_da_for(copy.pkgs, it) {
    fz_pkg_t src = copy.pkgs[it];
    fz_pkg_t pkg = src;
    sp_da_init(mem, pkg.releases);
    sp_da_for(src.releases, rt) {
      fz_release_t release = src.releases[rt];
      sp_da_init(mem, release.deps);
      sp_da_for(src.releases[rt].deps, dt) {
        fz_dep_t dep = src.releases[rt].deps[dt];
        dep.pkg = perm[dep.pkg];
        sp_da_push(release.deps, dep);
      }
      sp_da_push(pkg.releases, release);
    }
    copy.pkgs[it] = pkg;
  }
  sp_da_for(copy.roots, it) {
    copy.roots[it].pkg = perm[copy.roots[it].pkg];
  }
  return copy;
}

fz_universe_t fz_pin_universe(sp_mem_t mem, fz_universe_t* u, fz_solution_t solution) {
  fz_universe_t copy = fz_copy_universe(mem, u);
  sp_da_for(copy.pkgs, it) {
    sp_da(fz_release_t) kept = sp_da_new(mem, fz_release_t);
    sp_da_for(copy.pkgs[it].releases, rt) {
      bool used = false;
      sp_da_for(solution, st) {
        if (solution[st].pkg == (s32)it && spn_semver_eq(solution[st].version, copy.pkgs[it].releases[rt].version)) {
          used = true;
          break;
        }
      }
      if (used) {
        sp_da_push(kept, copy.pkgs[it].releases[rt]);
      }
    }
    copy.pkgs[it].releases = kept;
  }
  return copy;
}

fz_universe_t fz_extend_universe(sp_mem_t mem, sp_fuzz_prng_t* prng, fz_universe_t* u) {
  fz_universe_t copy = fz_copy_universe(mem, u);

  u32 candidates[FZ_MAX_PKGS];
  u32 count = 0;
  sp_da_for(copy.pkgs, it) {
    if (!copy.pkgs[it].local) {
      candidates[count++] = (u32)it;
    }
  }
  if (!count) {
    return copy;
  }
  u32 target = candidates[sp_fuzz_below(prng, count)];
  fz_pkg_t* pkg = &copy.pkgs[target];

  for (u32 attempt = 0; attempt < 16; attempt++) {
    spn_semver_t version = fz_sample_version(prng);
    bool held = false;
    sp_da_for(pkg->releases, jt) {
      if (spn_semver_eq(pkg->releases[jt].version, version)) {
        held = true;
        break;
      }
    }
    if (held) continue;

    fz_release_t release = { .version = version };
    sp_da_init(mem, release.deps);
    if (copy.profile.big) {
      u64 degree = sp_fuzz_below(prng, copy.profile.out_degree + 1);
      for (u64 dt = 0; dt < degree; dt++) {
        u64 jt = sp_fuzz_below(prng, sp_da_size(copy.pkgs));
        if (jt == target || copy.pkgs[jt].local) continue;
        sp_da_push(release.deps, fz_free_range(prng, &copy, (u32)jt));
      }
    }
    else {
      sp_da_for(copy.pkgs, jt) {
        if (jt == target) continue;
        if (copy.pkgs[jt].local) continue;
        if (!sp_fuzz_chance(prng, (u32)copy.profile.density, 4)) continue;
        sp_da_push(release.deps, fz_free_range(prng, &copy, (u32)jt));
      }
    }
    sp_da_push(pkg->releases, release);
    sp_da_sort(pkg->releases, fz_sort_release);
    break;
  }

  return copy;
}
