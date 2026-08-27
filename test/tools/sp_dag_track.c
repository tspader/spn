#include "sp_dag_track.h"

typedef struct {
  bool completed;
  bool discovery;
  bool executed;
  bool put_pathset;
  bool committed;
  bool weak_valid;
  u64 commit_sys;
  spn_dag_digest_t weak;
} action_t;

void sp_dag_track_init(sp_dag_track_t* track, sp_mem_t mem, sp_dag_track_persist_t persist) {
  track->mem = mem;
  track->persist = persist;
  sp_ht_init(mem, track->entries);
  sp_ht_init(mem, track->blobs);
  sp_ht_init(mem, track->pathsets);
}

static sp_dag_track_slot_t query(sp_dag_track_table_t table, spn_dag_digest_t key) {
  sp_dag_track_state_t* state = sp_ht_getp(table, key);
  if (!state) {
    return (sp_dag_track_slot_t) sp_zero;
  }
  return (sp_dag_track_slot_t) {
    .present = true,
    .sure = *state != SP_DAG_TRACK_UNKNOWN,
  };
}

static void affirm(sp_dag_track_table_t* table, spn_dag_digest_t key, sp_dag_track_state_t state) {
  sp_dag_track_state_t* existing = sp_ht_getp(*table, key);
  if (existing && *existing < state) {
    return;
  }
  sp_ht_insert(*table, key, state);
}

static void retract(sp_dag_track_table_t* table, spn_dag_digest_t key, bool certain) {
  if (certain) {
    sp_ht_erase(*table, key);
    return;
  }
  sp_dag_track_state_t* state = sp_ht_getp(*table, key);
  if (state) {
    *state = SP_DAG_TRACK_UNKNOWN;
  }
}

static void degrade(sp_dag_track_table_t* table, bool persist) {
  if (!persist) {
    sp_ht_clear(*table);
    return;
  }
  sp_ht_for_kv(*table, it) {
    if (*it.val == SP_DAG_TRACK_VOLATILE) {
      *it.val = SP_DAG_TRACK_UNKNOWN;
    }
  }
}

static sp_dag_track_state_t classify(u64 sys, u64 crash_at, bool lossy) {
  if (crash_at && sys > crash_at) {
    return SP_DAG_TRACK_UNKNOWN;
  }
  return lossy ? SP_DAG_TRACK_VOLATILE : SP_DAG_TRACK_SURE;
}

void sp_dag_track_run(sp_dag_track_t* track, const sp_dag_track_event_t* events, u64 count, u64 crash_at, bool lossy) {
  sp_mem_arena_marker_t s = sp_mem_begin_scratch();
  sp_ht(u32, action_t) actions = sp_zero;
  sp_ht_init(s.mem, actions);

  sp_for(it, count) {
    const spn_dag_trace_event_t* event = &events[it].event;
    u32 index = event->action.index;
    if (!sp_ht_getp(actions, index)) {
      sp_ht_insert(actions, index, (action_t) sp_zero);
    }
    action_t* action = sp_ht_getp(actions, index);
    switch (event->kind) {
      case SPN_DAG_TRACE_KEY: {
        action->weak = event->key;
        action->weak_valid = true;
        break;
      }
      case SPN_DAG_TRACE_DISCOVERY: {
        action->discovery = true;
        break;
      }
      case SPN_DAG_TRACE_EXECUTE: {
        action->executed = true;
        break;
      }
      case SPN_DAG_TRACE_RESOLVE: {
        if (action->executed) {
          action->put_pathset = true;
        }
        break;
      }
      case SPN_DAG_TRACE_CACHE: {
        if (event->present && event->hit) {
          action->completed = true;
        }
        break;
      }
      case SPN_DAG_TRACE_COMMIT: {
        action->completed = true;
        action->committed = true;
        action->commit_sys = events[it].sys;
        break;
      }
      default: {
        break;
      }
    }
  }

  sp_for(it, count) {
    const spn_dag_trace_event_t* event = &events[it].event;
    action_t* action = sp_ht_getp(actions, event->action.index);
    bool certain = (!crash_at || events[it].sys <= crash_at) && !lossy;
    switch (event->kind) {
      case SPN_DAG_TRACE_DISCOVERY: {
        if (event->hit) {
          affirm(&track->pathsets, event->key, SP_DAG_TRACK_UNKNOWN);
        }
        else {
          retract(&track->pathsets, event->key, certain);
        }
        break;
      }
      case SPN_DAG_TRACE_CACHE: {
        sp_dag_track_state_t* state = sp_ht_getp(track->entries, event->key);
        if (!event->present || !event->hit) {
          retract(&track->entries, event->key, certain);
        }
        else if (!state || *state == SP_DAG_TRACK_UNKNOWN) {
          affirm(&track->entries, event->key, classify(events[it].sys, crash_at, false));
        }
        break;
      }
      case SPN_DAG_TRACE_COMMIT: {
        if (event->hit) {
          affirm(&track->entries, event->key, classify(events[it].sys, crash_at, lossy));
        }
        break;
      }
      case SPN_DAG_TRACE_SETTLE: {
        if (action->completed || action->executed) {
          affirm(&track->blobs, event->key, classify(events[it].sys, crash_at, false));
        }
        break;
      }
      default: {
        break;
      }
    }
  }

  sp_ht_for_kv(actions, it) {
    action_t* action = it.val;
    if (!action->discovery || !action->put_pathset) {
      continue;
    }
    sp_assert(action->weak_valid);
    sp_dag_track_state_t state = action->committed
      ? classify(action->commit_sys, crash_at, lossy)
      : crash_at ? SP_DAG_TRACK_UNKNOWN : lossy ? SP_DAG_TRACK_VOLATILE : SP_DAG_TRACK_SURE;
    sp_ht_insert(track->pathsets, action->weak, state);
  }

  sp_mem_end_scratch(s);
}

void sp_dag_track_reboot(sp_dag_track_t* track) {
  degrade(&track->entries, track->persist.entries);
  degrade(&track->blobs, track->persist.blobs);
  degrade(&track->pathsets, track->persist.pathsets);
}

void sp_dag_track_reset_entries(sp_dag_track_t* track) {
  degrade(&track->entries, track->persist.entries);
}

void sp_dag_track_reset_discovery(sp_dag_track_t* track) {
  degrade(&track->pathsets, track->persist.pathsets);
}

void sp_dag_track_drop_entry(sp_dag_track_t* track, spn_dag_digest_t key) {
  sp_ht_erase(track->entries, key);
}

void sp_dag_track_drop_blob(sp_dag_track_t* track, spn_dag_digest_t digest) {
  sp_ht_erase(track->blobs, digest);
}

sp_dag_track_slot_t sp_dag_track_entry(sp_dag_track_t* track, spn_dag_digest_t key) {
  return query(track->entries, key);
}

sp_dag_track_slot_t sp_dag_track_blob(sp_dag_track_t* track, spn_dag_digest_t digest) {
  return query(track->blobs, digest);
}

sp_dag_track_slot_t sp_dag_track_pathset(sp_dag_track_t* track, spn_dag_digest_t weak) {
  return query(track->pathsets, weak);
}
