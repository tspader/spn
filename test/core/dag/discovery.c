#include "common.h"

typedef struct {
  spn_dag_obs_kind_t kind;
  const c8* path;
} discovery_obs_t;

typedef struct {
  const c8* key;
  discovery_obs_t obs [DAG_TEST_MAX_INPUTS];
} discovery_entry_t;

typedef struct {
  bool hit;
  discovery_obs_t obs [DAG_TEST_MAX_INPUTS];
} discovery_expect_t;

typedef struct {
  discovery_entry_t entries [DAG_TEST_MAX_OPS];
  const c8* key;
  discovery_expect_t expect;
} discovery_test_t;

typedef struct {
  const c8* name;
  discovery_entry_t entries [DAG_TEST_MAX_OPS];
} discovery_persistence_test_t;

typedef struct {
  const c8* name;
} discovery_load_test_t;

UTEST_EMPTY_FIXTURE(discovery)

static spn_dag_digest_t discovery_key(const c8* key) {
  sp_str_t str = sp_str_view(key);
  return spn_dag_digest(str.data, str.len);
}

static u32 discovery_build_obs(discovery_obs_t* specs, spn_dag_obs_t* obs) {
  u32 count = 0;
  sp_for(it, DAG_TEST_MAX_INPUTS) {
    if (!specs[it].path) {
      break;
    }
    obs[count++] = (spn_dag_obs_t) {
      .kind = specs[it].kind,
      .path = sp_str_view(specs[it].path)
    };
  }
  return count;
}

static void discovery_put(spn_dag_discovery_t* discovery, discovery_entry_t entry) {
  spn_dag_obs_t obs [DAG_TEST_MAX_INPUTS] = sp_zero;
  u32 count = discovery_build_obs(entry.obs, obs);
  spn_dag_discovery_put(discovery, discovery_key(entry.key), obs, count);
}

static void discovery_expect_obs(s32* utest_result, const spn_dag_pathset_t* set, discovery_obs_t* expect) {
  spn_dag_obs_t obs [DAG_TEST_MAX_INPUTS] = sp_zero;
  u32 count = discovery_build_obs(expect, obs);
  ASSERT_EQ(count, (u32)sp_da_size(set->obs));
  sp_for(it, count) {
    EXPECT_EQ(obs[it].kind, set->obs[it].kind);
    EXPECT_TRUE(sp_str_equal(set->obs[it].path, obs[it].path));
  }
}

static void discovery_expect(s32* utest_result, spn_dag_discovery_t* discovery, const c8* key, discovery_expect_t expect) {
  const spn_dag_pathset_t* set = spn_dag_discovery_get(discovery, discovery_key(key));
  EXPECT_EQ(expect.hit, set != SP_NULLPTR);
  if (!expect.hit || !set) {
    return;
  }

  discovery_expect_obs(utest_result, set, expect.obs);
}

static void run_discovery_test(s32* utest_result, discovery_test_t t) {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  spn_dag_discovery_t discovery = sp_zero;
  spn_dag_discovery_init(&discovery, scratch.mem, sp_str_lit(""));

  sp_carr_for(t.entries, it) {
    if (!t.entries[it].key) {
      break;
    }
    discovery_put(&discovery, t.entries[it]);
  }

  discovery_expect(utest_result, &discovery, t.key, t.expect);
  sp_mem_end_scratch(scratch);
}

static void run_discovery_persistence_test(s32* utest_result, discovery_persistence_test_t t) {
  tmpfs_t fs = sp_zero;
  tmpfs_init_named(&fs, t.name);
  sp_str_t dir = tmpfs_get(&fs, sp_str_lit("manifests"));

  spn_dag_discovery_t saved = sp_zero;
  spn_dag_discovery_init(&saved, fs.mem, dir);
  sp_carr_for(t.entries, it) {
    if (!t.entries[it].key) {
      break;
    }
    discovery_put(&saved, t.entries[it]);
  }

  spn_dag_discovery_t loaded = sp_zero;
  spn_dag_discovery_init(&loaded, fs.mem, dir);
  sp_carr_for(t.entries, it) {
    if (!t.entries[it].key) {
      break;
    }
    const spn_dag_pathset_t* set = spn_dag_discovery_get(&loaded, discovery_key(t.entries[it].key));
    ASSERT_TRUE(set);
    discovery_expect_obs(utest_result, set, t.entries[it].obs);
  }

  tmpfs_deinit(&fs);
}

static void run_discovery_load_test(s32* utest_result, discovery_load_test_t t) {
  tmpfs_t fs = sp_zero;
  tmpfs_init_named(&fs, t.name);

  spn_dag_discovery_t discovery = sp_zero;
  spn_dag_discovery_init(&discovery, fs.mem, tmpfs_get(&fs, sp_str_lit("manifests")));
  EXPECT_EQ(SP_NULLPTR, spn_dag_discovery_get(&discovery, discovery_key("K")));

  tmpfs_deinit(&fs);
}

UTEST_F(discovery, missing_key_misses) {
  run_discovery_test(&ur, (discovery_test_t) {
    .key = "K"
  });
}

UTEST_F(discovery, stored_pathset_hits) {
  run_discovery_test(&ur, (discovery_test_t) {
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
  run_discovery_test(&ur, (discovery_test_t) {
    .entries = {
      { .key = "K" }
    },
    .key = "K",
    .expect = { .hit = true }
  });
}

UTEST_F(discovery, keys_are_independent) {
  run_discovery_test(&ur, (discovery_test_t) {
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

UTEST_F(discovery, new_pathset_replaces_existing) {
  run_discovery_test(&ur, (discovery_test_t) {
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

UTEST_F(discovery, save_load_roundtrip) {
  run_discovery_persistence_test(&ur, (discovery_persistence_test_t) {
    .name = "discovery_roundtrip",
    .entries = {
      {
        .key = "K",
        .obs = {
          { .path = "A" },
          { .kind = SPN_DAG_OBS_ABSENT, .path = "B" }
        }
      },
      { .key = "K2" }
    }
  });
}

UTEST_F(discovery, missing_manifest_misses) {
  run_discovery_load_test(&ur, (discovery_load_test_t) {
    .name = "discovery_missing"
  });
}
