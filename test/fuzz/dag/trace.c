#include "fuzz.h"
#include "sp_sim.h"
#include "sp/io.h"

typedef struct {
  u64 count;
  sp_str_t bytes [FZ_MAX_PRODUCES];
} fz_cached_t;

typedef struct {
  sp_mem_t mem;
  sp_sim_t* sim;
  spn_dag_store_t store;
  spn_dag_file_cache_t files;
  spn_dag_action_cache_t cache;
  spn_dag_discovery_t discovery;
  sp_str_t disco_dir;
  spn_dag_env_t env;
  fz_executor_t ex;
  sp_ht(spn_dag_digest_t, fz_cached_t) mirror;
  sp_ht(spn_dag_digest_t, fz_shape_t) pathsets;
  u64* believed;
  bool* dirty;
  bool honest;
} fz_world_t;

static void fz_executor_submit(spn_dag_executor_t* base, spn_dag_job_t job) {
  fz_executor_t* ex = (fz_executor_t*)base;
  sp_da_push(ex->jobs, job);
  sp_da_push(ex->submitted, ex->sim->syscalls);
}

static spn_dag_job_t fz_executor_poll(spn_dag_executor_t* base) {
  fz_executor_t* ex = (fz_executor_t*)base;
  sp_assert(!sp_da_empty(ex->jobs));

  u64 pick = sp_fuzz_below(&ex->prng, sp_da_size(ex->jobs));
  spn_dag_job_t job = ex->jobs[pick];
  u64 submitted = ex->submitted[pick];
  ex->jobs[pick] = *sp_da_back(ex->jobs);
  ex->submitted[pick] = *sp_da_back(ex->submitted);
  sp_da_pop(ex->jobs);
  sp_da_pop(ex->submitted);

  ex->ran = -1;
  job.fn(job.data);
  if (ex->ran >= 0) {
    sp_da_back(ex->log)->submitted = submitted;
  }
  return job;
}

void fz_executor_init(fz_executor_t* ex, sp_mem_t mem, sp_sim_t* sim, sp_fuzz_prng_t prng) {
  *ex = (fz_executor_t) {
    .base = {
      .submit = fz_executor_submit,
      .poll = fz_executor_poll,
    },
    .prng = prng,
    .sim = sim,
    .ran = -1,
  };
  sp_da_init(mem, ex->jobs);
  sp_da_init(mem, ex->submitted);
  sp_da_init(mem, ex->log);
}

static bool fz_covered_write(fz_universe_t* u, fz_world_t* w, sp_mem_t mem, u64 at, u64 lo, u64 hi) {
  sp_da_for(u->actions[at].obs, ot) {
    fz_obs_t obs = u->actions[at].obs[ot];
    if (obs.probe) {
      continue;
    }
    s64 producer = u->artifacts[obs.artifact].producer;
    if (producer < 0 || (u64)producer == at) {
      continue;
    }
    sp_da_for(u->actions[producer].produces, pt) {
      sp_str_t target = fz_artifact_sim_path(mem, u, u->actions[producer].produces[pt]);
      sp_da_for(w->sim->events, et) {
        sp_sim_event_t* event = &w->sim->events[et];
        if (event->sys > lo && event->sys <= hi && sp_str_equal(event->path, target)) {
          return true;
        }
      }
    }
  }
  return false;
}

static u64 fz_action_requeues(fz_universe_t* u, fz_world_t* w, sp_mem_t mem, u64 log_start, u64 at) {
  fz_executor_t* ex = &w->ex;
  u64 requeues = 0;
  s64 prev = -1;
  for (u64 it = log_start; it < sp_da_size(ex->log); it++) {
    if (ex->log[it].action != at) {
      continue;
    }
    if (prev >= 0) {
      u64 lo = u->profile.run_ex ? ex->log[prev].submitted : ex->log[prev].started;
      if (fz_covered_write(u, w, mem, at, lo, ex->log[it].started)) {
        requeues++;
      }
    }
    prev = (s64)it;
  }
  return requeues;
}

