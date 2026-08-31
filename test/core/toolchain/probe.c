#include "toolchain.h"
#include "toolchain/probe.h"

#define PROBE_MAX_FILES 6
#define PROBE_MAX_ACTIONS 6
#define PROBE_MAX_DIRS 2
#define PROBE_MAX_SLOTS 4
#define PROBE_MAX_PAIRS 2
#define PROBE_MAX_SPLIT 3
#define PROBE_MAX_PROGRAMS 4

#if defined(SP_WIN32)
  #define PROBE_SEP ";"
  #define PROBE_EXE ".exe"
#else
  #define PROBE_SEP ":"
  #define PROBE_EXE ""
#endif

typedef enum {
  PROBE_ACTION_NONE,
  PROBE_ACTION_FILE,
  PROBE_ACTION_REMOVE,
  PROBE_ACTION_POISON,
  PROBE_ACTION_FLUSH,
  PROBE_ACTION_RELOAD,
  PROBE_ACTION_CORRUPT,
  PROBE_ACTION_PROBE,
} action_kind_t;

typedef struct {
  const c8* path;
  const c8* content;
} file_t;

typedef struct {
  action_kind_t kind;
  union {
    file_t file;
    struct {
      u32 slot;
      const c8* dirs [PROBE_MAX_DIRS];
      spn_err_t err;
      const c8* program;
      const c8* resolved [PROBE_MAX_PROGRAMS];
      bool cxx_dropped;
    } probe;
  };
} action_t;

typedef struct {
  u32 a;
  u32 b;
} pair_t;

typedef struct {
  u32 entries;
  pair_t same [PROBE_MAX_PAIRS];
  pair_t differ [PROBE_MAX_PAIRS];
} expect_t;

typedef struct {
  const c8* name;
  struct {
    const c8* compiler;
    const c8* linker;
    const c8* archiver;
    const c8* cxx;
    bool no_cxx;
  } programs;
  file_t files [PROBE_MAX_FILES];
  action_t actions [PROBE_MAX_ACTIONS];
  expect_t expect;
} test_t;

#define STANDARD_FILES { { "A/cc" }, { "A/ar" }, { "A/c++" } }

