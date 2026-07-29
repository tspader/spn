#include "spn_test.h"

#include "enum/enum.h"
#include "event/event.h"
#include "lock/lock.h"
#include "semver/convert.h"

///////////
// MOCKS //
///////////
void spn_event_buffer_push(spn_event_buffer_t* evs, spn_build_event_t e) {}

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
  const c8* deps [4];
} lock_dep_t;

typedef struct {
  const c8* name;
  const c8* system_deps [4];
  lock_dep_t deps [8];
} test_t;

static const test_t tests [] = {
  {
    .name = "empty",
  },
  {
    .name = "single_dep",
    .deps = {
      {
        .name = "A",
        .version = "1.0.0",
        .commit = "abc123",
        .kind = "index",
        .visibility = "public",
      },
    },
  },
  {
    .name = "transitive_deps",
    .deps = {
      {
        .name = "A",
        .version = "2.1.0",
        .commit = "aaa111",
        .kind = "index",
        .visibility = "public",
        .deps = { "B" },
      },
      {
        .name = "B",
        .version = "1.5.0",
        .commit = "bbb222",
        .kind = "index",
        .visibility = "public",
      },
      {
        .name = "C",
        .version = "3.0.0",
        .commit = "ccc333",
        .kind = "index",
        .visibility = "public",
        .deps = { "B" },
      },
    },
  },
  {
    .name = "system_deps",
    .system_deps = { "S1", "S2", "S3" },
    .deps = {
      {
        .name = "A",
        .version = "1.0.0",
        .commit = "def456",
        .kind = "index",
        .visibility = "public",
      },
    },
  },
  {
    .name = "visibility_kinds",
    .deps = {
      {
        .name = "A",
        .version = "1.0.0",
        .commit = "aaa111",
        .kind = "index",
        .visibility = "public",
      },
      {
        .name = "B",
        .version = "2.0.0",
        .commit = "bbb222",
        .kind = "index",
        .visibility = "build",
      },
      {
        .name = "C",
        .version = "3.0.0",
        .commit = "ccc333",
        .kind = "index",
        .visibility = "test",
      },
    },
  },
  {
    .name = "package_kinds",
    .deps = {
      {
        .name = "A",
        .version = "1.0.0",
        .commit = "aaa111",
        .kind = "index",
        .visibility = "public",
      },
      {
        .name = "B",
        .version = "2.0.0",
        .commit = "bbb222",
        .kind = "file",
        .visibility = "public",
      },
    },
  },
  {
    .name = "rebuild_dependents",
    .deps = {
      {
        .name = "A",
        .version = "1.0.0",
        .commit = "aaa111",
        .kind = "index",
        .visibility = "public",
        .deps = { "B", "C" },
      },
      {
        .name = "B",
        .version = "1.0.0",
        .commit = "bbb222",
        .kind = "index",
        .visibility = "public",
        .deps = { "C" },
      },
      {
        .name = "C",
        .version = "1.0.0",
        .commit = "ccc333",
        .kind = "index",
        .visibility = "public",
      },
    },
  },
  // source pointer roundtrip
  {
    .name = "source_pointers",
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
  },
  // split manifest roundtrip
  {
    .name = "split_manifest",
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
  },
  // empty manifest fields (common case: manifest == source)
  {
    .name = "same_manifest",
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
  },
};

static void build_lock(sp_mem_t mem, const test_t* fixture, spn_lock_file_t* lock) {
  spn_lock_file_init(mem, lock);

  sp_carr_for(fixture->system_deps, i) {
    if (!fixture->system_deps[i]) break;
    sp_ht_insert(lock->system_deps, sp_str_view(fixture->system_deps[i]), true);
  }

  sp_carr_for(fixture->deps, i) {
    const lock_dep_t* d = &fixture->deps[i];
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

    sp_da_init(mem, entry.deps);
    sp_carr_for(d->deps, j) {
      if (!d->deps[j]) break;
      sp_da_push(entry.deps, sp_str_view(d->deps[j]));
    }

    sp_ht_insert(lock->entries, entry.name, entry);
  }
}

sp_test_each(lock, roundtrip, test_t, tests) {
  sp_mem_t mem = sp_test_arena(t);

  spn_lock_file_t lock = sp_zero;
  build_lock(mem, it, &lock);

  sp_str_t toml = spn_lock_file_to_str(mem, &lock);
  spn_lock_file_t loaded = spn_lock_file_parse(mem, toml, SP_NULLPTR);

  // compare system deps
  sp_expect_eq(t, sp_ht_size(lock.system_deps), sp_ht_size(loaded.system_deps));
  sp_carr_for(it->system_deps, i) {
    if (!it->system_deps[i]) break;
    sp_str_t key = sp_str_view(it->system_deps[i]);
    bool* found = sp_ht_getp(loaded.system_deps, key);
    sp_expect(t, found != SP_NULLPTR);
  }

  // compare entries
  sp_expect_eq(t, sp_ht_size(lock.entries), sp_ht_size(loaded.entries));

  sp_carr_for(it->deps, i) {
    const lock_dep_t* d = &it->deps[i];
    if (!d->name) break;

    sp_str_t name = sp_str_view(d->name);
    spn_lock_entry_t* orig = sp_ht_getp(lock.entries, name);
    spn_lock_entry_t* got = sp_ht_getp(loaded.entries, name);

    sp_must(t, orig != SP_NULLPTR);
    sp_must(t, got != SP_NULLPTR);

    sp_expect_str_eq(t, orig->name, got->name);
    sp_expect_eq(t, orig->version.major, got->version.major);
    sp_expect_eq(t, orig->version.minor, got->version.minor);
    sp_expect_eq(t, orig->version.patch, got->version.patch);
    sp_expect_str_eq(t, orig->commit, got->commit);
    sp_expect_eq(t, orig->kind, got->kind);
    sp_expect_str_eq(t, orig->source.url, got->source.url);
    sp_expect_str_eq(t, orig->source.rev, got->source.rev);
    sp_expect_str_eq(t, orig->source.dir, got->source.dir);
    sp_expect_str_eq(t, orig->manifest.url, got->manifest.url);
    sp_expect_str_eq(t, orig->manifest.rev, got->manifest.rev);
    sp_expect_str_eq(t, orig->manifest.dir, got->manifest.dir);

    // compare deps
    sp_expect_eq(t, sp_da_size(orig->deps), sp_da_size(got->deps));
    sp_da_for(orig->deps, j) {
      sp_expect_str_eq(t, orig->deps[j], got->deps[j]);
    }
  }

  // verify dependents are correctly reconstructed
  sp_ht_for_kv(loaded.entries, kv) {
    sp_da_for(kv.val->deps, dep_it) {
      spn_lock_entry_t* dep = sp_ht_getp(loaded.entries, kv.val->deps[dep_it]);
      if (dep) {
        bool found = false;
        sp_da_for(dep->dependents, d_it) {
          if (sp_str_equal(dep->dependents[d_it], kv.val->name)) {
            found = true;
            break;
          }
        }
        sp_expect(t, found);
      }
    }
  }

  return SP_OK;
}
