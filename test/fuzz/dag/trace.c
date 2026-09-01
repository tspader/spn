#include "fuzz.h"
#include "sim/sim.h"
#include "dag/track.h"
#include "paths/paths.h"
#include "io/io.h"

typedef enum {
  MEMO_SURE,
  MEMO_VOLATILE,
  MEMO_UNKNOWN,
} memo_state_t;

typedef struct {
  sp_str_t* bytes;
  bool tainted;
  memo_state_t state;
} memo_t;

typedef struct {
  sp_mem_t mem;
  sp_sim_t* sim;
  spn_dag_store_t store;
  spn_dag_file_cache_t files;
  spn_dag_action_cache_t cache;
  spn_dag_obs_table_t discovery;
  sp_str_t disco_dir;
  sp_str_t cache_dir;
  spn_path_roots_t roots;
  spn_dag_env_t env;
  fz_executor_t ex;
  sp_dag_track_t track;
  sp_ht(spn_dag_digest_t, fz_shape_t) shapes;
  sp_ht(spn_dag_digest_t, memo_t) memo;
  sp_da(sp_dag_track_event_t) events;
  fz_state_t state;
  u64* believed;
  bool* dirty;
  bool* tainted;
  u64 last_sys;
  u64 runs;
  const fz_sweep_t* sweep;
  fz_journal_t* j;
  bool stealthy;
} world_t;

static void executor_submit(spn_thread_pool_executor_t* base, spn_thread_pool_job_t job) {
  fz_executor_t* ex = (fz_executor_t*)base;
  sp_da_push(ex->jobs, job);
}

static spn_thread_pool_job_t executor_poll(spn_thread_pool_executor_t* base) {
  fz_executor_t* ex = (fz_executor_t*)base;
  sp_assert(!sp_da_empty(ex->jobs));

  u64 pick = sp_fuzz_below(&ex->prng, sp_da_size(ex->jobs));
  spn_thread_pool_job_t job = ex->jobs[pick];
  ex->jobs[pick] = *sp_da_back(ex->jobs);
  sp_da_pop(ex->jobs);

  job.fn(job.data);
  return job;
}

static spn_thread_pool_job_t executor_try_poll(spn_thread_pool_executor_t* base) {
  fz_executor_t* ex = (fz_executor_t*)base;
  if (sp_da_empty(ex->jobs)) {
    return (spn_thread_pool_job_t) sp_zero;
  }
  if (sp_fuzz_below(&ex->prng, 2)) {
    return (spn_thread_pool_job_t) sp_zero;
  }
  return executor_poll(base);
}

void fz_executor_init(fz_executor_t* ex, sp_mem_t mem, sp_sim_t* sim, sp_fuzz_prng_t prng) {
  *ex = (fz_executor_t) {
    .base = {
      .submit = executor_submit,
      .poll = executor_poll,
      .try_poll = executor_try_poll,
    },
    .prng = prng,
    .sim = sim,
  };
  sp_da_init(mem, ex->jobs);
  sp_da_init(mem, ex->log);
}

static s32 digest_order(const void* a, const void* b) {
  return sp_sys_memcmp(a, b, sizeof(spn_dag_digest_t));
}

static sp_da(spn_dag_digest_t) table_digests(sp_mem_t mem, sp_dag_track_table_t table) {
  sp_da(spn_dag_digest_t) digests = sp_da_new(mem, spn_dag_digest_t);
  sp_ht_for_kv(table, it) {
    sp_da_push(digests, *it.key);
  }
  sp_da_sort(digests, digest_order);
  return digests;
}