static const test_t tests [] = {
  {
    .name = "resolves_all_programs",
    .files = STANDARD_FILES,
    .actions = {
      { .kind = PROBE_ACTION_PROBE, .probe = { .slot = 1, .resolved = { "A/cc", "A/cc", "A/ar", "A/c++" } } },
    },
    .expect = { .entries = 3 },
  },
  {
    .name = "missing_compiler",
    .files = { { "A/ar" }, { "A/c++" } },
    .actions = {
      { .kind = PROBE_ACTION_PROBE, .probe = { .err = SPN_ERR_TOOLCHAIN_MISSING, .program = "cc" } },
    },
  },
  {
    .name = "missing_linker",
    .programs = { .linker = "ld" },
    .files = STANDARD_FILES,
    .actions = {
      { .kind = PROBE_ACTION_PROBE, .probe = { .err = SPN_ERR_TOOLCHAIN_MISSING, .program = "ld" } },
    },
  },
  {
    .name = "missing_archiver",
    .files = { { "A/cc" }, { "A/c++" } },
    .actions = {
      { .kind = PROBE_ACTION_PROBE, .probe = { .err = SPN_ERR_TOOLCHAIN_MISSING, .program = "ar" } },
    },
  },
  {
    .name = "missing_cxx_degrades",
    .files = { { "A/cc" }, { "A/ar" } },
    .actions = {
      { .kind = PROBE_ACTION_PROBE, .probe = { .slot = 1, .cxx_dropped = true } },
    },
    .expect = { .entries = 2 },
  },
  {
    .name = "cxx_appearing_changes_identity",
    .files = { { "A/cc" }, { "A/ar" } },
    .actions = {
      { .kind = PROBE_ACTION_PROBE, .probe = { .slot = 1, .cxx_dropped = true } },
      { .kind = PROBE_ACTION_FILE, .file = { "A/c++" } },
      { .kind = PROBE_ACTION_PROBE, .probe = { .slot = 2 } },
    },
    .expect = { .differ = { { 1, 2 } } },
  },
  {
    .name = "unset_cxx_is_skipped",
    .programs = { .no_cxx = true },
    .files = { { "A/cc" }, { "A/ar" } },
    .actions = {
      { .kind = PROBE_ACTION_PROBE, .probe = { .slot = 1 } },
    },
    .expect = { .entries = 2 },
  },
  {
    .name = "absolute_program_bypasses_search",
    .programs = { .compiler = "B/cc", .linker = "B/cc", .archiver = "B/ar", .cxx = "B/c++" },
    .files = { { "B/cc" }, { "B/ar" }, { "B/c++" } },
    .actions = {
      { .kind = PROBE_ACTION_PROBE, .probe = { .slot = 1, .resolved = { "B/cc", "B/cc", "B/ar", "B/c++" } } },
    },
    .expect = { .entries = 3 },
  },
  {
    .name = "absolute_program_missing",
    .programs = { .compiler = "B/cc" },
    .files = STANDARD_FILES,
    .actions = {
      { .kind = PROBE_ACTION_PROBE, .probe = { .err = SPN_ERR_TOOLCHAIN_MISSING, .program = "B/cc" } },
    },
  },
  {
    .name = "first_dir_wins",
    .files = { { "A/cc", "1" }, { "B/cc", "2" }, { "A/ar" }, { "B/ar" }, { "A/c++" }, { "B/c++" } },
    .actions = {
      { .kind = PROBE_ACTION_PROBE, .probe = { .slot = 1, .dirs = { "A", "B" }, .resolved = { "A/cc" } } },
      { .kind = PROBE_ACTION_PROBE, .probe = { .slot = 2, .dirs = { "B", "A" }, .resolved = { "B/cc" } } },
      { .kind = PROBE_ACTION_PROBE, .probe = { .slot = 3, .dirs = { "A" } } },
    },
    .expect = {
      .same = { { 1, 3 } },
      .differ = { { 1, 2 } },
    },
  },
  {
    .name = "later_dir_is_searched",
    .files = { { "A/cc" }, { "B/ar" }, { "B/c++" } },
    .actions = {
      { .kind = PROBE_ACTION_PROBE, .probe = { .slot = 1, .dirs = { "A", "B" }, .resolved = { "A/cc", "A/cc", "B/ar", "B/c++" } } },
    },
    .expect = { .entries = 3 },
  },
  {
    .name = "identity_tracks_binary_bytes",
    .files = STANDARD_FILES,
    .actions = {
      { .kind = PROBE_ACTION_PROBE, .probe = { .slot = 1 } },
      { .kind = PROBE_ACTION_FILE, .file = { "A/cc", "BB" } },
      { .kind = PROBE_ACTION_PROBE, .probe = { .slot = 2 } },
    },
    .expect = { .differ = { { 1, 2 } } },
  },
  {
    .name = "rewrite_same_bytes_keeps_identity",
    .files = STANDARD_FILES,
    .actions = {
      { .kind = PROBE_ACTION_PROBE, .probe = { .slot = 1 } },
      { .kind = PROBE_ACTION_FILE, .file = { "A/cc" } },
      { .kind = PROBE_ACTION_PROBE, .probe = { .slot = 2 } },
    },
    .expect = { .same = { { 1, 2 } } },
  },
  {
    .name = "memoizes_by_stat",
    .files = STANDARD_FILES,
    .actions = {
      { .kind = PROBE_ACTION_PROBE, .probe = { .slot = 1 } },
      { .kind = PROBE_ACTION_POISON, .file = { "A/cc" } },
      { .kind = PROBE_ACTION_PROBE, .probe = { .slot = 2 } },
    },
    .expect = { .differ = { { 1, 2 } } },
  },
  {
    .name = "cache_round_trips",
    .files = STANDARD_FILES,
    .actions = {
      { .kind = PROBE_ACTION_PROBE, .probe = { .slot = 1 } },
      { .kind = PROBE_ACTION_FLUSH },
      { .kind = PROBE_ACTION_RELOAD },
      { .kind = PROBE_ACTION_POISON, .file = { "A/cc" } },
      { .kind = PROBE_ACTION_PROBE, .probe = { .slot = 2 } },
    },
    .expect = { .differ = { { 1, 2 } } },
  },
  {
    .name = "corrupt_cache_starts_empty",
    .files = STANDARD_FILES,
    .actions = {
      { .kind = PROBE_ACTION_CORRUPT },
      { .kind = PROBE_ACTION_RELOAD },
      { .kind = PROBE_ACTION_PROBE, .probe = { .slot = 1 } },
    },
    .expect = { .entries = 3 },
  },
  {
    .name = "stale_entry_reports_missing",
    .files = STANDARD_FILES,
    .actions = {
      { .kind = PROBE_ACTION_PROBE, .probe = { .slot = 1 } },
      { .kind = PROBE_ACTION_REMOVE, .file = { "A/cc" } },
      { .kind = PROBE_ACTION_PROBE, .probe = { .err = SPN_ERR_TOOLCHAIN_MISSING, .program = "cc" } },
    },
  },
};

