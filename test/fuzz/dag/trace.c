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
  spn_dag_env_t env;
  sp_ht(spn_dag_digest_t, fz_cached_t) mirror;
  u64* believed;
  bool* dirty;
  bool honest;
} fz_world_t;

static void fz_world_init(fz_world_t* w, sp_mem_t mem, sp_sim_t* sim, fz_universe_t* u) {
  sp_fs_create_dir(sp_str_lit("/src"));
  sp_fs_create_dir(sp_str_lit("/out"));
  sp_fs_create_dir(sp_str_lit("/scratch"));

  spn_dag_store_init(&w->store, (spn_dag_store_config_t) {
    .kind = u->profile.store_fs ? SPN_DAG_STORE_FILESYSTEM : SPN_DAG_STORE_MEM,
    .mem = mem,
    .dir = sp_str_lit("/store"),
  });
  spn_dag_file_cache_init(&w->files, mem);
  spn_dag_action_cache_init(&w->cache, mem, sp_str_lit(""));
  w->env = (spn_dag_env_t) {
    .files = &w->files,
    .cache = &w->cache,
    .store = &w->store,
    .scratch = sp_str_lit("/scratch"),
  };
  sp_ht_init(mem, w->mirror);

  w->mem = mem;
  w->sim = sim;
  u64 artifacts = sp_da_size(u->artifacts);
  w->believed = sp_alloc_n(mem, u64, artifacts);
  w->dirty = sp_alloc_n(mem, bool, artifacts);
  sp_mem_zero(w->believed, artifacts * sizeof(u64));
  sp_mem_zero(w->dirty, artifacts * sizeof(bool));
  w->honest = true;
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

  spn_dag_digest_t key = fz_model_key(u, key_bytes, at);
  fz_cached_t* cached = sp_ht_getp(w->mirror, key);
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
  u64 count = sp_da_size(action->consumes);
  sp_str_t* inputs = sp_alloc_n(mem, sp_str_t, count);
  sp_da_for(action->consumes, ct) {
    inputs[ct] = disk_bytes[action->consumes[ct]];
  }

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

  must(!spn_dag_run(low.g, &w->env), FZ_ERR_RUN_FAILED);

  sp_da_for(u->actions, at) {
    u64 want = misses[at] ? 1 : 0;
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

static fz_err_t fz_trace_body(sp_mem_t mem, sp_sim_t* sim, fz_universe_t* u, fz_trace_t* trace) {
  fz_world_t w = sp_zero;
  fz_world_init(&w, mem, sim, u);

  sp_da_for(u->artifacts, it) {
    if (u->artifacts[it].kind == FZ_ARTIFACT_SOURCE) {
      fz_write_source(mem, u, it);
      w.dirty[it] = true;
    }
  }

  if (u->cyclic) {
    fz_lowered_t low = sp_zero;
    fz_lower(&low, mem, u);
    must(spn_dag_run(low.g, &w.env), FZ_ERR_RUN_CYCLIC);
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
      case FZ_STEP_RUN: {
        try(fz_trace_check_run(mem, u, &w));
        break;
      }
      case FZ_STEP_COUNT: {
        break;
      }
    }
  }

  return FZ_OK;
}

fz_err_t fz_run_trace(sp_mem_t mem, fz_universe_t* u, fz_trace_t* trace) {
  sp_da_for(u->actions, at) {
    if (u->actions[at].discover) {
      return FZ_OK;
    }
  }

  sp_sim_t sim = sp_zero;
  sp_sim_init(&sim, mem);
  sp_sim_install(&sim);
  fz_err_t err = fz_trace_body(mem, &sim, u, trace);
  sp_sim_remove(&sim);
  return err;
}
