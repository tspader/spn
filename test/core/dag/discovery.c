#include "common.h"

typedef struct {
  const c8* key;
  dag_test_obs_t obs [DAG_TEST_MAX_INPUTS];
} discovery_entry_t;

typedef struct {
  bool hit;
  dag_test_obs_t obs [DAG_TEST_MAX_INPUTS];
} discovery_expect_t;

typedef struct {
  const c8* name;
  discovery_entry_t entries [DAG_TEST_MAX_OPS];
  const c8* corrupt;
  bool reload;
  const c8* key;
  discovery_expect_t expect;
} discovery_test_t;

UTEST_EMPTY_FIXTURE(discovery)

static void discovery_put(spn_dag_discovery_t* discovery, discovery_entry_t entry) {
  spn_dag_obs_t obs [DAG_TEST_MAX_INPUTS] = sp_zero;
  u32 count = dag_test_obs_build(entry.obs, DAG_TEST_MAX_INPUTS, obs);
  spn_dag_discovery_put(discovery, dag_test_digest(entry.key), obs, count);
}

static void discovery_expect_obs(s32* utest_result, const spn_dag_pathset_t* set, dag_test_obs_t* expect) {
  spn_dag_obs_t obs [DAG_TEST_MAX_INPUTS] = sp_zero;
  u32 count = dag_test_obs_build(expect, DAG_TEST_MAX_INPUTS, obs);
  ASSERT_EQ(count, (u32)sp_da_size(set->obs));
  sp_for(it, count) {
    EXPECT_EQ(obs[it].kind, set->obs[it].kind);
    EXPECT_TRUE(sp_str_equal(set->obs[it].path, obs[it].path));
  }
}

static void discovery_expect(s32* utest_result, spn_dag_discovery_t* discovery, const c8* key, discovery_expect_t expect) {
  const spn_dag_pathset_t* set = spn_dag_discovery_get(discovery, dag_test_digest(key));
  EXPECT_EQ(expect.hit, set != SP_NULLPTR);
  if (!expect.hit || !set) {
    return;
  }

  discovery_expect_obs(utest_result, set, expect.obs);
}

static void run_test(s32* utest_result, discovery_test_t t) {
  tmpfs_t fs = sp_zero;
  tmpfs_init_named(&fs, t.name);
  sp_str_t dir = tmpfs_get(&fs, sp_str_lit("manifests"));

  spn_dag_discovery_t discovery = sp_zero;
  spn_dag_discovery_init(&discovery, fs.mem, dir);

  sp_carr_for(t.entries, it) {
    if (!t.entries[it].key) {
      break;
    }
    discovery_put(&discovery, t.entries[it]);
  }

  if (t.corrupt) {
    sp_str_t hex = spn_dag_digest_hex(fs.mem, dag_test_digest(t.corrupt));
    sp_str_t path = sp_fs_join_path(fs.mem, dir, sp_fmt(fs.mem, "{}.jsonl", sp_fmt_str(hex)).value);
    ASSERT_EQ(SP_OK, sp_fs_create_file_cstr(path, "not json\n"));
  }

  if (t.reload) {
    spn_dag_discovery_init(&discovery, fs.mem, dir);
  }

  discovery_expect(utest_result, &discovery, t.key, t.expect);
  tmpfs_deinit(&fs);
}

UTEST_F(discovery, missing_key_misses) {
  run_test(&ur, (discovery_test_t) {
    .name = "discovery_missing",
    .key = "K"
  });
}

UTEST_F(discovery, stored_pathset_hits) {
  run_test(&ur, (discovery_test_t) {
    .name = "discovery_stored",
    .entries = {
      {
        .key = "K",
        .obs = {
          { .path = "A" },
          { .kind = SPN_DAG_OBS_ABSENT, .path = "B" }
        }
      }
    },
    .key = "K",
    .expect = {
      .hit = true,
      .obs = {
        { .path = "A" },
        { .kind = SPN_DAG_OBS_ABSENT, .path = "B" }
      }
    }
  });
}

UTEST_F(discovery, empty_pathset_hits) {
  run_test(&ur, (discovery_test_t) {
    .name = "discovery_empty",
    .entries = {
      { .key = "K" }
    },
    .key = "K",
    .expect = { .hit = true }
  });
}

UTEST_F(discovery, keys_are_independent) {
  run_test(&ur, (discovery_test_t) {
    .name = "discovery_independent",
    .entries = {
      { .key = "K", .obs = { { .path = "A" } } },
      { .key = "K2", .obs = { { .path = "B" } } }
    },
    .key = "K",
    .expect = {
      .hit = true,
      .obs = { { .path = "A" } }
    }
  });
}

UTEST_F(discovery, reload_roundtrips_manifest) {
  run_test(&ur, (discovery_test_t) {
    .name = "discovery_reload",
    .entries = {
      {
        .key = "K",
        .obs = {
          { .path = "A" },
          { .kind = SPN_DAG_OBS_ABSENT, .path = "B" }
        }
      }
    },
    .reload = true,
    .key = "K",
    .expect = {
      .hit = true,
      .obs = {
        { .path = "A" },
        { .kind = SPN_DAG_OBS_ABSENT, .path = "B" }
      }
    }
  });
}

UTEST_F(discovery, corrupt_manifest_misses) {
  run_test(&ur, (discovery_test_t) {
    .name = "discovery_corrupt",
    .entries = {
      { .key = "K", .obs = { { .path = "A" } } }
    },
    .corrupt = "K",
    .reload = true,
    .key = "K"
  });
}

UTEST_F(discovery, new_pathset_replaces_existing) {
  run_test(&ur, (discovery_test_t) {
    .name = "discovery_replace",
    .entries = {
      { .key = "K", .obs = { { .path = "A" } } },
      { .key = "K", .obs = { { .path = "B" } } }
    },
    .key = "K",
    .expect = {
      .hit = true,
      .obs = { { .path = "B" } }
    }
  });
}
