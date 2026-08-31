#include "fuzz.h"
#include "sp.h"
#include "spn/core.h"
#include "io/io.h"

void fz_journal_init(fz_journal_t* j, sp_mem_t mem) {
  j->mem = mem;
  j->sim = SP_NULLPTR;
  sp_da_init(mem, j->lines);
}

static void emit(fz_journal_t* j, sp_str_t body) {
  sp_da_push(j->lines, sp_fmt(j->mem, "{{{}}}", sp_fmt_str(body)).value);
}

static u64 sys_now(fz_journal_t* j) {
  return j->sim ? j->sim->syscalls : 0;
}

static sp_str_t json_bool(bool value) {
  return value ? sp_str_lit("true") : sp_str_lit("false");
}

static sp_str_t json_key(sp_mem_t mem, spn_dag_digest_t key) {
  return sp_str_prefix(spn_dag_digest_hex(mem, key), 16);
}

static sp_str_t json_bytes(sp_str_t bytes) {
  return bytes.len > 16 ? sp_str_prefix(bytes, 16) : bytes;
}

static sp_str_t json_list(sp_mem_t mem, sp_str_t* items, u32 count) {
  return sp_fmt(mem, "[{}]", sp_fmt_str(sp_str_join_n(mem, items, count, sp_str_lit(",")))).value;
}

static sp_str_t json_ids(sp_mem_t mem, sp_da(u64) ids) {
  sp_str_t* items = sp_alloc_n(mem, sp_str_t, sp_da_size(ids) ? sp_da_size(ids) : 1);
  sp_da_for(ids, it) {
    items[it] = sp_fmt(mem, "{}", sp_fmt_uint(ids[it])).value;
  }
  return json_list(mem, items, (u32)sp_da_size(ids));
}

static sp_str_t json_obs(sp_mem_t mem, sp_da(fz_obs_t) obs) {
  sp_str_t* items = sp_alloc_n(mem, sp_str_t, sp_da_size(obs) ? sp_da_size(obs) : 1);
  sp_da_for(obs, it) {
    items[it] = obs[it].probe
      ? sp_fmt(mem, "\"g{}\"", sp_fmt_uint(obs[it].phantom)).value
      : sp_fmt(mem, "\"f{}\"", sp_fmt_uint(obs[it].artifact)).value;
  }
  return json_list(mem, items, (u32)sp_da_size(obs));
}

static sp_str_t step_kind_str(fz_step_kind_t kind) {
  switch (kind) {
    case FZ_STEP_RUN:       return sp_str_lit("run");
    case FZ_STEP_MUTATE:    return sp_str_lit("mutate");
    case FZ_STEP_TOUCH:     return sp_str_lit("touch");
    case FZ_STEP_REVERT:    return sp_str_lit("revert");
    case FZ_STEP_STEALTH:   return sp_str_lit("stealth");
    case FZ_STEP_DELETE:    return sp_str_lit("delete");
    case FZ_STEP_PHANTOM:   return sp_str_lit("phantom");
    case FZ_STEP_DISCOVERY: return sp_str_lit("discovery");
    case FZ_STEP_EIO:       return sp_str_lit("eio");
    case FZ_STEP_CRASH:     return sp_str_lit("crash");
    case FZ_STEP_BLOB:      return sp_str_lit("blob");
    case FZ_STEP_EVICT:     return sp_str_lit("evict");
    case FZ_STEP_COUNT:     break;
  }
  sp_unreachable_return(sp_str_lit("unknown"));
}

