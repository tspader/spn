#ifndef SPN_FUZZ_DAG_H
#define SPN_FUZZ_DAG_H

#include "sp.h"

#include "sp_fuzz.h"
#include "dag/dag.h"
#include "sp_sim.h"

typedef struct {
  u64 actions;
  u64 small_actions;
  u64 sources;
  u64 produces;
  u64 phantoms;
  u64 obs;
} fz_limits_t;

typedef enum {
  FZ_OK = 0,
  FZ_ERR_RUN_FAILED,
  FZ_ERR_RUN_CYCLIC,
  FZ_ERR_STALE_OUTPUT,
  FZ_ERR_EXEC_MISSING,
  FZ_ERR_EXEC_SPURIOUS,
  FZ_ERR_MODEL,
  FZ_ERR_SCHEDULE,
  FZ_ERR_COUNT,
} fz_err_t;

typedef enum {
  FZ_STEP_RUN,
  FZ_STEP_MUTATE,
  FZ_STEP_TOUCH,
  FZ_STEP_REVERT,
  FZ_STEP_STEALTH,
  FZ_STEP_DELETE,
  FZ_STEP_PHANTOM,
  FZ_STEP_DISCOVERY,
  FZ_STEP_EIO,
  FZ_STEP_CRASH,
  FZ_STEP_BLOB,
  FZ_STEP_EVICT,
  FZ_STEP_COUNT,
} fz_step_kind_t;

typedef struct {
  fz_step_kind_t kind;
  u64 artifact;
  u64 content;
  u64 entropy;
  u64 rate;
} fz_step_t;

typedef struct {
  sp_da(fz_step_t) steps;
} fz_trace_t;

typedef enum {
  FZ_ARTIFACT_VALUE,
  FZ_ARTIFACT_SOURCE,
  FZ_ARTIFACT_OUTPUT,
} fz_artifact_kind_t;

typedef struct {
  fz_artifact_kind_t kind;
  u64 content;
  s64 producer;
} fz_artifact_t;

typedef struct {
  bool probe;
  u64 artifact;
  u64 phantom;
} fz_obs_t;

typedef struct {
  bool present;
  u64 content;
} fz_phantom_t;

typedef struct {
  u64* contents;
  fz_phantom_t* phantoms;
} fz_state_t;

typedef enum {
  FZ_WORLD_CLEAN,
  FZ_WORLD_STEALTHY,
  FZ_WORLD_MURKY,
  FZ_WORLD_TAINTED,
} fz_world_state_t;

typedef struct {
  bool* file;
} fz_shape_t;

typedef struct {
  bool discover;
  u64 identity;
  sp_da(u64) consumes;
  sp_da(u64) produces;
  sp_da(fz_obs_t) obs;
} fz_action_t;

typedef struct {
  fz_limits_t limits;
  u64 action_count;
  u64 source_count;
  u64 value_count;
  u64 content_count;
  u64 identity_count;
  u64 density;
  u64 out_degree;
  u64 produce_count;
  u64 discover_pct;
  u64 obs_count;
  u64 absent_pct;
  u64 obs_output_pct;
  u64 back_density;
  u64 steps;
  u64 step_weights [FZ_STEP_COUNT];
  bool store_fs;
  bool disco_fs;
  bool cache_fs;
  bool run_ex;
  bool big;
} fz_profile_t;

typedef struct {
  fz_profile_t profile;
  sp_da(fz_artifact_t) artifacts;
  sp_da(fz_action_t) actions;
  sp_da(u64) order;
  sp_da(u64) outputs;
  bool cyclic;
  bool obs_cyclic;
} fz_universe_t;

typedef struct {
  u64 action;
  u64 submitted;
  u64 started;
} fz_flight_t;

typedef struct {
  spn_dag_executor_t base;
  sp_fuzz_prng_t prng;
  sp_sim_t* sim;
  sp_da(spn_dag_job_t) jobs;
  sp_da(u64) submitted;
  sp_da(fz_flight_t) log;
  s64 ran;
} fz_executor_t;

typedef struct {
  sp_mem_t mem;
  sp_sim_t* sim;
  sp_da(sp_str_t) lines;
} fz_journal_t;

typedef struct {
  fz_universe_t* u;
  const fz_state_t* state;
  spn_dag_t* g;
  sp_mem_t mem;
  fz_executor_t* ex;
  fz_journal_t* journal;
  sp_da(spn_dag_id_t) ids;
  sp_da(u64) execs;
} fz_lowered_t;

#define try(expr) do { fz_err_t __err = (expr); if (__err) return __err; } while (0)
#define must(expr, err) do { if (!(expr)) return err; } while (0)

