// The sp and spit implementations live alone in this TU, like main.c does
// for the utest binaries
#define SP_IMPLEMENTATION
#include "sp.h"
#include "sp/atomic_file.h"

#include "spit.h"
#include "sp_sim.h"

static sp_sim_t dag_sim;
static sp_sys_vtable_t dag_sim_vtable;

// The spit runner anchors its per-test directories at the cwd; the sim traps
// that syscall, so pin the simulated cwd to a fixed virtual root
static s64 dag_sim_get_cwd_path(c8* buf, u64 size) {
  sp_str_t cwd = sp_str_lit("/sim");
  if (size < cwd.len) {
    return -1;
  }
  sp_mem_copy(buf, cwd.data, cwd.len);
  return (s64)cwd.len;
}

s32 main(s32 argc, const c8** argv) {
  if (!sp_str_empty(sp_os_env_get(sp_str_lit("SPN_TEST_SIM")))) {
    sp_sim_init(&dag_sim, sp_mem_os_new());
    sp_sim_install(&dag_sim);
    dag_sim_vtable = *sp_rt.vt;
    dag_sim_vtable.get_cwd_path = dag_sim_get_cwd_path;
    sp_sys_set_vtable(&dag_sim_vtable);
  }
  return sp_test_main(argc, argv, SP_NULLPTR);
}
