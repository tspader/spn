#include "sp.h"
#include "external/wasm/wasm.h"
#include "error/types.h"

#include "spn.h"
#include "ctx/types.h"
#include "event/types.h"
#include "event/event.h"
#include "unit/types.h"
#include "wasm_export.h"

#include "external/wasm/abi.h"

#define SPN_WASM_STACK_SIZE (8 * 1024 * 1024)
#define SPN_WASM_HEAP_SIZE  (16 * 1024 * 1024)
#define SPN_WASM_EXPORT_MAX 128

#define SPN_WASM_NO_ENV SP_NULL
#define SPN_WASM_NO_DIRS SP_NULL
#define SPN_WASM_NO_ARGS SP_NULL

spn_err_t spn_wasm_init() {
  static bool initialized = false;
  if (initialized) return SPN_OK;
  initialized = true;

  if (!wasm_runtime_init()) {
    return SPN_ERR_WASM_INIT_FAILED;
  }

  if (!wasm_runtime_set_default_running_mode(Mode_Fast_JIT)) {
    return SPN_ERR_WASM_INIT_FAILED;
  }

  if (!spn_wasm_register_api()) {
    return SPN_ERR_WASM_REGISTER_FAILED;
  }

  return SPN_OK;
}

void spn_wasm_script_init(spn_wasm_script_t* script, sp_str_t source, sp_str_t module) {
  *script = (spn_wasm_script_t) {
    .state = sp_fs_is_file(source) ? SPN_WASM_SCRIPT_CLOSED : SPN_WASM_SCRIPT_NONE,
    .path = module,
  };
  sp_mutex_init(&script->mutex, SP_MUTEX_PLAIN);
}

static spn_err_t script_fail(spn_pkg_unit_t* unit, spn_err_t err, spn_err_wasm_t wasm) {
  wasm.path = sp_str_copy(spn.mem, wasm.path);
  wasm.error = sp_str_copy(spn.mem, wasm.error);
  spn_event_buffer_push_ex(spn.events, unit->info, &unit->logs.io, (spn_build_event_t) {
    .kind = SPN_EVENT_ERR,
    .err = { .kind = err, .wasm = wasm },
  });
  return err;
}

static const c8* preopen(const c8* guest, sp_str_t host) {
  return sp_fmt_mem_cstr(spn.mem, "{}::{}", sp_fmt_cstr(guest), sp_fmt_str(host));
}

static spn_err_t script_open(spn_wasm_script_t* script, spn_pkg_unit_t* unit) {
  if (!wasm_runtime_init_thread_env()) {
    return script_fail(unit, SPN_ERR_WASM_THREAD_ENV_FAILED, (spn_err_wasm_t) { .path = script->path });
  }

  sp_str_t blob = sp_zero;
  if (sp_io_read_file(spn.mem, script->path, &blob)) {
    return script_fail(unit, SPN_ERR_WASM_READ_FAILED, (spn_err_wasm_t) { .path = script->path });
  }

  c8 error [128] = sp_zero;
  script->module = wasm_runtime_load((u8*)blob.data, blob.len, error, sizeof(error));
  if (!script->module) {
    return script_fail(unit, SPN_ERR_WASM_MODULE_LOAD_FAILED, (spn_err_wasm_t) {
      .path = script->path,
      .error = sp_cstr_as_str(error),
    });
  }

  script->preopens = (spn_wasm_preopens_t) {
    .work = preopen("/work", unit->paths.work),
    .source = preopen("/source", unit->paths.source),
    .store = preopen("/store", unit->paths.store),
  };
  wasm_runtime_set_wasi_args(
    script->module,
    SPN_WASM_NO_DIRS, SPN_WASM_NO_DIRS,
    script->preopens.array, sp_carr_len(script->preopens.array),
    SPN_WASM_NO_ENV, SPN_WASM_NO_ENV,
    SPN_WASM_NO_ARGS, SPN_WASM_NO_ARGS
  );

  script->instance = wasm_runtime_instantiate(script->module, SPN_WASM_STACK_SIZE, SPN_WASM_HEAP_SIZE, error, sizeof(error));
  if (!script->instance) {
    wasm_runtime_unload(script->module);
    script->module = SP_NULLPTR;
    return script_fail(unit, SPN_ERR_WASM_MODULE_INSTANCE_FAILED, (spn_err_wasm_t) {
      .path = script->path,
      .error = sp_cstr_as_str(error),
    });
  }

  script->handles = sp_alloc_type(spn.mem, spn_wasm_handles_t);
  sp_ht_init(spn.mem, script->handles->map);
  script->ctx = spn_wasm_add_handle(script->handles, unit, SPN_ABI_KIND_CTX);

  return SPN_OK;
}

spn_err_t spn_wasm_script_open(spn_wasm_script_t* script, spn_pkg_unit_t* unit) {
  SP_ASSERT(script->state != SPN_WASM_SCRIPT_NONE);

  spn_err_t err = SPN_OK;

  sp_mutex_lock(&script->mutex);
  switch (script->state) {
    case SPN_WASM_SCRIPT_NONE:
    case SPN_WASM_SCRIPT_OPEN: {
      break;
    }
    case SPN_WASM_SCRIPT_FAILED: {
      err = script->err;
      break;
    }
    case SPN_WASM_SCRIPT_CLOSED: {
      err = script_open(script, unit);
      script->err = err;
      script->state = err ? SPN_WASM_SCRIPT_FAILED : SPN_WASM_SCRIPT_OPEN;
      break;
    }
  }
  sp_mutex_unlock(&script->mutex);

  return err;
}

