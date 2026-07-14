#ifndef SPN_FUZZ_DAG_H
#define SPN_FUZZ_DAG_H

#include "sp.h"

#include "sp_fuzz.h"
#include "dag/dag.h"
#include "sp_sim.h"

#define FZ_MAX_ACTIONS 128
#define FZ_SMALL_ACTIONS 8
#define FZ_MAX_PRODUCES 3
#define FZ_MAX_PHANTOMS 4

typedef enum {
  FZ_OK = 0,
  FZ_ERR_GEN_PRODUCER,
  FZ_ERR_GEN_EDGE,
  FZ_ERR_GEN_OBS,
  FZ_ERR_GEN_CYCLE,
  FZ_ERR_RUN_FAILED,
  FZ_ERR_RUN_CYCLIC,
  FZ_ERR_STALE_OUTPUT,
  FZ_ERR_EXEC_MISSING,
  FZ_ERR_EXEC_SPURIOUS,
  FZ_ERR_MODEL,
  FZ_ERR_COUNT,
} fz_err_t;

typedef enum {
  FZ_STEP_RUN,
  FZ_STEP_MUTATE,
  FZ_STEP_TOUCH,
  FZ_STEP_REVERT,
  FZ_STEP_STEALTH,
  FZ_STEP_DELETE,
  FZ_STEP_COUNT,
} fz_step_kind_t;

typedef struct {
  fz_step_kind_t kind;
  u64 artifact;
  u64 content;
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
  bool absent;
  u64 artifact;
  u64 phantom;
} fz_obs_t;

typedef struct {
  bool discover;
  u64 identity;
  sp_da(u64) consumes;
  sp_da(u64) produces;
  sp_da(fz_obs_t) obs;
} fz_action_t;

typedef struct {
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
  bool big;
} fz_profile_t;

typedef struct {
  fz_profile_t profile;
  sp_da(fz_artifact_t) artifacts;
  sp_da(fz_action_t) actions;
  bool cyclic;
} fz_universe_t;

typedef struct {
  fz_universe_t* u;
  spn_dag_t* g;
  sp_mem_t mem;
  sp_da(spn_dag_id_t) ids;
  sp_da(u64) execs;
} fz_lowered_t;

#define try(expr) do { fz_err_t __err = (expr); if (__err) return __err; } while (0)
#define must(expr, err) do { if (!(expr)) return err; } while (0)

sp_str_t fz_err_to_str(fz_err_t err);

sp_str_t fz_artifact_path(sp_mem_t mem, fz_universe_t* u, u64 artifact);
sp_str_t fz_artifact_sim_path(sp_mem_t mem, fz_universe_t* u, u64 artifact);
sp_str_t fz_phantom_path(sp_mem_t mem, u64 phantom);
sp_str_t fz_content(sp_mem_t mem, u64 content);
sp_str_t fz_output_name(sp_mem_t mem, u64 artifact);

fz_profile_t  fz_gen_profile(sp_fuzz_prng_t* prng);
fz_universe_t fz_gen_universe(sp_mem_t mem, sp_fuzz_prng_t* prng, fz_profile_t profile);
fz_trace_t    fz_gen_trace(sp_mem_t mem, sp_fuzz_prng_t* prng, fz_universe_t* u);
bool          fz_universe_cyclic(fz_universe_t* u);
fz_err_t      fz_check_universe(fz_universe_t* u);

void             fz_lower(fz_lowered_t* low, sp_mem_t mem, fz_universe_t* u);
sp_str_t         fz_output_content(sp_mem_t mem, u64 identity, const sp_str_t* inputs, u64 count, sp_str_t name);
void             fz_expect(sp_mem_t mem, fz_universe_t* u, sp_str_t* bytes);
spn_dag_digest_t fz_model_key(fz_universe_t* u, const sp_str_t* bytes, u64 action);
fz_err_t         fz_run_trace(sp_mem_t mem, fz_universe_t* u, fz_trace_t* trace);

void fz_render_mermaid(sp_io_writer_t* io, fz_universe_t* u);
void fz_render_iteration(sp_mem_t mem, sp_str_t root, fz_universe_t* u, fz_trace_t* trace, u64 iter);

#endif
