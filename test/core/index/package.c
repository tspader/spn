#include "index.h"

#include "index/release.h"

typedef struct {
  const c8* url;
  const c8* rev;
  const c8* dir;
  const c8* local;
} root_t;

typedef struct {
  root_t source;
  root_t manifest;
} expect_t;

typedef struct {
  const c8* name;
  struct { const c8* url; const c8* commit; } upstream;
  root_t published;
  expect_t expect;
} test_t;

static const test_t tests [] = {
  {
    .name = "native",
    .published = { .url = "https://github.com/A/B.git", .rev = "R", .dir = "D" },
    .expect = {
      .source = { .url = "https://github.com/A/B.git", .rev = "R", .dir = "D" },
    },
  },
  {
    .name = "upstream",
    .upstream = { .url = "https://github.com/A/B.git", .commit = "R" },
    .published = { .url = "https://github.com/C/D.git", .rev = "S" },
    .expect = {
      .source = { .url = "https://github.com/A/B.git", .rev = "R" },
      .manifest = { .url = "https://github.com/C/D.git", .rev = "S" },
    },
  },
  {
    .name = "local",
    .published = { .local = "/A" },
    .expect = {
      .source = { .local = "/A" },
    },
  },
  {
    .name = "local_upstream",
    .upstream = { .url = "https://github.com/A/B.git", .commit = "R" },
    .published = { .local = "/A" },
    .expect = {
      .source = { .url = "https://github.com/A/B.git", .rev = "R" },
      .manifest = { .local = "/A" },
    },
  },
};

static spn_pkg_root_t build(root_t root) {
  if (root.local) {
    return (spn_pkg_root_t) { .kind = SPN_PKG_ROOT_LOCAL, .local = sp_cstr_as_str(root.local) };
  }
  return (spn_pkg_root_t) {
    .kind = SPN_PKG_ROOT_GIT,
    .git = {
      .url = sp_cstr_as_str(root.url),
      .rev = sp_cstr_as_str(root.rev ? root.rev : ""),
      .dir = sp_cstr_as_str(root.dir ? root.dir : ""),
    },
  };
}

static sp_err_t check(sp_test_t* t, root_t expect, spn_pkg_root_t root) {
  if (expect.local) {
    sp_must_eq(t, SPN_PKG_ROOT_LOCAL, root.kind);
    sp_expect_str_eq_c(t, root.local, expect.local);
  } else if (expect.url) {
    sp_must_eq(t, SPN_PKG_ROOT_GIT, root.kind);
    sp_expect_str_eq_c(t, root.git.url, expect.url);
    sp_expect_str_eq_c(t, root.git.rev, expect.rev ? expect.rev : "");
    sp_expect_str_eq_c(t, root.git.dir, expect.dir ? expect.dir : "");
  } else {
    sp_expect_eq(t, SPN_PKG_ROOT_NONE, root.kind);
  }
  return SP_OK;
}

sp_test_each(index_from_pkg, roots, test_t, tests) {
  sp_mem_t mem = sp_test_arena(t);

  spn_pkg_info_t info = sp_zero;
  info.qualified = sp_str_lit("core/A");
  if (it->upstream.url) {
    info.upstream.url = sp_cstr_as_str(it->upstream.url);
    info.upstream.commit = sp_cstr_as_str(it->upstream.commit);
  }

  spn_index_release_t release = sp_zero;
  sp_str_t dep = sp_zero;
  sp_must_eq(t, SPN_OK, spn_index_release_from_pkg(mem, &info, build(it->published), &release, &dep));

  if (check(t, it->expect.source, release.source)) {
    return SP_ERR;
  }
  return check(t, it->expect.manifest, release.manifest);
}
