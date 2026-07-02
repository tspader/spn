#include "sp.h"
#include "utest.h"

#include "fixture.h"
#include "toolchain/sha256.h"

typedef struct {
  const c8* data;
  u64 fill;
  bool file;
  bool missing;
  const c8* expect;
} sha256_case_t;

typedef struct {
  const c8* name;
  sha256_case_t cases [4];
} sha256_test_t;

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
  bool no_store_entry;
  bool err_reports_sha;
} provision_expect_t;

typedef struct {
  const c8* name;
  const c8* toolchains [2];
  bool local;
  provision_tarball_t tarball;
  const c8* sha;
  bool no_sha;
  const c8* mirror;
  bool fetch_fail;
  const c8* fail_url_containing;
  const c8* store_dir;
  bool stale_temp;
  provision_expect_t expect;
} provision_test_t;

typedef struct {
  const c8* url;
  const c8* mirror;
  const c8* expect;
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

static void run_sha256_test(s32* utest_result, sha256_test_t t) {
  tmpfs_t fs = sp_zero;
  tmpfs_init_named(&fs, t.name);
  sp_mem_t mem = fs.mem;

  sp_carr_for(t.cases, it) {
    sha256_case_t c = t.cases[it];
    if (!c.data && !c.fill && !c.missing) {
      break;
    }

    if (c.missing) {
      sp_str_t hex = sp_zero;
      EXPECT_EQ(SPN_ERROR, spn_sha256_file(mem, tmpfs_get(&fs, sp_str_lit("missing.bin")), &hex));
      continue;
    }

    sp_str_t data = sp_zero;
    if (c.fill) {
      c8* bytes = (c8*)sp_alloc(mem, c.fill);
      sp_mem_fill_u8(bytes, c.fill, (u8)0x61);
      data = sp_str(bytes, (u32)c.fill);
    } else {
      data = sp_str_view(c.data);
    }

    EXPECT_STR(spn_sha256_hex(mem, data.data, data.len), c.expect);

    if (c.file) {
      sp_str_t path = sp_fmt(mem, "{}.bin", sp_fmt_uint(it)).value;
      tmpfs_create(&fs, path, data);
      sp_str_t hex = sp_zero;
      ASSERT_EQ(SPN_OK, spn_sha256_file(mem, tmpfs_get(&fs, path), &hex));
      EXPECT_STR(hex, c.expect);
    }
  }
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
      tmpfs_create(&fs, sp_str_lit("tree/zig-fixture/zig"), sp_str_lit("#!/bin/sh\necho zig\n"));
      tmpfs_create(&fs, sp_str_lit("tree/zig-fixture/lib/std.zig"), sp_str_lit("std"));
      stub.tarball = tmpfs_get(&fs, sp_str_lit("zig-fixture.tar.gz"));
      sp_ps_output_t tar = sp_ps_run(mem, (sp_ps_config_t) {
        .command = sp_str_lit("tar"),
        .args = {
          sp_str_lit("czf"), stub.tarball,
          sp_str_lit("-C"), tmpfs_get(&fs, sp_str_lit("tree")),
          sp_str_lit("zig-fixture"),
        }
      });
      ASSERT_EQ(0, tar.status.exit_code);
      break;
    }
    case PROVISION_TARBALL_GARBAGE: {
      tmpfs_create(&fs, sp_str_lit("garbage.tar.gz"), sp_str_lit("this is not a tarball"));
      stub.tarball = tmpfs_get(&fs, sp_str_lit("garbage.tar.gz"));
      break;
    }
    case PROVISION_TARBALL_LOOSE: {
      tmpfs_create(&fs, sp_str_lit("loose.txt"), sp_str_lit("loose"));
      stub.tarball = tmpfs_get(&fs, sp_str_lit("loose.tar.gz"));
      sp_ps_output_t tar = sp_ps_run(mem, (sp_ps_config_t) {
        .command = sp_str_lit("tar"),
        .args = {
          sp_str_lit("czf"), stub.tarball,
          sp_str_lit("-C"), fs.root,
          sp_str_lit("loose.txt"),
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

  if (t.stale_temp) {
    sp_fs_create_dir(sp_fmt(mem, "{}/{}.123.tmp", sp_fmt_str(store.dir), sp_fmt_str(artifact_sha)).value);
    sp_fs_create_file(sp_fmt(mem, "{}/{}.123.download", sp_fmt_str(store.dir), sp_fmt_str(artifact_sha)).value);
  }

  sp_str_t roots [2] = sp_zero;
  u32 provisions = 0;
  spn_err_union_t err = sp_zero;

  sp_carr_for(t.toolchains, it) {
    const c8* name = t.toolchains[it];
    if (!name && !it) {
      name = "zig";
    }
    if (!name) {
      break;
    }

    spn_toolchain_t toolchain = fixture_local_toolchain(name, name);
    if (!t.local) {
      sp_opt_set(toolchain.artifact, ((spn_artifact_t) {
        .url = url,
        .sha256 = artifact_sha,
      }));
    }

    roots[it] = sp_str_lit("sentinel");
    err = spn_toolchain_provision(&store, &toolchain, &roots[it]);
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
    EXPECT_TRUE(sp_fs_is_file(sp_fs_join_path(mem, roots[0], sp_str_lit("zig"))));
    EXPECT_TRUE(sp_fs_is_file(sp_fs_join_path(mem, roots[0], sp_str_lit("lib/std.zig"))));
  }
  if (t.expect.no_store_entry) {
    EXPECT_FALSE(sp_fs_exists(sp_fs_join_path(mem, store.dir, artifact_sha)));
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
  spn_artifact_t artifact = {
    .url = sp_str_view(t.url),
    .sha256 = sp_str_lit("aaaa"),
  };
  EXPECT_STR(spn_artifact_resolve_url(mem, artifact, sp_str_view(t.mirror)), t.expect);
}

UTEST(sha256, known_vectors) {
  run_sha256_test(utest_result, (sha256_test_t) {
    .name = "sha256_vectors",
    .cases = {
      { .data = "abc", .expect = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad" },
      { .data = "", .expect = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855" },
      { .data = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", .expect = "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1" },
    },
  });
}

UTEST(sha256, padding_boundaries) {
  run_sha256_test(utest_result, (sha256_test_t) {
    .name = "sha256_padding",
    .cases = {
      { .fill = 55, .expect = "9f4390f8d30c2dd92ec9f095b65e2b9ae9b0a925a5258e241c9f1e910f734318" },
      { .fill = 56, .expect = "b35439a4ac6f0948b6d6f9e3c6af0f5f590ce20f1bde7090ef7970686ec6738a" },
      { .fill = 64, .expect = "ffe054fe7ae0cb6dc65c3af9b61d5209f439851db43d0ba5997337df154668eb" },
      { .fill = 65, .expect = "635361c48bb9eab14198e76ea8ab7f1a41685d6ad62aa9146d301d4f17eb0ae0" },
    },
  });
}

UTEST(sha256, file_larger_than_chunk) {
  run_sha256_test(utest_result, (sha256_test_t) {
    .name = "sha256_large",
    .cases = {
      { .fill = 1000000, .file = true, .expect = "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0" },
    },
  });
}

UTEST(sha256, file_matches_bytes) {
  run_sha256_test(utest_result, (sha256_test_t) {
    .name = "sha256_file",
    .cases = {
      { .data = "abc", .file = true, .expect = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad" },
      { .missing = true },
    },
  });
}

UTEST(provision, local_toolchain_is_noop) {
  run_provision_test(utest_result, (provision_test_t) {
    .name = "provision_local",
    .toolchains = { "system" },
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

UTEST(provision, cached_artifact_skips_fetch) {
  run_provision_test(utest_result, (provision_test_t) {
    .name = "provision_cached",
    .toolchains = { "zig", "zig" },
    .expect = { .calls = 1 },
  });
}

UTEST(provision, artifacts_share_store_by_sha) {
  run_provision_test(utest_result, (provision_test_t) {
    .name = "provision_shared",
    .toolchains = { "zig", "zag" },
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
      .no_store_entry = true,
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
      .no_store_entry = true,
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
      .no_store_entry = true,
    },
  });
}

UTEST(provision, mirror_override_rewrites_url) {
  run_resolve_test(utest_result, (resolve_test_t) {
    .url = "https://ziglang.org/download/0.15.2/zig-x86_64-linux-0.15.2.tar.xz",
    .mirror = "https://mirror.example.com/zig",
    .expect = "https://mirror.example.com/zig/zig-x86_64-linux-0.15.2.tar.xz",
  });
  run_resolve_test(utest_result, (resolve_test_t) {
    .url = "https://ziglang.org/download/0.15.2/zig-x86_64-linux-0.15.2.tar.xz",
    .mirror = "https://mirror.example.com/zig/",
    .expect = "https://mirror.example.com/zig/zig-x86_64-linux-0.15.2.tar.xz",
  });
  run_resolve_test(utest_result, (resolve_test_t) {
    .url = "https://ziglang.org/download/0.15.2/zig-x86_64-linux-0.15.2.tar.xz",
    .mirror = "",
    .expect = "https://ziglang.org/download/0.15.2/zig-x86_64-linux-0.15.2.tar.xz",
  });
  run_resolve_test(utest_result, (resolve_test_t) {
    .url = "https://tc.example.com/",
    .mirror = "https://mirror.example.com",
    .expect = "https://tc.example.com/",
  });
}

UTEST(provision, mirror_used_for_fetch) {
  run_provision_test(utest_result, (provision_test_t) {
    .name = "provision_mirror",
    .mirror = "https://mirror.example.com/zig",
    .expect = {
      .calls = 1,
      .last_url = "https://mirror.example.com/zig/zig-fixture.tar.gz",
    },
  });
}

UTEST(provision, broken_mirror_falls_back_to_canonical) {
  run_provision_test(utest_result, (provision_test_t) {
    .name = "provision_mirror_fallback",
    .mirror = "https://mirror.example.com/zig",
    .fail_url_containing = "mirror.example.com",
    .expect = {
      .calls = 2,
      .last_url = "https://tc.example.com/zig-fixture.tar.gz",
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
      .no_store_entry = true,
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

UTEST(provision, stale_temp_files_are_ignored) {
  run_provision_test(utest_result, (provision_test_t) {
    .name = "provision_stale_temp",
    .expect = {
      .calls = 1,
      .extracted = true,
    },
    .stale_temp = true,
  });
}

UTEST(provision, store_path_is_content_addressed) {
  sp_mem_t mem = sp_mem_arena_as_allocator(ctx_get()->arena);
  spn_toolchain_store_t store = { .mem = mem, .dir = sp_str_lit("/store") };
  spn_artifact_t artifact = { .url = sp_str_lit("https://x/y.tar.gz"), .sha256 = sp_str_lit("cafe") };
  EXPECT_TRUE(sp_str_equal(spn_toolchain_store_path(&store, artifact), sp_fs_join_path(mem, store.dir, sp_str_lit("cafe"))));
}
