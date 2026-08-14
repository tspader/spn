#include "dag_test.h"

typedef struct {
  const c8* name;
  const c8* blob;
} cache_output_t;

typedef enum {
  CACHE_OP_DONE,
  CACHE_OP_PUT,
  CACHE_OP_GET,
  CACHE_OP_REMOVE,
  CACHE_OP_RELOAD,
  CACHE_OP_CORRUPT,
} cache_op_kind_t;

typedef struct {
  bool hit;
} cache_expect_t;

typedef struct {
  cache_op_kind_t kind;
  const c8* key;
  cache_output_t outputs [DAG_TEST_MAX_OUTPUTS];
  cache_expect_t expect;
} cache_op_t;

typedef struct {
  const c8* name;
  cache_op_t ops [DAG_TEST_MAX_OPS];
} cache_test_t;

static const cache_test_t cache_tests [] = {
  {
    .name = "get_missing",
    .ops = {
      { .kind = CACHE_OP_GET, .key = "cc main.c" },
    }
  },
  {
    .name = "put_then_get",
    .ops = {
      { .kind = CACHE_OP_PUT, .key = "cc main.c", .outputs = { { "main.o", "obj" }, { "main.d", "deps" } } },
      { .kind = CACHE_OP_GET, .key = "cc main.c", .outputs = { { "main.o", "obj" }, { "main.d", "deps" } }, .expect = { .hit = true } },
    }
  },
  {
    .name = "remove_existing",
    .ops = {
      { .kind = CACHE_OP_PUT, .key = "cc main.c", .outputs = { { "main.o", "obj" } } },
      { .kind = CACHE_OP_REMOVE, .key = "cc main.c", .expect = { .hit = true } },
      { .kind = CACHE_OP_GET, .key = "cc main.c" },
    }
  },
  {
    .name = "put_overwrites_existing_entry",
    .ops = {
      { .kind = CACHE_OP_PUT, .key = "K", .outputs = { { "O", "A" } } },
      { .kind = CACHE_OP_PUT, .key = "K", .outputs = { { "O", "B" } } },
      { .kind = CACHE_OP_GET, .key = "K", .outputs = { { "O", "B" } }, .expect = { .hit = true } },
    }
  },
  {
    .name = "remove_missing",
    .ops = {
      { .kind = CACHE_OP_REMOVE, .key = "K" },
    }
  },
  {
    .name = "distinct_keys",
    .ops = {
      { .kind = CACHE_OP_PUT, .key = "cc main.c", .outputs = { { "main.o", "obj" } } },
      { .kind = CACHE_OP_PUT, .key = "cc spum.c", .outputs = { { "spum.o", "spum" } } },
      { .kind = CACHE_OP_GET, .key = "cc main.c", .outputs = { { "main.o", "obj" } }, .expect = { .hit = true } },
      { .kind = CACHE_OP_GET, .key = "cc spum.c", .outputs = { { "spum.o", "spum" } }, .expect = { .hit = true } },
    }
  },
  {
    .name = "empty_outputs",
    .ops = {
      { .kind = CACHE_OP_PUT, .key = "cc main.c" },
      { .kind = CACHE_OP_GET, .key = "cc main.c", .expect = { .hit = true } },
    }
  },
  {
    .name = "reload_roundtrip",
    .ops = {
      { .kind = CACHE_OP_PUT, .key = "cc main.c", .outputs = { { "main.o", "obj" }, { "main.d", "deps" } } },
      { .kind = CACHE_OP_PUT, .key = "cc spum.c", .outputs = { { "spum.o", "spum" } } },
      { .kind = CACHE_OP_RELOAD },
      { .kind = CACHE_OP_GET, .key = "cc main.c", .outputs = { { "main.o", "obj" }, { "main.d", "deps" } }, .expect = { .hit = true } },
      { .kind = CACHE_OP_GET, .key = "cc spum.c", .outputs = { { "spum.o", "spum" } }, .expect = { .hit = true } },
    }
  },
  {
    .name = "reload_empty_dir",
    .ops = {
      { .kind = CACHE_OP_RELOAD },
      { .kind = CACHE_OP_GET, .key = "cc main.c" },
    }
  },
  {
    .name = "remove_unlinks_entry",
    .ops = {
      { .kind = CACHE_OP_PUT, .key = "cc main.c", .outputs = { { "main.o", "obj" } } },
      { .kind = CACHE_OP_REMOVE, .key = "cc main.c", .expect = { .hit = true } },
      { .kind = CACHE_OP_RELOAD },
      { .kind = CACHE_OP_GET, .key = "cc main.c" },
    }
  },
  {
    .name = "corrupt_entry_misses",
    .ops = {
      { .kind = CACHE_OP_PUT, .key = "cc main.c", .outputs = { { "main.o", "obj" } } },
      { .kind = CACHE_OP_CORRUPT, .key = "cc main.c" },
      { .kind = CACHE_OP_RELOAD },
      { .kind = CACHE_OP_GET, .key = "cc main.c" },
      { .kind = CACHE_OP_GET, .key = "cc main.c" },
    }
  },
};