void fz_journal_universe(fz_journal_t* j, fz_universe_t* u, fz_trace_t* trace, u64 iter) {
  if (!j) return;
  sp_mem_t mem = j->mem;
  fz_profile_t* profile = &u->profile;

  emit(j, sp_fmt(mem,
    "\"ev\":\"meta\",\"iter\":{},\"seed\":\"0x{:x}\",\"cyclic\":{},\"obs_cyclic\":{},\"run_ex\":{},\"store_fs\":{},\"disco_fs\":{},\"cache_fs\":{},\"big\":{},\"granularity\":{}",
    sp_fmt_uint(iter), sp_fmt_uint(sp_fuzz_seed_get()),
    sp_fmt_str(json_bool(u->cyclic)), sp_fmt_str(json_bool(u->obs_cyclic)),
    sp_fmt_str(json_bool(profile->run_ex)), sp_fmt_str(json_bool(profile->store_fs)),
    sp_fmt_str(json_bool(profile->disco_fs)), sp_fmt_str(json_bool(profile->cache_fs)),
    sp_fmt_str(json_bool(profile->big)), sp_fmt_uint(profile->granularity)).value);

  sp_da_for(u->artifacts, it) {
    fz_artifact_t* artifact = &u->artifacts[it];
    switch (artifact->kind) {
      case FZ_ARTIFACT_VALUE: {
        emit(j, sp_fmt(mem, "\"ev\":\"artifact\",\"id\":{},\"kind\":\"value\",\"content\":{}",
          sp_fmt_uint(it), sp_fmt_uint(artifact->content)).value);
        break;
      }
      case FZ_ARTIFACT_SOURCE: {
        emit(j, sp_fmt(mem, "\"ev\":\"artifact\",\"id\":{},\"kind\":\"source\",\"content\":{},\"path\":\"{}\"",
          sp_fmt_uint(it), sp_fmt_uint(artifact->content), sp_fmt_str(fz_artifact_sim_path(mem, u, it))).value);
        break;
      }
      case FZ_ARTIFACT_OUTPUT: {
        emit(j, sp_fmt(mem, "\"ev\":\"artifact\",\"id\":{},\"kind\":\"output\",\"producer\":{},\"path\":\"{}\"",
          sp_fmt_uint(it), sp_fmt_uint((u64)artifact->producer), sp_fmt_str(fz_artifact_sim_path(mem, u, it))).value);
        break;
      }
    }
  }

  sp_da_for(u->actions, at) {
    fz_action_t* action = &u->actions[at];
    emit(j, sp_fmt(mem,
      "\"ev\":\"action\",\"id\":{},\"identity\":{},\"discover\":{},\"consumes\":{},\"produces\":{},\"obs\":{}",
      sp_fmt_uint(at), sp_fmt_uint(action->identity), sp_fmt_str(json_bool(action->discover)),
      sp_fmt_str(json_ids(mem, action->consumes)), sp_fmt_str(json_ids(mem, action->produces)),
      sp_fmt_str(json_obs(mem, action->obs))).value);
  }

  sp_da_for(trace->steps, st) {
    fz_step_t* step = &trace->steps[st];
    emit(j, sp_fmt(mem, "\"ev\":\"plan\",\"i\":{},\"kind\":\"{}\",\"artifact\":{},\"content\":{},\"rate\":{},\"tick\":{}",
      sp_fmt_uint(st), sp_fmt_str(step_kind_str(step->kind)), sp_fmt_uint(step->artifact), sp_fmt_uint(step->content), sp_fmt_uint(step->rate), sp_fmt_uint(step->tick)).value);
  }
}

void fz_journal_step(fz_journal_t* j, fz_step_t* step, u64 index) {
  if (!j) return;
  emit(j, sp_fmt(j->mem, "\"ev\":\"step\",\"i\":{},\"kind\":\"{}\",\"artifact\":{},\"content\":{},\"rate\":{},\"tick\":{},\"sys\":{}",
    sp_fmt_uint(index), sp_fmt_str(step_kind_str(step->kind)), sp_fmt_uint(step->artifact),
    sp_fmt_uint(step->content), sp_fmt_uint(step->rate), sp_fmt_uint(step->tick), sp_fmt_uint(sys_now(j))).value);
}

static sp_str_t run_err_str(u64 err) {
  switch ((spn_err_t)err) {
    case SPN_OK:                     return sp_str_lit("");
    case SPN_ERR_DAG_GLOB:           return sp_str_lit("a glob failed");
    case SPN_ERR_DAG_STAT:           return sp_str_lit("a stat failed");
    case SPN_ERR_DAG_MISSING_INPUT:  return sp_str_lit("an input is missing");
    case SPN_ERR_DAG_MISSING_OUTPUT: return sp_str_lit("an action did not produce an output");
    case SPN_ERR_DAG_SCRATCH:        return sp_str_lit("scratch dir failed");
    case SPN_ERR_DAG_ACTION:         return sp_str_lit("an action failed");
    case SPN_ERR_DAG_STORE_READ:     return sp_str_lit("a store read failed");
    case SPN_ERR_DAG_STORE_WRITE:    return sp_str_lit("a store write failed");
    case SPN_ERR_DAG_STORE_MISSING:  return sp_str_lit("a store blob is missing");
    case SPN_ERR_DAG_TREE:           return sp_str_lit("a tree failed to materialize");
    case SPN_ERR_DAG_STALLED:        return sp_str_lit("the scheduler stalled");
    default:                         return sp_str_lit("");
  }
}

