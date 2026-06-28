#define SP_IMPLEMENTATION
#include "sp.h"

#define UTEST_IMPLEMENTATION
#include "utest.h"

#include "test.h"

#include "event/event.h"
#include "lock/lock.h"
#include "semver/convert.h"
#include "enum/enum.h"

UTEST_STATE();


///////////
// MOCKS //
///////////
void spn_event_buffer_push(spn_event_buffer_t* evs, spn_build_event_t e) {}

// MAIN
s32 main(s32 num_args, const c8** args) {
  ctx_init(ctx_get());
  s32 result = utest_main(num_args, args);
  ctx_deinit(ctx_get());
  return result;
}


/////////////////
// DESCRIPTOR //
////////////////
typedef struct {
  const c8* name;
  const c8* version;
  const c8* commit;
  const c8* kind;
  const c8* visibility;
  const c8* source_url;
  const c8* source_rev;
  const c8* source_dir;
  const c8* manifest_url;
  const c8* manifest_rev;
  const c8* manifest_dir;
  const c8* deps[4];
} lock_dep_t;

typedef struct {
  const c8* system_deps[4];
  lock_dep_t deps[8];
} fixture_t;


///////////
// STATE //
///////////
struct lock_roundtrip {
  u32 dummy;
};

UTEST_F_SETUP(lock_roundtrip) {
  ctx_t* harness = ctx_get();
  sp_context_push_allocator(sp_mem_arena_as_allocator(harness->arena));
}

UTEST_F_TEARDOWN(lock_roundtrip) {
  sp_context_pop();
}


///////////////
// EXECUTOR //
//////////////
static void build_lock(fixture_t* fixture, spn_lock_file_t* lock) {
  spn_lock_file_init(lock);

  sp_carr_for(fixture->system_deps, i) {
    if (!fixture->system_deps[i]) break;
    sp_ht_insert(lock->system_deps, sp_str_view(fixture->system_deps[i]), true);
  }

  sp_carr_for(fixture->deps, i) {
    lock_dep_t* d = &fixture->deps[i];
    if (!d->name) break;

    spn_lock_entry_t entry = {
      .name = sp_str_view(d->name),
      .version = spn_semver_from_str(sp_str_view(d->version)),
      .commit = sp_str_view(d->commit),
      .kind = spn_pkg_source_from_str(sp_str_view(d->kind)),
      .source = {
        .url = d->source_url ? sp_str_view(d->source_url) : SP_LIT(""),
        .rev = d->source_rev ? sp_str_view(d->source_rev) : SP_LIT(""),
        .dir = d->source_dir ? sp_str_view(d->source_dir) : SP_LIT(""),
      },
      .manifest = {
        .url = d->manifest_url ? sp_str_view(d->manifest_url) : SP_LIT(""),
        .rev = d->manifest_rev ? sp_str_view(d->manifest_rev) : SP_LIT(""),
        .dir = d->manifest_dir ? sp_str_view(d->manifest_dir) : SP_LIT(""),
      },
    };

    sp_carr_for(d->deps, j) {
      if (!d->deps[j]) break;
      sp_da_push(entry.deps, sp_str_view(d->deps[j]));
    }

    sp_ht_insert(lock->entries, entry.name, entry);
  }
}

static void run_fixture(s32* utest_result, fixture_t fixture) {
  spn_lock_file_t lock = SP_ZERO_INITIALIZE();
  build_lock(&fixture, &lock);

  sp_str_t toml = spn_lock_file_to_str(&lock);
  spn_lock_file_t loaded = spn_lock_file_parse(toml, SP_NULLPTR);

  // compare system deps
  EXPECT_EQ(sp_ht_size(lock.system_deps), sp_ht_size(loaded.system_deps));
  sp_carr_for(fixture.system_deps, i) {
    if (!fixture.system_deps[i]) break;
    sp_str_t key = sp_str_view(fixture.system_deps[i]);
    bool* found = sp_ht_getp(loaded.system_deps, key);
    EXPECT_TRUE(found != SP_NULLPTR);
  }

  // compare entries
  EXPECT_EQ(sp_ht_size(lock.entries), sp_ht_size(loaded.entries));

  sp_carr_for(fixture.deps, i) {
    lock_dep_t* d = &fixture.deps[i];
    if (!d->name) break;

    sp_str_t name = sp_str_view(d->name);
    spn_lock_entry_t* orig = sp_ht_getp(lock.entries, name);
    spn_lock_entry_t* got = sp_ht_getp(loaded.entries, name);

    ASSERT_TRUE(orig != SP_NULLPTR);
    ASSERT_TRUE(got != SP_NULLPTR);

    SP_EXPECT_STR_EQ(orig->name, got->name);
    EXPECT_EQ(orig->version.major, got->version.major);
    EXPECT_EQ(orig->version.minor, got->version.minor);
    EXPECT_EQ(orig->version.patch, got->version.patch);
    SP_EXPECT_STR_EQ(orig->commit, got->commit);
    EXPECT_EQ(orig->kind, got->kind);
    SP_EXPECT_STR_EQ(orig->source.url, got->source.url);
    SP_EXPECT_STR_EQ(orig->source.rev, got->source.rev);
    SP_EXPECT_STR_EQ(orig->source.dir, got->source.dir);
    SP_EXPECT_STR_EQ(orig->manifest.url, got->manifest.url);
    SP_EXPECT_STR_EQ(orig->manifest.rev, got->manifest.rev);
    SP_EXPECT_STR_EQ(orig->manifest.dir, got->manifest.dir);

    // compare deps
    EXPECT_EQ(sp_dyn_array_size(orig->deps), sp_dyn_array_size(got->deps));
    sp_da_for(orig->deps, j) {
      SP_EXPECT_STR_EQ(orig->deps[j], got->deps[j]);
    }
  }

  // verify dependents are correctly reconstructed
  sp_ht_for_kv(loaded.entries, it) {
    sp_da_for(it.val->deps, dep_it) {
      spn_lock_entry_t* dep = sp_ht_getp(loaded.entries, it.val->deps[dep_it]);
      if (dep) {
        bool found = false;
        sp_da_for(dep->dependents, d_it) {
          if (sp_str_equal(dep->dependents[d_it], it.val->name)) {
            found = true;
            break;
          }
        }
        EXPECT_TRUE(found);
      }
    }
  }
}


