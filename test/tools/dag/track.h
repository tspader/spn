#ifndef SP_DAG_TRACK_H
#define SP_DAG_TRACK_H

#include "sp.h"
#include "dag/types.h"

typedef struct {
  bool present;
  bool sure;
} sp_dag_track_slot_t;

typedef struct {
  spn_dag_trace_event_t event;
  u64 sys;
} sp_dag_track_event_t;

typedef struct {
  bool entries;
  bool blobs;
  bool pathsets;
} sp_dag_track_persist_t;

typedef enum {
  SP_DAG_TRACK_SURE,
  SP_DAG_TRACK_VOLATILE,
  SP_DAG_TRACK_UNKNOWN,
} sp_dag_track_state_t;

typedef sp_ht(spn_dag_digest_t, sp_dag_track_state_t) sp_dag_track_table_t;

typedef struct {
  sp_mem_t mem;
  sp_dag_track_persist_t persist;
  sp_dag_track_table_t entries;
  sp_dag_track_table_t blobs;
  sp_dag_track_table_t pathsets;
} sp_dag_track_t;

void                sp_dag_track_init(sp_dag_track_t* track, sp_mem_t mem, sp_dag_track_persist_t persist);
void                sp_dag_track_run(sp_dag_track_t* track, const sp_dag_track_event_t* events, u64 count, u64 crash_at, bool lossy);
void                sp_dag_track_reboot(sp_dag_track_t* track);
void                sp_dag_track_reset_entries(sp_dag_track_t* track);
void                sp_dag_track_reset_discovery(sp_dag_track_t* track);
void                sp_dag_track_drop_entry(sp_dag_track_t* track, spn_dag_digest_t key);
void                sp_dag_track_drop_blob(sp_dag_track_t* track, spn_dag_digest_t digest);
sp_dag_track_slot_t sp_dag_track_entry(sp_dag_track_t* track, spn_dag_digest_t key);
sp_dag_track_slot_t sp_dag_track_blob(sp_dag_track_t* track, spn_dag_digest_t digest);
sp_dag_track_slot_t sp_dag_track_pathset(sp_dag_track_t* track, spn_dag_digest_t weak);

#endif
