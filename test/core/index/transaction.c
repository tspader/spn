#include "index.h"

typedef enum {
  TXN_ACTION_NONE,
  TXN_ACTION_PUBLISH,
  TXN_ACTION_SYNC,
  TXN_ACTION_ADVANCE_REMOTE,
  TXN_ACTION_APPEND_CLONE_FILE,
  TXN_ACTION_CREATE_CLONE_FILE,
  TXN_ACTION_DETACH_HEAD,
} txn_action_kind_t;

typedef struct {
  txn_action_kind_t kind;

  union {
    struct { const c8* name; spn_semver_t version; spn_err_t err; } publish;
    struct { const c8* file; const c8* line; } write;
  };
} txn_action_t;

typedef struct {
  const c8* file;
  u32 lines;
} txn_file_t;

typedef struct {
  const c8* name;

  struct {
    bool empty;
    bool reject_push;
    const c8* pin;
  } remote;

  txn_action_t actions [8];

  struct {
    txn_file_t files [2];
    const c8* head;
  } expect;
} txn_test_t;

static const txn_test_t tests [] = {
  {
    .name = "publish_clones_commits_pushes",
    .actions = {
      { .kind = TXN_ACTION_PUBLISH, .publish = { .name = "spum", .version = { 1, 0, 0 } } },
    },
    .expect = {
      .files = { { .file = "core/spum.jsonl", .lines = 1 } },
      .head = "core/spum 1.0.0",
    },
  },
  {
    .name = "duplicate_rejected_after_freshen",
    .actions = {
      { .kind = TXN_ACTION_PUBLISH, .publish = { .name = "spum", .version = { 1, 0, 0 } } },
      { .kind = TXN_ACTION_PUBLISH, .publish = { .name = "spum", .version = { 1, 0, 0 }, .err = SPN_ERR_VERSION_EXISTS } },
    },
    .expect = {
      .files = { { .file = "core/spum.jsonl", .lines = 1 } },
    },
  },
  {
    .name = "retries_after_remote_advances",
    .actions = {
      { .kind = TXN_ACTION_PUBLISH, .publish = { .name = "spum", .version = { 1, 0, 0 } } },
      {
        .kind = TXN_ACTION_ADVANCE_REMOTE,
        .write = {
          .file = "core/other.jsonl",
          .line = "{" kv("namespace", "core") "," kv("name", "other") "," kv("version", "1.0.0") "," kv("yanked", false) "}",
        },
      },
      { .kind = TXN_ACTION_PUBLISH, .publish = { .name = "spum", .version = { 1, 1, 0 } } },
    },
    .expect = {
      .files = {
        { .file = "core/spum.jsonl", .lines = 2 },
        { .file = "core/other.jsonl", .lines = 1 },
      },
    },
  },
  {
    .name = "discards_tracked_damage",
    .actions = {
      { .kind = TXN_ACTION_PUBLISH, .publish = { .name = "spum", .version = { 1, 0, 0 } } },
      { .kind = TXN_ACTION_APPEND_CLONE_FILE, .write = { .file = "core/spum.jsonl", .line = "not json" } },
      { .kind = TXN_ACTION_PUBLISH, .publish = { .name = "spum", .version = { 1, 1, 0 } } },
    },
    .expect = {
      .files = { { .file = "core/spum.jsonl", .lines = 2 } },
    },
  },
  {
    .name = "retries_rejected_push",
    .remote = { .reject_push = true },
    .actions = {
      { .kind = TXN_ACTION_PUBLISH, .publish = { .name = "spum", .version = { 1, 0, 0 } } },
    },
    .expect = {
      .files = { { .file = "core/spum.jsonl", .lines = 1 } },
      .head = "core/spum 1.0.0",
    },
  },
  {
    .name = "bootstraps_empty_remote",
    .remote = { .empty = true },
    .actions = {
      { .kind = TXN_ACTION_PUBLISH, .publish = { .name = "spum", .version = { 1, 0, 0 } } },
    },
    .expect = {
      .files = { { .file = "core/spum.jsonl", .lines = 1 } },
    },
  },
  {
    .name = "retries_rejected_bootstrap_push",
    .remote = { .empty = true, .reject_push = true },
    .actions = {
      { .kind = TXN_ACTION_PUBLISH, .publish = { .name = "spum", .version = { 1, 0, 0 } } },
    },
    .expect = {
      .files = { { .file = "core/spum.jsonl", .lines = 1 } },
      .head = "core/spum 1.0.0",
    },
  },
  {
    .name = "cleans_untracked_damage",
    .actions = {
      { .kind = TXN_ACTION_SYNC },
      { .kind = TXN_ACTION_CREATE_CLONE_FILE, .write = { .file = "core/spum.jsonl", .line = "not json" } },
      { .kind = TXN_ACTION_PUBLISH, .publish = { .name = "spum", .version = { 1, 0, 0 } } },
    },
    .expect = {
      .files = { { .file = "core/spum.jsonl", .lines = 1 } },
    },
  },
  {
    .name = "pinned_rejected",
    .remote = { .pin = "abc123" },
    .actions = {
      { .kind = TXN_ACTION_PUBLISH, .publish = { .name = "spum", .version = { 1, 0, 0 }, .err = SPN_ERR_INDEX_PINNED } },
    },
  },
  {
    .name = "reattaches_detached_head",
    .actions = {
      { .kind = TXN_ACTION_SYNC },
      { .kind = TXN_ACTION_DETACH_HEAD },
      { .kind = TXN_ACTION_PUBLISH, .publish = { .name = "spum", .version = { 1, 0, 0 } } },
    },
    .expect = {
      .head = "core/spum 1.0.0",
    },
  },
};

