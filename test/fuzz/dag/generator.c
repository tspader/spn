#include "fuzz.h"

sp_str_t fz_artifact_path(sp_mem_t mem, fz_universe_t* u, u32 artifact) {
  switch (u->artifacts[artifact].kind) {
    case FZ_ARTIFACT_SOURCE: return sp_fmt(mem, "src/f{}", sp_fmt_uint(artifact)).value;
    case FZ_ARTIFACT_OUTPUT: return sp_fmt(mem, "out/f{}", sp_fmt_uint(artifact)).value;
    case FZ_ARTIFACT_VALUE:  break;
  }
  sp_unreachable_return(sp_str_lit(""));
}

sp_str_t fz_phantom_path(sp_mem_t mem, u32 phantom) {
  return sp_fmt(mem, "gone/g{}", sp_fmt_uint(phantom)).value;
}

// A universe is a self-contained build problem: a set of input artifacts
// (value blobs and source files whose contents the harness owns), a set of
// actions wired to them, and for discovery actions a scripted observation
// set. Creation order is a topological order for the static edges, so the
// graph is acyclic unless back-edges are injected; contents and identities
// draw from small pools so digests and action keys collide densely.
//
// Swarm testing: each run samples a qualitatively different regime instead
// of an average over all of them. Most knobs are zeroed entirely for a slice
// of runs — no discovery, no back-edges, a single content class — so the
// simple regimes stay in rotation at full strength. Big universes trade
// density for scale: they stress the executor's bookkeeping and accidental
// quadratics, not the cache semantics.
fz_profile_t fz_gen_profile(sp_fuzz_prng_t* prng) {
  fz_profile_t profile = sp_zero;
  profile.big = sp_fuzz_chance(prng, 1, 8);
  if (profile.big) {
    profile.action_count = sp_fuzz_range(prng, FZ_SMALL_ACTIONS + 1, FZ_MAX_ACTIONS);
    profile.out_degree = sp_fuzz_range(prng, 1, 4);
    profile.source_count = sp_fuzz_range(prng, 1, 16);
  }
  else {
    profile.action_count = sp_fuzz_range(prng, 1, FZ_SMALL_ACTIONS);
    profile.source_count = sp_fuzz_range(prng, 1, 4);
    profile.density = sp_fuzz_below(prng, 4);
  }
  profile.value_count = sp_fuzz_chance(prng, 1, 2) ? sp_fuzz_range(prng, 1, 3) : 0;
  profile.content_count = sp_fuzz_range(prng, 1, 6);
  profile.identity_count = sp_fuzz_chance(prng, 1, 8) ? sp_fuzz_range(prng, 1, 2) : sp_fuzz_range(prng, 4, 64);
  profile.produce_count = sp_fuzz_range(prng, 1, FZ_MAX_PRODUCES);
  profile.discover_pct = sp_fuzz_chance(prng, 1, 2) ? sp_fuzz_range(prng, 2, 12) : 0;
  if (profile.discover_pct) {
    profile.obs_count = sp_fuzz_range(prng, 1, 4);
    profile.absent_pct = sp_fuzz_below(prng, 6);
    profile.obs_output_pct = sp_fuzz_below(prng, 8);
  }
  profile.back_density = sp_fuzz_chance(prng, 1, 8) ? sp_fuzz_range(prng, 1, 2) : 0;
  return profile;
}

static u32 fz_pick_content(sp_fuzz_prng_t* prng, fz_profile_t* profile) {
  return (u32)sp_fuzz_below(prng, profile->content_count);
}

