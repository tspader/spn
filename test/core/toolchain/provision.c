#include "sp.h"
#include "utest.h"

#include "fixture.h"
#include "toolchain/sha256.h"

typedef struct {
  u32 calls;
  sp_str_t tarball;
  sp_str_t last_url;
  bool fail;
} fetch_stub_t;

static spn_err_t fetch_stub(void* user_data, sp_str_t url, sp_str_t dest) {
  fetch_stub_t* stub = (fetch_stub_t*)user_data;
  stub->calls++;
  stub->last_url = sp_str_copy(sp_mem_arena_as_allocator(ctx_get()->arena), url);
  if (stub->fail) return SPN_ERROR;
  if (sp_fs_copy(stub->tarball, dest)) return SPN_ERROR;
  return SPN_OK;
}

typedef struct {
  tmpfs_t fs;
  sp_mem_t mem;
  fetch_stub_t stub;
  spn_toolchain_store_t store;
  sp_str_t sha;
} provision_fixture_t;

static void provision_fixture_init(s32* utest_result, provision_fixture_t* fixture, const c8* name) {
  tmpfs_init_named(&fixture->fs, name);
  fixture->mem = fixture->fs.mem;

  tmpfs_create(&fixture->fs, sp_str_lit("tree/zig-fixture/zig"), sp_str_lit("#!/bin/sh\necho zig\n"));
  tmpfs_create(&fixture->fs, sp_str_lit("tree/zig-fixture/lib/std.zig"), sp_str_lit("std"));

  sp_str_t tree = tmpfs_get(&fixture->fs, sp_str_lit("tree"));
  fixture->stub.tarball = tmpfs_get(&fixture->fs, sp_str_lit("zig-fixture.tar.gz"));

  sp_ps_output_t tar = sp_ps_run(fixture->mem, (sp_ps_config_t) {
    .command = sp_str_lit("tar"),
    .args = {
      sp_str_lit("czf"), fixture->stub.tarball,
      sp_str_lit("-C"), tree,
      sp_str_lit("zig-fixture"),
    }
  });
  ASSERT_EQ(0, tar.status.exit_code);

  ASSERT_EQ(SPN_OK, spn_sha256_file(fixture->mem, fixture->stub.tarball, &fixture->sha));
  ASSERT_EQ(64u, fixture->sha.len);

  fixture->store = (spn_toolchain_store_t) {
    .mem = fixture->mem,
    .dir = tmpfs_get(&fixture->fs, sp_str_lit("store")),
    .fetch = fetch_stub,
    .fetch_user_data = &fixture->stub,
  };
  sp_fs_create_dir(fixture->store.dir);
}

static spn_toolchain_t provision_fixture_toolchain(provision_fixture_t* fixture) {
  spn_toolchain_t toolchain = fixture_local_toolchain("zig", "zig");
  sp_opt_set(toolchain.artifact, ((spn_artifact_t) {
    .url = sp_str_lit("https://tc.example.com/zig-fixture.tar.gz"),
    .sha256 = fixture->sha,
  }));
  return toolchain;
}