static void get_output_count(const cache_op_t* op, u32* count) {
  *count = 0;
  sp_carr_for(op->outputs, it) {
    if (!op->outputs[it].name) {
      break;
    }
    (*count)++;
  }
}

static sp_str_t get_path(sp_mem_t mem, sp_str_t dir, const c8* key) {
  sp_str_t hex = spn_dag_digest_hex(mem, dag_test_digest(key));
  return sp_fs_join_path(mem, dir, sp_fmt(mem, "{}.txt", sp_fmt_str(hex)).value);
}

sp_test_each(dag_action_cache, ops, cache_test_t, cache_tests) {
  sp_mem_t mem = sp_test_arena(t);
  sp_str_t dir = sp_fs_join_path(mem, sp_test_dir(t), sp_str_lit("strong"));

  spn_dag_action_cache_t c = sp_zero;
  spn_dag_action_cache_init(&c, mem, dir);

  sp_carr_for(it->ops, ot) {
    cache_op_t op = it->ops[ot];
    if (op.kind == CACHE_OP_DONE) {
      break;
    }

    switch (op.kind) {
      case CACHE_OP_DONE: {
        break;
      }
      case CACHE_OP_PUT: {
        spn_dag_action_output_t outputs [DAG_TEST_MAX_OUTPUTS] = sp_zero;
        c8 names [DAG_TEST_MAX_OUTPUTS][SP_PATH_MAX] = sp_zero;
        u32 count = 0;
        get_output_count(&op, &count);
        sp_for(oi, count) {
          u32 len = sp_cstr_len(op.outputs[oi].name);
          sp_cstr_copy_to_n(op.outputs[oi].name, len, names[oi], sizeof(names[oi]));
          outputs[oi] = (spn_dag_action_output_t) {
            .name = sp_str(names[oi], len),
            .digest = dag_test_digest(op.outputs[oi].blob)
          };
        }
        spn_dag_action_cache_put(&c, dag_test_digest(op.key), outputs, count);
        sp_mem_fill_u8(outputs, sizeof(outputs), 69);
        sp_mem_fill_u8(names, sizeof(names), 69);
        break;
      }
      case CACHE_OP_GET: {
        spn_dag_action_entry_t entry = sp_zero;
        bool present = spn_dag_action_cache_get(&c, dag_test_digest(op.key), &entry);
        sp_expect_eq(t, op.expect.hit, present);
        if (op.expect.hit && present) {
          u32 count = 0;
          get_output_count(&op, &count);
          sp_must_eq(t, count, (u32)sp_da_size(entry.outputs));
          sp_for(oi, count) {
            sp_expect_str_eq_c(t, entry.outputs[oi].name, op.outputs[oi].name);
            sp_expect(t, spn_dag_digest_equal(entry.outputs[oi].digest, dag_test_digest(op.outputs[oi].blob)));
          }
        }
        break;
      }
      case CACHE_OP_REMOVE: {
        sp_expect_eq(t, op.expect.hit, spn_dag_action_cache_remove(&c, dag_test_digest(op.key)));
        break;
      }
      case CACHE_OP_RELOAD: {
        spn_dag_action_cache_init(&c, mem, dir);
        break;
      }
      case CACHE_OP_CORRUPT: {
        sp_must_eq(t, SP_OK, sp_fs_create_file_cstr(get_path(mem, dir, op.key), "not json\n"));
        break;
      }
    }
  }

  return SP_OK;
}