// Static edges point backward through creation order, so the base graph is
// acyclic by construction; back-edges are how cycles may enter, and whether
// one actually closed a loop is derived afterward, not assumed. An action
// occasionally repeats an input it already consumes: registration pushes
// edges blindly, so the executor's pending bookkeeping must stay symmetric
// under duplicates. Discovery observations are assigned last so they can
// reference any file in the universe: an observation on another action's
// output — even a later one — is what exercises the executor's defer path,
// and one on the action's own output is the compiler depfile pattern.
fz_universe_t fz_gen_universe(sp_mem_t mem, sp_fuzz_prng_t* prng, fz_profile_t profile) {
  fz_universe_t u = sp_zero;
  u.profile = profile;
  sp_da_init(mem, u.artifacts);
  sp_da_init(mem, u.actions);

  sp_for(it, profile.value_count) {
    sp_da_push(u.artifacts, ((fz_artifact_t) {
      .kind = FZ_ARTIFACT_VALUE,
      .content = fz_pick_content(prng, &profile),
      .producer = -1,
    }));
  }
  sp_for(it, profile.source_count) {
    sp_da_push(u.artifacts, ((fz_artifact_t) {
      .kind = FZ_ARTIFACT_SOURCE,
      .content = fz_pick_content(prng, &profile),
      .producer = -1,
    }));
  }

  sp_for(at, profile.action_count) {
    fz_action_t action = {
      .discover = profile.discover_pct && sp_fuzz_chance(prng, (u32)profile.discover_pct, 16),
      .identity = (u32)sp_fuzz_below(prng, profile.identity_count),
    };
    sp_da_init(mem, action.consumes);
    sp_da_init(mem, action.produces);
    sp_da_init(mem, action.obs);

    u64 have = sp_da_size(u.artifacts);
    if (profile.big) {
      u64 degree = sp_fuzz_below(prng, profile.out_degree + 1);
      sp_for(dt, degree) {
        sp_da_push(action.consumes, (u32)sp_fuzz_below(prng, have));
      }
    }
    else {
      sp_for(jt, have) {
        if (sp_fuzz_chance(prng, (u32)profile.density, 8)) {
          sp_da_push(action.consumes, (u32)jt);
        }
      }
    }
    if (!sp_da_empty(action.consumes) && sp_fuzz_chance(prng, 1, 8)) {
      u32 dup = action.consumes[sp_fuzz_below(prng, sp_da_size(action.consumes))];
      sp_da_push(action.consumes, dup);
    }

    u64 produced = 1 + sp_fuzz_below(prng, profile.produce_count);
    sp_for(pt, produced) {
      u32 artifact = (u32)sp_da_size(u.artifacts);
      sp_da_push(u.artifacts, ((fz_artifact_t) {
        .kind = FZ_ARTIFACT_OUTPUT,
        .producer = (s32)at,
      }));
      sp_da_push(action.produces, artifact);
    }

    sp_da_push(u.actions, action);
  }

  sp_da_for(u.actions, at) {
    if (!sp_fuzz_chance(prng, (u32)profile.back_density, 8)) {
      continue;
    }
    u64 jt = sp_fuzz_range(prng, at, sp_da_size(u.actions) - 1);
    fz_action_t* later = &u.actions[jt];
    sp_da_push(u.actions[at].consumes, later->produces[sp_fuzz_below(prng, sp_da_size(later->produces))]);
  }

  sp_da_for(u.actions, at) {
    fz_action_t* action = &u.actions[at];
    if (!action->discover) {
      continue;
    }

    u64 count = 1 + sp_fuzz_below(prng, profile.obs_count);
    sp_for(ot, count) {
      fz_obs_t obs = sp_zero;
      if (sp_fuzz_chance(prng, (u32)profile.absent_pct, 16)) {
        obs.absent = true;
        obs.phantom = (u32)sp_fuzz_below(prng, FZ_MAX_PHANTOMS);
      }
      else if (sp_fuzz_chance(prng, (u32)profile.obs_output_pct, 16)) {
        fz_action_t* owner = &u.actions[sp_fuzz_below(prng, sp_da_size(u.actions))];
        obs.artifact = owner->produces[sp_fuzz_below(prng, sp_da_size(owner->produces))];
      }
      else {
        obs.artifact = (u32)(profile.value_count + sp_fuzz_below(prng, profile.source_count));
      }
      sp_da_push(action->obs, obs);
    }
    if (sp_fuzz_chance(prng, 1, 8)) {
      fz_obs_t dup = action->obs[sp_fuzz_below(prng, sp_da_size(action->obs))];
      sp_da_push(action->obs, dup);
    }
  }

  u.cyclic = fz_universe_cyclic(&u);
  return u;
}

static bool fz_cyclic_at(fz_universe_t* u, u8* states, u32 action) {
  if (states[action] == 2) return false;
  if (states[action] == 1) return true;
  states[action] = 1;

  sp_da_for(u->actions[action].consumes, ct) {
    s32 producer = u->artifacts[u->actions[action].consumes[ct]].producer;
    if (producer >= 0 && fz_cyclic_at(u, states, (u32)producer)) {
      return true;
    }
  }

  states[action] = 2;
  return false;
}

bool fz_universe_cyclic(fz_universe_t* u) {
  u8 states[FZ_MAX_ACTIONS] = sp_zero;
  sp_da_for(u->actions, at) {
    if (fz_cyclic_at(u, states, (u32)at)) {
      return true;
    }
  }
  return false;
}

fz_err_t fz_check_universe(fz_universe_t* u) {
  sp_da_for(u->artifacts, it) {
    fz_artifact_t* artifact = &u->artifacts[it];
    switch (artifact->kind) {
      case FZ_ARTIFACT_VALUE:
      case FZ_ARTIFACT_SOURCE: {
        must(artifact->producer < 0, FZ_ERR_GEN_PRODUCER);
        must(artifact->content < u->profile.content_count, FZ_ERR_GEN_EDGE);
        break;
      }
      case FZ_ARTIFACT_OUTPUT: {
        must(artifact->producer >= 0 && (u64)artifact->producer < sp_da_size(u->actions), FZ_ERR_GEN_PRODUCER);
        bool held = false;
        fz_action_t* producer = &u->actions[artifact->producer];
        sp_da_for(producer->produces, pt) {
          if (producer->produces[pt] == (u32)it) {
            held = true;
            break;
          }
        }
        must(held, FZ_ERR_GEN_PRODUCER);
        break;
      }
    }
  }

  sp_da_for(u->actions, at) {
    fz_action_t* action = &u->actions[at];
    sp_da_for(action->consumes, ct) {
      must(action->consumes[ct] < sp_da_size(u->artifacts), FZ_ERR_GEN_EDGE);
    }
    sp_da_for(action->produces, pt) {
      fz_artifact_t* artifact = &u->artifacts[action->produces[pt]];
      must(artifact->kind == FZ_ARTIFACT_OUTPUT, FZ_ERR_GEN_PRODUCER);
      must(artifact->producer == (s32)at, FZ_ERR_GEN_PRODUCER);
    }
    must(action->discover == !sp_da_empty(action->obs), FZ_ERR_GEN_OBS);
    sp_da_for(action->obs, ot) {
      fz_obs_t obs = action->obs[ot];
      if (obs.absent) {
        must(obs.phantom < FZ_MAX_PHANTOMS, FZ_ERR_GEN_OBS);
      }
      else {
        must(obs.artifact < sp_da_size(u->artifacts), FZ_ERR_GEN_OBS);
        must(u->artifacts[obs.artifact].kind != FZ_ARTIFACT_VALUE, FZ_ERR_GEN_OBS);
      }
    }
  }

  if (!u->profile.back_density) {
    must(!u->cyclic, FZ_ERR_GEN_CYCLE);
  }

  return FZ_OK;
}
