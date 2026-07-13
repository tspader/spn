#define SP_IMPLEMENTATION
#include "sp.h"

#define UTEST_IMPLEMENTATION
#include "utest.h"

#include "test.h"
#include "dag/dag.h"

#define DAG_TEST_MAX_INPUTS 4
#define DAG_TEST_MAX_BLOBS 4

#define EXPECT_STR(actual, cstr) EXPECT_TRUE(sp_str_equal((actual), sp_str_view(cstr)))

typedef struct {
  const c8* data;
  const c8* hex;
} digest_test_t;

UTEST_EMPTY_FIXTURE(digest)

static void run_digest_test(s32* utest_result, digest_test_t t) {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  sp_str_t data = sp_str_view(t.data);
  spn_dag_digest_t digest = spn_dag_digest(data.data, data.len);
  EXPECT_STR(spn_dag_digest_hex(scratch.mem, digest), t.hex);
  sp_mem_end_scratch(scratch);
}

UTEST_F(digest, empty) {
  run_digest_test(&ur, (digest_test_t) {
    .data = "",
    .hex = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"
  });
}

UTEST_F(digest, abc) {
  run_digest_test(&ur, (digest_test_t) {
    .data = "abc",
    .hex = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"
  });
}

typedef struct {
  const c8* salt;
  const c8* inputs [DAG_TEST_MAX_INPUTS];
} key_action_t;

typedef struct {
  bool equal;
} key_expect_t;

typedef struct {
  key_action_t a;
  key_action_t b;
  key_expect_t expect;
} key_test_t;

UTEST_EMPTY_FIXTURE(key)

static spn_dag_digest_t build_action_key(spn_dag_t* g, key_action_t spec) {
  spn_dag_digest_t salt = sp_zero;
  if (spec.salt) {
    sp_str_t str = sp_str_view(spec.salt);
    salt = spn_dag_digest(str.data, str.len);
  }

  spn_dag_id_t action = spn_dag_add_action(g, (spn_dag_action_config_t) {
    .salt = salt
  });

  sp_carr_for(spec.inputs, it) {
    if (!spec.inputs[it]) {
      break;
    }
    sp_str_t str = sp_str_view(spec.inputs[it]);
    spn_dag_id_t value = spn_dag_add_value(g, str.data, str.len);
    spn_dag_action_add_input(g, action, value);
  }

  return spn_dag_action_key(g, action);
}

static void run_key_test(s32* utest_result, key_test_t t) {
  spn_dag_t* g = spn_dag_new(sp_mem_os_new());
  spn_dag_digest_t a = build_action_key(g, t.a);
  spn_dag_digest_t b = build_action_key(g, t.b);
  EXPECT_EQ(t.expect.equal, spn_dag_digest_equal(a, b));
}

UTEST_F(key, identical_actions_match) {
  run_key_test(&ur, (key_test_t) {
    .a = { .salt = "cc -c", .inputs = { "main.c", "sp.h" } },
    .b = { .salt = "cc -c", .inputs = { "main.c", "sp.h" } },
    .expect = { .equal = true }
  });
}

UTEST_F(key, input_order_changes_key) {
  run_key_test(&ur, (key_test_t) {
    .a = { .inputs = { "main.c", "sp.h" } },
    .b = { .inputs = { "sp.h", "main.c" } },
  });
}

UTEST_F(key, input_content_changes_key) {
  run_key_test(&ur, (key_test_t) {
    .a = { .inputs = { "main.c" } },
    .b = { .inputs = { "main.d" } },
  });
}

UTEST_F(key, extra_input_changes_key) {
  run_key_test(&ur, (key_test_t) {
    .a = { .inputs = { "main.c" } },
    .b = { .inputs = { "main.c", "sp.h" } },
  });
}

UTEST_F(key, salt_changes_key) {
  run_key_test(&ur, (key_test_t) {
    .a = { .salt = "cc -c -O0", .inputs = { "main.c" } },
    .b = { .salt = "cc -c -O2", .inputs = { "main.c" } },
  });
}

typedef struct {
  const c8* name;
  spn_dag_store_kind_t kind;
  const c8* blobs [DAG_TEST_MAX_BLOBS];
} store_test_t;

UTEST_EMPTY_FIXTURE(store)

static void run_store_test(s32* utest_result, store_test_t t) {
  tmpfs_t fs = sp_zero;
  tmpfs_init_named(&fs, t.name);
  sp_mem_t mem = fs.mem;

  spn_dag_store_t store = sp_zero;
  spn_dag_store_init(&store, (spn_dag_store_config_t) {
    .kind = t.kind,
    .mem = mem,
    .dir = tmpfs_get(&fs, sp_str_lit("store"))
  });

  spn_dag_digest_t missing = spn_dag_digest("missing", 7);
  EXPECT_FALSE(spn_dag_store_has(&store, missing));
  sp_str_t content = sp_zero;
  EXPECT_EQ(SPN_ERROR, spn_dag_store_get(&store, missing, mem, &content));
  EXPECT_EQ(SPN_ERROR, spn_dag_store_materialize(&store, missing, tmpfs_get(&fs, sp_str_lit("missing.bin"))));

  sp_carr_for(t.blobs, it) {
    if (!t.blobs[it]) {
      break;
    }
    sp_str_t blob = sp_str_view(t.blobs[it]);

    spn_dag_digest_t digest = sp_zero;
    ASSERT_EQ(SPN_OK, spn_dag_store_put(&store, blob.data, blob.len, &digest));
    EXPECT_TRUE(spn_dag_digest_equal(digest, spn_dag_digest(blob.data, blob.len)));
    EXPECT_TRUE(spn_dag_store_has(&store, digest));

    spn_dag_digest_t again = sp_zero;
    ASSERT_EQ(SPN_OK, spn_dag_store_put(&store, blob.data, blob.len, &again));
    EXPECT_TRUE(spn_dag_digest_equal(digest, again));

    sp_str_t fetched = sp_zero;
    ASSERT_EQ(SPN_OK, spn_dag_store_get(&store, digest, mem, &fetched));
    EXPECT_STR(fetched, t.blobs[it]);

    sp_str_t materialized = tmpfs_get(&fs, sp_fmt(mem, "{}.bin", sp_fmt_uint(it)).value);
    ASSERT_EQ(SPN_OK, spn_dag_store_materialize(&store, digest, materialized));
    sp_str_t from_disk = sp_zero;
    ASSERT_EQ(SP_OK, sp_io_read_file(mem, materialized, &from_disk));
    EXPECT_STR(from_disk, t.blobs[it]);

    spn_dag_digest_t from_file = sp_zero;
    ASSERT_EQ(SPN_OK, spn_dag_store_put_file(&store, materialized, &from_file));
    EXPECT_TRUE(spn_dag_digest_equal(digest, from_file));
  }

  EXPECT_EQ(SPN_ERROR, spn_dag_store_put_file(&store, tmpfs_get(&fs, sp_str_lit("absent.bin")), &missing));

  tmpfs_deinit(&fs);
}

UTEST_F(store, mem_roundtrip) {
  run_store_test(&ur, (store_test_t) {
    .name = "store_mem",
    .kind = SPN_DAG_STORE_MEM,
    .blobs = { "int main() {}", "", "spum" }
  });
}

UTEST_F(store, filesystem_roundtrip) {
  run_store_test(&ur, (store_test_t) {
    .name = "store_fs",
    .kind = SPN_DAG_STORE_FILESYSTEM,
    .blobs = { "int main() {}", "", "spum" }
  });
}

UTEST_MAIN();
