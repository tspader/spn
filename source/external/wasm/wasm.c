#include "sp.h"
#include "external/wasm/wasm.h"
#include "error/types.h"
#include "intern/intern.h"

#include "sp/macro.h"
#include "spn.h"
#include "sp/sp_om.h"
#include "wasm/types.h"
#include "wasm_export.h"

#define SPN_WASM_STACK_SIZE 8192

static int spn_wasm_host_add(wasm_exec_env_t exec_env, int a, int b) {
  (void)exec_env;
  return a + b;
}

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

  pkg.instance = wasm_runtime_instantiate(module, SPN_WASM_STACK_SIZE, 0, error, sizeof(error));
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
  sp_assert(!initted);
  initted = true;
  if (!wasm_runtime_init()) {
    return SPN_ERR_WASM_INIT_FAILED;
  }

  static NativeSymbol spn_wasm_natives[] = {
    { "host_add", spn_wasm_host_add, "(ii)i", SP_NULLPTR },
  };

  if (!wasm_runtime_register_natives("env", spn_wasm_natives, sp_carr_len(spn_wasm_natives))) {
    return SPN_ERR_WASM_REGISTER_FAILED;
  }

  return SPN_OK;
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