UTEST(sha256, known_vectors) {
  sp_mem_t mem = sp_mem_arena_as_allocator(ctx_get()->arena);

  sp_str_t abc = spn_sha256_hex(mem, "abc", 3);
  EXPECT_STR(abc, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");

  sp_str_t empty = spn_sha256_hex(mem, "", 0);
  EXPECT_STR(empty, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");

  sp_str_t longer = spn_sha256_hex(mem, "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 56);
  EXPECT_STR(longer, "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
}

UTEST(sha256, file_matches_bytes) {
  tmpfs_t fs;
  tmpfs_init_named(&fs, "sha256_file");
  sp_mem_t mem = fs.mem;

  tmpfs_create(&fs, sp_str_lit("data.bin"), sp_str_lit("abc"));

  sp_str_t hex = sp_zero;
  ASSERT_EQ(SPN_OK, spn_sha256_file(mem, tmpfs_get(&fs, sp_str_lit("data.bin")), &hex));
  EXPECT_STR(hex, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");

  sp_str_t missing = sp_zero;
  EXPECT_EQ(SPN_ERROR, spn_sha256_file(mem, tmpfs_get(&fs, sp_str_lit("nope.bin")), &missing));
}

UTEST(provision, local_toolchain_is_noop) {
  provision_fixture_t fixture = sp_zero;
  provision_fixture_init(utest_result, &fixture, "provision_local");

  spn_toolchain_t local = fixture_local_toolchain("system", "cc");
  sp_str_t root = sp_str_lit("sentinel");
  ASSERT_EQ(SPN_OK, spn_toolchain_provision(&fixture.store, &local, &root));
  EXPECT_TRUE(sp_str_empty(root));
  EXPECT_EQ(0u, fixture.stub.calls);
}

UTEST(provision, fresh_artifact_downloads_and_extracts) {
  provision_fixture_t fixture = sp_zero;
  provision_fixture_init(utest_result, &fixture, "provision_fresh");
  spn_toolchain_t toolchain = provision_fixture_toolchain(&fixture);

  sp_str_t root = sp_zero;
  ASSERT_EQ(SPN_OK, spn_toolchain_provision(&fixture.store, &toolchain, &root));

  sp_str_t expected = sp_fs_join_path(fixture.mem, fixture.store.dir, fixture.sha);
  EXPECT_TRUE(sp_str_equal(root, expected));
  EXPECT_TRUE(sp_fs_is_dir(root));
  EXPECT_TRUE(sp_fs_is_file(sp_fs_join_path(fixture.mem, root, sp_str_lit("zig"))));
  EXPECT_TRUE(sp_fs_is_file(sp_fs_join_path(fixture.mem, root, sp_str_lit("lib/std.zig"))));
  EXPECT_EQ(1u, fixture.stub.calls);
}

UTEST(provision, cached_artifact_skips_fetch) {
  provision_fixture_t fixture = sp_zero;
  provision_fixture_init(utest_result, &fixture, "provision_cached");
  spn_toolchain_t toolchain = provision_fixture_toolchain(&fixture);

  sp_str_t first = sp_zero;
  ASSERT_EQ(SPN_OK, spn_toolchain_provision(&fixture.store, &toolchain, &first));
  ASSERT_EQ(1u, fixture.stub.calls);

  sp_str_t second = sp_zero;
  ASSERT_EQ(SPN_OK, spn_toolchain_provision(&fixture.store, &toolchain, &second));
  EXPECT_EQ(1u, fixture.stub.calls);
  EXPECT_TRUE(sp_str_equal(first, second));
}

UTEST(provision, artifacts_share_store_by_sha) {
  provision_fixture_t fixture = sp_zero;
  provision_fixture_init(utest_result, &fixture, "provision_shared");

  spn_toolchain_t zig = provision_fixture_toolchain(&fixture);
  spn_toolchain_t fork = provision_fixture_toolchain(&fixture);
  fork.name = sp_str_lit("zag");

  sp_str_t zig_root = sp_zero;
  sp_str_t fork_root = sp_zero;
  ASSERT_EQ(SPN_OK, spn_toolchain_provision(&fixture.store, &zig, &zig_root));
  ASSERT_EQ(SPN_OK, spn_toolchain_provision(&fixture.store, &fork, &fork_root));

  EXPECT_TRUE(sp_str_equal(zig_root, fork_root));
  EXPECT_EQ(1u, fixture.stub.calls);
}

UTEST(provision, sha_mismatch_fails_and_leaves_no_store_entry) {
  provision_fixture_t fixture = sp_zero;
  provision_fixture_init(utest_result, &fixture, "provision_mismatch");

  spn_toolchain_t toolchain = provision_fixture_toolchain(&fixture);
  sp_str_t lie = sp_str_lit("beefbeefbeefbeefbeefbeefbeefbeefbeefbeefbeefbeefbeefbeefbeefbeef");
  sp_opt_set(toolchain.artifact, ((spn_artifact_t) {
    .url = sp_str_lit("https://tc.example.com/zig-fixture.tar.gz"),
    .sha256 = lie,
  }));

  sp_str_t root = sp_zero;
  EXPECT_EQ(SPN_ERROR, spn_toolchain_provision(&fixture.store, &toolchain, &root));
  EXPECT_FALSE(sp_fs_exists(sp_fs_join_path(fixture.mem, fixture.store.dir, lie)));
  EXPECT_EQ(1u, fixture.stub.calls);
}

UTEST(provision, corrupt_archive_fails_and_leaves_no_store_entry) {
  provision_fixture_t fixture = sp_zero;
  provision_fixture_init(utest_result, &fixture, "provision_corrupt");

  tmpfs_create(&fixture.fs, sp_str_lit("garbage.tar.gz"), sp_str_lit("this is not a tarball"));
  fixture.stub.tarball = tmpfs_get(&fixture.fs, sp_str_lit("garbage.tar.gz"));

  sp_str_t garbage_sha = sp_zero;
  ASSERT_EQ(SPN_OK, spn_sha256_file(fixture.mem, fixture.stub.tarball, &garbage_sha));

  spn_toolchain_t toolchain = fixture_local_toolchain("zig", "zig");
  sp_opt_set(toolchain.artifact, ((spn_artifact_t) {
    .url = sp_str_lit("https://tc.example.com/garbage.tar.gz"),
    .sha256 = garbage_sha,
  }));

  sp_str_t root = sp_zero;
  EXPECT_EQ(SPN_ERROR, spn_toolchain_provision(&fixture.store, &toolchain, &root));
  EXPECT_FALSE(sp_fs_exists(sp_fs_join_path(fixture.mem, fixture.store.dir, garbage_sha)));
}

UTEST(provision, fetch_failure_propagates) {
  provision_fixture_t fixture = sp_zero;
  provision_fixture_init(utest_result, &fixture, "provision_fetch_fail");
  fixture.stub.fail = true;

  spn_toolchain_t toolchain = provision_fixture_toolchain(&fixture);
  sp_str_t root = sp_zero;
  EXPECT_EQ(SPN_ERROR, spn_toolchain_provision(&fixture.store, &toolchain, &root));
  EXPECT_FALSE(sp_fs_exists(sp_fs_join_path(fixture.mem, fixture.store.dir, fixture.sha)));
}

UTEST(provision, mirror_override_rewrites_url) {
  sp_mem_t mem = sp_mem_arena_as_allocator(ctx_get()->arena);
  spn_artifact_t artifact = {
    .url = sp_str_lit("https://ziglang.org/download/0.15.2/zig-x86_64-linux-0.15.2.tar.xz"),
    .sha256 = sp_str_lit("aaaa"),
  };

  sp_str_t resolved = spn_artifact_resolve_url(mem, artifact, sp_str_lit("https://mirror.example.com/zig"));
  EXPECT_STR(resolved, "https://mirror.example.com/zig/zig-x86_64-linux-0.15.2.tar.xz");

  sp_str_t canonical = spn_artifact_resolve_url(mem, artifact, sp_str_lit(""));
  EXPECT_STR(canonical, "https://ziglang.org/download/0.15.2/zig-x86_64-linux-0.15.2.tar.xz");
}

UTEST(provision, mirror_used_for_fetch) {
  provision_fixture_t fixture = sp_zero;
  provision_fixture_init(utest_result, &fixture, "provision_mirror");
  fixture.store.mirror = sp_str_lit("https://mirror.example.com/zig");

  spn_toolchain_t toolchain = provision_fixture_toolchain(&fixture);
  sp_str_t root = sp_zero;
  ASSERT_EQ(SPN_OK, spn_toolchain_provision(&fixture.store, &toolchain, &root));
  EXPECT_STR(fixture.stub.last_url, "https://mirror.example.com/zig/zig-fixture.tar.gz");
}

UTEST(provision, store_path_is_content_addressed) {
  provision_fixture_t fixture = sp_zero;
  provision_fixture_init(utest_result, &fixture, "provision_store_path");

  spn_artifact_t artifact = { .url = sp_str_lit("https://x/y.tar.gz"), .sha256 = sp_str_lit("cafe") };
  sp_str_t path = spn_toolchain_store_path(&fixture.store, artifact);
  EXPECT_TRUE(sp_str_equal(path, sp_fs_join_path(fixture.mem, fixture.store.dir, sp_str_lit("cafe"))));
}
