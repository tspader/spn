#include "fuzz.h"
#include "sp_sim.h"
#include "sp/io.h"

typedef struct {
  u64 count;
  sp_str_t* bytes;
} fz_cached_t;

typedef struct {
  sp_mem_t mem;
  sp_sim_t* sim;
  spn_dag_store_t store;
  spn_dag_file_cache_t files;
  spn_dag_action_cache_t cache;
  spn_dag_discovery_t discovery;
  sp_str_t disco_dir;
  sp_str_t cache_dir;
  spn_dag_env_t env;
  fz_executor_t ex;
  sp_ht(spn_dag_digest_t, fz_cached_t) mirror;
  sp_ht(spn_dag_digest_t, fz_shape_t) pathsets;
  fz_state_t state;
  u64* believed;
  bool* dirty;
  u64 last_sys;
  fz_journal_t* j;
  fz_world_state_t world;
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

static spn_dag_job_t fz_executor_try_poll(spn_dag_executor_t* base) {
  fz_executor_t* ex = (fz_executor_t*)base;
  if (sp_da_empty(ex->jobs)) {
    return (spn_dag_job_t) sp_zero;
  }
  if (sp_fuzz_below(&ex->prng, 2)) {
    return (spn_dag_job_t) sp_zero;
  }
  return fz_executor_poll(base);
}

void fz_executor_init(fz_executor_t* ex, sp_mem_t mem, sp_sim_t* sim, sp_fuzz_prng_t prng) {
  *ex = (fz_executor_t) {
    .base = {
      .submit = fz_executor_submit,
      .poll = fz_executor_poll,
      .try_poll = fz_executor_try_poll,
    },
    .prng = prng,
    .sim = sim,
    .ran = -1,
  };
  sp_da_init(mem, ex->jobs);
  sp_da_init(mem, ex->submitted);
  sp_da_init(mem, ex->log);
}

static s32 fz_entry_order(const void* a, const void* b) {
  return sp_str_compare_alphabetical(((const sp_fs_entry_t*)a)->name, ((const sp_fs_entry_t*)b)->name);
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

static void init_world(fz_world_t* w, sp_mem_t mem, sp_sim_t* sim, fz_universe_t* u, sp_fuzz_prng_t schedule, fz_journal_t* j) {
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
  w->cache_dir = u->profile.cache_fs ? sp_str_lit("/cache") : sp_str_lit("");
  spn_dag_action_cache_init(&w->cache, mem, w->cache_dir);
  w->disco_dir = u->profile.disco_fs ? sp_str_lit("/manifests") : sp_str_lit("");
  spn_dag_discovery_init(&w->discovery, mem, w->disco_dir);
  w->env = (spn_dag_env_t) {
    .files = &w->files,
    .cache = &w->cache,
    .store = &w->store,
    .discovery = &w->discovery,
    .scratch = sp_str_lit("/scratch"),
  };
  w->j = j;
  if (j) {
    j->sim = sim;
    w->env.trace = fz_journal_trace_hook;
    w->env.trace_data = j;
  }
  fz_executor_init(&w->ex, mem, sim, schedule);
  sp_ht_init(mem, w->mirror);
  sp_ht_init(mem, w->pathsets);

  w->mem = mem;
  w->sim = sim;
  u64 artifacts = sp_da_size(u->artifacts);
  w->state.contents = sp_alloc_n(mem, u64, artifacts);
  sp_da_for(u->artifacts, it) {
    w->state.contents[it] = u->artifacts[it].content;
  }
  w->state.phantoms = sp_alloc_n(mem, fz_phantom_t, u->profile.limits.phantoms);
  sp_mem_zero(w->state.phantoms, u->profile.limits.phantoms * sizeof(fz_phantom_t));
  w->believed = sp_alloc_n(mem, u64, artifacts);
  w->dirty = sp_alloc_n(mem, bool, artifacts);
  sp_mem_zero(w->believed, artifacts * sizeof(u64));
  sp_mem_zero(w->dirty, artifacts * sizeof(bool));
  w->world = FZ_WORLD_CLEAN;
}

static spn_err_t run_world(fz_world_t* w, fz_universe_t* u, spn_dag_t* g) {
  if (u->profile.run_ex) {
    return spn_dag_run_executor(g, &w->env, &w->ex.base);
  }
  return spn_dag_run(g, &w->env);
}

static void mark_world(fz_world_t* w, fz_world_state_t mark) {
  if (w->world == FZ_WORLD_CLEAN) {
    w->world = mark;
  }
  else if (w->world != mark) {
    w->world = FZ_WORLD_TAINTED;
  }
  fz_journal_world(w->j, w->world);
}

static bool is_stealth(fz_world_t* w) {
  return w->world == FZ_WORLD_STEALTHY || w->world == FZ_WORLD_TAINTED;
}

static bool is_diverged(fz_world_t* w) {
  return w->world == FZ_WORLD_MURKY || w->world == FZ_WORLD_TAINTED;
}

static void reboot_world(fz_world_t* w, fz_universe_t* u) {
  spn_dag_store_init(&w->store, (spn_dag_store_config_t) {
    .kind = u->profile.store_fs ? SPN_DAG_STORE_FILESYSTEM : SPN_DAG_STORE_MEM,
    .mem = w->mem,
    .dir = sp_str_lit("/store"),
  });
  spn_dag_file_cache_init(&w->files, w->mem);
  spn_dag_action_cache_init(&w->cache, w->mem, w->cache_dir);
  spn_dag_discovery_init(&w->discovery, w->mem, w->disco_dir);
  if (sp_str_empty(w->disco_dir)) {
    sp_ht_clear(w->pathsets);
  }

  sp_da_for(u->artifacts, it) {
    if (u->artifacts[it].kind == FZ_ARTIFACT_SOURCE) {
      w->dirty[it] = true;
    }
  }
}

static void fz_write_source(sp_mem_t mem, fz_universe_t* u, fz_state_t* state, u64 artifact) {
  sp_fs_create_file_str(fz_artifact_sim_path(mem, u, artifact), fz_content(mem, state->contents[artifact]));
}

static void fz_eval_action(fz_world_t* w, fz_universe_t* u, sp_str_t* key_bytes, sp_str_t* disk_bytes, fz_predict_row_t* predict, u64 at) {
  fz_action_t* action = &u->actions[at];
  spn_dag_digest_t prelim = fz_model_key(u, key_bytes, at);
  spn_dag_digest_t key = prelim;
  bool resolved = true;
  if (action->discover) {
    fz_shape_t* stored = sp_ht_getp(w->pathsets, prelim);
    resolved = stored != SP_NULLPTR;
    if (stored) {
      sp_da_for(action->obs, ot) {
        fz_obs_t obs = action->obs[ot];
        if (obs.probe && stored->file[ot] && !w->state.phantoms[obs.phantom].present) {
          resolved = false;
          break;
        }
      }
    }
    if (resolved) {
      key = fz_model_strong(u, &w->state, key_bytes, prelim, at, stored);
    }
  }

  fz_cached_t* cached = resolved ? sp_ht_getp(w->mirror, key) : SP_NULLPTR;
  if (cached) {
    predict[at] = (fz_predict_row_t) { .action = at, .key = key, .resolved = resolved, .hit = true };
    sp_da_for(action->produces, pt) {
      u64 out = action->produces[pt];
      key_bytes[out] = cached->bytes[pt];
      disk_bytes[out] = cached->bytes[pt];
    }
    return;
  }

  sp_mem_t mem = w->mem;
  if (action->discover) {
    fz_shape_t shape = fz_shape_now(mem, u, &w->state, at);
    sp_ht_insert(w->pathsets, prelim, shape);
    key = fz_model_strong(u, &w->state, key_bytes, prelim, at, &shape);
  }
  predict[at] = (fz_predict_row_t) { .action = at, .key = key, .resolved = resolved, .hit = false };

  sp_str_t* inputs = SP_NULLPTR;
  u64 count = fz_action_inputs(mem, u, &w->state, at, disk_bytes, &inputs);

  fz_cached_t entry = sp_zero;
  entry.count = sp_da_size(action->produces);
  entry.bytes = sp_alloc_n(mem, sp_str_t, entry.count ? entry.count : 1);
  sp_da_for(action->produces, pt) {
    u64 out = action->produces[pt];
    sp_str_t content = fz_output_content(mem, action->identity, inputs, count, fz_output_name(mem, out));
    key_bytes[out] = content;
    disk_bytes[out] = content;
    entry.bytes[pt] = content;
  }
  sp_ht_insert(w->mirror, key, entry);
}

static fz_err_t fz_trace_check_run(sp_mem_t mem, fz_universe_t* u, fz_world_t* w, fz_step_t* step) {
  spn_dag_file_cache_invalidate_all(&w->files);

  sp_da_for(u->artifacts, it) {
    if (u->artifacts[it].kind == FZ_ARTIFACT_SOURCE && w->dirty[it]) {
      w->believed[it] = w->state.contents[it];
      w->dirty[it] = false;
    }
  }

  fz_lowered_t low = sp_zero;
  fz_lower(&low, mem, u);
  low.state = &w->state;
  low.ex = &w->ex;
  low.journal = w->j;

  u64 artifacts = sp_da_size(u->artifacts);
  sp_str_t* key_bytes = sp_alloc_n(mem, sp_str_t, artifacts);
  sp_str_t* disk_bytes = sp_alloc_n(mem, sp_str_t, artifacts);
  sp_da_for(u->artifacts, it) {
    switch (u->artifacts[it].kind) {
      case FZ_ARTIFACT_VALUE: {
        key_bytes[it] = fz_content(mem, w->state.contents[it]);
        disk_bytes[it] = key_bytes[it];
        break;
      }
      case FZ_ARTIFACT_SOURCE: {
        key_bytes[it] = fz_content(mem, w->believed[it]);
        disk_bytes[it] = fz_content(mem, w->state.contents[it]);
        break;
      }
      case FZ_ARTIFACT_OUTPUT: {
        break;
      }
    }
  }

  u64 actions = sp_da_size(u->actions);
  fz_predict_row_t* predict = sp_alloc_n(mem, fz_predict_row_t, actions ? actions : 1);
  sp_mem_zero(predict, actions * sizeof(fz_predict_row_t));
  sp_da_for(u->order, ot) {
    fz_eval_action(w, u, key_bytes, disk_bytes, predict, u->order[ot]);
  }
  fz_journal_predict(w->j, predict, actions);

  u64 log_start = sp_da_size(w->ex.log);
  switch (step->kind) {
    case FZ_STEP_EIO: {
      sp_sim_fault_eio(w->sim, step->artifact, step->content);
      break;
    }
    case FZ_STEP_CRASH: {
      sp_sim_fault_crash(w->sim, 1 + step->artifact % sp_max(w->last_sys, 256));
      break;
    }
    default: {
      break;
    }
  }

  u64 sys_start = w->sim->syscalls;
  spn_err_t err = run_world(w, u, low.g);
  u64 fired = step->kind == FZ_STEP_EIO ? w->sim->faults : 0;
  bool crashed = w->sim->crashed;
  sp_sim_fault_clear(w->sim);
  w->last_sys = w->sim->syscalls - sys_start;
  if (step->kind == FZ_STEP_EIO) {
    sp_da_for(w->sim->fault_log, ft) {
      fz_journal_sim_fault(w->j, w->sim->fault_log[ft]);
    }
  }
  fz_journal_run_done(w->j, (u64)err, fired, crashed);

  if (fired || crashed) {
    mark_world(w, FZ_WORLD_MURKY);
  }
  if (crashed) {
    sp_sim_crash_restore(w->sim);
    reboot_world(w, u);
    return FZ_OK;
  }
  if (fired && err) {
    return FZ_OK;
  }
  must(!err, FZ_ERR_RUN_FAILED);

  fz_exec_row_t* exec_rows = SP_NULLPTR;
  if (!is_diverged(w)) {
    exec_rows = sp_alloc_n(mem, fz_exec_row_t, actions ? actions : 1);
    sp_da_for(u->actions, at) {
      u64 requeues = fz_action_requeues(u, w, mem, log_start, at);
      u64 want = (predict[at].hit ? 0 : 1) + requeues;
      exec_rows[at] = (fz_exec_row_t) {
        .action = at,
        .execs = low.execs[at],
        .want = want,
        .requeues = requeues,
        .miss = !predict[at].hit,
        .ok = low.execs[at] == want,
      };
    }
    fz_journal_check_execs(w->j, exec_rows, actions);
  }

  if (w->world == FZ_WORLD_TAINTED) {
    return FZ_OK;
  }

  u64 outputs = 0;
  sp_da_for(u->artifacts, it) {
    if (u->artifacts[it].kind == FZ_ARTIFACT_OUTPUT) {
      outputs++;
    }
  }

  fz_bytes_row_t* model_rows = SP_NULLPTR;
  if (!is_stealth(w)) {
    sp_str_t* clean = sp_alloc_n(mem, sp_str_t, artifacts);
    fz_expect(mem, u, &w->state, clean);
    model_rows = sp_alloc_n(mem, fz_bytes_row_t, outputs ? outputs : 1);
    u64 row = 0;
    sp_da_for(u->artifacts, it) {
      if (u->artifacts[it].kind != FZ_ARTIFACT_OUTPUT) {
        continue;
      }
      model_rows[row++] = (fz_bytes_row_t) {
        .artifact = it,
        .want = clean[it],
        .got = disk_bytes[it],
        .ok = sp_str_equal(clean[it], disk_bytes[it]),
      };
    }
    fz_journal_check_bytes(w->j, sp_str_lit("model"), model_rows, outputs);
  }

  fz_bytes_row_t* disk_rows = sp_alloc_n(mem, fz_bytes_row_t, outputs ? outputs : 1);
  u64 row = 0;
  sp_da_for(u->artifacts, it) {
    if (u->artifacts[it].kind != FZ_ARTIFACT_OUTPUT) {
      continue;
    }
    sp_str_t disk = sp_zero;
    bool read = !sp_io_read_file(mem, fz_artifact_sim_path(mem, u, it), &disk);
    disk_rows[row++] = (fz_bytes_row_t) {
      .artifact = it,
      .want = disk_bytes[it],
      .got = read ? disk : sp_str_lit("missing"),
      .ok = read && sp_str_equal(disk, disk_bytes[it]),
    };
  }
  fz_journal_check_bytes(w->j, sp_str_lit("disk"), disk_rows, outputs);

  sp_for(it, outputs) {
    if (disk_rows[it].ok) {
      continue;
    }
    u64 id = disk_rows[it].artifact;
    sp_str_t path = fz_artifact_sim_path(mem, u, id);
    sp_da_for(w->sim->events, et) {
      if (sp_str_equal(w->sim->events[et].path, path)) {
        fz_journal_sim_write(w->j, id, path, w->sim->events[et].sys);
      }
    }
    spn_dag_digest_t want = spn_dag_digest(disk_bytes[id].data, disk_bytes[id].len);
    sp_str_t blob = spn_dag_store_path(&w->store, mem, want);
    sp_str_t blob_bytes = sp_zero;
    bool blob_read = !sp_str_empty(blob) && !sp_io_read_file(mem, blob, &blob_bytes);
    fz_journal_blob(w->j, id, disk_bytes[id], blob_read ? blob_bytes : sp_str_lit("missing"));
  }

  if (exec_rows) {
    sp_for(at, actions) {
      must(exec_rows[at].execs >= exec_rows[at].want, FZ_ERR_EXEC_MISSING);
      must(exec_rows[at].execs <= exec_rows[at].want, FZ_ERR_EXEC_SPURIOUS);
    }
  }
  if (model_rows) {
    sp_for(it, outputs) {
      must(model_rows[it].ok, FZ_ERR_MODEL);
    }
  }
  sp_for(it, outputs) {
    must(disk_rows[it].ok, FZ_ERR_STALE_OUTPUT);
  }

  return FZ_OK;
}

static fz_err_t fz_trace_body(sp_mem_t mem, sp_sim_t* sim, fz_universe_t* u, fz_trace_t* trace, sp_fuzz_prng_t schedule, sp_str_t* final, fz_journal_t* j) {
  fz_world_t w = sp_zero;
  init_world(&w, mem, sim, u, schedule, j);

  sp_da_for(u->artifacts, it) {
    if (u->artifacts[it].kind == FZ_ARTIFACT_SOURCE) {
      fz_write_source(mem, u, &w.state, it);
      w.dirty[it] = true;
    }
  }

  if (u->cyclic || u->obs_cyclic) {
    fz_lowered_t low = sp_zero;
    fz_lower(&low, mem, u);
    low.state = &w.state;
    low.ex = &w.ex;
    low.journal = w.j;
    must(run_world(&w, u, low.g), FZ_ERR_RUN_CYCLIC);
    return FZ_OK;
  }

  sp_da_for(trace->steps, st) {
    fz_step_t* step = &trace->steps[st];
    fz_journal_step(w.j, step, st);
    switch (step->kind) {
      case FZ_STEP_MUTATE:
      case FZ_STEP_REVERT: {
        w.state.contents[step->artifact] = step->content;
        fz_write_source(mem, u, &w.state, step->artifact);
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
        if (w.state.contents[step->artifact] != step->content) {
          w.state.contents[step->artifact] = step->content;
          bool wrote = sp_sim_stealth_write(w.sim, fz_artifact_sim_path(mem, u, step->artifact), fz_content(mem, step->content));
          sp_assert(wrote);
          mark_world(&w, FZ_WORLD_STEALTHY);
        }
        break;
      }
      case FZ_STEP_DELETE: {
        sp_fs_remove_file(fz_artifact_sim_path(mem, u, step->artifact));
        break;
      }
      case FZ_STEP_PHANTOM: {
        fz_phantom_t* phantom = &w.state.phantoms[step->artifact];
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
      case FZ_STEP_BLOB: {
        if (!u->profile.store_fs) {
          break;
        }
        sp_da(sp_fs_entry_t) entries = sp_fs_collect(mem, sp_str_lit("/store"));
        sp_da(sp_fs_entry_t) blobs = sp_da_new(mem, sp_fs_entry_t);
        sp_da_for(entries, et) {
          if (entries[et].kind == SP_FS_KIND_DIR) {
            sp_da_push(blobs, entries[et]);
          }
        }
        if (sp_da_empty(blobs)) {
          break;
        }
        sp_da_sort(blobs, fz_entry_order);
        sp_fs_entry_t* blob = &blobs[step->artifact % sp_da_size(blobs)];
        sp_da(sp_fs_entry_t) files = sp_fs_collect(mem, blob->path);
        sp_da_for(files, ft) {
          sp_fs_remove_file(files[ft].path);
        }
        sp_fs_remove_dir(blob->path);
        fz_journal_drop(w.j, sp_str_lit("blob"), blob->path);
        mark_world(&w, FZ_WORLD_MURKY);
        break;
      }
      case FZ_STEP_EVICT: {
        if (!u->profile.cache_fs) {
          break;
        }
        sp_da(sp_fs_entry_t) entries = sp_fs_collect(mem, w.cache_dir);
        sp_da(sp_fs_entry_t) cached = sp_da_new(mem, sp_fs_entry_t);
        sp_da_for(entries, et) {
          if (entries[et].kind == SP_FS_KIND_FILE) {
            sp_da_push(cached, entries[et]);
          }
        }
        if (sp_da_empty(cached)) {
          break;
        }
        sp_da_sort(cached, fz_entry_order);
        sp_str_t evicted = cached[step->artifact % sp_da_size(cached)].path;
        sp_fs_remove_file(evicted);
        spn_dag_action_cache_init(&w.cache, mem, w.cache_dir);
        fz_journal_drop(w.j, sp_str_lit("cache"), evicted);
        mark_world(&w, FZ_WORLD_MURKY);
        break;
      }
      case FZ_STEP_RUN:
      case FZ_STEP_EIO:
      case FZ_STEP_CRASH: {
        try(fz_trace_check_run(mem, u, &w, step));
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

static fz_err_t fz_trace_pass(sp_mem_t mem, fz_universe_t* u, fz_trace_t* trace, sp_fuzz_prng_t schedule, sp_str_t* final, fz_journal_t* j) {
  sp_sim_t sim = sp_zero;
  sp_sim_init(&sim, mem);
  sp_sim_install(&sim);
  fz_err_t err = fz_trace_body(mem, &sim, u, trace, schedule, final, j);
  sp_sim_remove(&sim);
  if (j) {
    j->sim = SP_NULLPTR;
  }
  return err;
}

fz_err_t fz_run_trace(sp_mem_t mem, sp_fuzz_prng_t* prng, fz_universe_t* u, fz_trace_t* trace, fz_journal_t* j) {
  sp_fuzz_prng_t schedule = { .state = sp_fuzz_next(prng) };
  sp_fuzz_prng_t reseed = { .state = sp_fuzz_next(prng) };

  u64 artifacts = sp_da_size(u->artifacts);
  sp_str_t* final = sp_alloc_n(mem, sp_str_t, artifacts);
  sp_mem_zero(final, artifacts * sizeof(sp_str_t));
  fz_journal_pass(j, sp_str_lit("main"));
  try(fz_trace_pass(mem, u, trace, schedule, final, j));

  bool faulted = false;
  sp_da_for(trace->steps, st) {
    if (trace->steps[st].kind == FZ_STEP_EIO || trace->steps[st].kind == FZ_STEP_CRASH) {
      faulted = true;
    }
  }
  if (u->cyclic || u->obs_cyclic || !u->profile.run_ex || faulted) {
    return FZ_OK;
  }

  sp_str_t* reseeded = sp_alloc_n(mem, sp_str_t, artifacts);
  sp_mem_zero(reseeded, artifacts * sizeof(sp_str_t));
  fz_journal_pass(j, sp_str_lit("reseed"));
  try(fz_trace_pass(mem, u, trace, reseed, reseeded, j));

  u64 outputs = 0;
  sp_da_for(u->artifacts, it) {
    if (u->artifacts[it].kind == FZ_ARTIFACT_OUTPUT) {
      outputs++;
    }
  }
  fz_bytes_row_t* rows = sp_alloc_n(mem, fz_bytes_row_t, outputs ? outputs : 1);
  u64 row = 0;
  sp_da_for(u->artifacts, it) {
    if (u->artifacts[it].kind != FZ_ARTIFACT_OUTPUT) {
      continue;
    }
    rows[row++] = (fz_bytes_row_t) {
      .artifact = it,
      .want = final[it],
      .got = reseeded[it],
      .ok = sp_str_equal(final[it], reseeded[it]),
    };
  }
  fz_journal_check_bytes(j, sp_str_lit("schedule"), rows, outputs);
  sp_for(it, outputs) {
    must(rows[it].ok, FZ_ERR_SCHEDULE);
  }

  return FZ_OK;
}