static bool is_absolute(const c8* program) {
  return sp_str_find_c8(sp_cstr_as_str(program), '/') >= 0;
}

static sp_str_t file_path(sp_mem_t mem, sp_str_t root, const c8* spec) {
  sp_str_t path = sp_fs_join_path(mem, root, sp_cstr_as_str(spec));
  return sp_fmt(mem, "{}" PROBE_EXE, sp_fmt_str(path)).value;
}

static sp_str_t program_str(sp_mem_t mem, sp_str_t root, const c8* program) {
  if (is_absolute(program)) {
    return sp_fs_join_path(mem, root, sp_cstr_as_str(program));
  }
  return sp_cstr_as_str(program);
}

static spn_toolchain_launcher_t launcher(sp_mem_t mem, sp_str_t root, const c8* program) {
  return (spn_toolchain_launcher_t) { .program = spn_arg_lit(program_str(mem, root, program)) };
}

static void write_file(sp_mem_t mem, sp_str_t root, file_t file) {
  sp_str_t content = sp_cstr_as_str(file.content ? file.content : file.path);
  sp_fs_create_file_str(file_path(mem, root, file.path), content);
}

static sp_da(sp_str_t) search_dirs(sp_mem_t mem, sp_str_t root, const c8* const* dirs) {
  sp_da(sp_str_t) result = sp_da_new(mem, sp_str_t);
  bool any = false;
  sp_for(it, PROBE_MAX_DIRS) {
    if (!dirs[it]) {
      break;
    }
    any = true;
    sp_da_push(result, sp_fs_join_path(mem, root, sp_cstr_as_str(dirs[it])));
  }
  if (!any) {
    sp_da_push(result, sp_fs_join_path(mem, root, sp_str_lit("A")));
  }
  return result;
}

static spn_cc_toolchain_t make_cc(sp_mem_t mem, sp_str_t root, const test_t* it) {
  const c8* compiler = it->programs.compiler ? it->programs.compiler : "cc";
  spn_cc_toolchain_t cc = {
    .name = sp_str_lit("A"),
    .driver = SPN_CC_DRIVER_GCC,
    .compiler = launcher(mem, root, compiler),
    .linker = launcher(mem, root, it->programs.linker ? it->programs.linker : compiler),
    .archiver = launcher(mem, root, it->programs.archiver ? it->programs.archiver : "ar"),
  };
  if (!it->programs.no_cxx) {
    cc.cxx = launcher(mem, root, it->programs.cxx ? it->programs.cxx : "c++");
  }
  return cc;
}