void fz_journal_run_done(fz_journal_t* j, u64 err, u64 fired, bool crashed) {
  if (!j) return;
  emit(j, sp_fmt(j->mem, "\"ev\":\"run_done\",\"err\":{},\"err_str\":\"{}\",\"fired\":{},\"crashed\":{},\"sys\":{}",
    sp_fmt_uint(err), sp_fmt_str(run_err_str(err)), sp_fmt_uint(fired), sp_fmt_str(json_bool(crashed)),
    sp_fmt_uint(sys_now(j))).value);
}

void fz_journal_drop(fz_journal_t* j, sp_str_t what, sp_str_t path) {
  if (!j) return;
  emit(j, sp_fmt(j->mem, "\"ev\":\"drop\",\"what\":\"{}\",\"path\":\"{}\",\"sys\":{}",
    sp_fmt_str(what), sp_fmt_str(path), sp_fmt_uint(sys_now(j))).value);
}

void fz_journal_sim_fault(fz_journal_t* j, u64 sys) {
  if (!j) return;
  emit(j, sp_fmt(j->mem, "\"ev\":\"sim.fault\",\"sys\":{}", sp_fmt_uint(sys)).value);
}

void fz_journal_stealth(fz_journal_t* j) {
  if (!j) return;
  emit(j, sp_fmt(j->mem, "\"ev\":\"stealth\",\"sys\":{}", sp_fmt_uint(sys_now(j))).value);
}

static sp_str_t sweep_kind_str(fz_sweep_kind_t kind) {
  switch (kind) {
    case FZ_SWEEP_NONE: return sp_str_lit("none");
    case FZ_SWEEP_AT:   return sp_str_lit("at");
    case FZ_SWEEP_FROM: return sp_str_lit("from");
  }
  sp_unreachable_return(sp_str_lit("unknown"));
}

void fz_journal_sweep(fz_journal_t* j, const fz_sweep_t* sweep) {
  if (!j) return;
  emit(j, sp_fmt(j->mem, "\"ev\":\"sweep\",\"kind\":\"{}\",\"run\":{},\"nth\":{}",
    sp_fmt_str(sweep_kind_str(sweep->kind)), sp_fmt_uint(sweep->run), sp_fmt_uint(sweep->nth)).value);
}

void fz_journal_model(fz_journal_t* j, sp_str_t kind, const fz_predict_row_t* rows, u64 count) {
  if (!j) return;
  sp_mem_t mem = j->mem;
  sp_str_t* items = sp_alloc_n(mem, sp_str_t, count ? count : 1);
  sp_for(it, count) {
    items[it] = sp_fmt(mem, "{{\"action\":{},\"weak\":\"{}\",\"key\":\"{}\",\"commit\":\"{}\",\"resolved\":{},\"hit\":{},\"certain\":{},\"keyed\":{}}}",
      sp_fmt_uint(rows[it].action), sp_fmt_str(json_key(mem, rows[it].weak)),
      sp_fmt_str(json_key(mem, rows[it].key)), sp_fmt_str(json_key(mem, rows[it].commit)),
      sp_fmt_str(json_bool(rows[it].resolved)), sp_fmt_str(json_bool(rows[it].hit)),
      sp_fmt_str(json_bool(rows[it].certain)), sp_fmt_str(json_bool(rows[it].keyed))).value;
  }
  emit(j, sp_fmt(mem, "\"ev\":\"model\",\"kind\":\"{}\",\"rows\":{},\"sys\":{}",
    sp_fmt_str(kind), sp_fmt_str(json_list(mem, items, (u32)count)), sp_fmt_uint(sys_now(j))).value);
}

