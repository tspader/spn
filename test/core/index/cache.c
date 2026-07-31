#include "index.h"

#include "index/cache.h"

typedef struct {
  const c8* name;
  struct {
    spn_index_protocol_t protocol;
    const c8* fixture;
  } indexes [2];
  bool memoized;

  struct {
    bool exists;
    spn_err_t err;
    spn_semver_t version;
  } expect;
} cache_test_t;

static const cache_test_t tests [] = {
  {
    .name = "first_wins_dir_first",
    .indexes = {
      { .protocol = SPN_INDEX_PROTOCOL_DIR, .fixture = "dir" },
      { .fixture = "jsonl" },
    },
    .expect = {
      .exists = true,
      .version = { 1, 5, 0 },
    },
  },
  {
    .name = "first_wins_jsonl_first",
    .indexes = {
      { .fixture = "jsonl" },
      { .protocol = SPN_INDEX_PROTOCOL_DIR, .fixture = "dir" },
    },
    .expect = {
      .exists = true,
      .version = { 1, 9, 0 },
    },
  },
  {
    .name = "fallthrough_when_absent",
    .indexes = {
      { .protocol = SPN_INDEX_PROTOCOL_DIR },
      { .protocol = SPN_INDEX_PROTOCOL_DIR, .fixture = "dir" },
    },
    .expect = {
      .exists = true,
      .version = { 1, 5, 0 },
    },
  },
  {
    .name = "error_stops_fallthrough",
    .indexes = {
      { .protocol = SPN_INDEX_PROTOCOL_DIR, .fixture = "broken" },
      { .fixture = "jsonl" },
    },
    .expect = {
      .err = SPN_ERR_MANIFEST_ISSUES,
    },
  },
  {
    .name = "memoized",
    .indexes = {
      { .protocol = SPN_INDEX_PROTOCOL_DIR, .fixture = "dir" },
      { .fixture = "jsonl" },
    },
    .memoized = true,
    .expect = {
      .exists = true,
      .version = { 1, 5, 0 },
    },
  },
};

sp_test_each(index_cache, get_package, cache_test_t, tests, .setup = spn_test_ctx_setup) {
  sp_mem_t mem = sp_test_arena(t);

  u32 slots = 0;
  sp_carr_detect_len(it->indexes, slots, it->indexes[slots].protocol || it->indexes[slots].fixture);

  spn_index_arr_t indexes = sp_da_new(mem, spn_index_info_t);
  sp_for(slot, slots) {
    sp_str_t location = sp_zero;
    if (it->indexes[slot].fixture) {
      location = test_repo_path(mem, sp_test_format(t, "test/core/index/indexes/{}", sp_fmt_cstr(it->indexes[slot].fixture)));
    } else {
      location = sp_fs_join_path(mem, sp_test_dir(t), sp_test_format(t, "{}", sp_fmt_uint(slot)));
      sp_fs_create_dir(location);
    }

    spn_index_info_t index = {
      .location = location,
      .protocol = it->indexes[slot].protocol,
    };
    sp_da_push(indexes, index);
  }

  spn_index_cache_t cache = sp_zero;
  spn_index_cache_init(&cache, mem, spn.intern, &indexes);

  spn_pkg_name_t request = {
    .namespace = sp_str_lit("core"),
    .name = sp_str_lit("A"),
  };

  spn_index_pkg_t* pkg = SP_NULLPTR;
  spn_err_union_t err = spn_index_cache_get_package(&cache, request, &pkg);

  sp_expect_eq(t, it->expect.err, err.kind);
  sp_must_eq(t, it->expect.exists, pkg != SP_NULLPTR);

  if (pkg) {
    sp_must_eq(t, 1, sp_da_size(pkg->releases));
    sp_expect(t, spn_semver_eq(it->expect.version, pkg->releases[0].version));
  }

  if (it->memoized) {
    spn_index_pkg_t* again = SP_NULLPTR;
    spn_index_cache_get_package(&cache, request, &again);
    sp_must(t, again == pkg);
  }

  sp_da_for(indexes, at) {
  }
  return SP_OK;
}
