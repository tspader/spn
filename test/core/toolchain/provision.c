#include "toolchain.h"

#define PROVISION_MAX_TOOLCHAINS 2

typedef enum {
  PROVISION_TARBALL_TREE,
  PROVISION_TARBALL_GARBAGE,
  PROVISION_TARBALL_LOOSE,
} provision_tarball_t;

typedef struct {
  spn_err_t kind;
  u32 calls;
  const c8* last_url;
  bool root_empty;
  bool root_in_store;
  bool extracted;
  bool store_clean;
  bool err_reports_sha;
} provision_expect_t;

typedef struct {
  const c8* name;
  const c8* toolchains [PROVISION_MAX_TOOLCHAINS];
  bool local;
  provision_tarball_t tarball;
  const c8* sha;
  bool no_sha;
  const c8* mirror;
  bool fetch_fail;
  const c8* fail_url_containing;
  const c8* store_dir;
  bool dest_file;
  provision_expect_t expect;
} provision_test_t;

typedef struct {
  const c8* name;
  const c8* url;
  const c8* mirror;
  const c8* expect;
} resolve_test_t;

typedef struct {
  u32 calls;
  sp_mem_t mem;
  sp_str_t tarball;
  sp_str_t last_url;
  sp_str_t fail_url_containing;
  bool fail;
} fetch_stub_t;

static const provision_test_t tests [] = {
  {
    .name = "local_toolchain_is_noop",
    .local = true,
    .expect = { .root_empty = true },
  },
  {
    .name = "fresh_artifact_downloads_and_extracts",
    .expect = {
      .calls = 1,
      .root_in_store = true,
      .extracted = true,
    },
  },
  {
    .name = "artifacts_share_store_by_sha",
    .toolchains = { "A", "B" },
    .expect = { .calls = 1 },
  },
  {
    .name = "sha_mismatch_fails_and_leaves_no_store_entry",
    .sha = "beefbeefbeefbeefbeefbeefbeefbeefbeefbeefbeefbeefbeefbeefbeefbeef",
    .expect = {
      .kind = SPN_ERR_TOOLCHAIN_SHA,
      .calls = 1,
      .store_clean = true,
      .err_reports_sha = true,
    },
  },
  {
    .name = "corrupt_archive_fails_and_leaves_no_store_entry",
    .tarball = PROVISION_TARBALL_GARBAGE,
    .expect = {
      .kind = SPN_ERR_TOOLCHAIN_EXTRACT,
      .calls = 1,
      .store_clean = true,
    },
  },
  {
    .name = "fetch_failure_propagates",
    .fetch_fail = true,
    .expect = {
      .kind = SPN_ERR_TOOLCHAIN_FETCH,
      .calls = 1,
      .store_clean = true,
    },
  },
  {
    .name = "mirror_used_for_fetch",
    .mirror = "https://mirror.example.com/M",
    .expect = {
      .calls = 1,
      .last_url = "https://mirror.example.com/M/A.tar.gz",
    },
  },
  {
    .name = "broken_mirror_falls_back_to_canonical",
    .mirror = "https://mirror.example.com/M",
    .fail_url_containing = "mirror.example.com",
    .expect = {
      .calls = 2,
      .last_url = "https://tc.example.com/A.tar.gz",
    },
  },
  {
    .name = "mirror_matching_canonical_is_not_retried",
    .mirror = "https://tc.example.com",
    .fetch_fail = true,
    .expect = {
      .kind = SPN_ERR_TOOLCHAIN_FETCH,
      .calls = 1,
      .last_url = "https://tc.example.com/A.tar.gz",
    },
  },
  {
    .name = "single_file_archive_fails_extract",
    .tarball = PROVISION_TARBALL_LOOSE,
    .expect = {
      .kind = SPN_ERR_TOOLCHAIN_EXTRACT,
      .calls = 1,
      .store_clean = true,
    },
  },
  {
    .name = "dest_occupied_by_file_fails",
    .dest_file = true,
    .expect = {
      .kind = SPN_ERR_TOOLCHAIN_EXTRACT,
      .calls = 1,
    },
  },
  {
    .name = "empty_sha_is_rejected",
    .no_sha = true,
    .expect = { .kind = SPN_ERR_TOOLCHAIN_NO_SHA },
  },
  {
    .name = "missing_store_dir_is_created",
    .store_dir = "store/nested/deeper",
    .expect = {
      .calls = 1,
      .extracted = true,
    },
  },
};

