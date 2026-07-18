#define SP_IMPLEMENTATION
#include "sp.h"
#include "test.h"
#include "utest.h"
#include "action.h"
#include "harness.h"

UTEST_MAIN()

#define SPN_TEST_SPANDEX_URL "https://github.com/tspader/spandex.git"
#define SPN_TEST_SPANDEX_PIN "55e546c8859d56f2cae08256bed6e0cce23d1bc8"

struct e2e {
  fixture_t fixture;
};

UTEST_INITIALIZER(e2e_init_tmpfs_top_level) {
  sp_mem_t mem = sp_mem_os_new();
  sp_str_t tmp = sp_fs_normalize_path(mem, sp_os_env_get(sp_str_lit("SPN_TEST_TMP")));
  if (sp_str_empty(tmp)) {
    tmp = sp_str_lit(".tmp");
  }

  sp_tm_epoch_t now = sp_tm_now_epoch();
  sp_str_t iso = sp_tm_epoch_to_iso8601(mem, now);
  c8* sanitized = (c8*)sp_alloc(mem, iso.len);
  sp_for(it, iso.len) {
    sanitized[it] = iso.data[it] == ':' ? '-' : iso.data[it];
  }

  tmpfs_set_top_level(sp_fs_join_path(mem, tmp, sp_str(sanitized, iso.len)));
}

UTEST_F_SETUP(e2e) {
  sp_mem_t mem = sp_mem_os_new();
  uf->fixture.paths.root = test_repo_root(mem);
#if defined(SPN_TEST_BIN)
  uf->fixture.paths.spn = test_repo_path(mem, sp_str_lit(SPN_TEST_BIN));
#else
  uf->fixture.paths.spn = test_repo_path(mem, exe("spn"));
#endif
  ASSERT_TRUE(sp_fs_exists(uf->fixture.paths.spn));
}

UTEST_F_TEARDOWN(e2e) {
}

static bool e2e_enabled(void) {
  sp_str_t flag = sp_os_env_get(sp_str_lit("SPN_TEST_LIVE"));
  return sp_str_equal(flag, sp_str_lit("1"));
}

static void e2e_prepare(s32* utest_result, fixture_t* fixture, const c8* project, const c8* const* copy) {
  sp_mem_t mem = fixture->fs.mem;

  fixture->paths.config = tmpfs_get(&fixture->fs, sp_str_lit(".home/config"));
  fixture->paths.storage = sp_fs_join_path(mem, fixture->paths.root, sp_str_lit(".cache/e2e/storage"));
  fixture->paths.include = sp_fs_join_path(mem, fixture->paths.storage, sp_str_lit("spn/include"));
  sp_fs_create_dir(fixture->paths.config);
  sp_fs_create_dir(fixture->paths.storage);
  sp_fs_create_dir(fixture->paths.include);

  setup_e2e_config(
    &fixture->fs,
    fixture->paths.config,
    fixture->paths.root,
    sp_str_lit(SPN_TEST_SPANDEX_URL),
    sp_str_lit(SPN_TEST_SPANDEX_PIN)
  );

  sp_fs_copy(sp_fs_join_path(mem, fixture->paths.root, sp_str_lit("include/spn.h")), fixture->paths.include);

  sp_str_t path = sp_fs_join_path(mem, fixture->paths.root, sp_str_view(project));
  fixture_copy_project(utest_result, fixture, path, copy);
}

UTEST_F(e2e, sp) {
  if (!e2e_enabled()) {
    sp_log("{.yellow} (set SPN_TEST_LIVE=1 to run)", SP_FMT_CSTR("skipped e2e.sp"));
    return;
  }

  tmpfs_init_named(&uf->fixture.fs, "e2e_sp");
  e2e_prepare(utest_result, &uf->fixture, "test/e2e/fixtures/sp", SP_NULLPTR);

  run_actions(utest_result, &uf->fixture, (action_t[]) {
    { .kind = ACTION_RUN_CLI, .cli = { "build" } },
    { .kind = ACTION_VERIFY_PKG_LOCKED, .verify_locked = { .name = "spader/sp" } },
    { .kind = ACTION_RUN_BIN, .bin = { .name = "main", .rc = 0 } },
  });
}

UTEST_F(e2e, fmt) {
  if (!e2e_enabled()) {
    sp_log("{.yellow} (set SPN_TEST_LIVE=1 to run)", SP_FMT_CSTR("skipped e2e.fmt"));
    return;
  }

  tmpfs_init_named(&uf->fixture.fs, "e2e_fmt");
  e2e_prepare(utest_result, &uf->fixture, "test/e2e/fixtures/fmt", (const c8*[]) { "main.cpp", "vendor/fmt/spn.toml", SP_NULLPTR });

  run_actions(utest_result, &uf->fixture, (action_t[]) {
    { .kind = ACTION_RUN_CLI, .cli = { "build" } },
    { .kind = ACTION_RUN_BIN, .bin = { .name = "main", .rc = 0 } },
  });
}