static void fz_world_init(fz_world_t* w, sp_mem_t mem, sp_sim_t* sim, fz_universe_t* u, sp_fuzz_prng_t schedule) {
  sp_fs_create_dir(sp_str_lit("/src"));
  sp_fs_create_dir(sp_str_lit("/out"));
  sp_fs_create_dir(sp_str_lit("/scratch"));
  sp_fs_create_dir(sp_str_lit("/gone"));

  spn_dag_store_init(&w->store, (spn_dag_store_config_t) {
    .kind = u->profile.store_fs ? SPN_DAG_STORE_FILESYSTEM : SPN_DAG_STORE_MEM,
    .mem = mem,
    .dir = sp_str_lit("/store"),
  });
  spn_dag_file_cache_init(&w->files, mem);
  spn_dag_action_cache_init(&w->cache, mem, sp_str_lit(""));
  w->disco_dir = u->profile.disco_fs ? sp_str_lit("/manifests") : sp_str_lit("");
  spn_dag_discovery_init(&w->discovery, mem, w->disco_dir);
  w->env = (spn_dag_env_t) {
    .files = &w->files,
    .cache = &w->cache,
    .store = &w->store,
    .discovery = &w->discovery,
    .scratch = sp_str_lit("/scratch"),
  };
  fz_executor_init(&w->ex, mem, sim, schedule);
  sp_ht_init(mem, w->mirror);
  sp_ht_init(mem, w->pathsets);

  w->mem = mem;
  w->sim = sim;
  u64 artifacts = sp_da_size(u->artifacts);
  w->believed = sp_alloc_n(mem, u64, artifacts);
  w->dirty = sp_alloc_n(mem, bool, artifacts);
  sp_mem_zero(w->believed, artifacts * sizeof(u64));
  sp_mem_zero(w->dirty, artifacts * sizeof(bool));
  w->honest = true;
}

static spn_err_t fz_world_run(fz_world_t* w, fz_universe_t* u, spn_dag_t* g) {
  if (u->profile.run_ex) {
    return spn_dag_run_ex(g, &w->env, &w->ex.base);
  }
  return spn_dag_run(g, &w->env);
}

static void fz_write_source(sp_mem_t mem, fz_universe_t* u, u64 artifact) {
  sp_fs_create_file_str(fz_artifact_sim_path(mem, u, artifact), fz_content(mem, u->artifacts[artifact].content));
}

static void fz_eval_action(fz_world_t* w, fz_universe_t* u, sp_str_t* key_bytes, sp_str_t* disk_bytes, bool* done, bool* misses, u64 at) {
  if (done[at]) {
    return;
  }
  done[at] = true;

  fz_action_t* action = &u->actions[at];
  sp_da_for(action->consumes, ct) {
    s64 producer = u->artifacts[action->consumes[ct]].producer;
    if (producer >= 0) {
      fz_eval_action(w, u, key_bytes, disk_bytes, done, misses, (u64)producer);
    }
  }
  sp_da_for(action->obs, ot) {
    fz_obs_t obs = action->obs[ot];
    if (obs.probe) {
      continue;
    }
    s64 producer = u->artifacts[obs.artifact].producer;
    if (producer >= 0 && (u64)producer != at) {
      fz_eval_action(w, u, key_bytes, disk_bytes, done, misses, (u64)producer);
    }
  }

  spn_dag_digest_t prelim = fz_model_key(u, key_bytes, at);
  spn_dag_digest_t key = prelim;
  bool resolved = true;
  if (action->discover) {
    fz_shape_t* stored = sp_ht_getp(w->pathsets, prelim);
    resolved = stored != SP_NULLPTR;
    if (stored) {
      sp_da_for(action->obs, ot) {
        fz_obs_t obs = action->obs[ot];
        if (obs.probe && stored->file[ot] && !u->phantoms[obs.phantom].present) {
          resolved = false;
          break;
        }
      }
    }
    if (resolved) {
      key = fz_model_strong(u, key_bytes, prelim, at, stored);
    }
  }

  fz_cached_t* cached = resolved ? sp_ht_getp(w->mirror, key) : SP_NULLPTR;
  if (cached) {
    misses[at] = false;
    sp_da_for(action->produces, pt) {
      u64 out = action->produces[pt];
      key_bytes[out] = cached->bytes[pt];
      disk_bytes[out] = cached->bytes[pt];
    }
    return;
  }

  misses[at] = true;
  sp_mem_t mem = w->mem;
  if (action->discover) {
    fz_shape_t shape = fz_shape_now(u, at);
    sp_ht_insert(w->pathsets, prelim, shape);
    key = fz_model_strong(u, key_bytes, prelim, at, &shape);
  }

  sp_str_t* inputs = SP_NULLPTR;
  u64 count = fz_action_inputs(mem, u, at, disk_bytes, &inputs);

  fz_cached_t entry = sp_zero;
  entry.count = sp_da_size(action->produces);
  sp_da_for(action->produces, pt) {
    u64 out = action->produces[pt];
    sp_str_t content = fz_output_content(mem, action->identity, inputs, count, fz_output_name(mem, out));
    key_bytes[out] = content;
    disk_bytes[out] = content;
    entry.bytes[pt] = content;
  }
  sp_ht_insert(w->mirror, key, entry);
}

