#ifndef SPN_FUZZ_DAG_H
#define SPN_FUZZ_DAG_H

#include "sp.h"

#include "sp_fuzz.h"

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
  FZ_ERR_COUNT,
} fz_err_t;

typedef enum {
  FZ_ARTIFACT_VALUE,
  FZ_ARTIFACT_SOURCE,
  FZ_ARTIFACT_OUTPUT,
} fz_artifact_kind_t;

typedef struct {
  fz_artifact_kind_t kind;
  u32 content;
  s32 producer;
} fz_artifact_t;

typedef struct {
  bool absent;
  u32 artifact;
  u32 phantom;
} fz_obs_t;

typedef struct {
  bool discover;
  u32 identity;
  sp_da(u32) consumes;
  sp_da(u32) produces;
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
  bool big;
} fz_profile_t;

typedef struct {
  fz_profile_t profile;
  sp_da(fz_artifact_t) artifacts;
  sp_da(fz_action_t) actions;
  bool cyclic;
} fz_universe_t;

#define try(expr) do { fz_err_t __err = (expr); if (__err) return __err; } while (0)
#define must(expr, err) do { if (!(expr)) return err; } while (0)

sp_str_t fz_err_to_str(fz_err_t err);

sp_str_t fz_artifact_path(sp_mem_t mem, fz_universe_t* u, u32 artifact);
sp_str_t fz_phantom_path(sp_mem_t mem, u32 phantom);

fz_profile_t  fz_gen_profile(sp_fuzz_prng_t* prng);
fz_universe_t fz_gen_universe(sp_mem_t mem, sp_fuzz_prng_t* prng, fz_profile_t profile);
bool          fz_universe_cyclic(fz_universe_t* u);
fz_err_t      fz_check_universe(fz_universe_t* u);

void fz_render_mermaid(sp_io_writer_t* io, fz_universe_t* u);
void fz_render_iteration(sp_mem_t mem, sp_str_t root, fz_universe_t* u, u64 iter);

#endif