static void export_name(sp_str_t name, c8* buffer) {
  sp_str_copy_to(name, buffer, SPN_WASM_EXPORT_MAX - 1);
}

bool spn_wasm_script_exports(spn_wasm_script_t* script, sp_str_t name) {
  if (script->state != SPN_WASM_SCRIPT_OPEN) return false;

  c8 buffer [SPN_WASM_EXPORT_MAX] = sp_zero;
  export_name(name, buffer);
  return wasm_runtime_lookup_function(script->instance, buffer) != SP_NULLPTR;
}

spn_err_t spn_wasm_script_call(spn_wasm_script_t* script, spn_pkg_unit_t* unit, sp_str_t name, spn_abi_kind_t kind, void* arg) {
  SP_ASSERT(script->state == SPN_WASM_SCRIPT_OPEN);

  c8 buffer [SPN_WASM_EXPORT_MAX] = sp_zero;
  export_name(name, buffer);

  wasm_function_inst_t fn = wasm_runtime_lookup_function(script->instance, buffer);
  if (!fn) {
    return script_fail(unit, SPN_ERR_WASM_EXPORT_NOT_FOUND, (spn_err_wasm_t) {
      .path = script->path,
      .error = name,
    });
  }

  if (!wasm_runtime_init_thread_env()) {
    return script_fail(unit, SPN_ERR_WASM_THREAD_ENV_FAILED, (spn_err_wasm_t) { .path = script->path });
  }

  sp_mutex_lock(&script->mutex);

  spn_err_t err = SPN_OK;

  wasm_exec_env_t env = wasm_runtime_create_exec_env(script->instance, SPN_WASM_STACK_SIZE);
  if (!env) {
    sp_mutex_unlock(&script->mutex);
    return script_fail(unit, SPN_ERR_WASM_CTX_FAILED, (spn_err_wasm_t) { .path = script->path });
  }

  wasm_runtime_set_user_data(env, script->handles);
  u32 token = spn_wasm_add_handle(script->handles, arg, kind);

  wasm_val_t args [2] = {
    { .kind = WASM_I32, .of.i32 = (s32)script->ctx },
    { .kind = WASM_I32, .of.i32 = (s32)token },
  };
  wasm_val_t results [1] = sp_zero;
  if (!wasm_runtime_call_wasm_a(env, fn, 1, results, sp_carr_len(args), args)) {
    err = script_fail(unit, SPN_ERR_WASM_MODULE_CALL_FAILED, (spn_err_wasm_t) {
      .path = script->path,
      .error = sp_cstr_as_str(wasm_runtime_get_exception(script->instance)),
    });
  }
  else if (results[0].of.i32) {
    err = script_fail(unit, SPN_ERR_WASM_SCRIPT_ERROR, (spn_err_wasm_t) {
      .path = script->path,
      .rc = results[0].of.i32,
    });
  }

  spn_wasm_remove_handle(script->handles, token);
  wasm_runtime_destroy_exec_env(env);
  sp_mutex_unlock(&script->mutex);

  return err;
}

spn_err_t spn_wasm_find_export(spn_pkg_unit_t* unit, sp_str_t name, spn_wasm_script_t** script) {
  *script = SP_NULLPTR;

  spn_wasm_script_t* candidates [] = { &unit->wasm.build, &unit->wasm.configure };
  sp_carr_for(candidates, it) {
    spn_wasm_script_t* candidate = candidates[it];
    if (candidate->state == SPN_WASM_SCRIPT_NONE) continue;

    spn_try(spn_wasm_script_open(candidate, unit));
    if (spn_wasm_script_exports(candidate, name)) {
      *script = candidate;
      return SPN_OK;
    }
  }

  return SPN_OK;
}

spn_err_t spn_wasm_call_export(spn_pkg_unit_t* unit, sp_str_t name, spn_abi_kind_t kind, void* arg) {
  spn_wasm_script_t* script = SP_NULLPTR;
  spn_try(spn_wasm_find_export(unit, name, &script));

  if (!script) {
    if (unit->wasm.build.state != SPN_WASM_SCRIPT_NONE) {
      return script_fail(unit, SPN_ERR_WASM_EXPORT_NOT_FOUND, (spn_err_wasm_t) {
        .path = unit->wasm.build.path,
        .error = name,
      });
    }
    if (unit->wasm.configure.state != SPN_WASM_SCRIPT_NONE) {
      return script_fail(unit, SPN_ERR_WASM_EXPORT_NOT_FOUND, (spn_err_wasm_t) {
        .path = unit->wasm.configure.path,
        .error = name,
      });
    }
    return script_fail(unit, SPN_ERR_WASM_NO_SCRIPT, (spn_err_wasm_t) { .error = name });
  }

  return spn_wasm_script_call(script, unit, name, kind, arg);
}
