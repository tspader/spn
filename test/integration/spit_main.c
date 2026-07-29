#define SP_IMPLEMENTATION
#include "sp.h"

#include "spn_test.h"
#include "spit_harness.h"

#include "spit_harness_test.c"
#include "spit_cli.c"
#include "spit_options.c"
#include "spit_worlds.c"

s32 main(s32 argc, const c8** argv) {
  bool jobs = false;
  sp_for(it, (u32)argc) {
    if (sp_str_starts_with(sp_cstr_as_str(argv[it]), sp_str_lit("--jobs"))) {
      jobs = true;
    }
  }
  if (jobs) {
    return sp_test_main(argc, argv, SP_NULLPTR);
  }

  sp_mem_t mem = sp_mem_os_new();
  const c8** args = sp_alloc_n(mem, const c8*, argc + 1);
  sp_for(it, (u32)argc) {
    args[it] = argv[it];
  }
  args[argc] = "--jobs=0";
  return sp_test_main(argc + 1, args, SP_NULLPTR);
}