void fz_journal_check_keys(fz_journal_t* j, const fz_key_row_t* rows, u64 count) {
  if (!j) return;
  sp_mem_t mem = j->mem;
  sp_str_t* items = sp_alloc_n(mem, sp_str_t, count ? count : 1);
  sp_for(it, count) {
    items[it] = sp_fmt(mem, "{{\"action\":{},\"want\":\"{}\",\"got\":\"{}\",\"ok\":{}}}",
      sp_fmt_uint(rows[it].action), sp_fmt_str(json_key(mem, rows[it].want)),
      sp_fmt_str(json_key(mem, rows[it].got)), sp_fmt_str(json_bool(rows[it].ok))).value;
  }
  emit(j, sp_fmt(mem, "\"ev\":\"check\",\"kind\":\"keys\",\"rows\":{},\"sys\":{}",
    sp_fmt_str(json_list(mem, items, (u32)count)), sp_fmt_uint(sys_now(j))).value);
}

void fz_journal_exec(fz_journal_t* j, u64 action) {
  if (!j) return;
  emit(j, sp_fmt(j->mem, "\"ev\":\"exec\",\"action\":{},\"sys\":{}",
    sp_fmt_uint(action), sp_fmt_uint(sys_now(j))).value);
}

void fz_journal_check_execs(fz_journal_t* j, const fz_exec_row_t* rows, u64 count) {
  if (!j) return;
  sp_mem_t mem = j->mem;
  sp_str_t* items = sp_alloc_n(mem, sp_str_t, count ? count : 1);
  sp_for(it, count) {
    items[it] = sp_fmt(mem, "{{\"action\":{},\"execs\":{},\"want\":{},\"requeues\":{},\"miss\":{},\"ok\":{}}}",
      sp_fmt_uint(rows[it].action), sp_fmt_uint(rows[it].execs), sp_fmt_uint(rows[it].want),
      sp_fmt_uint(rows[it].requeues), sp_fmt_str(json_bool(rows[it].miss)), sp_fmt_str(json_bool(rows[it].ok))).value;
  }
  emit(j, sp_fmt(mem, "\"ev\":\"check\",\"kind\":\"execs\",\"rows\":{},\"sys\":{}",
    sp_fmt_str(json_list(mem, items, (u32)count)), sp_fmt_uint(sys_now(j))).value);
}

void fz_journal_check_bytes(fz_journal_t* j, sp_str_t kind, const fz_bytes_row_t* rows, u64 count) {
  if (!j) return;
  sp_mem_t mem = j->mem;
  sp_str_t* items = sp_alloc_n(mem, sp_str_t, count ? count : 1);
  sp_for(it, count) {
    items[it] = sp_fmt(mem, "{{\"artifact\":{},\"want\":\"{}\",\"got\":\"{}\",\"ok\":{}}}",
      sp_fmt_uint(rows[it].artifact), sp_fmt_str(json_bytes(rows[it].want)),
      sp_fmt_str(json_bytes(rows[it].got)), sp_fmt_str(json_bool(rows[it].ok))).value;
  }
  emit(j, sp_fmt(mem, "\"ev\":\"check\",\"kind\":\"{}\",\"rows\":{},\"sys\":{}",
    sp_fmt_str(kind), sp_fmt_str(json_list(mem, items, (u32)count)), sp_fmt_uint(sys_now(j))).value);
}

void fz_journal_blob(fz_journal_t* j, u64 artifact, sp_str_t want, sp_str_t got) {
  if (!j) return;
  emit(j, sp_fmt(j->mem, "\"ev\":\"blob\",\"artifact\":{},\"want\":\"{}\",\"got\":\"{}\",\"sys\":{}",
    sp_fmt_uint(artifact), sp_fmt_str(json_bytes(want)), sp_fmt_str(json_bytes(got)),
    sp_fmt_uint(sys_now(j))).value);
}

void fz_journal_sim_write(fz_journal_t* j, u64 artifact, sp_str_t path, u64 sys) {
  if (!j) return;
  emit(j, sp_fmt(j->mem, "\"ev\":\"sim.write\",\"artifact\":{},\"path\":\"{}\",\"sys\":{}",
    sp_fmt_uint(artifact), sp_fmt_str(path), sp_fmt_uint(sys)).value);
}

void fz_journal_pass(fz_journal_t* j, sp_str_t name) {
  if (!j) return;
  emit(j, sp_fmt(j->mem, "\"ev\":\"pass\",\"name\":\"{}\"", sp_fmt_str(name)).value);
}

void fz_journal_done(fz_journal_t* j, fz_err_t err) {
  if (!j) return;
  emit(j, sp_fmt(j->mem, "\"ev\":\"done\",\"err\":{},\"str\":\"{}\"",
    sp_fmt_uint((u64)err), sp_fmt_str(fz_err_to_str(err))).value);
}

