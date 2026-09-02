#include "spn_test.h"

#include "event/event.h"
#include "intern/intern.h"
#include "toolchain/catalog.h"

static sp_test_once_t spn_ctx_once;
static sp_intern_t* spn_ctx_intern;

static sp_err_t spn_ctx_init(void* user) {
  spn.mem = sp_mem_os_new();
  spn_ctx_intern = sp_intern_new(spn.mem);
  return SP_OK;
}

sp_err_t spn_test_ctx_setup(sp_test_t* t) {
  sp_err_t err = sp_test_once(&spn_ctx_once, spn_ctx_init, SP_NULLPTR);
  spn.intern = spn_ctx_intern;
  spn.events = spn_event_buffer_new(spn.mem);
  sp_atomic_s32_store(&spn.error, 0, SP_ATOMIC_SEQ_CST);
  return err;
}

sp_da(spn_event_t) spn_test_drain_errs(sp_mem_t mem) {
  sp_da(spn_event_t) errs = sp_da_new(mem, spn_event_t);
  sp_da(spn_event_t) events = spn_event_buffer_drain(mem, spn.events);
  sp_da_for(events, it) {
    if (events[it].kind == SPN_EVENT_ERR) {
      sp_da_push(errs, events[it]);
    }
  }
  return errs;
}

sp_err_t spn_test_builtin_catalog(sp_test_t* t, spn_toolchain_catalog_t* catalog, spn_triple_t host) {
  sp_mem_t mem = sp_test_arena(t);
  sp_str_t path = test_repo_path(mem, sp_str_lit("source/core/toolchain/toolchains.json"));

  sp_str_t json = sp_zero;
  sp_must_ok(t, sp_io_read_file(mem, path, &json));
  spn_toolchain_catalog_init(catalog, host, mem);
  sp_must_eq(t, (u32)SPN_OK, (u32)spn_toolchain_catalog_load(catalog, json));
  return SP_OK;
}