UTEST_F(e2e, tracy) {
  if (!e2e_enabled()) {
    sp_log("{.yellow} (set SPN_TEST_LIVE=1 to run)", SP_FMT_CSTR("skipped e2e.tracy"));
    return;
  }

  tmpfs_init_named(&uf->fixture.fs, "e2e_tracy");
  e2e_prepare(utest_result, &uf->fixture, "test/e2e/fixtures/tracy", (const c8*[]) { "main.cpp", "vendor/tracy/spn.toml", SP_NULLPTR });

  run_actions(utest_result, &uf->fixture, (action_t[]) {
    { .kind = ACTION_RUN_CLI, .cli = { "build" } },
    { .kind = ACTION_RUN_BIN, .bin = { .name = "main", .rc = 0 } },
  });
}

UTEST_F(e2e, lua) {
  if (!e2e_enabled()) {
    sp_log("{.yellow} (set SPN_TEST_LIVE=1 to run)", SP_FMT_CSTR("skipped e2e.lua"));
    return;
  }

  tmpfs_init_named(&uf->fixture.fs, "e2e_lua");
  e2e_prepare(utest_result, &uf->fixture, "test/e2e/fixtures/lua", (const c8*[]) { "vendor/lua/spn.toml", SP_NULLPTR });

  run_actions(utest_result, &uf->fixture, (action_t[]) {
    { .kind = ACTION_RUN_CLI, .cli = { "build" } },
    { .kind = ACTION_RUN_BIN, .bin = { .name = "main", .rc = 0 } },
  });
}

UTEST_F(e2e, sdl2) {
  if (!e2e_enabled()) {
    sp_log("{.yellow} (set SPN_TEST_LIVE=1 to run)", SP_FMT_CSTR("skipped e2e.sdl2"));
    return;
  }

  tmpfs_init_named(&uf->fixture.fs, "e2e_sdl2");
  e2e_prepare(utest_result, &uf->fixture, "test/e2e/fixtures/sdl2", (const c8*[]) { "vendor/sdl2/*", SP_NULLPTR });

  run_actions(utest_result, &uf->fixture, (action_t[]) {
    { .kind = ACTION_RUN_CLI, .cli = { "build" } },
    { .kind = ACTION_RUN_BIN, .bin = { .name = "main", .rc = 0 } },
  });
}

UTEST_F(e2e, sdl2_mixer) {
  if (!e2e_enabled()) {
    sp_log("{.yellow} (set SPN_TEST_LIVE=1 to run)", SP_FMT_CSTR("skipped e2e.sdl2_mixer"));
    return;
  }

  tmpfs_init_named(&uf->fixture.fs, "e2e_sdl2_mixer");
  e2e_prepare(utest_result, &uf->fixture, "test/e2e/fixtures/sdl2_mixer", (const c8*[]) { "vendor/sdl2/*", "vendor/sdl2_mixer/*", SP_NULLPTR });

  run_actions(utest_result, &uf->fixture, (action_t[]) {
    { .kind = ACTION_RUN_CLI, .cli = { "build" } },
    { .kind = ACTION_RUN_BIN, .bin = { .name = "main", .rc = 0 } },
  });
}

UTEST_F(e2e, cpptrace) {
  if (!e2e_enabled()) {
    sp_log("{.yellow} (set SPN_TEST_LIVE=1 to run)", SP_FMT_CSTR("skipped e2e.cpptrace"));
    return;
  }

  tmpfs_init_named(&uf->fixture.fs, "e2e_cpptrace");
  e2e_prepare(utest_result, &uf->fixture, "test/e2e/fixtures/cpptrace", (const c8*[]) { "main.cpp", "vendor/cpptrace/*", SP_NULLPTR });

  run_actions(utest_result, &uf->fixture, (action_t[]) {
    { .kind = ACTION_RUN_CLI, .cli = { "build" } },
    { .kind = ACTION_RUN_BIN, .bin = { .name = "main", .rc = 0 } },
  });
}

UTEST_F(e2e, freetype) {
  if (!e2e_enabled()) {
    sp_log("{.yellow} (set SPN_TEST_LIVE=1 to run)", SP_FMT_CSTR("skipped e2e.freetype"));
    return;
  }

  tmpfs_init_named(&uf->fixture.fs, "e2e_freetype");
  e2e_prepare(utest_result, &uf->fixture, "test/e2e/fixtures/freetype", (const c8*[]) { "vendor/freetype/*", SP_NULLPTR });

  run_actions(utest_result, &uf->fixture, (action_t[]) {
    { .kind = ACTION_RUN_CLI, .cli = { "build" } },
    { .kind = ACTION_RUN_BIN, .bin = { .name = "main", .rc = 0 } },
  });
}