static void git_run(sp_str_t cwd, const c8* a, sp_str_t b, sp_str_t c, sp_str_t d, sp_str_t e) {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  sp_ps_output_t result = sp_ps_run(scratch.mem, (sp_ps_config_t) {
    .command = SP_LIT("git"),
    .args = { SP_LIT("-C"), cwd, sp_str_view(a), b, c, d, e },
  });
  sp_assert(!result.status.exit_code);
  sp_mem_end_scratch(scratch);
}

static u32 remote_line_count(sp_mem_t mem, sp_str_t remote, const c8* path) {
  sp_ps_output_t result = sp_ps_run(mem, (sp_ps_config_t) {
    .command = SP_LIT("git"),
    .args = { SP_LIT("-C"), remote, SP_LIT("show"), sp_fmt(mem, "HEAD:{}", sp_fmt_cstr(path)).value },
  });
  if (result.status.exit_code) {
    return 0;
  }

  u32 count = 0;
  sp_str_for_line(result.out, it) {
    if (!sp_str_empty(sp_str_trim(it.line))) {
      count++;
    }
  }
  return count;
}

static sp_str_t remote_head_message(sp_mem_t mem, sp_str_t remote) {
  sp_ps_output_t result = sp_ps_run(mem, (sp_ps_config_t) {
    .command = SP_LIT("git"),
    .args = { SP_LIT("-C"), remote, SP_LIT("log"), SP_LIT("--format=%s"), SP_LIT("-1") },
  });
  return sp_str_trim_right(result.out);
}

