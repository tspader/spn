#include "dag_test.h"
#include "dag/track.h"

#define TRACK_TEST_MAX_OPS 14
#define TRACK_TEST_MAX_CHECKS 4

typedef enum {
  TRACK_OP_DONE,
  TRACK_OP_KEY,
  TRACK_OP_DISCOVERY,
  TRACK_OP_CACHE,
  TRACK_OP_EXECUTE,
  TRACK_OP_RESOLVE,
  TRACK_OP_COMMIT,
  TRACK_OP_SETTLE,
  TRACK_OP_RUN,
  TRACK_OP_DROP_ENTRY,
  TRACK_OP_DROP_BLOB,
  TRACK_OP_REBOOT,
  TRACK_OP_RESET_ENTRIES,
  TRACK_OP_RESET_DISCOVERY,
} op_kind_t;

typedef struct {
  op_kind_t kind;
  u32 action;
  const c8* key;
  bool present;
  bool hit;
  bool lossy;
  u64 sys;
  u64 crash_at;
} op_t;

typedef enum {
  SLOT_ENTRY,
  SLOT_BLOB,
  SLOT_PATHSET,
} slot_kind_t;

typedef struct {
  slot_kind_t kind;
  const c8* key;
  bool present;
  bool sure;
} check_t;

typedef struct {
  const c8* name;
  sp_dag_track_persist_t persist;
  op_t ops [TRACK_TEST_MAX_OPS];
  check_t checks [TRACK_TEST_MAX_CHECKS];
} test_t;

