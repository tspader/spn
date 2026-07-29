#include "dag_test.h"
#include "dag/wasi.h"
#include "wasm_emit.h"

#define DAG_WASM_MAX_FILES 4
#define DAG_WASM_MAX_OBS 4
#define DAG_WASM_MAX_OPS 4
#define DAG_WASM_MAX_CALLS 2
#define DAG_WASM_STACK_SIZE (64 * 1024)
#define DAG_WASM_HEAP_SIZE (64 * 1024)

#define WASI_ENOENT 44
#define WASI_ENOTCAPABLE 76

typedef struct {
  const c8* path;
} wasm_file_t;

typedef struct {
  spn_dag_obs_kind_t kind;
  const c8* path;
} wasm_obs_t;

typedef struct {
  s32 rc;
  wasm_obs_t obs [DAG_WASM_MAX_OBS];
} wasm_expect_t;

typedef struct {
  const c8* fn;
  wasm_emit_op_t ops [DAG_WASM_MAX_OPS];
  wasm_expect_t expect;
} wasm_call_t;

typedef struct {
  const c8* name;
  wasm_file_t files [DAG_WASM_MAX_FILES];
  wasm_call_t calls [DAG_WASM_MAX_CALLS];
} wasm_test_t;

// wasm_runtime_init and spn_dag_wasi_install are process-global
sp_test_suite(wasm, .serial = true);

static const wasm_test_t wasm_tests [] = {
  {
    .name = "open_read",
    .files = { { "work/H" } },
    .calls = {
      { .fn = "run",
        .ops = { { WASM_EMIT_OPEN_READ, "H" } },
        .expect = { .obs = { { .path = "work/H" } } } },
    }
  },
  {
    .name = "open_absent",
    .calls = {
      { .fn = "run",
        .ops = { { WASM_EMIT_OPEN_READ, "H" } },
        .expect = { .rc = WASI_ENOENT, .obs = { { .kind = SPN_DAG_OBS_ABSENT, .path = "work/H" } } } },
    }
  },
  {
    .name = "open_write",
    .calls = {
      { .fn = "run",
        .ops = { { WASM_EMIT_OPEN_WRITE, "O" } } },
    }
  },
  {
    .name = "write_then_read",
    .calls = {
      { .fn = "run",
        .ops = {
          { WASM_EMIT_OPEN_WRITE, "O" },
          { WASM_EMIT_CLOSE },
          { WASM_EMIT_OPEN_READ, "O" },
        } },
    }
  },
  {
    .name = "stat_file",
    .files = { { "work/H" } },
    .calls = {
      { .fn = "run",
        .ops = { { WASM_EMIT_STAT, "H" } },
        .expect = { .obs = { { .path = "work/H" } } } },
    }
  },
  {
    .name = "stat_absent",
    .calls = {
      { .fn = "run",
        .ops = { { WASM_EMIT_STAT, "H" } },
        .expect = { .rc = WASI_ENOENT, .obs = { { .kind = SPN_DAG_OBS_ABSENT, .path = "work/H" } } } },
    }
  },
  {
    .name = "readdir",
    .files = { { "work/H" } },
    .calls = {
      { .fn = "run",
        .ops = { { WASM_EMIT_READDIR } },
        .expect = { .obs = { { .kind = SPN_DAG_OBS_ENUMERATION, .path = "work" } } } },
    }
  },
  {
    .name = "subdir",
    .files = { { "work/D/H" } },
    .calls = {
      { .fn = "run",
        .ops = {
          { WASM_EMIT_OPEN_DIR, "D" },
          { WASM_EMIT_OPEN_AT, "H" },
        },
        .expect = { .obs = { { .path = "work/D/H" } } } },
    }
  },
  {
    .name = "close_reuse",
    .files = { { "work/D/G" }, { "work/E/H" } },
    .calls = {
      { .fn = "run",
        .ops = {
          { WASM_EMIT_OPEN_DIR, "D" },
          { WASM_EMIT_CLOSE },
          { WASM_EMIT_OPEN_DIR, "E" },
          { WASM_EMIT_OPEN_AT, "H" },
        },
        .expect = { .obs = { { .path = "work/E/H" } } } },
    }
  },
  {
    .name = "mounts",
    .files = { { "source/H" }, { "store/H" } },
    .calls = {
      { .fn = "run",
        .ops = {
          { WASM_EMIT_OPEN_READ, "H", .mount = 1 },
          { WASM_EMIT_OPEN_READ, "H", .mount = 2 },
        },
        .expect = { .obs = { { .path = "source/H" }, { .path = "store/H" } } } },
    }
  },
  {
    .name = "cross_call",
    .files = { { "work/D/H" } },
    .calls = {
      { .fn = "A",
        .ops = { { WASM_EMIT_OPEN_DIR, "D" } } },
      { .fn = "B",
        .ops = { { WASM_EMIT_OPEN_AT, "H" } },
        .expect = { .obs = { { .path = "work/D/H" } } } },
    }
  },
  {
    .name = "escape",
    .calls = {
      { .fn = "run",
        .ops = { { WASM_EMIT_OPEN_READ, "../H" } },
        .expect = { .rc = WASI_ENOTCAPABLE } },
    }
  },
};

static sp_test_once_t wasm_runtime_once;

