#include "common.h"
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

UTEST_EMPTY_FIXTURE(wasm)

static void expect_obs(s32* utest_result, tmpfs_t* fs, wasm_expect_t* expect, sp_da(spn_dag_obs_t) obs) {
  u32 expected = 0;
  sp_carr_for(expect->obs, it) {
    wasm_obs_t* e = &expect->obs[it];
    if (!e->path) {
      break;
    }
    expected++;

    sp_str_t host = tmpfs_get(fs, sp_str_view(e->path));
    bool found = false;
    sp_da_for(obs, ot) {
      if (obs[ot].kind == e->kind && sp_str_equal(obs[ot].path, host)) {
        found = true;
        break;
      }
    }
    EXPECT_TRUE(found);
  }
  EXPECT_EQ(expected, (u32)sp_da_size(obs));
}

static void run_wasm_test(s32* utest_result, wasm_test_t t) {
  static bool runtime_ready = false;
  if (!runtime_ready) {
    ASSERT_TRUE(wasm_runtime_init());
    ASSERT_EQ(SPN_OK, spn_dag_wasi_install());
    runtime_ready = true;
  }

  tmpfs_t fs = sp_zero;
  tmpfs_init_named(&fs, t.name);

  spn_dag_wasi_mount_t mounts [] = {
    { .guest = "/work",   .host = tmpfs_get(&fs, sp_str_lit("work")) },
    { .guest = "/source", .host = tmpfs_get(&fs, sp_str_lit("source")) },
    { .guest = "/store",  .host = tmpfs_get(&fs, sp_str_lit("store")) },
  };
  sp_carr_for(mounts, it) {
    sp_fs_create_dir(mounts[it].host);
  }
  sp_carr_for(t.files, it) {
    if (!t.files[it].path) {
      break;
    }
    tmpfs_create(&fs, sp_str_view(t.files[it].path), sp_str_lit("A"));
  }

  wasm_emit_fn_t fns [DAG_WASM_MAX_CALLS] = sp_zero;
  u32 num_fns = 0;
  sp_carr_for(t.calls, it) {
    wasm_call_t* call = &t.calls[it];
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
  sp_str_t blob = wasm_emit_module(fs.mem, fns, num_fns);

  c8 error [128] = sp_zero;
  wasm_module_t module = wasm_runtime_load((u8*)blob.data, (u32)blob.len, error, sizeof(error));
  ASSERT_TRUE(module != SP_NULLPTR);

  const c8* preopens [sp_carr_len(mounts)] = sp_zero;
  sp_carr_for(mounts, it) {
    preopens[it] = sp_fmt_mem_cstr(fs.mem, "{}::{}", sp_fmt_cstr(mounts[it].guest), sp_fmt_str(mounts[it].host));
  }
  wasm_runtime_set_wasi_args(module, SP_NULLPTR, 0, preopens, sp_carr_len(preopens), SP_NULLPTR, 0, SP_NULLPTR, 0);

  wasm_module_inst_t instance = wasm_runtime_instantiate(module, DAG_WASM_STACK_SIZE, DAG_WASM_HEAP_SIZE, error, sizeof(error));
  ASSERT_TRUE(instance != SP_NULLPTR);

  spn_dag_wasi_t* w = spn_dag_wasi_new(fs.mem, mounts, sp_carr_len(mounts));
  spn_dag_wasi_bind(w, instance);

  wasm_exec_env_t env = wasm_runtime_create_exec_env(instance, DAG_WASM_STACK_SIZE);
  ASSERT_TRUE(env != SP_NULLPTR);

  sp_carr_for(t.calls, it) {
    wasm_call_t* call = &t.calls[it];
    if (!call->fn) {
      break;
    }

    wasm_function_inst_t fn = wasm_runtime_lookup_function(instance, call->fn);
    ASSERT_TRUE(fn != SP_NULLPTR);

    sp_da(spn_dag_obs_t) obs = sp_da_new(fs.mem, spn_dag_obs_t);
    spn_dag_wasi_begin(w, fs.mem, &obs);

    wasm_val_t results [1] = sp_zero;
    bool called = wasm_runtime_call_wasm_a(env, fn, 1, results, 0, SP_NULLPTR);
    spn_dag_wasi_end(w);

    ASSERT_TRUE(called);
    EXPECT_EQ(call->expect.rc, results[0].of.i32);
    expect_obs(utest_result, &fs, &call->expect, obs);
  }

  wasm_runtime_destroy_exec_env(env);
  wasm_runtime_deinstantiate(instance);
  wasm_runtime_unload(module);
  tmpfs_deinit(&fs);
}

UTEST_F(wasm, open_read) {
  run_wasm_test(&ur, (wasm_test_t) {
    .name = "open_read",
    .files = { { "work/H" } },
    .calls = {
      { .fn = "run",
        .ops = { { WASM_EMIT_OPEN_READ, "H" } },
        .expect = { .obs = { { .path = "work/H" } } } },
    }
  });
}

UTEST_F(wasm, open_absent) {
  run_wasm_test(&ur, (wasm_test_t) {
    .name = "open_absent",
    .calls = {
      { .fn = "run",
        .ops = { { WASM_EMIT_OPEN_READ, "H" } },
        .expect = { .rc = WASI_ENOENT, .obs = { { .kind = SPN_DAG_OBS_ABSENT, .path = "work/H" } } } },
    }
  });
}

UTEST_F(wasm, open_write) {
  run_wasm_test(&ur, (wasm_test_t) {
    .name = "open_write",
    .calls = {
      { .fn = "run",
        .ops = { { WASM_EMIT_OPEN_WRITE, "O" } } },
    }
  });
}

UTEST_F(wasm, write_then_read) {
  run_wasm_test(&ur, (wasm_test_t) {
    .name = "write_then_read",
    .calls = {
      { .fn = "run",
        .ops = {
          { WASM_EMIT_OPEN_WRITE, "O" },
          { WASM_EMIT_CLOSE },
          { WASM_EMIT_OPEN_READ, "O" },
        } },
    }
  });
}

UTEST_F(wasm, stat_file) {
  run_wasm_test(&ur, (wasm_test_t) {
    .name = "stat_file",
    .files = { { "work/H" } },
    .calls = {
      { .fn = "run",
        .ops = { { WASM_EMIT_STAT, "H" } },
        .expect = { .obs = { { .path = "work/H" } } } },
    }
  });
}

UTEST_F(wasm, stat_absent) {
  run_wasm_test(&ur, (wasm_test_t) {
    .name = "stat_absent",
    .calls = {
      { .fn = "run",
        .ops = { { WASM_EMIT_STAT, "H" } },
        .expect = { .rc = WASI_ENOENT, .obs = { { .kind = SPN_DAG_OBS_ABSENT, .path = "work/H" } } } },
    }
  });
}

UTEST_F(wasm, readdir) {
  run_wasm_test(&ur, (wasm_test_t) {
    .name = "readdir",
    .files = { { "work/H" } },
    .calls = {
      { .fn = "run",
        .ops = { { WASM_EMIT_READDIR } },
        .expect = { .obs = { { .kind = SPN_DAG_OBS_ENUMERATION, .path = "work" } } } },
    }
  });
}

UTEST_F(wasm, subdir) {
  run_wasm_test(&ur, (wasm_test_t) {
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
  });
}

UTEST_F(wasm, close_reuse) {
  run_wasm_test(&ur, (wasm_test_t) {
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
  });
}

UTEST_F(wasm, mounts) {
  run_wasm_test(&ur, (wasm_test_t) {
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
  });
}

UTEST_F(wasm, cross_call) {
  run_wasm_test(&ur, (wasm_test_t) {
    .name = "cross_call",
    .files = { { "work/D/H" } },
    .calls = {
      { .fn = "A",
        .ops = { { WASM_EMIT_OPEN_DIR, "D" } } },
      { .fn = "B",
        .ops = { { WASM_EMIT_OPEN_AT, "H" } },
        .expect = { .obs = { { .path = "work/D/H" } } } },
    }
  });
}

UTEST_F(wasm, escape) {
  run_wasm_test(&ur, (wasm_test_t) {
    .name = "escape",
    .calls = {
      { .fn = "run",
        .ops = { { WASM_EMIT_OPEN_READ, "../H" } },
        .expect = { .rc = WASI_ENOTCAPABLE } },
    }
  });
}
