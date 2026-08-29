#define SP_IMPLEMENTATION
#include "sp.h"
#include "sp/sp_cli.h"
#include "sp/atomic_file.h"
#include "sp/sp_glob.h"
#include "sp/sp_prompt.h"

#include "fuzz.h"
#include "fuzz.gen.h"

sp_str_t fz_err_to_str(fz_err_t err) {
  switch (err) {
    case FZ_OK:               return sp_str_lit("ok");
    case FZ_ERR_RUN_FAILED:   return sp_str_lit("run failed where the model expected success");
    case FZ_ERR_RUN_CYCLIC:   return sp_str_lit("run succeeded on a cyclic graph");
    case FZ_ERR_STALE_OUTPUT: return sp_str_lit("an output's bytes diverge from the model");
    case FZ_ERR_EXEC_MISSING: return sp_str_lit("an action hit the cache where the model expected execution");
    case FZ_ERR_EXEC_SPURIOUS: return sp_str_lit("an action executed where the model expected a cache hit");
    case FZ_ERR_KEY:          return sp_str_lit("a dag key diverges from the model");
    case FZ_ERR_SCHEDULE:     return sp_str_lit("a schedule reseed changed output bytes");
    case FZ_ERR_COUNT:        break;
  }
  sp_unreachable_return(sp_str_lit("unknown"));
}

static sp_str_t err_str(u32 err) {
  return fz_err_to_str((fz_err_t)err);
}

static void write_failure(sp_mem_t mem, sp_str_t dir, fz_err_t err, u64 iter) {
  sp_io_file_writer_t out = sp_zero;
  if (sp_io_file_writer_from_path(&out, sp_fs_join_path(mem, dir, sp_str_lit("failure.json")))) {
    return;
  }
  sp_fmt_io(&out.base, "{{\"err\":{},\"str\":\"{}\",\"seed\":\"0x{:x}\",\"iter\":{},\"repro\":\"{}\"}}\n",
    sp_fmt_uint((u64)err), sp_fmt_str(fz_err_to_str(err)), sp_fmt_uint(sp_fuzz_seed_get()), sp_fmt_uint(iter),
    sp_fmt_str(sp_fuzz_repro_args(mem, iter)));
  sp_io_file_writer_close(&out);
}

static u32 run_iteration(sp_mem_t mem, sp_fuzz_prng_t prng, u64 iter) {
  fz_profile_t profile = fz_gen_profile(&prng, fz_gen_limits(sp_fuzz_graph()));
  if (sp_fuzz_sweep()) {
    profile.step_weights[FZ_STEP_EIO] = 0;
    profile.step_weights[FZ_STEP_CRASH] = 0;
  }
  fz_universe_t universe = fz_gen_universe(mem, &prng, profile);
  fz_trace_t trace = fz_gen_trace(mem, &prng, &universe);

  fz_journal_t journal = sp_zero;
  fz_journal_t* j = SP_NULLPTR;
  sp_str_t dir = sp_zero;
  sp_str_t render = sp_fuzz_render_path();
  if (!sp_str_empty(render)) {
    dir = fz_render_iteration(mem, render, &universe, &trace, iter);
    fz_journal_init(&journal, mem);
    fz_journal_universe(&journal, &universe, &trace, iter);
    j = &journal;
  }

  fz_err_t err = sp_fuzz_sweep() && !universe.cyclic && !universe.obs_cyclic
    ? fz_run_sweep(mem, &prng, &universe, &trace, j)
    : fz_run_trace(mem, &prng, &universe, &trace, j);

  if (j) {
    fz_journal_done(j, err);
    sp_io_file_writer_t out = sp_zero;
    if (!sp_io_file_writer_from_path(&out, sp_fs_join_path(mem, dir, sp_str_lit("journal.jsonl")))) {
      fz_journal_write(j, &out.base);
      sp_io_file_writer_close(&out);
    }
    if (err) {
      write_failure(mem, dir, err, iter);
    }
  }

  return (u32)err;
}

s32 main(s32 num_args, c8** args) {
  sp_fuzz_desc_t desc = {
    .name = "fuzz_dag",
    .summary = "Deterministic fuzzer for the content-addressed DAG",
    .iters = 512,
    .errs = FZ_ERR_COUNT,
    .err_str = err_str,
    .run = run_iteration,
  };
  return sp_fuzz_main(num_args, args, &desc);
}
