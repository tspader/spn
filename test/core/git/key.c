#include "sp.h"
#include "utest.h"
#include "test.h"

#include "git/key.h"


/////////////////
// DESCRIPTOR //
////////////////
typedef struct {
  const c8* url;
  const c8* expected_name;
} url_name_case_t;

typedef struct {
  const c8* url_a;
  const c8* url_b;
} uniqueness_case_t;

typedef struct {
  const c8* url;
  const c8* rev;
  const c8* dir;
  sp_hash_t patches;
} checkout_key_input_t;


///////////
// STATE //
///////////
struct git_key {
  u32 dummy;
};

UTEST_F_SETUP(git_key) {
}

UTEST_F_TEARDOWN(git_key) {
}


///////////////
// EXECUTOR //
//////////////
static void run_url_name(s32* utest_result, url_name_case_t c) {
  sp_str_t name = spn_git_url_name(sp_str_view(c.url));
  SP_EXPECT_STR_EQ_CSTR(name, c.expected_name);
}

static void run_db_key_format(s32* utest_result, const c8* url) {
  sp_mem_t mem = sp_mem_arena_as_allocator(ctx_get()->arena);
  sp_str_t key = spn_git_db_key(mem, sp_str_view(url));
  sp_str_t name = spn_git_url_name(sp_str_view(url));

  ASSERT_TRUE(key.len > name.len + 1);
  EXPECT_TRUE(sp_str_starts_with(key, name));
  EXPECT_EQ(key.data[name.len], '-');

  sp_str_t hash_part = sp_str_suffix(key, key.len - name.len - 1);
  ASSERT_EQ(hash_part.len, 16);

  sp_for(it, hash_part.len) {
    c8 c = hash_part.data[it];
    EXPECT_TRUE((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'));
  }
}

static void run_db_key_determinism(s32* utest_result, const c8* url) {
  sp_mem_t mem = sp_mem_arena_as_allocator(ctx_get()->arena);
  sp_str_t a = spn_git_db_key(mem, sp_str_view(url));
  sp_str_t b = spn_git_db_key(mem, sp_str_view(url));
  SP_EXPECT_STR_EQ(a, b);
}

static void run_db_key_uniqueness(s32* utest_result, uniqueness_case_t c) {
  sp_mem_t mem = sp_mem_arena_as_allocator(ctx_get()->arena);
  sp_str_t a = spn_git_db_key(mem, sp_str_view(c.url_a));
  sp_str_t b = spn_git_db_key(mem, sp_str_view(c.url_b));
  EXPECT_FALSE(sp_str_equal(a, b));
}

static spn_git_checkout_id_t checkout_id(checkout_key_input_t c) {
  return (spn_git_checkout_id_t) {
    .url = sp_str_view(c.url),
    .rev = sp_str_view(c.rev),
    .dir = sp_str_view(c.dir),
    .patches.hash = c.patches,
  };
}


static void run_checkout_key_format(s32* utest_result, checkout_key_input_t c) {
  sp_mem_t mem = sp_mem_arena_as_allocator(ctx_get()->arena);
  sp_str_t key = spn_git_checkout_key(mem, checkout_id(c));
  sp_str_t name = spn_git_url_name(sp_str_view(c.url));

  ASSERT_TRUE(key.len > name.len + 1);
  EXPECT_TRUE(sp_str_starts_with(key, name));
  EXPECT_EQ(key.data[name.len], '-');

  sp_str_t hash_part = sp_str_suffix(key, key.len - name.len - 1);
  ASSERT_EQ(hash_part.len, 16);
}

static void run_checkout_key_uniqueness(s32* utest_result, checkout_key_input_t a, checkout_key_input_t b) {
  sp_mem_t mem = sp_mem_arena_as_allocator(ctx_get()->arena);
  sp_str_t ka = spn_git_checkout_key(mem, checkout_id(a));
  sp_str_t kb = spn_git_checkout_key(mem, checkout_id(b));
  EXPECT_FALSE(sp_str_equal(ka, kb));
}


////////////
// CASES //
///////////

// url name extraction
UTEST_F(git_key, url_name_https_git_suffix) {
  run_url_name(utest_result, (url_name_case_t) {
    .url = "https://github.com/foo/sqlite.git",
    .expected_name = "sqlite",
  });
}

UTEST_F(git_key, url_name_https_no_suffix) {
  run_url_name(utest_result, (url_name_case_t) {
    .url = "https://github.com/foo/bar",
    .expected_name = "bar",
  });
}

UTEST_F(git_key, url_name_ssh) {
  run_url_name(utest_result, (url_name_case_t) {
    .url = "git@github.com:foo/baz.git",
    .expected_name = "baz",
  });
}

UTEST_F(git_key, url_name_trailing_slash) {
  run_url_name(utest_result, (url_name_case_t) {
    .url = "https://github.com/foo/bar/",
    .expected_name = "bar",
  });
}

UTEST_F(git_key, url_name_local_path) {
  run_url_name(utest_result, (url_name_case_t) {
    .url = "/home/user/repos/mylib",
    .expected_name = "mylib",
  });
}

// db key format: <name>-<16 hex chars>
UTEST_F(git_key, db_key_format) {
  run_db_key_format(utest_result, "https://github.com/foo/sqlite.git");
}

// determinism
UTEST_F(git_key, db_key_deterministic) {
  run_db_key_determinism(utest_result, "https://github.com/foo/sqlite.git");
}

// uniqueness
UTEST_F(git_key, db_key_different_urls) {
  run_db_key_uniqueness(utest_result, (uniqueness_case_t) {
    .url_a = "https://github.com/foo/sqlite.git",
    .url_b = "https://github.com/bar/sqlite.git",
  });
}

// checkout key format
UTEST_F(git_key, checkout_key_format) {
  run_checkout_key_format(utest_result, (checkout_key_input_t) {
    .url = "https://github.com/foo/sqlite.git",
    .rev = "abc123",
    .dir = "",
  });
}

// checkout key: different rev -> different key
UTEST_F(git_key, checkout_key_different_rev) {
  run_checkout_key_uniqueness(utest_result,
    (checkout_key_input_t) { .url = "https://github.com/foo/bar.git", .rev = "aaa", .dir = "" },
    (checkout_key_input_t) { .url = "https://github.com/foo/bar.git", .rev = "bbb", .dir = "" }
  );
}

// checkout key: different dir -> different key
UTEST_F(git_key, checkout_key_different_dir) {
  run_checkout_key_uniqueness(utest_result,
    (checkout_key_input_t) { .url = "https://github.com/foo/bar.git", .rev = "aaa", .dir = "" },
    (checkout_key_input_t) { .url = "https://github.com/foo/bar.git", .rev = "aaa", .dir = "packages/math" }
  );
}

// checkout key: different url -> different key
UTEST_F(git_key, checkout_key_different_url) {
  run_checkout_key_uniqueness(utest_result,
    (checkout_key_input_t) { .url = "https://github.com/foo/bar.git", .rev = "aaa", .dir = "" },
    (checkout_key_input_t) { .url = "https://github.com/foo/baz.git", .rev = "aaa", .dir = "" }
  );
}

// checkout key: patched -> different key than pristine
UTEST_F(git_key, checkout_key_patched_differs) {
  run_checkout_key_uniqueness(utest_result,
    (checkout_key_input_t) { .url = "https://github.com/foo/bar.git", .rev = "aaa", .dir = "" },
    (checkout_key_input_t) { .url = "https://github.com/foo/bar.git", .rev = "aaa", .dir = "", .patches = 0xdead }
  );
}

// checkout key: different patch sets -> different keys
UTEST_F(git_key, checkout_key_different_patches) {
  run_checkout_key_uniqueness(utest_result,
    (checkout_key_input_t) { .url = "https://github.com/foo/bar.git", .rev = "aaa", .dir = "", .patches = 0xdead },
    (checkout_key_input_t) { .url = "https://github.com/foo/bar.git", .rev = "aaa", .dir = "", .patches = 0xbeef }
  );
}

// checkout key: golden. Existing global caches depend on unpatched keys never
// changing; an implementation that folds a zero patch part into the hash (or
// otherwise perturbs the formula) breaks every cache dir on disk.
UTEST_F(git_key, checkout_key_golden) {
  sp_mem_t mem = sp_mem_arena_as_allocator(ctx_get()->arena);
  sp_str_t key = spn_git_checkout_key(mem, checkout_id((checkout_key_input_t) {
    .url = "https://github.com/foo/bar.git", .rev = "aaa", .dir = ""
  }));
  SP_EXPECT_STR_EQ_CSTR(key, "bar-51a9a462b5f729ea");
}

// checkout key: patched keys keep the <name>-<16 hex> format
UTEST_F(git_key, checkout_key_patched_format) {
  run_checkout_key_format(utest_result, (checkout_key_input_t) {
    .url = "https://github.com/foo/sqlite.git",
    .rev = "abc123",
    .dir = "",
    .patches = 0xdead,
  });
}