static fz_err_t fz_trace_check_run(sp_mem_t mem, fz_universe_t* u, fz_world_t* w) {
  spn_dag_file_cache_refresh(&w->files);

  sp_da_for(u->artifacts, it) {
    if (u->artifacts[it].kind == FZ_ARTIFACT_SOURCE && w->dirty[it]) {
      w->believed[it] = u->artifacts[it].content;
      w->dirty[it] = false;
    }
  }

  fz_lowered_t low = sp_zero;
  fz_lower(&low, mem, u);
  low.ex = &w->ex;

  u64 artifacts = sp_da_size(u->artifacts);
  sp_str_t* key_bytes = sp_alloc_n(mem, sp_str_t, artifacts);
  sp_str_t* disk_bytes = sp_alloc_n(mem, sp_str_t, artifacts);
  sp_da_for(u->artifacts, it) {
    switch (u->artifacts[it].kind) {
      case FZ_ARTIFACT_VALUE: {
        key_bytes[it] = fz_content(mem, u->artifacts[it].content);
        disk_bytes[it] = key_bytes[it];
        break;
      }
      case FZ_ARTIFACT_SOURCE: {
        key_bytes[it] = fz_content(mem, w->believed[it]);
        disk_bytes[it] = fz_content(mem, u->artifacts[it].content);
        break;
      }
      case FZ_ARTIFACT_OUTPUT: {
        break;
      }
    }
  }

  bool done[FZ_MAX_ACTIONS] = sp_zero;
  bool misses[FZ_MAX_ACTIONS] = sp_zero;
  sp_da_for(u->actions, at) {
    fz_eval_action(w, u, key_bytes, disk_bytes, done, misses, at);
  }

  u64 log_start = sp_da_size(w->ex.log);
  must(!fz_world_run(w, u, low.g), FZ_ERR_RUN_FAILED);

  sp_da_for(u->actions, at) {
    u64 want = misses[at] ? 1 : 0;
    want += fz_action_requeues(u, w, mem, log_start, at);
    must(low.execs[at] >= want, FZ_ERR_EXEC_MISSING);
    must(low.execs[at] <= want, FZ_ERR_EXEC_SPURIOUS);
  }

  if (w->honest) {
    sp_str_t* clean = sp_alloc_n(mem, sp_str_t, artifacts);
    fz_expect(mem, u, clean);
    sp_da_for(u->artifacts, it) {
      if (u->artifacts[it].kind == FZ_ARTIFACT_OUTPUT) {
        must(sp_str_equal(clean[it], disk_bytes[it]), FZ_ERR_MODEL);
      }
    }
  }

  sp_da_for(u->artifacts, it) {
    if (u->artifacts[it].kind != FZ_ARTIFACT_OUTPUT) {
      continue;
    }
    sp_str_t disk = sp_zero;
    must(!sp_io_read_file(mem, fz_artifact_sim_path(mem, u, it), &disk), FZ_ERR_STALE_OUTPUT);
    must(sp_str_equal(disk, disk_bytes[it]), FZ_ERR_STALE_OUTPUT);
  }

  return FZ_OK;
}