// The game's own sources come from a local checkout; the fixture supplies
// spn.toml and the vendor package manifests. SDL2, SDL2_mixer, freetype,
// fmt, cpptrace, lua, and tracy come from the index-shaped manifests while
// flecs, imgui, glad, sol, and the header-only bits stay vendored in-tree.
// A few game headers reach tracy and fmt through vendor-relative paths, so
// they get rewritten to the store layout after the copy.
static void bloodgun_copy_checkout(s32* result, fixture_t* fixture, sp_str_t checkout) {
  UTEST_RESULT(result);
  sp_mem_t mem = fixture->fs.mem;

  const c8* roots [] = { "host", "game", "shared", "pch" };
  sp_carr_for(roots, it) {
    sp_str_t from = sp_fs_join_path(mem, checkout, sp_str_view(roots[it]));
    ASSERT_TRUE(sp_fs_is_dir(from));
    sp_fs_copy(from, fixture->fs.root);
  }

  sp_str_t vendor = tmpfs_get(&fixture->fs, sp_str_lit("vendor"));
  const c8* vendored [] = {
    "vendor/flecs",
    "vendor/imgui",
    "vendor/glad",
    "vendor/sol",
    "vendor/glm",
    "vendor/cereal",
    "vendor/stb_image.h",
    "vendor/stb_truetype.h",
    "vendor/json.hpp",
    "vendor/cr.h",
    "vendor/b_stacktrace.h",
  };
  sp_carr_for(vendored, it) {
    sp_str_t from = sp_fs_join_path(mem, checkout, sp_str_view(vendored[it]));
    ASSERT_TRUE(sp_fs_exists(from));
    sp_fs_copy(from, vendor);
  }
}

UTEST_F(e2e, bloodgun) {
  sp_str_t checkout = sp_os_env_get(sp_str_lit("SPN_TEST_BLOODGUN"));
  if (!e2e_enabled() || sp_str_empty(checkout)) {
    sp_log("{.yellow} (set SPN_TEST_LIVE=1 and SPN_TEST_BLOODGUN=<checkout> to run)", SP_FMT_CSTR("skipped e2e.bloodgun"));
    return;
  }

  tmpfs_init_named(&uf->fixture.fs, "e2e_bloodgun");
  e2e_prepare(utest_result, &uf->fixture, "test/e2e/fixtures/bloodgun", (const c8*[]) {
    "vendor/sdl2/*",
    "vendor/sdl2_mixer/*",
    "vendor/freetype/*",
    "vendor/fmt/*",
    "vendor/cpptrace/*",
    "vendor/lua/*",
    "vendor/tracy/*",
    SP_NULLPTR,
  });
  bloodgun_copy_checkout(utest_result, &uf->fixture, checkout);

  run_actions(utest_result, &uf->fixture, (action_t[]) {
    {
      .kind = ACTION_SUBPROCESS,
      .process.config = {
        .command = sp_str_lit("sed"),
        .cwd = uf->fixture.fs.root,
        .args = {
          sp_str_lit("-i"),
          sp_str_lit("s|tracy/public/tracy/|tracy/|g"),
          sp_str_lit("host/include/host.hpp"),
          sp_str_lit("shared/include/defs.hpp"),
          sp_str_lit("pch/game.hpp"),
        },
      },
    },
    {
      .kind = ACTION_SUBPROCESS,
      .process.config = {
        .command = sp_str_lit("sed"),
        .cwd = uf->fixture.fs.root,
        .args = {
          sp_str_lit("-i"),
          sp_str_lit("s|fmt/include/fmt/|fmt/|g"),
          sp_str_lit("pch/stdafx.hpp"),
        },
      },
    },
    {
      .kind = ACTION_SUBPROCESS,
      .process.config = {
        .command = sp_str_lit("sed"),
        .cwd = uf->fixture.fs.root,
        .args = {
          sp_str_lit("-i"),
          sp_str_lit("6755c static_assert(sizeof...(Args) != sizeof...(Args), \"sol::optional<T&>::emplace is unsupported\");"),
          sp_str_lit("vendor/sol/sol.hpp"),
        },
      },
    },
    { .kind = ACTION_RUN_CLI, .cli = { "build" } },
    { .kind = ACTION_VERIFY_EXISTS, .exists = store_file("bin/bitgun-survivors") },
    { .kind = ACTION_VERIFY_NO_EVENT, .verify_event = { .event = "resolve_package", .key = "name", .value = "wolfpld/tracy" } },
    { .kind = ACTION_RUN_CLI, .cli = { "build", .args = { "-p", "profiling" } } },
    { .kind = ACTION_VERIFY_EXISTS, .exists = profile_store_file("profiling", "bin/bitgun-survivors") },
    { .kind = ACTION_VERIFY_EVENT, .verify_event = { .event = "resolve_package", .key = "name", .value = "wolfpld/tracy" } },
  });
}