void fz_journal_trace_hook(const spn_dag_trace_event_t* event, void* user_data) {
  fz_journal_t* j = (fz_journal_t*)user_data;
  sp_mem_t mem = j->mem;
  u64 action = event->action.index;
  u64 sys = sys_now(j);

  switch (event->kind) {
    case SPN_DAG_TRACE_KEY: {
      emit(j, sp_fmt(mem, "\"ev\":\"dag.key\",\"action\":{},\"key\":\"{}\",\"sys\":{}",
        sp_fmt_uint(action), sp_fmt_str(json_key(mem, event->key)), sp_fmt_uint(sys)).value);
      break;
    }
    case SPN_DAG_TRACE_DISCOVERY: {
      emit(j, sp_fmt(mem, "\"ev\":\"dag.discovery\",\"action\":{},\"found\":{},\"sys\":{}",
        sp_fmt_uint(action), sp_fmt_str(json_bool(event->hit)), sp_fmt_uint(sys)).value);
      break;
    }
    case SPN_DAG_TRACE_RESOLVE: {
      emit(j, sp_fmt(mem, "\"ev\":\"dag.resolve\",\"action\":{},\"ok\":{},\"sys\":{}",
        sp_fmt_uint(action), sp_fmt_str(json_bool(event->hit)), sp_fmt_uint(sys)).value);
      break;
    }
    case SPN_DAG_TRACE_STRONG: {
      emit(j, sp_fmt(mem, "\"ev\":\"dag.strong\",\"action\":{},\"key\":\"{}\",\"sys\":{}",
        sp_fmt_uint(action), sp_fmt_str(json_key(mem, event->key)), sp_fmt_uint(sys)).value);
      break;
    }
    case SPN_DAG_TRACE_CACHE: {
      emit(j, sp_fmt(mem, "\"ev\":\"dag.cache\",\"action\":{},\"key\":\"{}\",\"present\":{},\"hit\":{},\"sys\":{}",
        sp_fmt_uint(action), sp_fmt_str(json_key(mem, event->key)), sp_fmt_str(json_bool(event->present)),
        sp_fmt_str(json_bool(event->hit)), sp_fmt_uint(sys)).value);
      break;
    }
    case SPN_DAG_TRACE_EXECUTE: {
      emit(j, sp_fmt(mem, "\"ev\":\"dag.execute\",\"action\":{},\"sys\":{}",
        sp_fmt_uint(action), sp_fmt_uint(sys)).value);
      break;
    }
    case SPN_DAG_TRACE_COMMIT: {
      emit(j, sp_fmt(mem, "\"ev\":\"dag.commit\",\"action\":{},\"key\":\"{}\",\"recorded\":{},\"sys\":{}",
        sp_fmt_uint(action), sp_fmt_str(json_key(mem, event->key)), sp_fmt_str(json_bool(event->hit)), sp_fmt_uint(sys)).value);
      break;
    }
    case SPN_DAG_TRACE_DEFER: {
      emit(j, sp_fmt(mem, "\"ev\":\"dag.defer\",\"action\":{},\"producer\":{},\"sys\":{}",
        sp_fmt_uint(action), sp_fmt_uint(event->producer.index), sp_fmt_uint(sys)).value);
      break;
    }
    case SPN_DAG_TRACE_REQUEUE: {
      emit(j, sp_fmt(mem, "\"ev\":\"dag.requeue\",\"action\":{},\"producer\":{},\"sys\":{}",
        sp_fmt_uint(action), sp_fmt_uint(event->producer.index), sp_fmt_uint(sys)).value);
      break;
    }
    case SPN_DAG_TRACE_SETTLE: {
      emit(j, sp_fmt(mem, "\"ev\":\"dag.settle\",\"action\":{},\"artifact\":{},\"digest\":\"{}\",\"skipped\":{},\"sys\":{}",
        sp_fmt_uint(action), sp_fmt_uint(event->producer.index), sp_fmt_str(json_key(mem, event->key)),
        sp_fmt_str(json_bool(event->hit)), sp_fmt_uint(sys)).value);
      break;
    }
  }
}

void fz_journal_write(fz_journal_t* j, sp_io_writer_t* io) {
  sp_da_for(j->lines, it) {
    sp_io_write_str(io, j->lines[it], SP_NULLPTR);
    sp_io_write_c8(io, '\n');
  }
}
