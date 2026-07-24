#include "fixture.h"

#define PROVISION_MAX_TOOLCHAINS 2
#define RESOLVE_MAX_CASES 4

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
  const c8* url;
  const c8* mirror;
  const c8* expect;
} resolve_case_t;

typedef struct {
  resolve_case_t cases [RESOLVE_MAX_CASES];
} resolve_test_t;

typedef struct {
  u32 calls;
  sp_str_t tarball;
  sp_str_t last_url;
  sp_str_t fail_url_containing;
  bool fail;
} fetch_stub_t;

static spn_err_t fetch_stub(sp_str_t url, sp_str_t dest, void* user_data) {
  fetch_stub_t* stub = (fetch_stub_t*)user_data;
  stub->calls++;
  stub->last_url = sp_str_copy(sp_mem_arena_as_allocator(ctx_get()->arena), url);
  if (stub->fail) return SPN_ERROR;
  if (!sp_str_empty(stub->fail_url_containing) && sp_str_contains(url, stub->fail_url_containing)) return SPN_ERROR;
  if (sp_fs_copy(stub->tarball, dest)) return SPN_ERROR;
  return SPN_OK;
}

static void run_provision_test(s32* utest_result, provision_test_t t) {
  tmpfs_t fs = sp_zero;
  tmpfs_init_named(&fs, t.name);
  sp_mem_t mem = fs.mem;

  fetch_stub_t stub = sp_zero;
  stub.fail = t.fetch_fail;
  if (t.fail_url_containing) {
    stub.fail_url_containing = sp_str_view(t.fail_url_containing);
  }

  switch (t.tarball) {
    case PROVISION_TARBALL_TREE: {
      tmpfs_create(&fs, sp_str_lit("tree/A/B"), sp_str_lit("B"));
      tmpfs_create(&fs, sp_str_lit("tree/A/lib/C"), sp_str_lit("C"));
      stub.tarball = tmpfs_get(&fs, sp_str_lit("A.tar.gz"));
      sp_ps_output_t tar = sp_ps_run(mem, (sp_ps_config_t) {
        .command = sp_str_lit("tar"),
        .args = {
          sp_str_lit("czf"), stub.tarball,
          sp_str_lit("-C"), tmpfs_get(&fs, sp_str_lit("tree")),
          sp_str_lit("A"),
        }
      });
      ASSERT_EQ(0, tar.status.exit_code);
      break;
    }
    case PROVISION_TARBALL_GARBAGE: {
      tmpfs_create(&fs, sp_str_lit("A.tar.gz"), sp_str_lit("A"));
      stub.tarball = tmpfs_get(&fs, sp_str_lit("A.tar.gz"));
      break;
    }
    case PROVISION_TARBALL_LOOSE: {
      tmpfs_create(&fs, sp_str_lit("B.txt"), sp_str_lit("B"));
      stub.tarball = tmpfs_get(&fs, sp_str_lit("A.tar.gz"));
      sp_ps_output_t tar = sp_ps_run(mem, (sp_ps_config_t) {
        .command = sp_str_lit("tar"),
        .args = {
          sp_str_lit("czf"), stub.tarball,
          sp_str_lit("-C"), fs.root,
          sp_str_lit("B.txt"),
        }
      });
      ASSERT_EQ(0, tar.status.exit_code);
      break;
    }
  }

  sp_str_t sha = sp_zero;
  ASSERT_EQ(SPN_OK, spn_sha256_file(mem, stub.tarball, &sha));
  ASSERT_EQ(64u, sha.len);

  spn_toolchain_store_t store = {
    .mem = mem,
    .dir = tmpfs_get(&fs, sp_str_view(t.store_dir ? t.store_dir : "store")),
    .fetch = fetch_stub,
    .fetch_user_data = &stub,
  };
  if (t.mirror) {
    store.mirror = sp_str_view(t.mirror);
  }
  if (!t.store_dir) {
    sp_fs_create_dir(store.dir);
  }

  sp_str_t artifact_sha = t.no_sha ? sp_str_lit("") : (t.sha ? sp_str_view(t.sha) : sha);
  sp_str_t url = sp_fmt(mem, "https://tc.example.com/{}", sp_fmt_str(sp_fs_get_name(stub.tarball))).value;

  if (t.dest_file) {
    sp_fs_create_file_str(sp_fs_join_path(mem, store.dir, artifact_sha), sp_str_lit("A"));
  }

  sp_str_t roots [PROVISION_MAX_TOOLCHAINS] = sp_zero;
  u32 provisions = 0;
  spn_err_union_t err = sp_zero;

  sp_carr_for(t.toolchains, it) {
    const c8* name = t.toolchains[it];
    if (!name && !it) {
      name = "A";
    }
    if (!name) {
      break;
    }

    spn_toolchain_info_t toolchain = fixture_local_toolchain(name, name);
    spn_opt_artifact_t artifact = sp_zero;
    if (!t.local) {
      toolchain.source = SPN_TOOLCHAIN_SOURCE_DISTRIBUTION;
      sp_opt_set(artifact, ((spn_artifact_t) {
        .url = url,
        .sha256 = artifact_sha,
      }));
    }

    roots[it] = sp_str_lit("sentinel");
    err = spn_toolchain_provision(&store, &toolchain, artifact, &roots[it]);
    ASSERT_EQ((u32)t.expect.kind, (u32)err.kind);
    if (err.kind) {
      EXPECT_STR(err.artifact.name, name);
    }
    provisions++;
  }

  EXPECT_EQ(t.expect.calls, stub.calls);

  if (t.expect.last_url) {
    EXPECT_STR(stub.last_url, t.expect.last_url);
  }
  if (t.expect.root_empty) {
    EXPECT_TRUE(sp_str_empty(roots[0]));
  }
  if (t.expect.root_in_store) {
    EXPECT_TRUE(sp_str_equal(roots[0], sp_fs_join_path(mem, store.dir, sha)));
  }
  if (t.expect.extracted) {
    EXPECT_TRUE(sp_fs_is_dir(roots[0]));
    EXPECT_TRUE(sp_fs_is_file(sp_fs_join_path(mem, roots[0], sp_str_lit("B"))));
    EXPECT_TRUE(sp_fs_is_file(sp_fs_join_path(mem, roots[0], sp_str_lit("lib/C"))));
  }
  if (t.expect.store_clean) {
    sp_str_t lock = sp_fmt(mem, "{}.lock", sp_fmt_str(artifact_sha)).value;
    sp_da(sp_fs_entry_t) entries = sp_fs_collect(mem, store.dir);
    ASSERT_EQ(1u, (u32)sp_da_size(entries));
    EXPECT_TRUE(sp_str_equal(entries[0].name, lock));
  }
  if (t.expect.err_reports_sha) {
    EXPECT_TRUE(sp_str_equal(err.artifact.expected, artifact_sha));
    EXPECT_TRUE(sp_str_equal(err.artifact.actual, sha));
  }
  if (provisions > 1) {
    EXPECT_TRUE(sp_str_equal(roots[0], roots[1]));
  }
}

