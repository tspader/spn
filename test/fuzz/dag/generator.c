#include "fuzz.h"

sp_str_t fz_artifact_path(sp_mem_t mem, fz_universe_t* u, u64 artifact) {
  switch (u->artifacts[artifact].kind) {
    case FZ_ARTIFACT_SOURCE: return sp_fmt(mem, "src/f{}", sp_fmt_uint(artifact)).value;
    case FZ_ARTIFACT_OUTPUT: return sp_fmt(mem, "out/f{}", sp_fmt_uint(artifact)).value;
    case FZ_ARTIFACT_VALUE:  break;
  }
  sp_unreachable_return(sp_str_lit(""));
}

sp_str_t fz_artifact_sim_path(sp_mem_t mem, fz_universe_t* u, u64 artifact) {
  return sp_fmt(mem, "/{}", sp_fmt_str(fz_artifact_path(mem, u, artifact))).value;
}

sp_str_t fz_phantom_path(sp_mem_t mem, u64 phantom) {
  return sp_fmt(mem, "gone/g{}", sp_fmt_uint(phantom)).value;
}

sp_str_t fz_phantom_sim_path(sp_mem_t mem, u64 phantom) {
  return sp_fmt(mem, "/{}", sp_fmt_str(fz_phantom_path(mem, phantom))).value;
}

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
  profile.steps = sp_fuzz_range(prng, 1, profile.big ? 4 : 12);
  sp_fuzz_swarm(prng, profile.step_weights, FZ_STEP_COUNT);
  profile.store_fs = sp_fuzz_chance(prng, 1, 2);
  profile.disco_fs = sp_fuzz_chance(prng, 1, 2);
  profile.cache_fs = sp_fuzz_chance(prng, 1, 2);
  profile.run_ex = sp_fuzz_chance(prng, 1, 2);
  return profile;
}

static u64 fz_pick_content(sp_fuzz_prng_t* prng, fz_profile_t* profile) {
  return sp_fuzz_below(prng, profile->content_count);
}

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

  sp_for(it, profile.action_count) {
    fz_action_t action = {
      .discover = profile.discover_pct && sp_fuzz_chance(prng, profile.discover_pct, 16),
      .identity = sp_fuzz_below(prng, profile.identity_count),
    };
    sp_da_init(mem, action.consumes);
    sp_da_init(mem, action.produces);
    sp_da_init(mem, action.obs);

    u64 have = sp_da_size(u.artifacts);
    if (profile.big) {
      u64 degree = sp_fuzz_below(prng, profile.out_degree + 1);
      sp_for(dt, degree) {
        sp_da_push(action.consumes, sp_fuzz_below(prng, have));
      }
    }
    else {
      sp_for(j, have) {
        if (sp_fuzz_chance(prng, profile.density, 8)) {
          sp_da_push(action.consumes, j);
        }
      }
    }
    if (!sp_da_empty(action.consumes) && sp_fuzz_chance(prng, 1, 8)) {
      u64 dup = action.consumes[sp_fuzz_below(prng, sp_da_size(action.consumes))];
      sp_da_push(action.consumes, dup);
    }

    u64 produced = 1 + sp_fuzz_below(prng, profile.produce_count);
    sp_for(j, produced) {
      u64 artifact = sp_da_size(u.artifacts);
      sp_da_push(u.artifacts, ((fz_artifact_t) {
        .kind = FZ_ARTIFACT_OUTPUT,
        .producer = (s64)it,
      }));
      sp_da_push(action.produces, artifact);
    }

    sp_da_push(u.actions, action);
  }

  sp_da_for(u.actions, it) {
    if (!sp_fuzz_chance(prng, profile.back_density, 8)) {
      continue;
    }
    u64 j = sp_fuzz_range(prng, it, sp_da_size(u.actions) - 1);
    fz_action_t* later = &u.actions[j];
    u64 n = sp_fuzz_below(prng, sp_da_size(later->produces));
    sp_da_push(u.actions[it].consumes, later->produces[n]);
  }

  sp_da_for(u.actions, it) {
    fz_action_t* action = &u.actions[it];
    if (!action->discover) {
      continue;
    }

    u64 count = sp_fuzz_below(prng, profile.obs_count + 1);
    sp_for(ot, count) {
      fz_obs_t obs = sp_zero;
      if (sp_fuzz_chance(prng, profile.absent_pct, 16)) {
        obs.probe = true;
        obs.phantom = sp_fuzz_below(prng, FZ_MAX_PHANTOMS);
      }
      else if (sp_fuzz_chance(prng, profile.obs_output_pct, 16) && sp_da_size(u.actions) > 1) {
        u64 pick = sp_fuzz_below(prng, sp_da_size(u.actions) - 1);
        if (pick >= it) {
          pick++;
        }
        fz_action_t* owner = &u.actions[pick];
        obs.artifact = owner->produces[sp_fuzz_below(prng, sp_da_size(owner->produces))];
      }
      else {
        obs.artifact = profile.value_count + sp_fuzz_below(prng, profile.source_count);
      }
      sp_da_push(action->obs, obs);
    }
    if (!sp_da_empty(action->obs) && sp_fuzz_chance(prng, 1, 8)) {
      fz_obs_t dup = action->obs[sp_fuzz_below(prng, sp_da_size(action->obs))];
      sp_da_push(action->obs, dup);
    }
  }

  u.cyclic = fz_universe_cyclic(&u);
  u.obs_cyclic = fz_universe_obs_cyclic(&u);
  return u;
}