static const resolve_test_t resolve_tests [] = {
  {
    .name = "mirror_rewrites_url",
    .url = "https://tc.example.com/x/A.tar.gz",
    .mirror = "https://mirror.example.com/M",
    .expect = "https://mirror.example.com/M/A.tar.gz",
  },
  {
    .name = "mirror_trailing_slash",
    .url = "https://tc.example.com/x/A.tar.gz",
    .mirror = "https://mirror.example.com/M/",
    .expect = "https://mirror.example.com/M/A.tar.gz",
  },
  {
    .name = "empty_mirror_is_identity",
    .url = "https://tc.example.com/x/A.tar.gz",
    .mirror = "",
    .expect = "https://tc.example.com/x/A.tar.gz",
  },
  {
    .name = "url_without_file_is_unchanged",
    .url = "https://tc.example.com/",
    .mirror = "https://mirror.example.com/M",
    .expect = "https://tc.example.com/",
  },
};

static spn_err_t fetch_stub(sp_str_t url, sp_str_t dest, void* user_data) {
  fetch_stub_t* stub = (fetch_stub_t*)user_data;
  stub->calls++;
  stub->last_url = sp_str_copy(stub->mem, url);
  if (stub->fail) return SPN_ERROR;
  if (!sp_str_empty(stub->fail_url_containing) && sp_str_contains(url, stub->fail_url_containing)) return SPN_ERROR;
  if (sp_fs_copy(stub->tarball, dest)) return SPN_ERROR;
  return SPN_OK;
}

static void provision_create(sp_mem_t mem, sp_str_t dir, const c8* rel, const c8* content) {
  sp_str_t path = sp_fs_join_path(mem, dir, sp_str_view(rel));
  sp_fs_create_dir(sp_fs_parent_path(path));
  sp_fs_create_file_str(path, sp_str_view(content));
}