static void run_resolve_test(s32* utest_result, resolve_test_t t) {
  sp_mem_t mem = sp_mem_arena_as_allocator(ctx_get()->arena);

  sp_carr_for(t.cases, it) {
    resolve_case_t c = t.cases[it];
    if (!c.url) {
      break;
    }
    spn_artifact_t artifact = {
      .url = sp_str_view(c.url),
      .sha256 = sp_str_lit("aa"),
    };
    EXPECT_STR(spn_artifact_resolve_url(mem, artifact, sp_str_view(c.mirror)), c.expect);
  }
}

UTEST(provision, local_toolchain_is_noop) {
  run_provision_test(utest_result, (provision_test_t) {
    .name = "provision_local",
    .local = true,
    .expect = { .root_empty = true },
  });
}

UTEST(provision, fresh_artifact_downloads_and_extracts) {
  run_provision_test(utest_result, (provision_test_t) {
    .name = "provision_fresh",
    .expect = {
      .calls = 1,
      .root_in_store = true,
      .extracted = true,
    },
  });
}

UTEST(provision, artifacts_share_store_by_sha) {
  run_provision_test(utest_result, (provision_test_t) {
    .name = "provision_shared",
    .toolchains = { "A", "B" },
    .expect = { .calls = 1 },
  });
}

UTEST(provision, sha_mismatch_fails_and_leaves_no_store_entry) {
  run_provision_test(utest_result, (provision_test_t) {
    .name = "provision_mismatch",
    .sha = "beefbeefbeefbeefbeefbeefbeefbeefbeefbeefbeefbeefbeefbeefbeefbeef",
    .expect = {
      .kind = SPN_ERR_TOOLCHAIN_SHA,
      .calls = 1,
      .store_clean = true,
      .err_reports_sha = true,
    },
  });
}