sp_test_each(probe, resolve, test_t, tests, .setup = spn_test_ctx_setup) {
  sp_mem_t mem = sp_test_arena(t);
  sp_str_t root = sp_test_dir(t);
  sp_fs_create_dir(sp_fs_join_path(mem, root, sp_str_lit("A")));
  sp_fs_create_dir(sp_fs_join_path(mem, root, sp_str_lit("B")));

  sp_carr_for(it->files, at) {
    if (!it->files[at].path) {
      break;
    }
    write_file(mem, root, it->files[at]);
  }

  sp_str_t cache_file = sp_fs_join_path(mem, root, sp_str_lit("probe.cache"));
  spn_probe_cache_t cache = sp_zero;
  spn_probe_cache_load(&cache, cache_file, mem);

  sp_hash_t slots [PROBE_MAX_SLOTS + 1] = sp_zero;

  sp_carr_for(it->actions, at) {
    action_t action = it->actions[at];
    switch (action.kind) {
      case PROBE_ACTION_NONE: {
        at = sp_carr_len(it->actions);
        break;
      }
      case PROBE_ACTION_FILE: {
        write_file(mem, root, action.file);
        break;
      }
      case PROBE_ACTION_REMOVE: {
        sp_fs_remove_file(file_path(mem, root, action.file.path));
        break;
      }
      case PROBE_ACTION_POISON: {
        spn_probe_entry_t** entry = sp_str_om_getp(cache.entries, file_path(mem, root, action.file.path));
        sp_must(t, entry);
        (*entry)->hash += 1;
        break;
      }
      case PROBE_ACTION_FLUSH: {
        sp_must_eq(t, (u32)SPN_OK, (u32)spn_probe_cache_flush(&cache));
        break;
      }
      case PROBE_ACTION_RELOAD: {
        spn_probe_cache_load(&cache, cache_file, mem);
        break;
      }
      case PROBE_ACTION_CORRUPT: {
        sp_fs_create_file_str(cache_file, sp_str_lit("not a cache"));
        break;
      }
      case PROBE_ACTION_PROBE: {
        spn_cc_toolchain_t cc = make_cc(mem, root, it);
        sp_hash_t identity = sp_zero;
        spn_err_t err = spn_toolchain_probe(&cc, search_dirs(mem, root, action.probe.dirs), &cache, mem, &identity);
        sp_must_eq(t, (u32)action.probe.err, (u32)err);
        if (err) {
          sp_da(spn_event_t) errs = spn_test_drain_errs(mem);
          sp_must_eq(t, 1, sp_da_size(errs));
          sp_expect_eq(t, errs[0].err.kind, action.probe.err);
          sp_expect_str_eq_c(t, errs[0].err.program.name, "A");
          sp_expect_str_eq(t, errs[0].err.program.program, program_str(mem, root, action.probe.program));
          break;
        }
        sp_expect(t, identity != 0);
        sp_expect_eq(t, action.probe.cxx_dropped || it->programs.no_cxx, spn_arg_empty(cc.cxx.program));
        const spn_toolchain_launcher_t* launchers [PROBE_MAX_PROGRAMS] = { &cc.compiler, &cc.linker, &cc.archiver, &cc.cxx };
        sp_carr_for(action.probe.resolved, pt) {
          if (!action.probe.resolved[pt]) {
            continue;
          }
          sp_expect_str_eq(t, launchers[pt]->program.prefix, file_path(mem, root, action.probe.resolved[pt]));
        }
        if (action.probe.slot) {
          slots[action.probe.slot] = identity;
        }
        break;
      }
    }
  }

  if (it->expect.entries) {
    sp_expect_eq(t, it->expect.entries, (u32)sp_str_om_size(cache.entries));
  }
  sp_carr_for(it->expect.same, at) {
    pair_t pair = it->expect.same[at];
    if (!pair.a) {
      break;
    }
    sp_expect_eq(t, slots[pair.a], slots[pair.b]);
  }
  sp_carr_for(it->expect.differ, at) {
    pair_t pair = it->expect.differ[at];
    if (!pair.a) {
      break;
    }
    sp_expect(t, slots[pair.a] != slots[pair.b]);
  }

  return SP_OK;
}


typedef struct {
  const c8* name;
  const c8* path;
  const c8* expect [PROBE_MAX_SPLIT];
} split_t;

static const split_t split_tests [] = {
  { "single", "A", { "A" } },
  { "two", "A" PROBE_SEP "B", { "A", "B" } },
  { "empty_entries_dropped", PROBE_SEP "A" PROBE_SEP PROBE_SEP "B" PROBE_SEP, { "A", "B" } },
  { "empty", "" },
};

sp_test_each(probe, split_path, split_t, split_tests) {
  sp_da(sp_str_t) dirs = spn_probe_split_path(sp_test_arena(t), sp_cstr_as_str(it->path));
  sp_must_strs_eq(t, dirs, sp_da_size(dirs), it->expect);
  return SP_OK;
}