sp_test_each(index_transaction, publish, txn_test_t, tests) {
  sp_mem_t mem = sp_test_arena(t);
  sp_str_t tmp = sp_test_dir(t);

  sp_str_t seed = sp_fs_join_path(mem, tmp, sp_str_lit("seed"));
  sp_str_t remote = sp_fs_join_path(mem, tmp, sp_str_lit("remote.git"));

  spn_index_info_t index = {
    .name = sp_str_lit("test"),
    .url = remote,
    .location = sp_fs_join_path(mem, tmp, sp_str_lit("clone")),
    .protocol = SPN_INDEX_PROTOCOL_GIT,
  };
  if (it->remote.pin) {
    index.rev = sp_str_view(it->remote.pin);
  }
  spn_index_init(&index, mem);

  if (it->remote.empty) {
    git_run(tmp, "init", sp_str_lit("--quiet"), sp_str_lit("--bare"), sp_str_lit("remote.git"), sp_zero_s(sp_str_t));
  }
  else {
    sp_fs_create_dir(seed);
    git_repo_init(seed);
    git_run(seed, "symbolic-ref", sp_str_lit("HEAD"), sp_str_lit("refs/heads/main"), sp_zero_s(sp_str_t), sp_zero_s(sp_str_t));
    sp_fs_create_file_str(sp_fs_join_path(mem, seed, sp_str_lit("README.md")), sp_str_lit("index\n"));
    git_repo_stage_all(seed);
    git_repo_commit(seed, sp_str_lit("init"));
    git_run(tmp, "clone", sp_str_lit("--quiet"), sp_str_lit("--bare"), sp_str_lit("seed"), sp_str_lit("remote.git"));
  }

  if (it->remote.reject_push) {
    sp_str_t hook = sp_fs_join_path(mem, remote, sp_str_lit("hooks/pre-receive"));
    sp_fs_create_file_str(hook, sp_str_lit(
      "#!/bin/sh\n"
      "if [ ! -f \"$GIT_DIR/rejected-once\" ]; then\n"
      "  touch \"$GIT_DIR/rejected-once\"\n"
      "  exit 1\n"
      "fi\n"
      "exit 0\n"));
    sp_ps_output_t chmod = sp_ps_run(mem, (sp_ps_config_t) {
      .command = SP_LIT("chmod"),
      .args = { SP_LIT("+x"), hook },
    });
    sp_must_eq(t, 0, chmod.status.exit_code);
  }

  sp_carr_for(it->actions, at) {
    txn_action_t action = it->actions[at];
    if (action.kind == TXN_ACTION_NONE) {
      break;
    }

    switch (action.kind) {
      case TXN_ACTION_PUBLISH: {
        spn_index_release_t rel = {
          .id = {
            .namespace = sp_str_lit("core"),
            .name = sp_str_view(action.publish.name),
          },
          .version = action.publish.version,
          .source = { .url = sp_str_lit("https://example.com/pkg.git"), .rev = sp_str_lit("abc123") },
        };
        sp_da_init(mem, rel.deps);
        sp_da_init(mem, rel.targets);
        sp_expect_eq(t, action.publish.err, spn_index_publish(&index, &rel).kind);
        break;
      }
      case TXN_ACTION_SYNC: {
        sp_expect_eq(t, SPN_OK, spn_index_sync(&index, false));
        break;
      }
      case TXN_ACTION_ADVANCE_REMOTE: {
        git_run(seed, "pull", sp_str_lit("--quiet"), remote, sp_str_lit("main"), sp_zero_s(sp_str_t));
        sp_str_t file = sp_fs_join_path(mem, seed, sp_str_view(action.write.file));
        sp_fs_create_dir(sp_fs_parent_path(file));
        sp_fs_create_file_str(file, sp_fmt(mem, "{}\n", sp_fmt_cstr(action.write.line)).value);
        git_repo_stage_all(seed);
        git_repo_commit(seed, sp_str_lit("advance"));
        git_run(seed, "push", sp_str_lit("--quiet"), remote, sp_str_lit("HEAD:refs/heads/main"), sp_zero_s(sp_str_t));
        break;
      }
      case TXN_ACTION_APPEND_CLONE_FILE: {
        sp_str_t file = sp_fs_join_path(mem, index.location, sp_str_view(action.write.file));
        sp_fs_create_file_str(file, sp_fmt(mem, "{}{}\n",
          sp_fmt_str(test_read_file(mem, file)),
          sp_fmt_cstr(action.write.line)).value);
        break;
      }
      case TXN_ACTION_CREATE_CLONE_FILE: {
        sp_str_t file = sp_fs_join_path(mem, index.location, sp_str_view(action.write.file));
        sp_fs_create_dir(sp_fs_parent_path(file));
        sp_fs_create_file_str(file, sp_fmt(mem, "{}\n", sp_fmt_cstr(action.write.line)).value);
        break;
      }
      case TXN_ACTION_DETACH_HEAD: {
        sp_str_t head = sp_zero;
        sp_expect_eq(t, SPN_OK, spn_git_get_commit_full(mem, index.location, sp_str_lit("HEAD"), &head));
        sp_expect_eq(t, SPN_OK, spn_git_checkout(index.location, head));
        break;
      }
      case TXN_ACTION_NONE: {
        break;
      }
    }
  }

  sp_carr_for(it->expect.files, at) {
    txn_file_t file = it->expect.files[at];
    if (!file.file) {
      break;
    }
    sp_expect_eq(t, file.lines, remote_line_count(mem, remote, file.file));
  }
  if (it->expect.head) {
    sp_expect_str_eq(t, remote_head_message(mem, remote), sp_str_view(it->expect.head));
  }
  if (sp_fs_is_dir(index.location)) {
    sp_expect(t, !spn_git_is_dirty(index.location, index.location));
  }

  return SP_OK;
}
