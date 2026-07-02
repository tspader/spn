#include "sp.h"
#include "external/wasm/wasm.h"
#include "error/types.h"
#include "intern/intern.h"

#include "sp/macro.h"
#include "spn.h"
#include "sp/sp_om.h"
#include "ctx/types.h"
#include "event/types.h"
#include "event/event.h"
#include "unit/types.h"
#include "wasm/types.h"
#include "wasm_export.h"

#include "external/wasm/abi.h"

#define SPN_WASM_STACK_SIZE (64 * 1024)
#define SPN_WASM_HEAP_SIZE  (64 * 1024)

void spn_wasm_init(spn_wasm_t* wasm, sp_mem_t mem, sp_intern_t* interner) {
  wasm->mem = mem;
  wasm->interner = interner;
  sp_str_om_init(wasm->modules);
}

spn_wasm_module_t* spn_wasm_get_module(spn_wasm_t* wasm, sp_str_t path) {
  return *sp_om_get(wasm->modules, path);
}

spn_err_t spn_wasm_load_module(spn_wasm_t* wasm, sp_str_t path, spn_wasm_module_t** module) {
  spn_err_t err = SPN_OK;
  sp_mem_arena_marker_t s = sp_mem_begin_scratch();

  sp_str_t blob = sp_zero;
  if (sp_io_read_file(s.mem, path, &blob)) {
    err = SPN_ERR_WASM_MODULE_LOAD_FAILED; goto done;
  }

  c8 error [128] = sp_zero;
  *module = wasm_runtime_load((u8*)blob.data, blob.len, error, sizeof(error));
  if (!*module) {
    err = SPN_ERR_WASM_MODULE_LOAD_FAILED; goto done;
  }

  sp_om_insert(wasm->modules, path, *module);

done:
  sp_mem_end_scratch(s);
  return err;
}

void spn_wasm_pkg_init(spn_wasm_pkg_t* wasm) {
  sp_str_om_init(wasm->functions);
}

spn_err_t spn_wasm_instantiate_module(spn_wasm_t* wasm, spn_wasm_module_t* module, spn_wasm_pkg_t* result) {
  c8 error [128] = sp_zero;

  spn_wasm_pkg_t pkg = sp_zero;
  spn_wasm_pkg_init(&pkg);

  pkg.instance = wasm_runtime_instantiate(module, SPN_WASM_STACK_SIZE, SPN_WASM_HEAP_SIZE, error, sizeof(error));
  if (!pkg.instance) return SPN_ERR_WASM_MODULE_INSTANCE_FAILED;

  pkg.ctx = wasm_runtime_create_exec_env(pkg.instance, SPN_WASM_STACK_SIZE);
  if (!pkg.ctx) return SPN_ERR_WASM_CTX_FAILED;

  s32 count = wasm_runtime_get_export_count(module);
  if (count < 0) return SPN_ERR_WASM_CTX_FAILED;

  sp_for(it, (u32)count) {
    wasm_export_t entry = sp_zero;
    wasm_runtime_get_export_type(module, it, &entry);

    switch (entry.kind) {
      case WASM_IMPORT_EXPORT_KIND_TABLE:
      case WASM_IMPORT_EXPORT_KIND_MEMORY:
      case WASM_IMPORT_EXPORT_KIND_GLOBAL: break;
      case WASM_IMPORT_EXPORT_KIND_FUNC: {
        spn_wasm_fn_t* fn = wasm_runtime_lookup_function(pkg.instance, entry.name);
        sp_str_t name = sp_intern_get_or_insert_str(wasm->interner, sp_cstr_as_str(entry.name));
        sp_om_insert(pkg.functions, name, fn);
        break;
      }
    }
  }

  *result = pkg;

  return SPN_OK;
}

spn_err_t spn_wasm_init_stupid_global_runtime() {
  static bool initted = false;
  if (initted) return SPN_OK;
  initted = true;

  if (!wasm_runtime_init()) {
    return SPN_ERR_WASM_INIT_FAILED;
  }

  if (!spn_abi_register()) {
    return SPN_ERR_WASM_REGISTER_FAILED;
  }

  return SPN_OK;
}

