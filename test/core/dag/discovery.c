#include "dag_test.h"

typedef struct {
  const c8* key;
  dag_test_obs_t obs [DAG_TEST_MAX_INPUTS];
} discovery_entry_t;

typedef struct {
  bool hit;
  dag_test_obs_t obs [DAG_TEST_MAX_INPUTS];
} discovery_expect_t;

typedef struct {
  const c8* key;
  const c8* content;
} discovery_plant_t;

typedef struct {
  const c8* name;
  discovery_entry_t entries [DAG_TEST_MAX_OPS];
  discovery_plant_t plant;
  bool reload;
  const c8* key;
  discovery_expect_t expect;
} discovery_test_t;

static const discovery_test_t discovery_tests [] = {
  {
    .name = "missing_key_misses",
    .key = "K"
  },
  {
    .name = "stored_pathset_hits",
    .entries = {
      {
        .key = "K",
        .obs = {
          { .path = "/A" },
          { .kind = SPN_DAG_OBS_ABSENT, .path = "/B" }
        }
      }
    },
    .key = "K",
    .expect = {
      .hit = true,
      .obs = {
        { .path = "/A" },
        { .kind = SPN_DAG_OBS_ABSENT, .path = "/B" }
      }
    }
  },
  {
    .name = "empty_pathset_hits",
    .entries = {
      { .key = "K" }
    },
    .key = "K",
    .expect = { .hit = true }
  },
  {
    .name = "keys_are_independent",
    .entries = {
      { .key = "K", .obs = { { .path = "/A" } } },
      { .key = "K2", .obs = { { .path = "/B" } } }
    },
    .key = "K",
    .expect = {
      .hit = true,
      .obs = { { .path = "/A" } }
    }
  },
  {
    .name = "reload_roundtrips_manifest",
    .entries = {
      {
        .key = "K",
        .obs = {
          { .path = "/A" },
          { .kind = SPN_DAG_OBS_ABSENT, .path = "/B" }
        }
      }
    },
    .reload = true,
    .key = "K",
    .expect = {
      .hit = true,
      .obs = {
        { .path = "/A" },
        { .kind = SPN_DAG_OBS_ABSENT, .path = "/B" }
      }
    }
  },
  {
    .name = "corrupt_manifest_misses",
    .entries = {
      { .key = "K", .obs = { { .path = "/A" } } }
    },
    .plant = { .key = "K", .content = r("not json") },
    .reload = true,
    .key = "K"
  },
  {
    .name = "stale_version_misses",
    .entries = {
      { .key = "K", .obs = { { .path = "/A" } } }
    },
    .plant = { .key = "K", .content = r("5") r("file 0 /A") },
    .reload = true,
    .key = "K"
  },
  {
    .name = "new_pathset_replaces_existing",
    .entries = {
      { .key = "K", .obs = { { .path = "/A" } } },
      { .key = "K", .obs = { { .path = "/B" } } }
    },
    .key = "K",
    .expect = {
      .hit = true,
      .obs = { { .path = "/B" } }
    }
  },
  {
    .name = "reload_roundtrips_roots",
    .entries = {
      {
        .key = "K",
        .obs = {
          { .path = "H", .root = SPN_PATH_ROOT_PROJECT },
          { .path = "P/H", .root = SPN_PATH_ROOT_STORE },
          { .path = "/X/H" }
        }
      }
    },
    .reload = true,
    .key = "K",
    .expect = {
      .hit = true,
      .obs = {
        { .path = "H", .root = SPN_PATH_ROOT_PROJECT },
        { .path = "P/H", .root = SPN_PATH_ROOT_STORE },
        { .path = "/X/H" }
      }
    }
  },
};

static void discovery_put(spn_dag_obs_table_t* discovery, const discovery_entry_t* entry) {
  spn_dag_obs_t obs [DAG_TEST_MAX_INPUTS] = sp_zero;
  u32 count = dag_test_obs_build(entry->obs, DAG_TEST_MAX_INPUTS, obs);
  spn_dag_obs_table_put(discovery, dag_test_digest(entry->key), obs, count);
}

static sp_err_t discovery_expect_obs(sp_test_t* t, const spn_dag_pathset_t* set, const dag_test_obs_t* expect) {
  spn_dag_obs_t obs [DAG_TEST_MAX_INPUTS] = sp_zero;
  u32 count = dag_test_obs_build(expect, DAG_TEST_MAX_INPUTS, obs);
  sp_must_eq(t, count, (u32)sp_da_size(set->obs));
  sp_for(it, count) {
    sp_expect_eq(t, obs[it].kind, set->obs[it].kind);
    sp_expect_eq(t, obs[it].path.root, set->obs[it].path.root);
    sp_expect_str_eq(t, set->obs[it].path.sub, obs[it].path.sub);
  }
  return SP_OK;
}

static sp_err_t discovery_expect(sp_test_t* t, spn_dag_obs_table_t* discovery, const c8* key, const discovery_expect_t* expect) {
  spn_dag_pathset_t set = sp_zero;
  bool present = spn_dag_obs_table_get(discovery, dag_test_digest(key), sp_test_arena(t), &set);
  sp_must_eq(t, expect->hit, present);
  if (!expect->hit) {
    return SP_OK;
  }

  return discovery_expect_obs(t, &set, expect->obs);
}

sp_test_each(dag_discovery, table, discovery_test_t, discovery_tests) {
  sp_mem_t mem = sp_test_arena(t);
  sp_str_t dir = sp_fs_join_path(mem, sp_test_dir(t), sp_str_lit("manifests"));

  spn_dag_obs_table_t discovery = sp_zero;
  spn_dag_obs_table_init(&discovery, mem, dir);

  sp_carr_for(it->entries, et) {
    if (!it->entries[et].key) {
      break;
    }
    discovery_put(&discovery, &it->entries[et]);
  }

  if (it->plant.key) {
    sp_str_t hex = spn_dag_digest_hex(mem, dag_test_digest(it->plant.key));
    sp_str_t path = sp_fs_join_path(mem, dir, sp_fmt(mem, "{}.txt", sp_fmt_str(hex)).value);
    sp_must_eq(t, SP_OK, sp_fs_create_file_cstr(path, it->plant.content));
  }

  if (it->reload) {
    spn_dag_obs_table_init(&discovery, mem, dir);
  }

  return discovery_expect(t, &discovery, it->key, &it->expect);
}