sp_test_each(provision, store, provision_test_t, tests, .setup = spn_test_ctx_setup) {
  sp_mem_t mem = sp_test_arena(t);
  sp_str_t dir = sp_test_dir(t);

  fetch_stub_t stub = sp_zero;
  stub.mem = mem;
  stub.fail = it->fetch_fail;
  if (it->fail_url_containing) {
    stub.fail_url_containing = sp_str_view(it->fail_url_containing);
  }

  switch (it->tarball) {
    case PROVISION_TARBALL_TREE: {
      provision_create(mem, dir, "tree/A/B", "B");
      provision_create(mem, dir, "tree/A/lib/C", "C");
      stub.tarball = sp_fs_join_path(mem, dir, sp_str_lit("A.tar.gz"));
      sp_ps_output_t tar = sp_ps_run(mem, (sp_ps_config_t) {
        .command = sp_str_lit("tar"),
        .args = {
          sp_str_lit("czf"), stub.tarball,
          sp_str_lit("-C"), sp_fs_join_path(mem, dir, sp_str_lit("tree")),
          sp_str_lit("A"),
        }
      });
      sp_must_eq(t, 0, tar.status.exit_code);
      break;
    }
    case PROVISION_TARBALL_GARBAGE: {
      provision_create(mem, dir, "A.tar.gz", "A");
      stub.tarball = sp_fs_join_path(mem, dir, sp_str_lit("A.tar.gz"));
      break;
    }
    case PROVISION_TARBALL_LOOSE: {
      provision_create(mem, dir, "B.txt", "B");
      stub.tarball = sp_fs_join_path(mem, dir, sp_str_lit("A.tar.gz"));
      sp_ps_output_t tar = sp_ps_run(mem, (sp_ps_config_t) {
        .command = sp_str_lit("tar"),
        .args = {
          sp_str_lit("czf"), stub.tarball,
          sp_str_lit("-C"), dir,
          sp_str_lit("B.txt"),
        }
      });
      sp_must_eq(t, 0, tar.status.exit_code);
      break;
    }
  }

  sp_str_t sha = sp_zero;
  sp_must_eq(t, (u32)SPN_OK, (u32)spn_digest_file_hex(SPN_DIGEST_SHA256, mem, stub.tarball, &sha));
  sp_must_eq(t, 64u, sha.len);

  spn_toolchain_store_t store = {
    .mem = mem,
    .dir = sp_fs_join_path(mem, dir, sp_str_view(it->store_dir ? it->store_dir : "store")),
    .fetch = fetch_stub,
    .fetch_user_data = &stub,
  };
  if (it->mirror) {
    store.mirror = sp_str_view(it->mirror);
  }
  if (!it->store_dir) {
    sp_fs_create_dir(store.dir);
  }

  sp_str_t artifact_sha = it->no_sha ? sp_str_lit("") : (it->sha ? sp_str_view(it->sha) : sha);
  sp_str_t url = sp_fmt(mem, "https://tc.example.com/{}", sp_fmt_str(sp_fs_get_name(stub.tarball))).value;

  if (it->dest_file) {
    sp_fs_create_file_str(sp_fs_join_path(mem, store.dir, artifact_sha), sp_str_lit("A"));
  }

  sp_str_t roots [PROVISION_MAX_TOOLCHAINS] = sp_zero;
  u32 provisions = 0;
  spn_err_union_t payload = sp_zero;

  sp_carr_for(it->toolchains, at) {
    const c8* name = it->toolchains[at];
    if (!name && !at) {
      name = "A";
    }
    if (!name) {
      break;
    }

    spn_toolchain_info_t toolchain = fixture_local_toolchain(name, name);
    spn_opt_artifact_t artifact = sp_zero;
    if (!it->local) {
      toolchain.source = SPN_TOOLCHAIN_SOURCE_DISTRIBUTION;
      sp_opt_set(artifact, ((spn_artifact_t) {
        .url = url,
        .sha256 = artifact_sha,
      }));
    }

    roots[at] = sp_str_lit("sentinel");
    spn_err_t err = spn_toolchain_provision(&store, &toolchain, artifact, &roots[at]);
    sp_must_eq(t, (u32)it->expect.kind, (u32)err);
    if (err) {
      sp_da(spn_event_t) errs = spn_test_drain_errs(mem);
      sp_must_eq(t, 1, sp_da_size(errs));
      payload = errs[0].err;
      sp_expect_eq(t, payload.kind, err);
      sp_expect_str_eq_c(t, payload.artifact.name, name);
    }
    provisions++;
  }

  sp_expect_eq(t, it->expect.calls, stub.calls);

  if (it->expect.last_url) {
    sp_expect_str_eq_c(t, stub.last_url, it->expect.last_url);
  }
  if (it->expect.root_empty) {
    sp_expect(t, sp_str_empty(roots[0]));
  }
  if (it->expect.root_in_store) {
    sp_expect_str_eq(t, roots[0], sp_fs_join_path(mem, store.dir, sha));
  }
  if (it->expect.extracted) {
    sp_expect(t, sp_fs_is_dir(roots[0]));
    sp_expect(t, sp_fs_is_file(sp_fs_join_path(mem, roots[0], sp_str_lit("B"))));
    sp_expect(t, sp_fs_is_file(sp_fs_join_path(mem, roots[0], sp_str_lit("lib/C"))));
  }
  if (it->expect.store_clean) {
    sp_str_t lock = sp_fmt(mem, "{}.lock", sp_fmt_str(artifact_sha)).value;
    sp_da(sp_fs_entry_t) entries = sp_zero;
    sp_fs_collect(mem, store.dir, &entries);
    sp_must_eq(t, 1u, (u32)sp_da_size(entries));
    sp_expect_str_eq(t, entries[0].name, lock);
  }
  if (it->expect.err_reports_sha) {
    sp_expect_str_eq(t, payload.artifact.expected, artifact_sha);
    sp_expect_str_eq(t, payload.artifact.actual, sha);
  }
  if (provisions > 1) {
    sp_expect_str_eq(t, roots[0], roots[1]);
  }

  return SP_OK;
}

sp_test_each(provision, resolve_url, resolve_test_t, resolve_tests) {
  sp_mem_t mem = sp_test_arena(t);

  spn_artifact_t artifact = {
    .url = sp_str_view(it->url),
    .sha256 = sp_str_lit("aa"),
  };
  sp_expect_str_eq_c(t, spn_artifact_resolve_url(mem, artifact, sp_str_view(it->mirror)), it->expect);

  return SP_OK;
}

sp_test(provision, store_path_is_content_addressed) {
  sp_mem_t mem = sp_test_arena(t);
  spn_toolchain_store_t store = { .mem = mem, .dir = sp_str_lit("/store") };
  spn_artifact_t artifact = { .url = sp_str_lit("https://x/y.tar.gz"), .sha256 = sp_str_lit("cafe") };
  sp_expect_str_eq(t, spn_toolchain_store_path(&store, artifact), sp_fs_join_path(mem, store.dir, sp_str_lit("cafe")));
  return SP_OK;
}
