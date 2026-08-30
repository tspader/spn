#define SP_IMPLEMENTATION
#include "sp.h"
#include "sp/sp_test.h"

#include "sim/sim.h"

static sp_sim_t sim;
static sp_sys_vtable_t sim_vtable;

static s64 sim_get_cwd_path(c8* buf, u64 size) {
  sp_str_t cwd = sp_str_lit("/sim");
  if (size < cwd.len) {
    return -1;
  }
  sp_mem_copy(buf, cwd.data, cwd.len);
  return (s64)cwd.len;
}

s32 main(s32 argc, const c8** argv) {
  if (!sp_str_empty(sp_os_env_get(sp_str_lit("SPN_TEST_SIM")))) {
    sp_sim_init(&sim, sp_mem_os_new());
    sp_sim_install(&sim);
    sim_vtable = *sp_rt.vt;
    sim_vtable.get_cwd_path = sim_get_cwd_path;
    sp_sys_set_vtable(&sim_vtable);
  }
  return sp_test_main(argc, argv, SP_NULLPTR);
}
