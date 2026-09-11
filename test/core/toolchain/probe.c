#include "toolchain.h"
#include "toolchain/probe.h"
#include "toolchain/search.h"

#define PROBE_MAX_FILES 6
#define PROBE_MAX_ACTIONS 6
#define PROBE_MAX_DIRS 2
#define PROBE_MAX_SLOTS 4
#define PROBE_MAX_PAIRS 2
#define PROBE_MAX_PROGRAMS 3

#if defined(SP_WIN32)
  #define PROBE_EXE ".exe"
#else
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

typedef enum {
  PROBE_PROGRAM_NONE,
  PROBE_PROGRAM_COMPILER,
  PROBE_PROGRAM_ARCHIVER,
} program_slot_t;

typedef struct {
  const c8* path;
  spn_path_root_t root;
} resolved_t;

typedef struct {
  action_kind_t kind;
  union {
    file_t file;
    struct {
      u32 slot;
      const c8* dirs [PROBE_MAX_DIRS];
      spn_err_t err;
      program_slot_t missing;
      resolved_t resolved [PROBE_MAX_PROGRAMS];
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
    fixture_launcher_t compiler;
    fixture_launcher_t archiver;
    fixture_launcher_t cxx;
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
      { .kind = PROBE_ACTION_PROBE, .probe = { .slot = 1, .resolved = { { "A/cc" }, { "A/ar" }, { "A/c++" } } } },
    },
    .expect = { .entries = 3 },
  },
  {
    .name = "missing_compiler",
    .files = { { "A/ar" }, { "A/c++" } },
    .actions = {
      { .kind = PROBE_ACTION_PROBE, .probe = { .err = SPN_ERR_TOOLCHAIN_MISSING, .missing = PROBE_PROGRAM_COMPILER } },
    },
  },
  {
    .name = "missing_archiver",
    .files = { { "A/cc" }, { "A/c++" } },
    .actions = {
      { .kind = PROBE_ACTION_PROBE, .probe = { .err = SPN_ERR_TOOLCHAIN_MISSING, .missing = PROBE_PROGRAM_ARCHIVER } },
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
    .name = "absolute_path_program",
    .programs = { .compiler = { .path = "B/cc" }, .archiver = { .path = "B/ar" }, .cxx = { .path = "B/c++" } },
    .files = { { "B/cc" }, { "B/ar" }, { "B/c++" } },
    .actions = {
      { .kind = PROBE_ACTION_PROBE, .probe = { .slot = 1, .resolved = { { "B/cc" }, { "B/ar" }, { "B/c++" } } } },
    },
    .expect = { .entries = 3 },
  },
  {
    .name = "absolute_path_program_missing",
    .programs = { .compiler = { .path = "B/cc" } },
    .files = STANDARD_FILES,
    .actions = {
      { .kind = PROBE_ACTION_PROBE, .probe = { .err = SPN_ERR_TOOLCHAIN_MISSING, .missing = PROBE_PROGRAM_COMPILER } },
    },
  },
  {
    .name = "project_path_program",
    .programs = { .compiler = { .path = "B/cc", .root = SPN_PATH_ROOT_PROJECT } },
    .files = { { "P/B/cc" }, { "A/ar" }, { "A/c++" } },
    .actions = {
      { .kind = PROBE_ACTION_PROBE, .probe = { .slot = 1, .resolved = { { "B/cc", SPN_PATH_ROOT_PROJECT }, { "A/ar" }, { "A/c++" } } } },
    },
    .expect = { .entries = 3 },
  },
  {
    .name = "project_path_program_missing",
    .programs = { .compiler = { .path = "B/cc", .root = SPN_PATH_ROOT_PROJECT } },
    .files = STANDARD_FILES,
    .actions = {
      { .kind = PROBE_ACTION_PROBE, .probe = { .err = SPN_ERR_TOOLCHAIN_MISSING, .missing = PROBE_PROGRAM_COMPILER } },
    },
  },
  {
    .name = "archiver_beside_compiler",
    .programs = { .compiler = { .path = "B/cc" } },
    .files = { { "B/cc" }, { "B/ar" }, { "A/ar" }, { "A/c++" } },
    .actions = {
      { .kind = PROBE_ACTION_PROBE, .probe = { .slot = 1, .resolved = { { "B/cc" }, { "B/ar" }, { "A/c++" } } } },
    },
    .expect = { .entries = 3 },
  },
  {
    .name = "first_dir_wins",
    .files = { { "A/cc", "1" }, { "B/cc", "2" }, { "A/ar" }, { "B/ar" }, { "A/c++" }, { "B/c++" } },
    .actions = {
      { .kind = PROBE_ACTION_PROBE, .probe = { .slot = 1, .dirs = { "A", "B" }, .resolved = { { "A/cc" } } } },
      { .kind = PROBE_ACTION_PROBE, .probe = { .slot = 2, .dirs = { "B", "A" }, .resolved = { { "B/cc" } } } },
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
      { .kind = PROBE_ACTION_PROBE, .probe = { .slot = 1, .dirs = { "A", "B" }, .resolved = { { "A/cc" }, { "B/ar" }, { "B/c++" } } } },
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
      { .kind = PROBE_ACTION_PROBE, .probe = { .err = SPN_ERR_TOOLCHAIN_MISSING, .missing = PROBE_PROGRAM_COMPILER } },
    },
  },
};

static sp_str_t exe(sp_mem_t mem, sp_str_t path) {
  return sp_fmt(mem, "{}" PROBE_EXE, sp_fmt_str(path)).value;
}

static sp_str_t file_path(sp_mem_t mem, sp_str_t root, const c8* spec) {
  return exe(mem, sp_fs_join_path(mem, root, sp_cstr_as_str(spec)));
}

static test_arg_t resolved_arg(sp_mem_t mem, sp_str_t root, resolved_t resolved) {
  sp_str_t sub = resolved.root == SPN_PATH_ROOT_NONE ? file_path(mem, root, resolved.path) : exe(mem, sp_cstr_as_str(resolved.path));
  return (test_arg_t) { .path = sp_str_to_cstr(mem, sub), .root = resolved.root };
}

static spn_arg_t missing_program(const spn_cc_toolchain_t* cc, program_slot_t slot) {
  switch (slot) {
    case PROBE_PROGRAM_COMPILER: return cc->compiler.program;
    case PROBE_PROGRAM_ARCHIVER: return cc->archiver.program;
    case PROBE_PROGRAM_NONE: sp_unreachable_case();
  }
  sp_unreachable_return(sp_zero_struct(spn_arg_t));
}

static spn_toolchain_launcher_t launcher(sp_mem_t mem, sp_str_t root, fixture_launcher_t spec, const c8* fallback) {
  if (spec.path && spec.root == SPN_PATH_ROOT_NONE) {
    spec.path = sp_str_to_cstr(mem, sp_fs_join_path(mem, root, sp_cstr_as_str(spec.path)));
  }
  if (!spec.path && !spec.name) {
    spec.name = fallback;
  }
  return (spn_toolchain_launcher_t) { .program = fixture_arg(spec) };
}

static void write_file(sp_mem_t mem, sp_str_t root, file_t file) {
  sp_str_t content = sp_cstr_as_str(file.content ? file.content : file.path);
  sp_fs_create_file_str(file_path(mem, root, file.path), content);
}

static sp_str_t search_path(sp_mem_t mem, sp_str_t root, const c8* const* dirs) {
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
  c8 sep = spn_search_rules(spn_triple_host().os).sep;
  return sp_str_join_n(mem, result, sp_da_size(result), sp_str(&sep, 1));
}

static spn_cc_toolchain_t make_cc(sp_mem_t mem, sp_str_t root, const test_t* it) {
  spn_cc_toolchain_t cc = {
    .name = sp_str_lit("A"),
    .driver = SPN_CC_DRIVER_CLANG,
    .compiler = launcher(mem, root, it->programs.compiler, "cc"),
    .archiver = launcher(mem, root, it->programs.archiver, "ar"),
  };
  if (!it->programs.no_cxx) {
    cc.cxx = launcher(mem, root, it->programs.cxx, "c++");
  }
  return cc;
}

sp_test_each(probe, resolve, test_t, tests, .setup = spn_test_ctx_setup) {
  sp_mem_t mem = sp_test_arena(t);
  sp_str_t root = sp_test_dir(t);
  spn_path_roots_t roots = sp_zero;
  spn_path_roots_set(&roots, mem, SPN_PATH_ROOT_PROJECT, sp_fs_join_path(mem, root, sp_str_lit("P")));
  sp_fs_create_dir(sp_fs_join_path(mem, root, sp_str_lit("A")));
  sp_fs_create_dir(sp_fs_join_path(mem, root, sp_str_lit("B")));
  sp_fs_create_dir(sp_fs_join_path(mem, root, sp_str_lit("P/B")));

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
        spn_cc_toolchain_t declared = make_cc(mem, root, it);
        spn_cc_toolchain_t cc = declared;
        sp_hash_t identity = sp_zero;
        spn_err_t err = spn_toolchain_probe(&cc, &roots, spn_search_rules(spn_triple_host().os), search_path(mem, root, action.probe.dirs), &cache, mem, &identity);
        sp_must_eq(t, (u32)action.probe.err, (u32)err);
        if (err) {
          sp_da(spn_event_t) errs = spn_test_drain_errs(mem);
          sp_must_eq(t, 1, sp_da_size(errs));
          sp_expect_eq(t, errs[0].err.kind, action.probe.err);
          sp_expect_str_eq_c(t, errs[0].err.program.name, "A");
          sp_expect_str_eq(t, errs[0].err.program.program, spn_arg_str(&roots, mem, missing_program(&declared, action.probe.missing)));
          break;
        }
        sp_expect(t, identity != 0);
        sp_expect_eq(t, action.probe.cxx_dropped || it->programs.no_cxx, spn_arg_empty(cc.cxx.program));
        const spn_arg_t* programs [PROBE_MAX_PROGRAMS] = { &cc.compiler.program, &cc.archiver.program, &cc.cxx.program };
        sp_carr_for(programs, pt) {
          if (spn_arg_empty(*programs[pt])) {
            continue;
          }
          sp_expect(t, sp_str_empty(programs[pt]->prefix));
          sp_expect(t, !spn_path_empty(programs[pt]->path));
        }
        sp_carr_for(action.probe.resolved, pt) {
          if (!action.probe.resolved[pt].path) {
            continue;
          }
          if (test_check_arg(t, *programs[pt], resolved_arg(mem, root, action.probe.resolved[pt]))) {
            return SP_ERR;
          }
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

