#include "fuzz.h"
#include "sp_sim.h"
#include "sp/io.h"

typedef struct {
  spn_dag_store_t store;
  spn_dag_file_cache_t files;
  spn_dag_action_cache_t cache;
  spn_dag_env_t env;
  sp_ht(spn_dag_digest_t, bool) mirror;
} fz_world_t;

static void fz_world_init(fz_world_t* w, sp_mem_t mem, fz_profile_t* profile) {
  sp_fs_create_dir(sp_str_lit("/src"));
  sp_fs_create_dir(sp_str_lit("/out"));
  sp_fs_create_dir(sp_str_lit("/scratch"));

  spn_dag_store_init(&w->store, (spn_dag_store_config_t) {
    .kind = profile->store_fs ? SPN_DAG_STORE_FILESYSTEM : SPN_DAG_STORE_MEM,
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
}

static void fz_write_source(sp_mem_t mem, fz_universe_t* u, u64 artifact) {
  sp_fs_create_file_str(fz_artifact_sim_path(mem, u, artifact), fz_content(mem, u->artifacts[artifact].content));
}

static fz_err_t fz_trace_check_run(sp_mem_t mem, fz_universe_t* u, fz_world_t* w) {
  spn_dag_file_cache_refresh(&w->files);

  fz_lowered_t low = sp_zero;
  fz_lower(&low, mem, u);

  sp_str_t* bytes = sp_alloc_n(mem, sp_str_t, sp_da_size(u->artifacts));
  fz_expect(mem, u, bytes);

  u64 n = sp_da_size(u->actions);
  spn_dag_digest_t* keys = sp_alloc_n(mem, spn_dag_digest_t, n);
  bool* misses = sp_alloc_n(mem, bool, n);
  sp_da_for(u->actions, at) {
    keys[at] = fz_model_key(u, bytes, at);
    misses[at] = !sp_ht_getp(w->mirror, keys[at]);
  }

  must(!spn_dag_run(low.g, &w->env), FZ_ERR_RUN_FAILED);

  sp_da_for(u->actions, at) {
    u64 want = misses[at] ? 1 : 0;
    must(low.execs[at] >= want, FZ_ERR_EXEC_MISSING);
    must(low.execs[at] <= want, FZ_ERR_EXEC_SPURIOUS);
    if (misses[at]) {
      sp_ht_insert(w->mirror, keys[at], true);
    }
  }

  sp_da_for(u->artifacts, it) {
    if (u->artifacts[it].kind != FZ_ARTIFACT_OUTPUT) {
      continue;
    }
    sp_str_t disk = sp_zero;
    must(!sp_io_read_file(mem, fz_artifact_sim_path(mem, u, it), &disk), FZ_ERR_STALE_OUTPUT);
    must(sp_str_equal(disk, bytes[it]), FZ_ERR_STALE_OUTPUT);
  }

  return FZ_OK;
}

static fz_err_t fz_trace_body(sp_mem_t mem, fz_universe_t* u, fz_trace_t* trace) {
  fz_world_t w = sp_zero;
  fz_world_init(&w, mem, &u->profile);

  sp_da_for(u->artifacts, it) {
    if (u->artifacts[it].kind == FZ_ARTIFACT_SOURCE) {
      fz_write_source(mem, u, it);
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
      case FZ_STEP_MUTATE: {
        u->artifacts[step->artifact].content = step->content;
        fz_write_source(mem, u, step->artifact);
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
  fz_err_t err = fz_trace_body(mem, u, trace);
  sp_sim_remove(&sim);
  return err;
}
