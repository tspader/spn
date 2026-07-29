#define SP_IMPLEMENTATION
#include "sp.h"
#include "sp/atomic_file.h"

#include "spn_test.h"
#include "sp_sim.h"

#include "ctx/types.h"
#include "event/event.h"
#include "intern/intern.h"

spn_ctx_t spn;

static sp_test_once_t spn_ctx_once;
static sp_intern_t* spn_ctx_intern;
static spn_event_buffer_t* spn_ctx_events;

static sp_err_t spn_ctx_init(void* user) {
  spn.mem = sp_mem_os_new();
  spn_ctx_intern = sp_intern_new(spn.mem);
  spn_ctx_events = spn_event_buffer_new(spn.mem);
  return SP_OK;
}

sp_err_t spn_test_ctx_setup(sp_test_t* t) {
  sp_err_t err = sp_test_once(&spn_ctx_once, spn_ctx_init, SP_NULLPTR);
  spn.intern = spn_ctx_intern;
  spn.events = spn_ctx_events;
  return err;
}

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