static sp_err_t wasm_runtime_bring_up(void* user) {
  SP_UNUSED(user);
  if (!wasm_runtime_init()) {
    return SP_ERR;
  }
  if (spn_dag_wasi_install() != SPN_OK) {
    return SP_ERR;
  }
  return SP_OK;
}

static void expect_obs(sp_test_t* t, sp_mem_t mem, sp_str_t root, const wasm_expect_t* expect, sp_da(spn_dag_obs_t) obs) {
  u32 expected = 0;
  sp_carr_for(expect->obs, it) {
    const wasm_obs_t* e = &expect->obs[it];
    if (!e->path) {
      break;
    }
    expected++;

    sp_str_t host = sp_fs_join_path(mem, root, sp_str_view(e->path));
    bool found = false;
    sp_da_for(obs, ot) {
      if (obs[ot].kind == e->kind && sp_str_equal(obs[ot].path, host)) {
        found = true;
        break;
      }
    }
    sp_expect(t, found);
  }
  sp_expect_eq(t, expected, (u32)sp_da_size(obs));
}

sp_test_each(wasm, wasi, wasm_test_t, wasm_tests) {
  if (!sp_str_empty(sp_os_env_get(sp_str_lit("SPN_TEST_SIM")))) {
    return sp_test_skip(t, "wamr syscalls bypass the sim filesystem");
  }

  sp_must_ok(t, sp_test_once(&wasm_runtime_once, wasm_runtime_bring_up, SP_NULLPTR));

  sp_mem_t mem = sp_test_arena(t);
  sp_str_t root = sp_test_dir(t);

  spn_dag_wasi_mount_t mounts [] = {
    { .guest = "/work",   .host = sp_fs_join_path(mem, root, sp_str_lit("work")) },
    { .guest = "/source", .host = sp_fs_join_path(mem, root, sp_str_lit("source")) },
    { .guest = "/store",  .host = sp_fs_join_path(mem, root, sp_str_lit("store")) },
  };
  sp_carr_for(mounts, mt) {
    sp_fs_create_dir(mounts[mt].host);
  }
  sp_carr_for(it->files, ft) {
    if (!it->files[ft].path) {
      break;
    }
    dag_test_create(sp_fs_join_path(mem, root, sp_str_view(it->files[ft].path)), sp_str_lit("A"));
  }

  wasm_emit_fn_t fns [DAG_WASM_MAX_CALLS] = sp_zero;
  u32 num_fns = 0;
  sp_carr_for(it->calls, ct) {
    wasm_call_t* call = &it->calls[ct];
    if (!call->fn) {
      break;
    }
    u32 num_ops = 0;
    sp_carr_for(call->ops, ot) {
      if (!call->ops[ot].kind) {
        break;
      }
      num_ops++;
    }
    fns[num_fns++] = (wasm_emit_fn_t) {
      .name = call->fn,
      .ops = call->ops,
      .count = num_ops
    };
  }
  sp_str_t blob = wasm_emit_module(mem, fns, num_fns);

  c8 error [128] = sp_zero;
  wasm_module_t module = wasm_runtime_load((u8*)blob.data, (u32)blob.len, error, sizeof(error));
  sp_must(t, module != SP_NULLPTR);

  const c8* preopens [sp_carr_len(mounts)] = sp_zero;
  sp_carr_for(mounts, mt) {
    preopens[mt] = sp_fmt_mem_cstr(mem, "{}::{}", sp_fmt_cstr(mounts[mt].guest), sp_fmt_str(mounts[mt].host));
  }
  wasm_runtime_set_wasi_args(module, SP_NULLPTR, 0, preopens, sp_carr_len(preopens), SP_NULLPTR, 0, SP_NULLPTR, 0);

  wasm_module_inst_t instance = wasm_runtime_instantiate(module, DAG_WASM_STACK_SIZE, DAG_WASM_HEAP_SIZE, error, sizeof(error));
  sp_must(t, instance != SP_NULLPTR);

  spn_dag_wasi_t* w = spn_dag_wasi_new(mem, mounts, sp_carr_len(mounts));
  spn_dag_wasi_bind(w, instance);

  wasm_exec_env_t env = wasm_runtime_create_exec_env(instance, DAG_WASM_STACK_SIZE);
  sp_must(t, env != SP_NULLPTR);

  sp_carr_for(it->calls, ct) {
    wasm_call_t* call = &it->calls[ct];
    if (!call->fn) {
      break;
    }

    wasm_function_inst_t fn = wasm_runtime_lookup_function(instance, call->fn);
    sp_must(t, fn != SP_NULLPTR);

    sp_da(spn_dag_obs_t) obs = sp_da_new(mem, spn_dag_obs_t);
    spn_dag_wasi_begin(w, mem, &obs);

    wasm_val_t results [1] = sp_zero;
    bool called = wasm_runtime_call_wasm_a(env, fn, 1, results, 0, SP_NULLPTR);
    spn_dag_wasi_end(w);

    sp_must(t, called);
    sp_expect_eq(t, call->expect.rc, results[0].of.i32);
    expect_obs(t, mem, root, &call->expect, obs);
  }

  wasm_runtime_destroy_exec_env(env);
  wasm_runtime_deinstantiate(instance);
  wasm_runtime_unload(module);
  return SP_OK;
}
