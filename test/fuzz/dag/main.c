#define SP_IMPLEMENTATION
#include "sp.h"
#include "sp/sp_cli.h"

#include "fuzz.h"

sp_str_t fz_err_to_str(fz_err_t err) {
  switch (err) {
    case FZ_OK:               return sp_str_lit("ok");
    case FZ_ERR_GEN_PRODUCER: return sp_str_lit("generator wired an output to the wrong producer");
    case FZ_ERR_GEN_EDGE:     return sp_str_lit("generator emitted an edge to an artifact that does not exist");
    case FZ_ERR_GEN_OBS:      return sp_str_lit("generator scripted an observation that cannot be resolved");
    case FZ_ERR_GEN_CYCLE:    return sp_str_lit("generator built a cycle without injecting back-edges");
    case FZ_ERR_COUNT:        break;
  }
  sp_unreachable_return(sp_str_lit("unknown"));
}

static sp_str_t fz_err_str(u32 err) {
  return fz_err_to_str((fz_err_t)err);
}

static u32 fz_run_iteration(sp_mem_t mem, sp_fuzz_prng_t prng, u64 iter) {
  fz_profile_t profile = fz_gen_profile(&prng);
  fz_universe_t universe = fz_gen_universe(mem, &prng, profile);

  fz_sim_t sim = sp_zero;
  fz_sim_init(&sim, mem);
  fz_sim_install(&sim);
  fz_sim_remove(&sim);

  sp_str_t render = sp_fuzz_render_path();
  if (!sp_str_empty(render)) {
    fz_render_iteration(mem, render, &universe, iter);
  }

  return (u32)fz_check_universe(&universe);
}

s32 main(s32 num_args, c8** args) {
  sp_fuzz_desc_t desc = {
    .name = "fuzz_dag",
    .summary = "Deterministic fuzzer for the content-addressed DAG",
    .iters = 512,
    .errs = FZ_ERR_COUNT,
    .err_str = fz_err_str,
    .run = fz_run_iteration,
  };
  return sp_fuzz_main(num_args, args, &desc);
}
