#include "spn_test.h"

#include "event/event.h"
#include "intern/intern.h"
#include "codegen/toolchain.h"
#include "toml/loader.h"
#include "toolchain/catalog.h"
#include "lanes.h"

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

void spn_test_lower_toolchains(sp_test_t* t, sp_str_t toml, spn_path_root_t base, sp_da(spn_toolchain_decl_t)* decls, sp_da(spn_codegen_issue_t)* issues) {
  sp_mem_t mem = sp_test_arena(t);
  spn_toml_loader_t loader = sp_zero;
  spn_toml_loader_init(&loader, mem, sp_intern_new(mem));
  *decls = spn_toolchains_lower(&loader, toml, base);
  *issues = loader.issues;
}

sp_err_t spn_test_builtin_toml(sp_test_t* t, sp_str_t* toml) {
  sp_mem_t mem = sp_test_arena(t);
  sp_str_t path = test_repo_path(mem, sp_str_lit(SPN_LANES_BUILTIN));
  sp_must_ok(t, sp_io_read_file(mem, path, toml));
  return SP_OK;
}

sp_err_t spn_test_builtin_catalog(sp_test_t* t, spn_toolchain_catalog_t* catalog, spn_triple_t host) {
  sp_str_t toml = sp_zero;
  if (spn_test_builtin_toml(t, &toml)) {
    return SP_ERR;
  }
  sp_da(spn_toolchain_decl_t) decls = SP_NULLPTR;
  sp_da(spn_codegen_issue_t) issues = SP_NULLPTR;
  spn_test_lower_toolchains(t, toml, SPN_PATH_ROOT_NONE, &decls, &issues);
  sp_must(t, sp_da_empty(issues));
  spn_toolchain_catalog_init(catalog, host, sp_zero_struct(spn_sdk_host_t), sp_test_arena(t));
  sp_da_for(decls, it) {
    spn_toolchain_catalog_add(catalog, decls[it]);
  }
  return SP_OK;
}