static fz_err_t fz_trace_body(sp_mem_t mem, sp_sim_t* sim, fz_universe_t* u, fz_trace_t* trace, sp_fuzz_prng_t schedule, sp_str_t* final) {
  fz_world_t w = sp_zero;
  fz_world_init(&w, mem, sim, u, schedule);

  sp_da_for(u->artifacts, it) {
    if (u->artifacts[it].kind == FZ_ARTIFACT_SOURCE) {
      fz_write_source(mem, u, it);
      w.dirty[it] = true;
    }
  }

  if (u->cyclic || u->obs_cyclic) {
    fz_lowered_t low = sp_zero;
    fz_lower(&low, mem, u);
    low.ex = &w.ex;
    must(fz_world_run(&w, u, low.g), FZ_ERR_RUN_CYCLIC);
    return FZ_OK;
  }

  sp_da_for(trace->steps, st) {
    fz_step_t* step = &trace->steps[st];
    switch (step->kind) {
      case FZ_STEP_MUTATE:
      case FZ_STEP_REVERT: {
        u->artifacts[step->artifact].content = step->content;
        fz_write_source(mem, u, step->artifact);
        w.dirty[step->artifact] = true;
        break;
      }
      case FZ_STEP_TOUCH: {
        if (sp_sim_touch(w.sim, fz_artifact_sim_path(mem, u, step->artifact)) && u->artifacts[step->artifact].kind == FZ_ARTIFACT_SOURCE) {
          w.dirty[step->artifact] = true;
        }
        break;
      }
      case FZ_STEP_STEALTH: {
        fz_artifact_t* artifact = &u->artifacts[step->artifact];
        if (artifact->content != step->content) {
          artifact->content = step->content;
          bool wrote = sp_sim_stealth_write(w.sim, fz_artifact_sim_path(mem, u, step->artifact), fz_content(mem, step->content));
          sp_assert(wrote);
          w.honest = false;
        }
        break;
      }
      case FZ_STEP_DELETE: {
        sp_fs_remove_file(fz_artifact_sim_path(mem, u, step->artifact));
        break;
      }
      case FZ_STEP_PHANTOM: {
        fz_phantom_t* phantom = &u->phantoms[step->artifact];
        sp_str_t path = fz_phantom_sim_path(mem, step->artifact);
        if (phantom->present) {
          sp_fs_remove_file(path);
          phantom->present = false;
        }
        else {
          sp_fs_create_file_str(path, fz_content(mem, step->content));
          phantom->present = true;
          phantom->content = step->content;
        }
        break;
      }
      case FZ_STEP_DISCOVERY: {
        spn_dag_discovery_init(&w.discovery, mem, w.disco_dir);
        if (sp_str_empty(w.disco_dir)) {
          sp_ht_clear(w.pathsets);
        }
        break;
      }
      case FZ_STEP_RUN: {
        try(fz_trace_check_run(mem, u, &w));
        break;
      }
      case FZ_STEP_COUNT: {
        break;
      }
    }
  }

  if (final) {
    sp_da_for(u->artifacts, it) {
      if (u->artifacts[it].kind != FZ_ARTIFACT_OUTPUT) {
        continue;
      }
      sp_str_t disk = sp_zero;
      if (!sp_io_read_file(mem, fz_artifact_sim_path(mem, u, it), &disk)) {
        final[it] = disk;
      }
    }
  }

  return FZ_OK;
}

static fz_err_t fz_trace_pass(sp_mem_t mem, fz_universe_t* u, fz_trace_t* trace, sp_fuzz_prng_t schedule, sp_str_t* final) {
  sp_sim_t sim = sp_zero;
  sp_sim_init(&sim, mem);
  sp_sim_install(&sim);
  fz_err_t err = fz_trace_body(mem, &sim, u, trace, schedule, final);
  sp_sim_remove(&sim);
  return err;
}

fz_err_t fz_run_trace(sp_mem_t mem, sp_fuzz_prng_t* prng, fz_universe_t* u, fz_trace_t* trace) {
  sp_fuzz_prng_t schedule = { .state = sp_fuzz_next(prng) };
  sp_fuzz_prng_t reseed = { .state = sp_fuzz_next(prng) };

  u64 artifacts = sp_da_size(u->artifacts);
  u64* contents = sp_alloc_n(mem, u64, artifacts);
  sp_da_for(u->artifacts, it) {
    contents[it] = u->artifacts[it].content;
  }

  sp_str_t* final = sp_alloc_n(mem, sp_str_t, artifacts);
  sp_mem_zero(final, artifacts * sizeof(sp_str_t));
  try(fz_trace_pass(mem, u, trace, schedule, final));

  if (u->cyclic || u->obs_cyclic || !u->profile.run_ex) {
    return FZ_OK;
  }

  sp_da_for(u->artifacts, it) {
    u->artifacts[it].content = contents[it];
  }
  sp_mem_zero(u->phantoms, sizeof(u->phantoms));
  sp_str_t* reseeded = sp_alloc_n(mem, sp_str_t, artifacts);
  sp_mem_zero(reseeded, artifacts * sizeof(sp_str_t));
  try(fz_trace_pass(mem, u, trace, reseed, reseeded));

  sp_da_for(u->artifacts, it) {
    if (u->artifacts[it].kind != FZ_ARTIFACT_OUTPUT) {
      continue;
    }
    must(sp_str_equal(final[it], reseeded[it]), FZ_ERR_SCHEDULE);
  }

  return FZ_OK;
}
