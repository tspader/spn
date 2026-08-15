#include "dag_test.h"

#define HINTS_TEST_MAX_FILES 4

typedef struct {
  const c8* path;
  const c8* content;
  spn_path_root_t root;
} hints_file_t;

typedef struct {
  const c8* name;
  hints_file_t files [HINTS_TEST_MAX_FILES];
} hints_test_t;

static const hints_test_t hints_tests [] = {
  {
    .name = "rooted_file",
    .files = { { .path = "A", .content = "a", .root = SPN_PATH_ROOT_PROJECT } }
  },
  {
    .name = "nested_sub",
    .files = { { .path = "D/E/A", .content = "a", .root = SPN_PATH_ROOT_PROJECT } }
  },
  {
    .name = "outside_roots_absolute",
    .files = { { .path = "A", .content = "a" } }
  },
  {
    .name = "mixed_roots",
    .files = {
      { .path = "A", .content = "a", .root = SPN_PATH_ROOT_PROJECT },
      { .path = "D/B", .content = "bb", .root = SPN_PATH_ROOT_PROJECT },
      { .path = "C", .content = "ccc" },
    }
  },
};

static spn_path_t hints_key(dag_test_env_t* env, const hints_file_t* file) {
  spn_path_t rooted = dag_test_env_rooted(env, sp_str_view(file->path));
  if (file->root == SPN_PATH_ROOT_NONE) {
    return (spn_path_t) { .sub = dag_test_render(env, rooted) };
  }
  return rooted;
}

sp_test_each(dag_hints, roundtrip, hints_test_t, hints_tests) {
  dag_test_env_t env;
  dag_test_env_init(&env, t, (dag_test_env_config_t) sp_zero);
  sp_str_t path = dag_test_env_path(&env, sp_str_lit("files"));

  u32 count = 0;
  sp_carr_detect_len(it->files, count, it->files[count].path);
  sp_for(f, count) {
    dag_test_env_create(&env, sp_str_view(it->files[f].path), sp_str_view(it->files[f].content));
  }

  spn_dag_digest_t hashed [HINTS_TEST_MAX_FILES] = sp_zero;
  sp_for(f, count) {
    sp_must_eq(t, SPN_OK, spn_dag_file_cache_digest(&env.files, hints_key(&env, &it->files[f]), &hashed[f]));
  }
  spn_dag_file_cache_flush(&env.files, path);

  sp_str_t content = sp_zero;
  sp_must_eq(t, SP_OK, sp_io_read_file(env.mem, path, &content));
  sp_expect(t, sp_str_starts_with(content, sp_str_lit("3\n")));
  sp_for(f, count) {
    spn_path_t key = hints_key(&env, &it->files[f]);
    sp_str_t row = sp_fmt(env.mem, " {} {}\n", sp_fmt_uint(key.root), sp_fmt_str(key.sub)).value;
    sp_expect(t, sp_str_contains(content, row));
  }

  spn_dag_stats_t stats = sp_zero;
  spn_dag_file_cache_t reloaded = sp_zero;
  spn_dag_file_cache_init(&reloaded, env.mem, &env.roots);
  reloaded.stats = &stats;
  spn_dag_file_cache_load(&reloaded, path);
  sp_for(f, count) {
    spn_dag_digest_t digest = sp_zero;
    sp_must_eq(t, SPN_OK, spn_dag_file_cache_digest(&reloaded, hints_key(&env, &it->files[f]), &digest));
    sp_expect(t, spn_dag_digest_equal(digest, hashed[f]));
  }
  sp_expect_eq(t, 0u, sp_atomic_u32_load(&stats.hashed_files, SP_ATOMIC_SEQ_CST));

  return SP_OK;
}
