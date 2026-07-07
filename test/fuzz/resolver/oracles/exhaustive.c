#include "fuzz.h"

static bool fz_assignment_ok(fz_universe_t* u, s64* picks) {
  sp_da_for(u->roots, it) {
    fz_dep_t dep = u->roots[it];
    if (picks[dep.pkg] < 0) return false;
    if (!fz_range_sat(dep, u->pkgs[dep.pkg].releases[picks[dep.pkg]].version)) return false;
  }

  sp_da_for(u->pkgs, it) {
    if (picks[it] < 0) continue;
    fz_release_t* release = &u->pkgs[it].releases[picks[it]];
    sp_da_for(release->deps, dt) {
      fz_dep_t dep = release->deps[dt];
      if (picks[dep.pkg] < 0) return false;
      if (!fz_range_sat(dep, u->pkgs[dep.pkg].releases[picks[dep.pkg]].version)) return false;
    }
  }

  return true;
}

static bool fz_picks_acyclic_at(fz_universe_t* u, s64* picks, u8* states, u64 pkg) {
  if (states[pkg] == 2) return true;
  if (states[pkg] == 1) return false;
  states[pkg] = 1;

  fz_release_t* release = &u->pkgs[pkg].releases[picks[pkg]];
  sp_da_for(release->deps, dt) {
    if (!fz_picks_acyclic_at(u, picks, states, release->deps[dt].pkg)) {
      return false;
    }
  }

  states[pkg] = 2;
  return true;
}

static bool fz_picks_acyclic(fz_universe_t* u, s64* picks) {
  u8 states[FZ_MAX_PKGS] = sp_zero;
  sp_da_for(u->pkgs, it) {
    if (picks[it] < 0) continue;
    if (!fz_picks_acyclic_at(u, picks, states, it)) {
      return false;
    }
  }
  return true;
}

bool fz_oracle_sat(fz_universe_t* u) {
  s64 picks[FZ_MAX_PKGS];
  u64 count = sp_da_size(u->pkgs);
  sp_carr_for(picks, it) {
    picks[it] = -1;
  }

  while (true) {
    if (fz_assignment_ok(u, picks) && fz_picks_acyclic(u, picks)) {
      return true;
    }

    u64 slot = 0;
    while (slot < count) {
      picks[slot]++;
      if (picks[slot] < (s64)sp_da_size(u->pkgs[slot].releases)) break;
      picks[slot] = -1;
      slot++;
    }
    if (slot == count) {
      return false;
    }
  }
}