static const test_t tests [] = {
  {
    .name = "commit_records_entry",
    .ops = {
      { .kind = TRACK_OP_COMMIT, .key = "K", .hit = true, .sys = 9 },
      { .kind = TRACK_OP_RUN },
    },
    .checks = {
      { .kind = SLOT_ENTRY, .key = "K", .present = true, .sure = true },
    },
  },
  {
    .name = "discovered_miss_records_no_entry",
    .ops = {
      { .kind = TRACK_OP_KEY, .key = "W" },
      { .kind = TRACK_OP_DISCOVERY, .key = "W" },
      { .kind = TRACK_OP_EXECUTE },
      { .kind = TRACK_OP_RESOLVE },
      { .kind = TRACK_OP_COMMIT, .key = "W" },
      { .kind = TRACK_OP_RUN },
    },
    .checks = {
      { .kind = SLOT_ENTRY, .key = "W" },
      { .kind = SLOT_PATHSET, .key = "W", .present = true, .sure = true },
    },
  },
  {
    .name = "settle_of_committed_action_records_blob",
    .ops = {
      { .kind = TRACK_OP_SETTLE, .key = "D" },
      { .kind = TRACK_OP_COMMIT, .key = "K", .hit = true },
      { .kind = TRACK_OP_RUN },
    },
    .checks = {
      { .kind = SLOT_BLOB, .key = "D", .present = true, .sure = true },
    },
  },
  {
    .name = "settle_without_completion_ignored",
    .ops = {
      { .kind = TRACK_OP_SETTLE, .key = "D" },
      { .kind = TRACK_OP_RUN },
    },
    .checks = {
      { .kind = SLOT_BLOB, .key = "D" },
    },
  },
  {
    .name = "settle_of_executed_erred_action_records_blob",
    .ops = {
      { .kind = TRACK_OP_EXECUTE },
      { .kind = TRACK_OP_SETTLE, .key = "D" },
      { .kind = TRACK_OP_RUN },
    },
    .checks = {
      { .kind = SLOT_BLOB, .key = "D", .present = true, .sure = true },
    },
  },
  {
    .name = "executed_erred_settle_stays_sure_when_lossy",
    .ops = {
      { .kind = TRACK_OP_EXECUTE },
      { .kind = TRACK_OP_SETTLE, .key = "D" },
      { .kind = TRACK_OP_RUN, .lossy = true },
    },
    .checks = {
      { .kind = SLOT_BLOB, .key = "D", .present = true, .sure = true },
    },
  },
  {
    .name = "executed_erred_settle_after_crash_goes_unsure",
    .ops = {
      { .kind = TRACK_OP_EXECUTE, .sys = 3 },
      { .kind = TRACK_OP_SETTLE, .key = "D", .sys = 6 },
      { .kind = TRACK_OP_RUN, .crash_at = 5 },
    },
    .checks = {
      { .kind = SLOT_BLOB, .key = "D", .present = true, .sure = false },
    },
  },
  {
    .name = "settle_of_another_action_not_credited",
    .ops = {
      { .kind = TRACK_OP_SETTLE, .action = 1, .key = "D" },
      { .kind = TRACK_OP_COMMIT, .key = "K", .hit = true },
      { .kind = TRACK_OP_RUN },
    },
    .checks = {
      { .kind = SLOT_BLOB, .key = "D" },
      { .kind = SLOT_ENTRY, .key = "K", .present = true, .sure = true },
    },
  },
  {
    .name = "interleaved_actions_both_credited",
    .ops = {
      { .kind = TRACK_OP_SETTLE, .key = "D" },
      { .kind = TRACK_OP_SETTLE, .action = 1, .key = "E" },
      { .kind = TRACK_OP_CACHE, .action = 1, .key = "L", .present = true, .hit = true },
      { .kind = TRACK_OP_COMMIT, .key = "K", .hit = true },
      { .kind = TRACK_OP_RUN },
    },
    .checks = {
      { .kind = SLOT_BLOB, .key = "D", .present = true, .sure = true },
      { .kind = SLOT_BLOB, .key = "E", .present = true, .sure = true },
      { .kind = SLOT_ENTRY, .key = "K", .present = true, .sure = true },
      { .kind = SLOT_ENTRY, .key = "L", .present = true, .sure = true },
    },
  },
  {
    .name = "multi_output_settles",
    .ops = {
      { .kind = TRACK_OP_SETTLE, .key = "D" },
      { .kind = TRACK_OP_SETTLE, .key = "E" },
      { .kind = TRACK_OP_COMMIT, .key = "K", .hit = true },
      { .kind = TRACK_OP_RUN },
    },
    .checks = {
      { .kind = SLOT_BLOB, .key = "D", .present = true, .sure = true },
      { .kind = SLOT_BLOB, .key = "E", .present = true, .sure = true },
    },
  },
  {
    .name = "resolved_discovered_commit_records_both",
    .ops = {
      { .kind = TRACK_OP_KEY, .key = "W" },
      { .kind = TRACK_OP_DISCOVERY, .key = "W" },
      { .kind = TRACK_OP_EXECUTE },
      { .kind = TRACK_OP_RESOLVE, .hit = true },
      { .kind = TRACK_OP_COMMIT, .key = "K", .hit = true },
      { .kind = TRACK_OP_RUN },
    },
    .checks = {
      { .kind = SLOT_ENTRY, .key = "K", .present = true, .sure = true },
      { .kind = SLOT_PATHSET, .key = "W", .present = true, .sure = true },
    },
  },
  {
    .name = "rekey_uses_latest_weak",
    .ops = {
      { .kind = TRACK_OP_KEY, .key = "V" },
      { .kind = TRACK_OP_KEY, .key = "W" },
      { .kind = TRACK_OP_DISCOVERY, .key = "W" },
      { .kind = TRACK_OP_EXECUTE },
      { .kind = TRACK_OP_RESOLVE },
      { .kind = TRACK_OP_COMMIT, .key = "W" },
      { .kind = TRACK_OP_RUN },
    },
    .checks = {
      { .kind = SLOT_PATHSET, .key = "W", .present = true, .sure = true },
      { .kind = SLOT_PATHSET, .key = "V" },
    },
  },
  {
    .name = "erred_run_still_records_pathset",
    .ops = {
      { .kind = TRACK_OP_KEY, .key = "W" },
      { .kind = TRACK_OP_DISCOVERY, .key = "W" },
      { .kind = TRACK_OP_EXECUTE },
      { .kind = TRACK_OP_RESOLVE },
      { .kind = TRACK_OP_RUN, .lossy = true },
    },
    .checks = {
      { .kind = SLOT_PATHSET, .key = "W", .present = true, .sure = true },
    },
  },
  {
    .name = "lookup_resolve_is_not_a_put",
    .ops = {
      { .kind = TRACK_OP_KEY, .key = "W" },
      { .kind = TRACK_OP_DISCOVERY, .key = "W" },
      { .kind = TRACK_OP_RESOLVE, .hit = true },
      { .kind = TRACK_OP_RUN },
    },
    .checks = {
      { .kind = SLOT_PATHSET, .key = "W" },
    },
  },
  {
    .name = "cache_hit_completes_action",
    .ops = {
      { .kind = TRACK_OP_SETTLE, .key = "D" },
      { .kind = TRACK_OP_CACHE, .key = "K", .present = true, .hit = true },
      { .kind = TRACK_OP_RUN },
    },
    .checks = {
      { .kind = SLOT_BLOB, .key = "D", .present = true, .sure = true },
    },
  },
  {
    .name = "failed_restore_removes_entry",
    .ops = {
      { .kind = TRACK_OP_COMMIT, .key = "K", .hit = true },
      { .kind = TRACK_OP_RUN },
      { .kind = TRACK_OP_CACHE, .key = "K", .present = true },
      { .kind = TRACK_OP_RUN },
    },
    .checks = {
      { .kind = SLOT_ENTRY, .key = "K" },
    },
  },
  {
    .name = "lossy_failed_restore_downgrades",
    .persist = { .entries = true },
    .ops = {
      { .kind = TRACK_OP_COMMIT, .key = "K", .hit = true },
      { .kind = TRACK_OP_RUN },
      { .kind = TRACK_OP_CACHE, .key = "K", .present = true },
      { .kind = TRACK_OP_RUN, .lossy = true },
    },
    .checks = {
      { .kind = SLOT_ENTRY, .key = "K", .present = true },
    },
  },
  {
    .name = "lossy_commit_reliable_until_reboot",
    .persist = { .entries = true },
    .ops = {
      { .kind = TRACK_OP_COMMIT, .key = "K", .hit = true },
      { .kind = TRACK_OP_RUN, .lossy = true },
    },
    .checks = {
      { .kind = SLOT_ENTRY, .key = "K", .present = true, .sure = true },
    },
  },
  {
    .name = "lossy_commit_degrades_at_reboot",
    .persist = { .entries = true },
    .ops = {
      { .kind = TRACK_OP_COMMIT, .key = "K", .hit = true },
      { .kind = TRACK_OP_RUN, .lossy = true },
      { .kind = TRACK_OP_REBOOT },
    },
    .checks = {
      { .kind = SLOT_ENTRY, .key = "K", .present = true },
    },
  },
  {
    .name = "lossy_blob_survives_reboot",
    .persist = { .blobs = true },
    .ops = {
      { .kind = TRACK_OP_SETTLE, .key = "D" },
      { .kind = TRACK_OP_COMMIT, .key = "K", .hit = true },
      { .kind = TRACK_OP_RUN, .lossy = true },
      { .kind = TRACK_OP_REBOOT },
    },
    .checks = {
      { .kind = SLOT_BLOB, .key = "D", .present = true, .sure = true },
    },
  },
  {
    .name = "hit_proves_unknown_entry_on_disk",
    .persist = { .entries = true },
    .ops = {
      { .kind = TRACK_OP_COMMIT, .key = "K", .hit = true },
      { .kind = TRACK_OP_RUN, .lossy = true },
      { .kind = TRACK_OP_REBOOT },
      { .kind = TRACK_OP_CACHE, .key = "K", .present = true, .hit = true },
      { .kind = TRACK_OP_RUN },
    },
    .checks = {
      { .kind = SLOT_ENTRY, .key = "K", .present = true, .sure = true },
    },
  },
  {
    .name = "evict_reset_degrades_volatile_entries",
    .persist = { .entries = true },
    .ops = {
      { .kind = TRACK_OP_COMMIT, .key = "K", .hit = true },
      { .kind = TRACK_OP_RUN, .lossy = true },
      { .kind = TRACK_OP_RESET_ENTRIES },
    },
    .checks = {
      { .kind = SLOT_ENTRY, .key = "K", .present = true },
    },
  },
  {
    .name = "lossy_discovery_miss_downgrades",
    .persist = { .pathsets = true },
    .ops = {
      { .kind = TRACK_OP_KEY, .key = "W" },
      { .kind = TRACK_OP_DISCOVERY, .key = "W" },
      { .kind = TRACK_OP_EXECUTE },
      { .kind = TRACK_OP_RESOLVE },
      { .kind = TRACK_OP_COMMIT, .key = "W" },
      { .kind = TRACK_OP_RUN },
      { .kind = TRACK_OP_KEY, .key = "W" },
      { .kind = TRACK_OP_DISCOVERY, .key = "W" },
      { .kind = TRACK_OP_RUN, .lossy = true },
    },
    .checks = {
      { .kind = SLOT_PATHSET, .key = "W", .present = true },
    },
  },
  {
    .name = "cache_absence_is_ground_truth",
    .persist = { .entries = true },
    .ops = {
      { .kind = TRACK_OP_COMMIT, .key = "K", .hit = true, .sys = 10 },
      { .kind = TRACK_OP_RUN, .crash_at = 5 },
      { .kind = TRACK_OP_REBOOT },
      { .kind = TRACK_OP_CACHE, .key = "K" },
      { .kind = TRACK_OP_RUN },
    },
    .checks = {
      { .kind = SLOT_ENTRY, .key = "K" },
    },
  },
  {
    .name = "cache_hit_confirms_unsure_entry",
    .persist = { .entries = true },
    .ops = {
      { .kind = TRACK_OP_COMMIT, .key = "K", .hit = true, .sys = 10 },
      { .kind = TRACK_OP_RUN, .crash_at = 5 },
      { .kind = TRACK_OP_REBOOT },
      { .kind = TRACK_OP_CACHE, .key = "K", .present = true, .hit = true },
      { .kind = TRACK_OP_RUN },
    },
    .checks = {
      { .kind = SLOT_ENTRY, .key = "K", .present = true, .sure = true },
    },
  },
  {
    .name = "discovery_miss_is_ground_truth",
    .ops = {
      { .kind = TRACK_OP_KEY, .key = "W" },
      { .kind = TRACK_OP_DISCOVERY, .key = "W" },
      { .kind = TRACK_OP_EXECUTE },
      { .kind = TRACK_OP_RESOLVE },
      { .kind = TRACK_OP_COMMIT, .key = "W" },
      { .kind = TRACK_OP_RUN },
      { .kind = TRACK_OP_KEY, .key = "W" },
      { .kind = TRACK_OP_DISCOVERY, .key = "W" },
      { .kind = TRACK_OP_RUN },
    },
    .checks = {
      { .kind = SLOT_PATHSET, .key = "W" },
    },
  },
  {
    .name = "discovery_hit_cannot_restore_pathset_content",
    .persist = { .pathsets = true },
    .ops = {
      { .kind = TRACK_OP_KEY, .key = "W" },
      { .kind = TRACK_OP_DISCOVERY, .key = "W" },
      { .kind = TRACK_OP_EXECUTE },
      { .kind = TRACK_OP_RESOLVE },
      { .kind = TRACK_OP_COMMIT, .key = "W", .sys = 10 },
      { .kind = TRACK_OP_RUN, .crash_at = 5 },
      { .kind = TRACK_OP_REBOOT },
      { .kind = TRACK_OP_KEY, .key = "W" },
      { .kind = TRACK_OP_DISCOVERY, .key = "W", .hit = true },
      { .kind = TRACK_OP_RUN },
    },
    .checks = {
      { .kind = SLOT_PATHSET, .key = "W", .present = true },
    },
  },
  {
    .name = "recommit_restores_pathset_content",
    .persist = { .pathsets = true },
    .ops = {
      { .kind = TRACK_OP_KEY, .key = "W" },
      { .kind = TRACK_OP_DISCOVERY, .key = "W" },
      { .kind = TRACK_OP_EXECUTE },
      { .kind = TRACK_OP_RESOLVE },
      { .kind = TRACK_OP_COMMIT, .key = "W", .sys = 10 },
      { .kind = TRACK_OP_RUN, .crash_at = 5 },
      { .kind = TRACK_OP_REBOOT },
      { .kind = TRACK_OP_KEY, .key = "W" },
      { .kind = TRACK_OP_DISCOVERY, .key = "W", .hit = true },
      { .kind = TRACK_OP_EXECUTE },
      { .kind = TRACK_OP_RESOLVE },
      { .kind = TRACK_OP_COMMIT, .key = "W" },
      { .kind = TRACK_OP_RUN },
    },
    .checks = {
      { .kind = SLOT_PATHSET, .key = "W", .present = true, .sure = true },
    },
  },
  {
    .name = "removal_after_crash_point_is_undone",
    .persist = { .entries = true },
    .ops = {
      { .kind = TRACK_OP_COMMIT, .key = "K", .hit = true, .sys = 2 },
      { .kind = TRACK_OP_RUN },
      { .kind = TRACK_OP_CACHE, .key = "K", .present = true, .sys = 10 },
      { .kind = TRACK_OP_RUN, .crash_at = 5 },
      { .kind = TRACK_OP_REBOOT },
    },
    .checks = {
      { .kind = SLOT_ENTRY, .key = "K", .present = true },
    },
  },
  {
    .name = "commit_before_crash_stays_sure",
    .persist = { .entries = true },
    .ops = {
      { .kind = TRACK_OP_COMMIT, .key = "K", .hit = true, .sys = 5 },
      { .kind = TRACK_OP_RUN, .crash_at = 5 },
      { .kind = TRACK_OP_REBOOT },
    },
    .checks = {
      { .kind = SLOT_ENTRY, .key = "K", .present = true, .sure = true },
    },
  },
  {
    .name = "commit_after_crash_goes_unsure",
    .persist = { .entries = true },
    .ops = {
      { .kind = TRACK_OP_COMMIT, .key = "K", .hit = true, .sys = 6 },
      { .kind = TRACK_OP_RUN, .crash_at = 5 },
      { .kind = TRACK_OP_REBOOT },
      { .kind = TRACK_OP_RUN },
    },
    .checks = {
      { .kind = SLOT_ENTRY, .key = "K", .present = true },
    },
  },
  {
    .name = "settle_before_crash_keeps_blob_sure",
    .persist = { .blobs = true },
    .ops = {
      { .kind = TRACK_OP_SETTLE, .key = "D", .sys = 4 },
      { .kind = TRACK_OP_COMMIT, .key = "K", .hit = true, .sys = 6 },
      { .kind = TRACK_OP_RUN, .crash_at = 5 },
      { .kind = TRACK_OP_REBOOT },
    },
    .checks = {
      { .kind = SLOT_BLOB, .key = "D", .present = true, .sure = true },
    },
  },
  {
    .name = "settle_after_crash_goes_unsure",
    .persist = { .blobs = true },
    .ops = {
      { .kind = TRACK_OP_SETTLE, .key = "D", .sys = 6 },
      { .kind = TRACK_OP_COMMIT, .key = "K", .hit = true, .sys = 7 },
      { .kind = TRACK_OP_RUN, .crash_at = 5 },
      { .kind = TRACK_OP_REBOOT },
    },
    .checks = {
      { .kind = SLOT_BLOB, .key = "D", .present = true },
    },
  },
  {
    .name = "crash_run_hit_keeps_read_blob_sure",
    .persist = { .entries = true, .blobs = true },
    .ops = {
      { .kind = TRACK_OP_SETTLE, .key = "D", .sys = 4 },
      { .kind = TRACK_OP_CACHE, .key = "K", .present = true, .hit = true, .sys = 7 },
      { .kind = TRACK_OP_RUN, .crash_at = 5 },
      { .kind = TRACK_OP_REBOOT },
    },
    .checks = {
      { .kind = SLOT_BLOB, .key = "D", .present = true, .sure = true },
      { .kind = SLOT_ENTRY, .key = "K", .present = true },
    },
  },
  {
    .name = "reboot_wipes_volatile_layers",
    .ops = {
      { .kind = TRACK_OP_KEY, .key = "W" },
      { .kind = TRACK_OP_DISCOVERY, .key = "W" },
      { .kind = TRACK_OP_EXECUTE },
      { .kind = TRACK_OP_RESOLVE },
      { .kind = TRACK_OP_COMMIT, .key = "W" },
      { .kind = TRACK_OP_SETTLE, .action = 1, .key = "D" },
      { .kind = TRACK_OP_COMMIT, .action = 1, .key = "K", .hit = true },
      { .kind = TRACK_OP_RUN },
      { .kind = TRACK_OP_REBOOT },
    },
    .checks = {
      { .kind = SLOT_ENTRY, .key = "K" },
      { .kind = SLOT_BLOB, .key = "D" },
      { .kind = SLOT_PATHSET, .key = "W" },
    },
  },
  {
    .name = "reboot_keeps_persistent_layers",
    .persist = { .entries = true, .blobs = true, .pathsets = true },
    .ops = {
      { .kind = TRACK_OP_KEY, .key = "W" },
      { .kind = TRACK_OP_DISCOVERY, .key = "W" },
      { .kind = TRACK_OP_EXECUTE },
      { .kind = TRACK_OP_RESOLVE },
      { .kind = TRACK_OP_COMMIT, .key = "W" },
      { .kind = TRACK_OP_SETTLE, .action = 1, .key = "D" },
      { .kind = TRACK_OP_COMMIT, .action = 1, .key = "K", .hit = true },
      { .kind = TRACK_OP_RUN },
      { .kind = TRACK_OP_REBOOT },
    },
    .checks = {
      { .kind = SLOT_ENTRY, .key = "K", .present = true, .sure = true },
      { .kind = SLOT_BLOB, .key = "D", .present = true, .sure = true },
      { .kind = SLOT_PATHSET, .key = "W", .present = true, .sure = true },
    },
  },
  {
    .name = "drop_entry_removes",
    .persist = { .entries = true },
    .ops = {
      { .kind = TRACK_OP_COMMIT, .key = "K", .hit = true },
      { .kind = TRACK_OP_RUN },
      { .kind = TRACK_OP_DROP_ENTRY, .key = "K" },
    },
    .checks = {
      { .kind = SLOT_ENTRY, .key = "K" },
    },
  },
  {
    .name = "drop_blob_removes",
    .persist = { .blobs = true },
    .ops = {
      { .kind = TRACK_OP_SETTLE, .key = "D" },
      { .kind = TRACK_OP_COMMIT, .key = "K", .hit = true },
      { .kind = TRACK_OP_RUN },
      { .kind = TRACK_OP_DROP_BLOB, .key = "D" },
    },
    .checks = {
      { .kind = SLOT_BLOB, .key = "D" },
    },
  },
  {
    .name = "reset_discovery_wipes_volatile_pathsets",
    .ops = {
      { .kind = TRACK_OP_KEY, .key = "W" },
      { .kind = TRACK_OP_DISCOVERY, .key = "W" },
      { .kind = TRACK_OP_EXECUTE },
      { .kind = TRACK_OP_RESOLVE },
      { .kind = TRACK_OP_COMMIT, .key = "W" },
      { .kind = TRACK_OP_RUN },
      { .kind = TRACK_OP_RESET_DISCOVERY },
    },
    .checks = {
      { .kind = SLOT_PATHSET, .key = "W" },
    },
  },
  {
    .name = "reset_discovery_keeps_persistent_pathsets",
    .persist = { .pathsets = true },
    .ops = {
      { .kind = TRACK_OP_KEY, .key = "W" },
      { .kind = TRACK_OP_DISCOVERY, .key = "W" },
      { .kind = TRACK_OP_EXECUTE },
      { .kind = TRACK_OP_RESOLVE },
      { .kind = TRACK_OP_COMMIT, .key = "W" },
      { .kind = TRACK_OP_RUN },
      { .kind = TRACK_OP_RESET_DISCOVERY },
    },
    .checks = {
      { .kind = SLOT_PATHSET, .key = "W", .present = true, .sure = true },
    },
  },
};