////////////
// CASES //
///////////
UTEST_F(lock_roundtrip, empty) {
  run_fixture(utest_result, (fixture_t) {
    0
  });
}

UTEST_F(lock_roundtrip, single_dep) {
  run_fixture(utest_result, (fixture_t) {
    .deps = {
      {
        .name = "math",
        .version = "1.0.0",
        .commit = "abc123",
        .kind = "index",
        .visibility = "public",
      },
    },
  });
}

UTEST_F(lock_roundtrip, multiple_deps_with_transitive) {
  run_fixture(utest_result, (fixture_t) {
    .deps = {
      {
        .name = "audio",
        .version = "2.1.0",
        .commit = "aaa111",
        .kind = "index",
        .visibility = "public",
        .deps = { "math" },
      },
      {
        .name = "math",
        .version = "1.5.0",
        .commit = "bbb222",
        .kind = "index",
        .visibility = "public",
      },
      {
        .name = "renderer",
        .version = "3.0.0",
        .commit = "ccc333",
        .kind = "index",
        .visibility = "public",
        .deps = { "math" },
      },
    },
  });
}

UTEST_F(lock_roundtrip, system_deps) {
  run_fixture(utest_result, (fixture_t) {
    .system_deps = { "alsa", "x11", "opengl" },
    .deps = {
      {
        .name = "audio",
        .version = "1.0.0",
        .commit = "def456",
        .kind = "index",
        .visibility = "public",
      },
    },
  });
}

UTEST_F(lock_roundtrip, all_visibility_types) {
  run_fixture(utest_result, (fixture_t) {
    .deps = {
      {
        .name = "core",
        .version = "1.0.0",
        .commit = "aaa111",
        .kind = "index",
        .visibility = "public",
      },
      {
        .name = "devtools",
        .version = "2.0.0",
        .commit = "bbb222",
        .kind = "index",
        .visibility = "build",
      },
      {
        .name = "testutil",
        .version = "3.0.0",
        .commit = "ccc333",
        .kind = "index",
        .visibility = "test",
      },
    },
  });
}

UTEST_F(lock_roundtrip, all_package_kinds) {
  run_fixture(utest_result, (fixture_t) {
    .deps = {
      {
        .name = "indexed",
        .version = "1.0.0",
        .commit = "aaa111",
        .kind = "index",
        .visibility = "public",
      },
      {
        .name = "local",
        .version = "2.0.0",
        .commit = "bbb222",
        .kind = "file",
        .visibility = "public",
      },
    },
  });
}

UTEST_F(lock_roundtrip, dependents_reconstructed) {
  run_fixture(utest_result, (fixture_t) {
    .deps = {
      {
        .name = "app",
        .version = "1.0.0",
        .commit = "aaa111",
        .kind = "index",
        .visibility = "public",
        .deps = { "lib", "util" },
      },
      {
        .name = "lib",
        .version = "1.0.0",
        .commit = "bbb222",
        .kind = "index",
        .visibility = "public",
        .deps = { "util" },
      },
      {
        .name = "util",
        .version = "1.0.0",
        .commit = "ccc333",
        .kind = "index",
        .visibility = "public",
      },
    },
  });
}

// source pointer roundtrip
UTEST_F(lock_roundtrip, source_pointers) {
  run_fixture(utest_result, (fixture_t) {
    .deps = {
      {
        .name = "spum",
        .version = "1.0.0",
        .commit = "abc123",
        .kind = "index",
        .visibility = "public",
        .source_url = "https://github.com/foo/spum.git",
        .source_rev = "abc123",
        .source_dir = "packages/spum",
      },
    },
  });
}

// split manifest roundtrip
UTEST_F(lock_roundtrip, manifest_source_split) {
  run_fixture(utest_result, (fixture_t) {
    .deps = {
      {
        .name = "sqlite",
        .version = "3.45.0",
        .commit = "def456",
        .kind = "index",
        .visibility = "public",
        .source_url = "https://github.com/sqlite/sqlite.git",
        .source_rev = "def456",
        .manifest_url = "https://github.com/tspader/spn-packages.git",
        .manifest_rev = "789abc",
        .manifest_dir = "sqlite",
      },
    },
  });
}

// empty manifest fields (common case: manifest == source)
UTEST_F(lock_roundtrip, manifest_source_same) {
  run_fixture(utest_result, (fixture_t) {
    .deps = {
      {
        .name = "spum",
        .version = "2.0.0",
        .commit = "aaa111",
        .kind = "index",
        .visibility = "public",
        .source_url = "https://github.com/foo/spum.git",
        .source_rev = "aaa111",
      },
    },
  });
}