UTEST(provision, corrupt_archive_fails_and_leaves_no_store_entry) {
  run_provision_test(utest_result, (provision_test_t) {
    .name = "provision_corrupt",
    .tarball = PROVISION_TARBALL_GARBAGE,
    .expect = {
      .kind = SPN_ERR_TOOLCHAIN_EXTRACT,
      .calls = 1,
      .store_clean = true,
    },
  });
}

UTEST(provision, fetch_failure_propagates) {
  run_provision_test(utest_result, (provision_test_t) {
    .name = "provision_fetch_fail",
    .fetch_fail = true,
    .expect = {
      .kind = SPN_ERR_TOOLCHAIN_FETCH,
      .calls = 1,
      .store_clean = true,
    },
  });
}

UTEST(provision, mirror_rewrites_url) {
  run_resolve_test(utest_result, (resolve_test_t) {
    .cases = {
      {
        .url = "https://tc.example.com/x/A.tar.gz",
        .mirror = "https://mirror.example.com/M",
        .expect = "https://mirror.example.com/M/A.tar.gz",
      },
      {
        .url = "https://tc.example.com/x/A.tar.gz",
        .mirror = "https://mirror.example.com/M/",
        .expect = "https://mirror.example.com/M/A.tar.gz",
      },
      {
        .url = "https://tc.example.com/x/A.tar.gz",
        .mirror = "",
        .expect = "https://tc.example.com/x/A.tar.gz",
      },
      {
        .url = "https://tc.example.com/",
        .mirror = "https://mirror.example.com/M",
        .expect = "https://tc.example.com/",
      },
    },
  });
}

UTEST(provision, mirror_used_for_fetch) {
  run_provision_test(utest_result, (provision_test_t) {
    .name = "provision_mirror",
    .mirror = "https://mirror.example.com/M",
    .expect = {
      .calls = 1,
      .last_url = "https://mirror.example.com/M/A.tar.gz",
    },
  });
}

UTEST(provision, broken_mirror_falls_back_to_canonical) {
  run_provision_test(utest_result, (provision_test_t) {
    .name = "provision_mirror_fallback",
    .mirror = "https://mirror.example.com/M",
    .fail_url_containing = "mirror.example.com",
    .expect = {
      .calls = 2,
      .last_url = "https://tc.example.com/A.tar.gz",
    },
  });
}

UTEST(provision, mirror_matching_canonical_is_not_retried) {
  run_provision_test(utest_result, (provision_test_t) {
    .name = "provision_mirror_identity",
    .mirror = "https://tc.example.com",
    .fetch_fail = true,
    .expect = {
      .kind = SPN_ERR_TOOLCHAIN_FETCH,
      .calls = 1,
      .last_url = "https://tc.example.com/A.tar.gz",
    },
  });
}

UTEST(provision, single_file_archive_fails_extract) {
  run_provision_test(utest_result, (provision_test_t) {
    .name = "provision_single_file",
    .tarball = PROVISION_TARBALL_LOOSE,
    .expect = {
      .kind = SPN_ERR_TOOLCHAIN_EXTRACT,
      .calls = 1,
      .store_clean = true,
    },
  });
}

UTEST(provision, dest_occupied_by_file_fails) {
  run_provision_test(utest_result, (provision_test_t) {
    .name = "provision_dest_file",
    .dest_file = true,
    .expect = {
      .kind = SPN_ERR_TOOLCHAIN_EXTRACT,
      .calls = 1,
    },
  });
}

UTEST(provision, empty_sha_is_rejected) {
  run_provision_test(utest_result, (provision_test_t) {
    .name = "provision_empty_sha",
    .no_sha = true,
    .expect = { .kind = SPN_ERR_TOOLCHAIN_NO_SHA },
  });
}

UTEST(provision, missing_store_dir_is_created) {
  run_provision_test(utest_result, (provision_test_t) {
    .name = "provision_no_store_dir",
    .store_dir = "store/nested/deeper",
    .expect = {
      .calls = 1,
      .extracted = true,
    },
  });
}

UTEST(provision, store_path_is_content_addressed) {
  sp_mem_t mem = sp_mem_arena_as_allocator(ctx_get()->arena);
  spn_toolchain_store_t store = { .mem = mem, .dir = sp_str_lit("/store") };
  spn_artifact_t artifact = { .url = sp_str_lit("https://x/y.tar.gz"), .sha256 = sp_str_lit("cafe") };
  EXPECT_TRUE(sp_str_equal(spn_toolchain_store_path(&store, artifact), sp_fs_join_path(mem, store.dir, sp_str_lit("cafe"))));
}
