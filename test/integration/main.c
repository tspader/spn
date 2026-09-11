#define SP_IMPLEMENTATION
#include "sp.h"

#include "harness.h"

s32 main(s32 argc, const c8** argv) {
  bool jobs = false;
  bool dir = false;
  sp_for(it, (u32)argc) {
    sp_str_t arg = sp_cstr_as_str(argv[it]);
    if (sp_str_starts_with(arg, sp_str_lit("--jobs"))) jobs = true;
    if (sp_str_starts_with(arg, sp_str_lit("--dir"))) dir = true;
  }
  const test_toolchain_t* lane = test_toolchain();

  sp_mem_t mem = sp_mem_os_new();
  const c8** args = sp_alloc_n(mem, const c8*, argc + 2);
  u32 num_args = 0;
  sp_for(it, (u32)argc) {
    args[num_args++] = argv[it];
  }
  if (!jobs) {
    args[num_args++] = "--jobs=0";
  }
  if (!dir) {
    args[num_args++] = sp_str_to_cstr(mem, sp_fmt(mem, "--dir=.spn/test/{}", sp_fmt_cstr(lane->name)).value);
  }
  return sp_test_main((s32)num_args, args, SP_NULLPTR);
}