static bool covered_write(fz_universe_t* u, world_t* w, sp_mem_t mem, u64 at, u64 lo, u64 hi) {
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

static u64 action_requeues(fz_universe_t* u, world_t* w, sp_mem_t mem, u64 log_start, u64 at) {
  fz_executor_t* ex = &w->ex;
  u64 requeues = 0;
  s64 prev = -1;
  for (u64 it = log_start; it < sp_da_size(ex->log); it++) {
    if (ex->log[it].action != at) {
      continue;
    }
    if (prev >= 0) {
      if (covered_write(u, w, mem, at, ex->log[prev].started, ex->log[it].started)) {
        requeues++;
      }
    }
    prev = (s64)it;
  }
  return requeues;
}

static void reset_discovery(world_t* w) {
  spn_dag_obs_table_init(&w->discovery, w->mem, &w->roots, w->disco_dir);
  sp_dag_track_reset_discovery(&w->track);
  if (sp_str_empty(w->disco_dir)) {
    sp_ht_clear(w->shapes);
  }
}

static void degrade_memo(world_t* w) {
  sp_ht_for_kv(w->memo, it) {
    if (it.val->state == MEMO_VOLATILE) {
      it.val->state = MEMO_UNKNOWN;
    }
  }
}

static void reboot_world(world_t* w, fz_universe_t* u) {
  spn_dag_store_init(&w->store, (spn_dag_store_config_t) {
    .kind = u->profile.store_fs ? SPN_DAG_STORE_FILESYSTEM : SPN_DAG_STORE_MEM,
    .mem = w->mem,
    .roots = &w->roots,
    .dir = { .root = SPN_PATH_ROOT_NONE, .sub = sp_str_lit("/store") },
  });
  spn_dag_file_cache_init(&w->files, w->mem, &w->roots);
  spn_dag_action_cache_init(&w->cache, w->mem, w->cache_dir);
  reset_discovery(w);
  sp_dag_track_reboot(&w->track);
  degrade_memo(w);

  sp_da_for(u->artifacts, it) {
    if (u->artifacts[it].kind == FZ_ARTIFACT_SOURCE) {
      w->dirty[it] = true;
    }
  }
}

static void world_trace_hook(const spn_dag_trace_event_t* event, void* user_data) {
  world_t* w = (world_t*)user_data;
  sp_da_push(w->events, ((sp_dag_track_event_t) { .event = *event, .sys = w->sim->syscalls }));
  if (w->j) {
    fz_journal_trace_hook(event, w->j);
  }
}

static void init_world(world_t* w, sp_mem_t mem, sp_sim_t* sim, fz_universe_t* u, sp_fuzz_prng_t schedule, const fz_sweep_t* sweep, fz_journal_t* j) {
  sp_fs_create_dir(sp_str_lit("/src"));
  sp_fs_create_dir(sp_str_lit("/out"));
  sp_fs_create_dir(sp_str_lit("/scratch"));
  sp_fs_create_dir(sp_str_lit("/gone"));

  w->mem = mem;
  w->sim = sim;
  fz_roots_init(&w->roots);
  w->cache_dir = u->profile.cache_fs ? sp_str_lit("/cache") : sp_str_lit("");
  w->disco_dir = u->profile.disco_fs ? sp_str_lit("/manifests") : sp_str_lit("");
  sp_ht_init(mem, w->shapes);
  sp_ht_init(mem, w->memo);
  sp_da_init(mem, w->events);
  sp_dag_track_init(&w->track, mem, (sp_dag_track_persist_t) {
    .entries = u->profile.cache_fs,
    .blobs = u->profile.store_fs,
    .pathsets = u->profile.disco_fs,
  });

  u64 artifacts = sp_da_size(u->artifacts);
  w->state.contents = sp_alloc_n(mem, u64, artifacts);
  sp_da_for(u->artifacts, it) {
    w->state.contents[it] = u->artifacts[it].content;
  }
  w->state.phantoms = sp_alloc_n(mem, fz_phantom_t, u->profile.limits.phantoms);
  sp_mem_zero(w->state.phantoms, u->profile.limits.phantoms * sizeof(fz_phantom_t));
  w->believed = sp_alloc_n(mem, u64, artifacts);
  w->dirty = sp_alloc_n(mem, bool, artifacts);
  w->tainted = sp_alloc_n(mem, bool, artifacts);
  w->sweep = sweep;

  reboot_world(w, u);

  w->env = (spn_dag_env_t) {
    .files = &w->files,
    .cache = &w->cache,
    .store = &w->store,
    .discovery = &w->discovery,
    .scratch = { .root = SPN_PATH_ROOT_NONE, .sub = sp_str_lit("/scratch") },
    .trace = world_trace_hook,
    .trace_data = w,
  };
  w->j = j;
  if (j) {
    j->sim = sim;
  }
  fz_executor_init(&w->ex, mem, sim, schedule);
}

static spn_err_t run_world(world_t* w, fz_universe_t* u, spn_dag_t* g) {
  if (u->profile.run_ex) {
    return spn_dag_run_executor(g, &w->env, &w->ex.base);
  }
  return spn_dag_run(g, &w->env);
}

static void write_source(sp_mem_t mem, fz_universe_t* u, fz_state_t* state, u64 artifact) {
  sp_fs_create_file_str(fz_artifact_sim_path(mem, u, artifact), fz_content(mem, state->contents[artifact]));
}

static void mark_unrecorded(world_t* w, sp_mem_t mem, fz_universe_t* u) {
  sp_da_for(u->artifacts, it) {
    if (u->artifacts[it].kind != FZ_ARTIFACT_SOURCE) {
      continue;
    }
    spn_path_t path = spn_path_make(&w->roots, fz_artifact_sim_path(mem, u, it));
    if (!spn_dag_file_cache_recorded(&w->files, path)) {
      w->dirty[it] = true;
    }
  }
}

static const memo_t* predict_restore(world_t* w, fz_universe_t* u, fz_predict_row_t* row) {
  sp_dag_track_slot_t entry = sp_dag_track_entry(&w->track, row->key);
  if (!entry.present) {
    return SP_NULLPTR;
  }
  if (!entry.sure) {
    row->certain = false;
    return SP_NULLPTR;
  }

  memo_t* memo = sp_ht_getp(w->memo, row->key);
  if (!memo || (memo->state == MEMO_UNKNOWN && w->stealthy)) {
    row->certain = false;
    return SP_NULLPTR;
  }

  bool missing = false;
  bool unsure = false;
  sp_da_for(u->actions[row->action].produces, pt) {
    sp_str_t bytes = memo->bytes[pt];
    spn_dag_digest_t digest = spn_dag_digest(bytes.data, bytes.len);
    sp_dag_track_slot_t blob = sp_dag_track_blob(&w->track, digest);
    missing = missing || !blob.present;
    unsure = unsure || (blob.present && !blob.sure);
  }
  if (missing) {
    return SP_NULLPTR;
  }
  if (unsure) {
    row->certain = false;
    return SP_NULLPTR;
  }
  row->hit = true;
  return memo;
}

static fz_predict_row_t model_action(world_t* w, fz_universe_t* u, sp_mem_t mem, sp_str_t* key_bytes, sp_str_t* disk_bytes, u64 at) {
  fz_action_t* action = &u->actions[at];
  fz_predict_row_t row = { .action = at, .certain = true, .keyed = true };

  bool fed_taint = false;
  sp_da_for(action->consumes, ct) {
    fed_taint = fed_taint || w->tainted[action->consumes[ct]];
  }
  sp_da_for(action->obs, ot) {
    fz_obs_t fo = action->obs[ot];
    fed_taint = fed_taint || (!fo.probe && w->tainted[fo.artifact]);
  }
  if (fed_taint) {
    row.keyed = false;
    row.certain = false;
  }

  sp_str_t* inputs = SP_NULLPTR;
  u64 count = fz_action_inputs(mem, u, &w->state, at, disk_bytes, &inputs);
  sp_da_for(action->produces, pt) {
    u64 out = action->produces[pt];
    sp_str_t content = fz_output_content(mem, action->identity, inputs, count, fz_output_name(mem, out));
    key_bytes[out] = content;
    disk_bytes[out] = content;
  }

  row.weak = fz_model_weak(mem, u, key_bytes, at);
  if (action->discover) {
    fz_shape_t fresh = fz_shape_now(mem, u, &w->state, at);
    spn_dag_obs_t* fresh_obs = SP_NULLPTR;
    spn_dag_digest_t* fresh_digests = SP_NULLPTR;
    u32 fresh_count = fz_model_obs(mem, u, &w->state, key_bytes, at, &fresh, &fresh_obs, &fresh_digests);
    row.commit = spn_dag_strong_key(row.weak, spn_dag_pinned_digest(w->roots.pinned, fresh_obs, fresh_count), fresh_obs, fresh_digests, fresh_count);

    sp_dag_track_slot_t pathset = sp_dag_track_pathset(&w->track, row.weak);
    if (pathset.present && !pathset.sure) {
      row.certain = false;
    }
    if (pathset.present && pathset.sure) {
      fz_shape_t* stored = sp_ht_getp(w->shapes, row.weak);
      sp_assert(stored);
      row.resolved = true;
      sp_da_for(action->obs, ot) {
        fz_obs_t fo = action->obs[ot];
        if (fo.probe && stored->file[ot] && !w->state.phantoms[fo.phantom].present) {
          row.resolved = false;
          break;
        }
      }
      if (row.resolved) {
        spn_dag_obs_t* stored_obs = SP_NULLPTR;
        spn_dag_digest_t* stored_digests = SP_NULLPTR;
        u32 stored_count = fz_model_obs(mem, u, &w->state, key_bytes, at, stored, &stored_obs, &stored_digests);
        row.key = spn_dag_strong_key(row.weak, spn_dag_pinned_digest(w->roots.pinned, stored_obs, stored_count), stored_obs, stored_digests, stored_count);
      }
    }
  }
  else {
    row.commit = row.weak;
    row.key = row.weak;
    row.resolved = true;
  }

  return row;
}

static void predict_action(world_t* w, fz_universe_t* u, sp_mem_t mem, sp_str_t* key_bytes, sp_str_t* disk_bytes, fz_predict_row_t* predict, u64 at) {
  fz_action_t* action = &u->actions[at];
  fz_predict_row_t row = model_action(w, u, mem, key_bytes, disk_bytes, at);

  bool taint = !row.keyed;
  const memo_t* memo = SP_NULLPTR;
  if (row.keyed && spn_dag_digest_valid(row.key)) {
    memo = predict_restore(w, u, &row);
  }
  if (memo) {
    sp_da_for(action->produces, pt) {
      u64 out = action->produces[pt];
      key_bytes[out] = memo->bytes[pt];
      disk_bytes[out] = memo->bytes[pt];
    }
    taint = taint || memo->tainted;
  }
  else {
    taint = taint || (!row.certain && w->stealthy);
  }

  sp_da_for(action->produces, pt) {
    w->tainted[action->produces[pt]] = taint;
  }
  predict[at] = row;
}

static void seed_bytes(world_t* w, fz_universe_t* u, sp_mem_t mem, sp_str_t* key_bytes, sp_str_t* disk_bytes) {
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
}

typedef struct {
  bool hit;
  bool recorded;
  u64 commit_sys;
  spn_dag_digest_t hit_key;
  spn_dag_digest_t commit_key;
} action_facts_t;

typedef struct {
  action_facts_t* actions;
  bool* settled;
  spn_dag_digest_t* settles;
} facts_t;

static void collect_facts(world_t* w, sp_mem_t mem, u64 actions, u64 artifacts, facts_t* facts) {
  facts->actions = sp_alloc_n(mem, action_facts_t, actions ? actions : 1);
  facts->settled = sp_alloc_n(mem, bool, artifacts ? artifacts : 1);
  facts->settles = sp_alloc_n(mem, spn_dag_digest_t, artifacts ? artifacts : 1);

  sp_da_for(w->events, et) {
    const spn_dag_trace_event_t* event = &w->events[et].event;
    action_facts_t* fact = &facts->actions[event->action.index];
    switch (event->kind) {
      case SPN_DAG_TRACE_CACHE: {
        if (event->present && event->hit) {
          fact->hit = true;
          fact->hit_key = event->key;
        }
        break;
      }
      case SPN_DAG_TRACE_COMMIT: {
        if (event->hit) {
          fact->recorded = true;
          fact->commit_key = event->key;
          fact->commit_sys = w->events[et].sys;
        }
        break;
      }
      case SPN_DAG_TRACE_SETTLE: {
        facts->settled[event->producer.index] = true;
        facts->settles[event->producer.index] = event->key;
        break;
      }
      default: {
        break;
      }
    }
  }
}

static void reconcile_action(world_t* w, fz_universe_t* u, sp_mem_t mem, sp_str_t* key_bytes, sp_str_t* disk_bytes, fz_predict_row_t* rows, const facts_t* facts, bool lossy, u64 crash_at, u64 at) {
  fz_action_t* action = &u->actions[at];
  const action_facts_t* fact = &facts->actions[at];
  fz_predict_row_t row = model_action(w, u, mem, key_bytes, disk_bytes, at);
  row.hit = fact->hit;

  bool taint = !row.keyed;
  if (fact->hit) {
    memo_t* memo = sp_ht_getp(w->memo, fact->hit_key);
    if (memo) {
      bool sure = memo->state != MEMO_UNKNOWN || !w->stealthy;
      if (!sure) {
        bool proven = true;
        bool refuted = false;
        sp_da_for(action->produces, pt) {
          u64 out = action->produces[pt];
          sp_str_t bytes = memo->bytes[pt];
          spn_dag_digest_t digest = spn_dag_digest(bytes.data, bytes.len);
          bool matched = facts->settled[out] && spn_dag_digest_equal(facts->settles[out], digest);
          proven = proven && matched;
          refuted = refuted || (facts->settled[out] && !matched);
        }
        if (proven) {
          memo->state = MEMO_SURE;
          sure = true;
        }
        if (refuted) {
          memo->tainted = true;
        }
      }
      sp_da_for(action->produces, pt) {
        u64 out = action->produces[pt];
        key_bytes[out] = memo->bytes[pt];
        disk_bytes[out] = memo->bytes[pt];
      }
      taint = taint || memo->tainted || !sure;
    }
    else {
      taint = true;
    }
  }

  if (fact->recorded && (!crash_at || fact->commit_sys <= crash_at)) {
    memo_t fresh = { .tainted = taint, .state = lossy ? MEMO_VOLATILE : MEMO_SURE };
    u64 produced = sp_da_size(action->produces);
    fresh.bytes = sp_alloc_n(w->mem, sp_str_t, produced ? produced : 1);
    sp_da_for(action->produces, pt) {
      fresh.bytes[pt] = sp_str_copy(w->mem, key_bytes[action->produces[pt]]);
    }
    sp_ht_insert(w->memo, fact->commit_key, fresh);
  }

  sp_da_for(action->produces, pt) {
    w->tainted[action->produces[pt]] = taint;
  }
  rows[at] = row;
}

static void resync(world_t* w, fz_universe_t* u, sp_mem_t mem, u64 crash_at, bool lossy) {
  sp_dag_track_run(&w->track, w->events, sp_da_size(w->events), crash_at, lossy);

  u64 actions = sp_da_size(u->actions);
  spn_dag_digest_t* weak = sp_alloc_n(mem, spn_dag_digest_t, actions ? actions : 1);
  bool* executed = sp_alloc_n(mem, bool, actions ? actions : 1);
  bool* put = sp_alloc_n(mem, bool, actions ? actions : 1);
  sp_da_for(w->events, et) {
    const spn_dag_trace_event_t* event = &w->events[et].event;
    u64 at = event->action.index;
    switch (event->kind) {
      case SPN_DAG_TRACE_KEY: {
        weak[at] = event->key;
        break;
      }
      case SPN_DAG_TRACE_EXECUTE: {
        executed[at] = true;
        break;
      }
      case SPN_DAG_TRACE_RESOLVE: {
        put[at] = put[at] || executed[at];
        break;
      }
      default: {
        break;
      }
    }
  }
  sp_for(at, actions) {
    if (!put[at] || !u->actions[at].discover) {
      continue;
    }
    sp_assert(spn_dag_digest_valid(weak[at]));
    sp_ht_insert(w->shapes, weak[at], fz_shape_now(w->mem, u, &w->state, at));
  }
}

static bool settle_final(world_t* w, u64 at) {
  const spn_dag_trace_event_t* event = &w->events[at].event;
  for (u64 it = at + 1; it < sp_da_size(w->events); it++) {
    const spn_dag_trace_event_t* later = &w->events[it].event;
    if (later->kind == SPN_DAG_TRACE_SETTLE && later->producer.index == event->producer.index) {
      return false;
    }
  }
  return true;
}

static u64 key_rows(world_t* w, sp_mem_t mem, const fz_predict_row_t* predict, const sp_str_t* disk_bytes, bool fired, fz_key_row_t** rows) {
  sp_da(fz_key_row_t) out = sp_da_new(mem, fz_key_row_t);
  sp_da_for(w->events, et) {
    const spn_dag_trace_event_t* event = &w->events[et].event;
    const fz_predict_row_t* row = &predict[event->action.index];
    fz_key_row_t key = { .action = event->action.index, .got = event->key };
    switch (event->kind) {
      case SPN_DAG_TRACE_KEY: {
        if (!row->keyed) {
          continue;
        }
        key.want = row->weak;
        break;
      }
      case SPN_DAG_TRACE_COMMIT: {
        if (!row->keyed) {
          continue;
        }
        key.want = event->hit ? row->commit : row->weak;
        break;
      }
      case SPN_DAG_TRACE_CACHE: {
        if (!row->keyed || (!row->certain && !spn_dag_digest_valid(row->key))) {
          continue;
        }
        key.want = row->key;
        break;
      }
      case SPN_DAG_TRACE_SETTLE: {
        if (w->tainted[event->producer.index]) {
          continue;
        }
        if (fired && !settle_final(w, et)) {
          continue;
        }
        sp_str_t bytes = disk_bytes[event->producer.index];
        key.want = spn_dag_digest(bytes.data, bytes.len);
        break;
      }
      default: {
        continue;
      }
    }
    key.ok = spn_dag_digest_equal(key.want, key.got);
    sp_da_push(out, key);
  }
  *rows = out;
  return sp_da_size(out);
}

static fz_err_t trace_check_run(sp_mem_t mem, fz_universe_t* u, world_t* w, fz_step_t* step) {
  spn_dag_file_cache_invalidate_all(&w->files);

  sp_da_for(u->artifacts, it) {
    if (u->artifacts[it].kind == FZ_ARTIFACT_SOURCE && w->dirty[it]) {
      w->believed[it] = w->state.contents[it];
      w->dirty[it] = false;
    }
  }

  fz_lowered_t low = sp_zero;
  fz_lower(&low, mem, u, &w->roots);
  low.state = &w->state;
  low.ex = &w->ex;
  low.journal = w->j;

  u64 artifacts = sp_da_size(u->artifacts);
  sp_str_t* key_bytes = sp_alloc_n(mem, sp_str_t, artifacts);
  sp_str_t* disk_bytes = sp_alloc_n(mem, sp_str_t, artifacts);
  seed_bytes(w, u, mem, key_bytes, disk_bytes);

  u64 actions = sp_da_size(u->actions);
  fz_predict_row_t* predict = sp_alloc_n(mem, fz_predict_row_t, actions ? actions : 1);
  sp_mem_zero(w->tainted, artifacts * sizeof(bool));
  sp_da_for(u->order, ot) {
    predict_action(w, u, mem, key_bytes, disk_bytes, predict, u->order[ot]);
  }
  fz_journal_model(w->j, sp_str_lit("predict"), predict, actions);

  u64 log_start = sp_da_size(w->ex.log);
  switch (step->kind) {
    case FZ_STEP_EIO: {
      sp_sim_fault_eio(w->sim, step->entropy, step->rate);
      break;
    }
    case FZ_STEP_CRASH: {
      sp_sim_fault_crash(w->sim, 1 + step->entropy % sp_max(w->last_sys, 256));
      break;
    }
    default: {
      break;
    }
  }
  if (w->sweep->run == w->runs) {
    switch (w->sweep->kind) {
      case FZ_SWEEP_AT: {
        sp_sim_fault_at(w->sim, w->sweep->nth);
        break;
      }
      case FZ_SWEEP_FROM: {
        sp_sim_fault_from(w->sim, w->sweep->nth);
        break;
      }
      case FZ_SWEEP_NONE: {
        break;
      }
    }
  }

  sp_da_clear(w->events);
  u64 sys_start = w->sim->syscalls;
  u64 points_start = w->sim->fail_points;
  u64 faults_start = w->sim->faults;
  u64 fault_log_start = sp_da_size(w->sim->fault_log);
  spn_err_t err = run_world(w, u, low.g);
  u64 fired = w->sim->faults - faults_start;
  bool crashed = w->sim->crashed;
  u64 crash_at = crashed ? w->sim->crash_at : 0;
  sp_sim_fault_clear(w->sim);
  w->last_sys = w->sim->syscalls - sys_start;
  if (w->sweep->windows) {
    sp_da_push(*w->sweep->windows, w->sim->fail_points - points_start);
  }
  for (u64 ft = fault_log_start; ft < sp_da_size(w->sim->fault_log); ft++) {
    fz_journal_sim_fault(w->j, w->sim->fault_log[ft]);
  }
  fz_journal_run_done(w->j, (u64)err, fired, crashed);
  mark_unrecorded(w, mem, u);

  facts_t facts = sp_zero;
  collect_facts(w, mem, actions, artifacts, &facts);
  sp_str_t* actual_keys = sp_alloc_n(mem, sp_str_t, artifacts);
  sp_str_t* actual_disk = sp_alloc_n(mem, sp_str_t, artifacts);
  seed_bytes(w, u, mem, actual_keys, actual_disk);
  fz_predict_row_t* reconciled = sp_alloc_n(mem, fz_predict_row_t, actions ? actions : 1);
  sp_mem_zero(w->tainted, artifacts * sizeof(bool));
  sp_da_for(u->order, ot) {
    reconcile_action(w, u, mem, actual_keys, actual_disk, reconciled, &facts, fired > 0, crash_at, u->order[ot]);
  }
  fz_journal_model(w->j, sp_str_lit("reconcile"), reconciled, actions);

  resync(w, u, mem, crash_at, fired > 0);
  w->runs++;

  if (crashed) {
    sp_sim_crash_restore(w->sim);
    reboot_world(w, u);
    return FZ_OK;
  }
  if (fired && err) {
    return FZ_OK;
  }
  must(!err, FZ_ERR_RUN_FAILED);

  fz_key_row_t* keys = SP_NULLPTR;
  u64 key_count = key_rows(w, mem, reconciled, actual_disk, fired > 0, &keys);
  fz_journal_check_keys(w->j, keys, key_count);

  fz_exec_row_t* exec_rows = sp_alloc_n(mem, fz_exec_row_t, actions ? actions : 1);
  sp_da_for(u->actions, at) {
    u64 requeues = action_requeues(u, w, mem, log_start, at);
    u64 want = (predict[at].hit ? 0 : 1) + requeues;
    bool enough = low.execs[at] >= want;
    bool exact = enough && (fired || low.execs[at] == want);
    exec_rows[at] = (fz_exec_row_t) {
      .action = at,
      .execs = low.execs[at],
      .want = want,
      .requeues = requeues,
      .miss = !predict[at].hit,
      .ok = exact,
    };
  }
  fz_journal_check_execs(w->j, exec_rows, actions);

  u64 outputs = sp_da_size(u->outputs);
  fz_bytes_row_t* disk_rows = sp_alloc_n(mem, fz_bytes_row_t, outputs ? outputs : 1);
  sp_da_for(u->outputs, ot) {
    u64 it = u->outputs[ot];
    sp_str_t disk = sp_zero;
    bool read = !sp_io_read_file(mem, fz_artifact_sim_path(mem, u, it), &disk);
    disk_rows[ot] = (fz_bytes_row_t) {
      .artifact = it,
      .want = actual_disk[it],
      .got = read ? disk : sp_str_lit("missing"),
      .ok = w->tainted[it] || (read && sp_str_equal(disk, actual_disk[it])),
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
    spn_dag_digest_t want = spn_dag_digest(actual_disk[id].data, actual_disk[id].len);
    spn_path_t blob = spn_dag_store_path(&w->store, mem, want, path);
    sp_str_t blob_bytes = sp_zero;
    bool blob_read = !spn_path_empty(blob) && !sp_io_read_file(mem, spn_path_str(&w->roots, mem, blob), &blob_bytes);
    fz_journal_blob(w->j, id, actual_disk[id], blob_read ? blob_bytes : sp_str_lit("missing"));
  }

  sp_for(it, key_count) {
    must(keys[it].ok, FZ_ERR_KEY);
  }
  sp_for(at, actions) {
    if (!predict[at].certain) {
      continue;
    }
    must(exec_rows[at].execs >= exec_rows[at].want, FZ_ERR_EXEC_MISSING);
    must(exec_rows[at].ok, FZ_ERR_EXEC_SPURIOUS);
  }
  sp_for(it, outputs) {
    must(disk_rows[it].ok, FZ_ERR_STALE_OUTPUT);
  }

  return FZ_OK;
}

static fz_err_t trace_body(sp_mem_t mem, sp_sim_t* sim, fz_universe_t* u, fz_trace_t* trace, sp_fuzz_prng_t schedule, const fz_sweep_t* sweep, sp_str_t* final, fz_journal_t* j) {
  world_t w = sp_zero;
  init_world(&w, mem, sim, u, schedule, sweep, j);

  sp_da_for(u->artifacts, it) {
    if (u->artifacts[it].kind == FZ_ARTIFACT_SOURCE) {
      write_source(mem, u, &w.state, it);
      w.dirty[it] = true;
    }
  }

  if (u->cyclic || u->obs_cyclic) {
    fz_lowered_t low = sp_zero;
    fz_lower(&low, mem, u, &w.roots);
    low.state = &w.state;
    low.ex = &w.ex;
    low.journal = w.j;
    must(run_world(&w, u, low.g), FZ_ERR_RUN_CYCLIC);
    return FZ_OK;
  }

  sp_da_for(trace->steps, st) {
    fz_step_t* step = &trace->steps[st];
    sp_sim_advance(w.sim, step->tick);
    fz_journal_step(w.j, step, st);
    switch (step->kind) {
      case FZ_STEP_MUTATE:
      case FZ_STEP_REVERT: {
        w.state.contents[step->artifact] = step->content;
        write_source(mem, u, &w.state, step->artifact);
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
          w.stealthy = true;
          fz_journal_stealth(w.j);
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
        reset_discovery(&w);
        break;
      }
      case FZ_STEP_BLOB: {
        if (!u->profile.store_fs) {
          break;
        }
        sp_da(spn_dag_digest_t) blobs = table_digests(mem, w.track.blobs);
        if (sp_da_empty(blobs)) {
          break;
        }
        spn_dag_digest_t digest = blobs[step->entropy % sp_da_size(blobs)];
        sp_str_t dir = sp_fmt(mem, "/store/{}", sp_fmt_str(spn_dag_digest_hex(mem, digest))).value;
        sp_da(sp_fs_entry_t) entries = sp_zero;
        sp_fs_collect(mem, dir, &entries);
        sp_da_for(entries, et) {
          sp_fs_remove_file(entries[et].path);
          fz_journal_drop(w.j, sp_str_lit("blob"), entries[et].path);
        }
        sp_dag_track_drop_blob(&w.track, digest);
        break;
      }
      case FZ_STEP_EVICT: {
        if (!u->profile.cache_fs) {
          break;
        }
        sp_da(spn_dag_digest_t) keys = table_digests(mem, w.track.entries);
        if (sp_da_empty(keys)) {
          break;
        }
        spn_dag_digest_t key = keys[step->entropy % sp_da_size(keys)];
        sp_str_t evicted = sp_fs_join_path(mem, w.cache_dir, sp_fmt(mem, "{}.txt", sp_fmt_str(spn_dag_digest_hex(mem, key))).value);
        sp_fs_remove_file(evicted);
        spn_dag_action_cache_init(&w.cache, mem, w.cache_dir);
        sp_dag_track_reset_entries(&w.track);
        degrade_memo(&w);
        sp_dag_track_drop_entry(&w.track, key);
        fz_journal_drop(w.j, sp_str_lit("cache"), evicted);
        break;
      }
      case FZ_STEP_RUN:
      case FZ_STEP_EIO:
      case FZ_STEP_CRASH: {
        try(trace_check_run(mem, u, &w, step));
        break;
      }
      case FZ_STEP_COUNT: {
        break;
      }
    }
  }

  sp_da_for(u->outputs, ot) {
    u64 it = u->outputs[ot];
    sp_str_t disk = sp_zero;
    if (!sp_io_read_file(mem, fz_artifact_sim_path(mem, u, it), &disk)) {
      final[it] = disk;
    }
  }

  return FZ_OK;
}

static fz_err_t trace_pass(sp_mem_t mem, fz_universe_t* u, fz_trace_t* trace, sp_fuzz_prng_t schedule, const fz_sweep_t* sweep, sp_str_t* final, fz_journal_t* j) {
  sp_sim_t sim = sp_zero;
  sp_sim_init(&sim, mem);
  sim.granularity = u->profile.granularity;
  sim.autotick = 0;
  sp_sim_install(&sim);
  fz_err_t err = trace_body(mem, &sim, u, trace, schedule, sweep, final, j);
  sp_sim_remove(&sim);
  if (j) {
    j->sim = SP_NULLPTR;
  }
  return err;
}

fz_err_t fz_run_trace(sp_mem_t mem, sp_fuzz_prng_t* prng, fz_universe_t* u, fz_trace_t* trace, fz_journal_t* j) {
  sp_fuzz_prng_t schedule = { .state = sp_fuzz_next(prng) };
  sp_fuzz_prng_t reseed = { .state = sp_fuzz_next(prng) };
  fz_sweep_t none = sp_zero;

  u64 artifacts = sp_da_size(u->artifacts);
  sp_str_t* final = sp_alloc_n(mem, sp_str_t, artifacts);
  fz_journal_pass(j, sp_str_lit("main"));
  try(trace_pass(mem, u, trace, schedule, &none, final, j));

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
  fz_journal_pass(j, sp_str_lit("reseed"));
  try(trace_pass(mem, u, trace, reseed, &none, reseeded, j));

  u64 outputs = sp_da_size(u->outputs);
  fz_bytes_row_t* rows = sp_alloc_n(mem, fz_bytes_row_t, outputs ? outputs : 1);
  sp_da_for(u->outputs, ot) {
    u64 it = u->outputs[ot];
    rows[ot] = (fz_bytes_row_t) {
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

fz_err_t fz_run_sweep(sp_mem_t mem, sp_fuzz_prng_t* prng, fz_universe_t* u, fz_trace_t* trace, fz_journal_t* j) {
  sp_fuzz_prng_t schedule = { .state = sp_fuzz_next(prng) };

  u64 artifacts = sp_da_size(u->artifacts);
  sp_str_t* final = sp_alloc_n(mem, sp_str_t, artifacts);

  sp_da(u64) windows = sp_da_new(mem, u64);
  fz_sweep_t counting = { .windows = &windows };
  fz_journal_pass(j, sp_str_lit("main"));
  try(trace_pass(mem, u, trace, schedule, &counting, final, j));

  static const fz_sweep_kind_t kinds [] = { FZ_SWEEP_AT, FZ_SWEEP_FROM };
  sp_da_for(windows, run) {
    for (u64 nth = 1; nth <= windows[run]; nth++) {
      sp_carr_for(kinds, kt) {
        fz_sweep_t sweep = { .kind = kinds[kt], .run = run, .nth = nth };
        fz_err_t err = trace_pass(mem, u, trace, schedule, &sweep, final, SP_NULLPTR);
        if (err && j) {
          fz_journal_sweep(j, &sweep);
          trace_pass(mem, u, trace, schedule, &sweep, final, j);
        }
        try(err);
      }
    }
  }

  return FZ_OK;
}