sp_str_t fz_err_to_str(fz_err_t err);

sp_str_t fz_artifact_path(sp_mem_t mem, fz_universe_t* u, u64 artifact);
sp_str_t fz_artifact_sim_path(sp_mem_t mem, fz_universe_t* u, u64 artifact);
sp_str_t fz_phantom_path(sp_mem_t mem, u64 phantom);
sp_str_t fz_phantom_sim_path(sp_mem_t mem, u64 phantom);
sp_str_t fz_content(sp_mem_t mem, u64 content);
sp_str_t fz_output_name(sp_mem_t mem, u64 artifact);

fz_limits_t   fz_gen_limits(const spn_cg_fuzz_graph_t* graph);
fz_profile_t  fz_gen_profile(sp_fuzz_prng_t* prng, fz_limits_t limits);
fz_universe_t fz_gen_universe(sp_mem_t mem, sp_fuzz_prng_t* prng, fz_profile_t profile);
fz_trace_t    fz_gen_trace(sp_mem_t mem, sp_fuzz_prng_t* prng, fz_universe_t* u);

void             fz_lower(fz_lowered_t* low, sp_mem_t mem, fz_universe_t* u);
sp_str_t         fz_output_content(sp_mem_t mem, u64 identity, const sp_str_t* inputs, u64 count, sp_str_t name);
void             fz_expect(sp_mem_t mem, fz_universe_t* u, const fz_state_t* state, sp_str_t* bytes);
u64              fz_action_inputs(sp_mem_t mem, fz_universe_t* u, const fz_state_t* state, u64 action, const sp_str_t* bytes, sp_str_t** inputs);
spn_dag_digest_t fz_model_key(fz_universe_t* u, const sp_str_t* bytes, u64 action);
fz_shape_t       fz_shape_now(sp_mem_t mem, fz_universe_t* u, const fz_state_t* state, u64 action);
spn_dag_digest_t fz_model_strong(fz_universe_t* u, const fz_state_t* state, const sp_str_t* bytes, spn_dag_digest_t prelim, u64 action, const fz_shape_t* shape);
void             fz_executor_init(fz_executor_t* ex, sp_mem_t mem, sp_sim_t* sim, sp_fuzz_prng_t prng);
fz_err_t         fz_run_trace(sp_mem_t mem, sp_fuzz_prng_t* prng, fz_universe_t* u, fz_trace_t* trace, fz_journal_t* j);

typedef struct {
  u64 action;
  spn_dag_digest_t key;
  bool resolved;
  bool hit;
} fz_predict_row_t;

typedef struct {
  u64 action;
  u64 execs;
  u64 want;
  u64 requeues;
  bool miss;
  bool ok;
} fz_exec_row_t;

typedef struct {
  u64 artifact;
  sp_str_t want;
  sp_str_t got;
  bool ok;
} fz_bytes_row_t;

void fz_journal_init(fz_journal_t* j, sp_mem_t mem);
void fz_journal_universe(fz_journal_t* j, fz_universe_t* u, fz_trace_t* trace, u64 iter);
void fz_journal_step(fz_journal_t* j, fz_step_t* step, u64 index);
void fz_journal_run_done(fz_journal_t* j, u64 err, u64 fired, bool crashed);
void fz_journal_world(fz_journal_t* j, fz_world_state_t world);
void fz_journal_predict(fz_journal_t* j, const fz_predict_row_t* rows, u64 count);
void fz_journal_exec(fz_journal_t* j, u64 action);
void fz_journal_check_execs(fz_journal_t* j, const fz_exec_row_t* rows, u64 count);
void fz_journal_check_bytes(fz_journal_t* j, sp_str_t kind, const fz_bytes_row_t* rows, u64 count);
void fz_journal_blob(fz_journal_t* j, u64 artifact, sp_str_t want, sp_str_t got);
void fz_journal_drop(fz_journal_t* j, sp_str_t what, sp_str_t path);
void fz_journal_sim_fault(fz_journal_t* j, u64 sys);
void fz_journal_pass(fz_journal_t* j, sp_str_t name);
void fz_journal_sim_write(fz_journal_t* j, u64 artifact, sp_str_t path, u64 sys);
void fz_journal_done(fz_journal_t* j, fz_err_t err);
void fz_journal_trace_hook(const spn_dag_trace_event_t* event, void* user_data);
void fz_journal_write(fz_journal_t* j, sp_io_writer_t* io);

void     fz_render_mermaid(sp_io_writer_t* io, fz_universe_t* u);
sp_str_t fz_render_iteration(sp_mem_t mem, sp_str_t root, fz_universe_t* u, fz_trace_t* trace, u64 iter);

#endif