spn_err_t spn_wasm_run_configure(spn_pkg_unit_t* unit) {
  // wasm_runtime_thread_env_inited lies in interpreter-only builds (the signal
  // env check is compiled out with AOT), so init unconditionally; it's a no-op
  // on a thread that's already set up
  if (!wasm_runtime_init_thread_env()) {
    return SPN_ERR_WASM_CTX_FAILED;
  }

  spn_err_t err = SPN_OK;
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();

  wasm_module_t module = SP_NULLPTR;
  wasm_module_inst_t instance = SP_NULLPTR;
  wasm_exec_env_t env = SP_NULLPTR;
  sp_str_t detail = sp_zero;

  sp_str_t path = sp_fs_join_path(scratch.mem, unit->paths.generated, sp_str_lit("configure.wasm"));

  sp_str_t blob = sp_zero;
  if (sp_io_read_file(scratch.mem, path, &blob)) {
    detail = sp_str_lit("failed to read module");
    err = SPN_ERR_WASM_MODULE_LOAD_FAILED; goto done;
  }

  c8 error [128] = sp_zero;
  module = wasm_runtime_load((u8*)blob.data, blob.len, error, sizeof(error));
  if (!module) {
    detail = sp_str_copy(spn.mem, sp_cstr_as_str(error));
    err = SPN_ERR_WASM_MODULE_LOAD_FAILED; goto done;
  }

  instance = wasm_runtime_instantiate(module, SPN_WASM_STACK_SIZE, SPN_WASM_HEAP_SIZE, error, sizeof(error));
  if (!instance) {
    detail = sp_str_copy(spn.mem, sp_cstr_as_str(error));
    err = SPN_ERR_WASM_MODULE_INSTANCE_FAILED; goto done;
  }

  env = wasm_runtime_create_exec_env(instance, SPN_WASM_STACK_SIZE);
  if (!env) {
    detail = sp_str_lit("failed to create exec env");
    err = SPN_ERR_WASM_CTX_FAILED; goto done;
  }
  spn_abi_table_t table;
  spn_abi_table_init(&table, scratch.mem);
  wasm_runtime_set_user_data(env, &table);

  // WAMR runs _initialize during instantiation for WASI reactor modules;
  // calling it again trips wasi-libc's double-init trap
  wasm_function_inst_t configure = wasm_runtime_lookup_function(instance, "configure");
  if (!configure) {
    goto done;
  }

  wasm_val_t args [1] = { { .kind = WASM_I32, .of.i32 = (s32)spn_abi_table_add(&table, unit, SPN_ABI_KIND_CTX) } };
  wasm_val_t results [1] = sp_zero;
  if (!wasm_runtime_call_wasm_a(env, configure, 1, results, 1, args)) {
    detail = sp_fmt(spn.mem, "configure: {}", sp_fmt_cstr(wasm_runtime_get_exception(instance))).value;
    err = SPN_ERR_WASM_MODULE_CALL_FAILED; goto done;
  }

  if (results[0].of.i32 != SPN_OK) {
    detail = sp_fmt(spn.mem, "configure returned {}", sp_fmt_int(results[0].of.i32)).value;
    err = SPN_ERR_WASM_MODULE_CALL_FAILED; goto done;
  }

done:
  if (err) {
    spn_event_buffer_push_ex(spn.events, unit->info, &unit->logs.io, (spn_build_event_t) {
      .kind = SPN_EVENT_BUILD_SCRIPT_CRASHED,
      .crashed = { .path = sp_str_copy(spn.mem, path), .error = detail },
    });
  }
  if (env) wasm_runtime_destroy_exec_env(env);
  if (instance) wasm_runtime_deinstantiate(instance);
  if (module) wasm_runtime_unload(module);
  wasm_runtime_destroy_thread_env();
  sp_mem_end_scratch(scratch);
  return err;
}

spn_err_t spn_wasm_smoke(sp_mem_t mem, sp_intern_t* interner, sp_str_t path) {
  spn_wasm_t* wasm = sp_alloc_type(mem, spn_wasm_t);
  spn_wasm_init(wasm, mem, interner);

  spn_wasm_module_t* module = sp_zero;
  spn_try(spn_wasm_load_module(wasm, path, &module));

  spn_wasm_pkg_t pkg = sp_zero;
  spn_try(spn_wasm_instantiate_module(wasm, module, &pkg));

  spn_wasm_fn_t** fn = sp_str_om_get(pkg.functions, sp_str_lit("configure"));
  wasm_val_t results [1] = sp_zero;
  if (!wasm_runtime_call_wasm_a(pkg.ctx, *fn, 1, results, 0, SP_NULLPTR)) {
    return SPN_ERR_WASM_MODULE_CALL_FAILED;
  }

  if (results[0].of.i32 != 42) {
    return SPN_ERR_WASM_MODULE_CALL_FAILED;
  }

  return SPN_OK;
}
