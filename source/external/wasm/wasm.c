#include "sp.h"
#include "external/wasm/wasm.h"
#include "error/types.h"
#include "intern/intern.h"

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

bool spn_wasm_enabled(void) {
  return !sp_str_empty(sp_env_get(spn.env, sp_str_lit("SPN_USE_WASM")));
}

static spn_err_t spn_wasm_script_fail(spn_pkg_unit_t* unit, spn_err_t err, spn_err_wasm_t wasm) {
  wasm.path = sp_str_copy(spn.mem, wasm.path);
  wasm.error = sp_str_copy(spn.mem, wasm.error);
  spn_event_buffer_push_ex(spn.events, unit->info, &unit->logs.io, (spn_build_event_t) {
    .kind = SPN_EVENT_ERR,
    .err = { .kind = err, .wasm = wasm },
  });
  return err;
}

// wasm_runtime_thread_env_inited lies in interpreter-only builds (the signal
// env check is compiled out with AOT), so init unconditionally; it's a no-op
// on a thread that's already set up. Scripts open on a configure worker and
// their node fns run later on build workers, so every entry point re-inits.
static wasm_exec_env_t spn_wasm_script_enter(spn_wasm_script_t* script) {
  sp_mutex_lock(&script->mutex);

  wasm_exec_env_t env = SP_NULLPTR;
  if (wasm_runtime_init_thread_env()) {
    env = wasm_runtime_create_exec_env(script->instance, SPN_WASM_STACK_SIZE);
  }

  if (!env) {
    sp_mutex_unlock(&script->mutex);
    return SP_NULLPTR;
  }

  wasm_runtime_set_user_data(env, script->table);
  return env;
}

static void spn_wasm_script_exit(spn_wasm_script_t* script, wasm_exec_env_t env) {
  wasm_runtime_destroy_exec_env(env);
  sp_mutex_unlock(&script->mutex);
}

spn_err_t spn_wasm_script_open(spn_wasm_script_t** out, spn_pkg_unit_t* unit, sp_str_t path) {
  if (*out) return SPN_OK;

  spn_err_t err = SPN_OK;
  c8 error [128] = sp_zero;
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();

  spn_wasm_script_t* script = sp_alloc_type(spn.mem, spn_wasm_script_t);
  *script = sp_zero_s(spn_wasm_script_t);
  script->path = sp_str_copy(spn.mem, path);
  sp_mutex_init(&script->mutex, SP_MUTEX_PLAIN);
  script->table = sp_alloc_type(spn.mem, spn_abi_table_t);
  spn_abi_table_init(script->table, spn.mem);
  script->ctx = spn_abi_table_add(script->table, unit, SPN_ABI_KIND_CTX);

  if (!wasm_runtime_init_thread_env()) {
    err = SPN_ERR_WASM_THREAD_ENV_FAILED; goto done;
  }

  sp_str_t blob = sp_zero;
  if (sp_io_read_file(scratch.mem, path, &blob)) {
    err = SPN_ERR_WASM_READ_FAILED; goto done;
  }

  script->module = wasm_runtime_load((u8*)blob.data, blob.len, error, sizeof(error));
  if (!script->module) {
    err = SPN_ERR_WASM_MODULE_LOAD_FAILED; goto done;
  }

  // WAMR runs _initialize during instantiation for WASI reactor modules;
  // calling it again trips wasi-libc's double-init trap
  script->instance = wasm_runtime_instantiate(script->module, SPN_WASM_STACK_SIZE, SPN_WASM_HEAP_SIZE, error, sizeof(error));
  if (!script->instance) {
    err = SPN_ERR_WASM_MODULE_INSTANCE_FAILED; goto done;
  }

done:
  sp_mem_end_scratch(scratch);
  if (err) {
    if (script->module) wasm_runtime_unload(script->module);
    return spn_wasm_script_fail(unit, err, (spn_err_wasm_t) {
      .path = path,
      .error = sp_cstr_as_str(error),
    });
  }

  *out = script;
  return SPN_OK;
}

bool spn_wasm_script_has(spn_wasm_script_t* script, const c8* name) {
  return wasm_runtime_lookup_function(script->instance, name) != SP_NULLPTR;
}

spn_err_t spn_wasm_script_call_hook(spn_wasm_script_t* script, spn_pkg_unit_t* unit, const c8* name) {
  wasm_function_inst_t fn = wasm_runtime_lookup_function(script->instance, name);
  if (!fn) return SPN_OK;

  wasm_exec_env_t env = spn_wasm_script_enter(script);
  if (!env) {
    return spn_wasm_script_fail(unit, SPN_ERR_WASM_CTX_FAILED, (spn_err_wasm_t) { .path = script->path });
  }

  spn_err_t err = SPN_OK;

  wasm_val_t args [1] = { { .kind = WASM_I32, .of.i32 = (s32)script->ctx } };
  wasm_val_t results [1] = sp_zero;
  if (!wasm_runtime_call_wasm_a(env, fn, 1, results, 1, args)) {
    err = spn_wasm_script_fail(unit, SPN_ERR_WASM_MODULE_CALL_FAILED, (spn_err_wasm_t) {
      .path = script->path,
      .error = sp_cstr_as_str(wasm_runtime_get_exception(script->instance)),
    });
  }
  else if (results[0].of.i32 != SPN_OK) {
    err = spn_wasm_script_fail(unit, SPN_ERR_WASM_SCRIPT_ERROR, (spn_err_wasm_t) {
      .path = script->path,
      .rc = results[0].of.i32,
    });
  }

  spn_wasm_script_exit(script, env);
  return err;
}

spn_err_t spn_wasm_script_call_node(spn_wasm_script_t* script, spn_user_node_t* node) {
  spn_pkg_unit_t* unit = node->pkg;
  if (!script) {
    return spn_wasm_script_fail(unit, SPN_ERR_WASM_NO_SCRIPT, (spn_err_wasm_t) sp_zero);
  }

  wasm_exec_env_t env = spn_wasm_script_enter(script);
  if (!env) {
    return spn_wasm_script_fail(unit, SPN_ERR_WASM_CTX_FAILED, (spn_err_wasm_t) { .path = script->path });
  }

  spn_err_t err = SPN_OK;

  u32 argv [2] = { script->ctx, 0 };
  if (!wasm_runtime_call_indirect(env, node->wasm_fn, sp_carr_len(argv), argv)) {
    err = spn_wasm_script_fail(unit, SPN_ERR_WASM_MODULE_CALL_FAILED, (spn_err_wasm_t) {
      .path = script->path,
      .error = sp_cstr_as_str(wasm_runtime_get_exception(script->instance)),
    });
  }
  else if ((s32)argv[0]) {
    err = spn_wasm_script_fail(unit, SPN_ERR_WASM_SCRIPT_ERROR, (spn_err_wasm_t) {
      .path = script->path,
      .rc = (s32)argv[0],
    });
  }

  spn_wasm_script_exit(script, env);
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