fz_trace_t fz_gen_trace(sp_mem_t mem, sp_fuzz_prng_t* prng, fz_universe_t* u) {
  fz_profile_t* profile = &u->profile;
  fz_trace_t trace = sp_zero;
  sp_da_init(mem, trace.steps);

  sp_da(u64) outputs = sp_da_new(mem, u64);
  sp_da_for(u->artifacts, it) {
    if (u->artifacts[it].kind == FZ_ARTIFACT_OUTPUT) {
      sp_da_push(outputs, it);
    }
  }

  sp_da(u64)* history = sp_alloc_n(mem, sp_da(u64), profile->source_count);
  sp_for(st, profile->source_count) {
    sp_da_init(mem, history[st]);
    sp_da_push(history[st], u->artifacts[profile->value_count + st].content);
  }

  sp_for(it, profile->steps) {
    fz_step_t step = sp_zero;
    step.kind = (fz_step_kind_t)sp_fuzz_weighted(prng, profile->step_weights, FZ_STEP_COUNT);
    switch (step.kind) {
      case FZ_STEP_MUTATE: {
        u64 source = sp_fuzz_below(prng, profile->source_count);
        step.artifact = profile->value_count + source;
        step.content = fz_pick_content(prng, profile);
        sp_da_push(history[source], step.content);
        break;
      }
      case FZ_STEP_REVERT: {
        u64 source = sp_fuzz_below(prng, profile->source_count);
        step.artifact = profile->value_count + source;
        step.content = history[source][sp_fuzz_below(prng, sp_da_size(history[source]))];
        sp_da_push(history[source], step.content);
        break;
      }
      case FZ_STEP_STEALTH: {
        u64 source = sp_fuzz_below(prng, profile->source_count);
        u64 current = *sp_da_back(history[source]);
        step.artifact = profile->value_count + source;
        step.content = profile->content_count > 1
          ? (current + 1 + sp_fuzz_below(prng, profile->content_count - 1)) % profile->content_count
          : current;
        sp_da_push(history[source], step.content);
        break;
      }
      case FZ_STEP_TOUCH: {
        u64 pick = sp_fuzz_below(prng, profile->source_count + sp_da_size(outputs));
        step.artifact = pick < profile->source_count
          ? profile->value_count + pick
          : outputs[pick - profile->source_count];
        break;
      }
      case FZ_STEP_DELETE: {
        step.artifact = outputs[sp_fuzz_below(prng, sp_da_size(outputs))];
        break;
      }
      case FZ_STEP_PHANTOM: {
        step.artifact = sp_fuzz_below(prng, FZ_MAX_PHANTOMS);
        step.content = fz_pick_content(prng, profile);
        break;
      }
      case FZ_STEP_EIO: {
        step.artifact = sp_fuzz_next(prng);
        step.content = sp_fuzz_range(prng, 8, 512);
        break;
      }
      case FZ_STEP_CRASH:
      case FZ_STEP_BLOB:
      case FZ_STEP_EVICT: {
        step.artifact = sp_fuzz_next(prng);
        break;
      }
      case FZ_STEP_RUN:
      case FZ_STEP_DISCOVERY:
      case FZ_STEP_COUNT: {
        break;
      }
    }
    sp_da_push(trace.steps, step);
  }

  sp_da_push(trace.steps, ((fz_step_t) { .kind = FZ_STEP_RUN }));
  return trace;
}

static bool fz_cyclic_at(fz_universe_t* u, u8* states, bool obs, u64 action) {
  if (states[action] == 2) return false;
  if (states[action] == 1) return true;
  states[action] = 1;

  sp_da_for(u->actions[action].consumes, ct) {
    s64 producer = u->artifacts[u->actions[action].consumes[ct]].producer;
    if (producer >= 0 && fz_cyclic_at(u, states, obs, (u64)producer)) {
      return true;
    }
  }
  if (obs) {
    sp_da_for(u->actions[action].obs, ot) {
      fz_obs_t o = u->actions[action].obs[ot];
      if (o.probe) {
        continue;
      }
      s64 producer = u->artifacts[o.artifact].producer;
      if (producer >= 0 && (u64)producer != action && fz_cyclic_at(u, states, obs, (u64)producer)) {
        return true;
      }
    }
  }

  states[action] = 2;
  return false;
}

static bool fz_cyclic(fz_universe_t* u, bool obs) {
  u8 states[FZ_MAX_ACTIONS] = sp_zero;
  sp_da_for(u->actions, at) {
    if (fz_cyclic_at(u, states, obs, at)) {
      return true;
    }
  }
  return false;
}

bool fz_universe_cyclic(fz_universe_t* u) {
  return fz_cyclic(u, false);
}

bool fz_universe_obs_cyclic(fz_universe_t* u) {
  return fz_cyclic(u, true);
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
          if (producer->produces[pt] == it) {
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
      must(artifact->producer == (s64)at, FZ_ERR_GEN_PRODUCER);
    }
    must(action->discover || sp_da_empty(action->obs), FZ_ERR_GEN_OBS);
    must(sp_da_size(action->obs) <= FZ_MAX_OBS, FZ_ERR_GEN_OBS);
    sp_da_for(action->obs, ot) {
      fz_obs_t obs = action->obs[ot];
      if (obs.probe) {
        must(obs.phantom < FZ_MAX_PHANTOMS, FZ_ERR_GEN_OBS);
      }
      else {
        must(obs.artifact < sp_da_size(u->artifacts), FZ_ERR_GEN_OBS);
        must(u->artifacts[obs.artifact].kind != FZ_ARTIFACT_VALUE, FZ_ERR_GEN_OBS);
        must(u->artifacts[obs.artifact].producer != (s64)at, FZ_ERR_GEN_OBS);
      }
    }
  }

  if (!u->profile.back_density) {
    must(!u->cyclic, FZ_ERR_GEN_CYCLE);
  }

  return FZ_OK;
}
