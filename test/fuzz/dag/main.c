#define SP_IMPLEMENTATION
#include "sp.h"
#include "sp/sp_cli.h"
#include "sp/atomic_file.h"
#include "sp/sp_glob.h"
#include "sp/prompt.h"

#include "fuzz.h"

sp_str_t fz_err_to_str(fz_err_t err) {
  switch (err) {
    case FZ_OK:               return sp_str_lit("ok");
    case FZ_ERR_GEN_PRODUCER: return sp_str_lit("generator wired an output to the wrong producer");
    case FZ_ERR_GEN_EDGE:     return sp_str_lit("generator emitted an edge to an artifact that does not exist");
    case FZ_ERR_GEN_OBS:      return sp_str_lit("generator scripted an observation that cannot be resolved");
    case FZ_ERR_GEN_CYCLE:    return sp_str_lit("generator built a cycle without injecting back-edges");
    case FZ_ERR_RUN_FAILED:   return sp_str_lit("run failed where the model expected success");
    case FZ_ERR_RUN_CYCLIC:   return sp_str_lit("run succeeded on a cyclic graph");
    case FZ_ERR_STALE_OUTPUT: return sp_str_lit("an output's bytes diverge from the model");
    case FZ_ERR_EXEC_MISSING: return sp_str_lit("an action hit the cache where the model expected execution");
    case FZ_ERR_EXEC_SPURIOUS: return sp_str_lit("an action executed where the model expected a cache hit");
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
  fz_trace_t trace = fz_gen_trace(mem, &prng, &universe);

  sp_str_t render = sp_fuzz_render_path();
  if (!sp_str_empty(render)) {
    fz_render_iteration(mem, render, &universe, &trace, iter);
  }

  fz_err_t err = fz_check_universe(&universe);
  if (err) {
    return (u32)err;
  }
  return (u32)fz_run_trace(mem, &universe, &trace);
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