static spn_dag_trace_kind_t event_kind(op_kind_t kind) {
  switch (kind) {
    case TRACK_OP_KEY:       return SPN_DAG_TRACE_KEY;
    case TRACK_OP_DISCOVERY: return SPN_DAG_TRACE_DISCOVERY;
    case TRACK_OP_CACHE:     return SPN_DAG_TRACE_CACHE;
    case TRACK_OP_EXECUTE:   return SPN_DAG_TRACE_EXECUTE;
    case TRACK_OP_RESOLVE:   return SPN_DAG_TRACE_RESOLVE;
    case TRACK_OP_COMMIT:    return SPN_DAG_TRACE_COMMIT;
    case TRACK_OP_SETTLE:    return SPN_DAG_TRACE_SETTLE;
    default:                 break;
  }
  sp_unreachable_return(SPN_DAG_TRACE_KEY);
}

sp_test_each(dag_track, ops, test_t, tests) {
  sp_mem_t mem = sp_test_arena(t);
  sp_dag_track_t track = sp_zero;
  sp_dag_track_init(&track, mem, it->persist);
  sp_da(sp_dag_track_event_t) events = sp_da_new(mem, sp_dag_track_event_t);

  sp_carr_for(it->ops, ot) {
    const op_t* op = &it->ops[ot];
    if (op->kind == TRACK_OP_DONE) {
      break;
    }
    switch (op->kind) {
      case TRACK_OP_DONE: {
        break;
      }
      case TRACK_OP_KEY:
      case TRACK_OP_DISCOVERY:
      case TRACK_OP_CACHE:
      case TRACK_OP_EXECUTE:
      case TRACK_OP_RESOLVE:
      case TRACK_OP_COMMIT:
      case TRACK_OP_SETTLE: {
        sp_da_push(events, ((sp_dag_track_event_t) {
          .event = {
            .kind = event_kind(op->kind),
            .action = { .index = op->action, .occupied = true },
            .key = op->key ? dag_test_digest(op->key) : (spn_dag_digest_t) sp_zero,
            .present = op->present,
            .hit = op->hit,
          },
          .sys = op->sys,
        }));
        break;
      }
      case TRACK_OP_RUN: {
        sp_dag_track_run(&track, events, sp_da_size(events), op->crash_at, op->lossy);
        sp_da_clear(events);
        break;
      }
      case TRACK_OP_DROP_ENTRY: {
        sp_dag_track_drop_entry(&track, dag_test_digest(op->key));
        break;
      }
      case TRACK_OP_DROP_BLOB: {
        sp_dag_track_drop_blob(&track, dag_test_digest(op->key));
        break;
      }
      case TRACK_OP_REBOOT: {
        sp_dag_track_reboot(&track);
        break;
      }
      case TRACK_OP_RESET_ENTRIES: {
        sp_dag_track_reset_entries(&track);
        break;
      }
      case TRACK_OP_RESET_DISCOVERY: {
        sp_dag_track_reset_discovery(&track);
        break;
      }
    }
  }

  sp_carr_for(it->checks, ct) {
    const check_t* check = &it->checks[ct];
    if (!check->key) {
      break;
    }
    sp_test_kv(t, "check", sp_fmt(mem, "{}", sp_fmt_uint(ct)).value);
    sp_test_kv_c(t, "key", check->key);
    sp_dag_track_slot_t slot = sp_zero;
    switch (check->kind) {
      case SLOT_ENTRY: {
        slot = sp_dag_track_entry(&track, dag_test_digest(check->key));
        break;
      }
      case SLOT_BLOB: {
        slot = sp_dag_track_blob(&track, dag_test_digest(check->key));
        break;
      }
      case SLOT_PATHSET: {
        slot = sp_dag_track_pathset(&track, dag_test_digest(check->key));
        break;
      }
    }
    sp_expect_eq(t, check->present, slot.present);
    sp_expect_eq(t, check->sure, slot.sure);
  }

  return SP_OK;
}
