#include "git.h"

typedef struct {
  const c8* name;
  const c8* url;
  const c8* expect;
} url_name_t;

typedef struct {
  const c8* name;
  const c8* url;
} db_key_t;

typedef struct {
  const c8* name;
  const c8* url_a;
  const c8* url_b;
} db_key_pair_t;

typedef struct {
  const c8* url;
  const c8* rev;
  const c8* dir;
  sp_hash_t patches;
} checkout_key_input_t;

// Rows with only .a assert the <name>-<16 hex> key format; rows with .b assert
// that the two inputs produce different keys
typedef struct {
  const c8* name;
  checkout_key_input_t a;
  checkout_key_input_t b;
} checkout_key_t;

static const url_name_t url_names [] = {
  { .name = "https_git_suffix", .url = "https://github.com/foo/sqlite.git", .expect = "sqlite" },
  { .name = "https_no_suffix",  .url = "https://github.com/foo/bar",        .expect = "bar" },
  { .name = "ssh",              .url = "git@github.com:foo/baz.git",        .expect = "baz" },
  { .name = "trailing_slash",   .url = "https://github.com/foo/bar/",       .expect = "bar" },
  { .name = "local_path",       .url = "/home/user/repos/mylib",            .expect = "mylib" },
};

// db key format: <name>-<16 hex chars>
static const db_key_t db_keys [] = {
  { .name = "format_and_determinism", .url = "https://github.com/foo/sqlite.git" },
};

// uniqueness
static const db_key_pair_t db_key_pairs [] = {
  {
    .name = "different_urls",
    .url_a = "https://github.com/foo/sqlite.git",
    .url_b = "https://github.com/bar/sqlite.git",
  },
};

static const checkout_key_t checkout_keys [] = {
  // checkout key format
  {
    .name = "format",
    .a = { .url = "https://github.com/foo/sqlite.git", .rev = "abc123" },
  },
  // checkout key: patched keys keep the <name>-<16 hex> format
  {
    .name = "patched_format",
    .a = { .url = "https://github.com/foo/sqlite.git", .rev = "abc123", .patches = 0xdead },
  },
  // checkout key: different rev -> different key
  {
    .name = "different_rev",
    .a = { .url = "https://github.com/foo/bar.git", .rev = "aaa" },
    .b = { .url = "https://github.com/foo/bar.git", .rev = "bbb" },
  },
  // checkout key: different dir -> different key
  {
    .name = "different_dir",
    .a = { .url = "https://github.com/foo/bar.git", .rev = "aaa" },
    .b = { .url = "https://github.com/foo/bar.git", .rev = "aaa", .dir = "packages/math" },
  },
  // checkout key: different url -> different key
  {
    .name = "different_url",
    .a = { .url = "https://github.com/foo/bar.git", .rev = "aaa" },
    .b = { .url = "https://github.com/foo/baz.git", .rev = "aaa" },
  },
  // checkout key: patched -> different key than pristine
  {
    .name = "patched_differs",
    .a = { .url = "https://github.com/foo/bar.git", .rev = "aaa" },
    .b = { .url = "https://github.com/foo/bar.git", .rev = "aaa", .patches = 0xdead },
  },
  // checkout key: different patch sets -> different keys
  {
    .name = "different_patches",
    .a = { .url = "https://github.com/foo/bar.git", .rev = "aaa", .patches = 0xdead },
    .b = { .url = "https://github.com/foo/bar.git", .rev = "aaa", .patches = 0xbeef },
  },
};

static spn_git_checkout_id_t checkout_id(checkout_key_input_t c) {
  return (spn_git_checkout_id_t) {
    .url = sp_str_view(c.url),
    .rev = sp_str_view(c.rev),
    .dir = c.dir ? sp_str_view(c.dir) : SP_LIT(""),
    .patches.hash = c.patches,
  };
}

static sp_err_t expect_key_format(sp_test_t* t, sp_str_t key, sp_str_t url) {
  sp_str_t name = spn_git_url_name(url);

  sp_must(t, key.len > name.len + 1);
  sp_expect(t, sp_str_starts_with(key, name));
  sp_expect_eq(t, key.data[name.len], '-');

  sp_str_t hash = sp_str_suffix(key, key.len - name.len - 1);
  sp_must_eq(t, hash.len, 16);

  return SP_OK;
}

sp_test_each(git_key, url_name, url_name_t, url_names) {
  sp_str_t name = spn_git_url_name(sp_str_view(it->url));
  sp_expect_str_eq_c(t, name, it->expect);
  return SP_OK;
}

sp_test_each(git_key, db_key, db_key_t, db_keys) {
  sp_mem_t mem = sp_test_arena(t);
  sp_str_t url = sp_str_view(it->url);

  sp_str_t key = spn_git_db_key(mem, url);
  if (expect_key_format(t, key, url)) {
    return SP_ERR;
  }

  sp_str_t name = spn_git_url_name(url);
  sp_str_t hash = sp_str_suffix(key, key.len - name.len - 1);
  sp_for(i, hash.len) {
    c8 c = hash.data[i];
    sp_expect(t, (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'));
  }

  sp_expect_str_eq(t, key, spn_git_db_key(mem, url));
  return SP_OK;
}

sp_test_each(git_key, db_key_uniqueness, db_key_pair_t, db_key_pairs) {
  sp_mem_t mem = sp_test_arena(t);
  sp_str_t a = spn_git_db_key(mem, sp_str_view(it->url_a));
  sp_str_t b = spn_git_db_key(mem, sp_str_view(it->url_b));
  sp_expect(t, !sp_str_equal(a, b));
  return SP_OK;
}

sp_test_each(git_key, checkout_key, checkout_key_t, checkout_keys) {
  sp_mem_t mem = sp_test_arena(t);

  if (!it->b.url) {
    sp_str_t key = spn_git_checkout_key(mem, checkout_id(it->a));
    return expect_key_format(t, key, sp_str_view(it->a.url));
  }

  sp_str_t ka = spn_git_checkout_key(mem, checkout_id(it->a));
  sp_str_t kb = spn_git_checkout_key(mem, checkout_id(it->b));
  sp_expect(t, !sp_str_equal(ka, kb));
  return SP_OK;
}

// checkout key: golden. Existing global caches depend on unpatched keys never
// changing; an implementation that folds a zero patch part into the hash (or
// otherwise perturbs the formula) breaks every cache dir on disk.
sp_test(git_key, checkout_key_golden) {
  sp_str_t key = spn_git_checkout_key(sp_test_arena(t), checkout_id((checkout_key_input_t) {
    .url = "https://github.com/foo/bar.git", .rev = "aaa", .dir = ""
  }));
  sp_expect_str_eq_c(t, key, "bar-51a9a462b5f729ea");
  return SP_OK;
}
