#include "sp.h"
#include "utest.h"
#include "test.h"
#include "action.h"
#include "harness.h"

static void fixture_write_file(sp_str_t path, sp_str_t content) {
  sp_str_t parent = sp_fs_parent_path(path);
  if (!sp_str_empty(parent)) {
    sp_fs_create_dir(parent);
  }

  sp_io_file_writer_t f = sp_zero;
  sp_io_file_writer_from_path(&f, path);
  sp_io_write_str(&f.base, content, SP_NULLPTR);
  sp_io_file_writer_close(&f);
}

void copy_project_path(s32* result, tmpfs_t* fs, sp_str_t project, sp_str_t relative) {
  UTEST_RESULT(result);

  sp_str_t from = sp_fs_join_path(spn_allocator, project, relative);

  // there is no reason to specify something that does not exist
  if (sp_fs_is_glob(from)) {
    ASSERT_TRUE(sp_fs_exists(sp_fs_parent_path(from)));
  } else {
    ASSERT_TRUE(sp_fs_exists(from));
  }

  sp_str_t to = fs->root;

  // we never want to change paths inside the test harness; always 1:1
  sp_str_t parent = sp_fs_parent_path(relative);
  if (!sp_str_empty(parent)) {
    to = tmpfs_get(fs, parent);
    sp_fs_create_dir(to);
  }

  // source exists, destination exists, copy
  sp_fs_copy(from, to);
}

s32 sort_dirs_by_name(const void* a, const void* b) {
  const sp_fs_entry_t* lhs = (const sp_fs_entry_t*)a;
  const sp_fs_entry_t* rhs = (const sp_fs_entry_t*)b;
  return sp_str_sort_kernel_alphabetical(&lhs->name, &rhs->name);
}

static sp_str_t build_release_json(sp_str_t name, sp_str_t version, sp_str_t repo_url, sp_str_t commit) {
  sp_str_builder_t b = SP_ZERO_INITIALIZE();
  sp_str_builder_append_cstr(&b, "{\"namespace\":\"core\",\"name\":\"");
  sp_str_builder_append(&b, name);
  sp_str_builder_append_cstr(&b, "\",\"version\":\"");
  sp_str_builder_append(&b, version);
  sp_str_builder_append_cstr(&b, "\",\"yanked\":false,\"source\":{\"url\":\"");
  sp_str_builder_append(&b, sp_str_replace_c8(spn_allocator, repo_url, '\\', '/'));
  sp_str_builder_append_cstr(&b, "\",\"rev\":\"");
  sp_str_builder_append(&b, commit);
  sp_str_builder_append_cstr(&b, "\",\"dir\":\"\"},\"paths\":{\"manifest\":\"spn.toml\",\"script\":\"spn.c\"},\"deps\":[]}");
  return sp_str_builder_to_str(&b);
}

void setup_fixture_index_from_remote(s32* result, tmpfs_t* fs, sp_str_t index, sp_str_t project) {
  UTEST_RESULT(result);

  sp_str_t remote = sp_fs_join_path(spn_allocator, project, sp_str_lit("remote"));
  if (!sp_fs_exists(remote)) {
    return;
  }

  ASSERT_TRUE(sp_fs_is_dir(remote));

  // create core/ namespace directory under index
  sp_str_t core_dir = sp_fs_join_path(spn_allocator, index, sp_str_lit("core"));
  sp_fs_create_dir(core_dir);

  sp_da(sp_fs_entry_t) entries = sp_fs_collect(spn_allocator, remote);
  sp_da_for(entries, it) {
    sp_fs_entry_t* entry = &entries[it];
    if (!sp_fs_is_dir(entry->path)) {
      continue;
    }

    sp_da(sp_fs_entry_t) versions = sp_fs_collect(spn_allocator, entry->path);
    ASSERT_FALSE(sp_da_empty(versions));
    sp_dyn_array_sort(versions, sort_dirs_by_name);

    sp_str_t repo = tmpfs_get(fs, sp_fs_join_path(spn_allocator, sp_str_lit("remote"), entry->name));
    sp_str_t jsonl_path = sp_fs_join_path(spn_allocator, core_dir, sp_format("{}.jsonl", SP_FMT_STR(entry->name)));

    git_repo_init(repo);

    sp_str_builder_t jsonl = SP_ZERO_INITIALIZE();

    sp_da_for(versions, v) {
      sp_fs_entry_t* dir = &versions[v];
      ASSERT_TRUE(sp_fs_is_dir(dir->path));

      sp_str_t source_manifest = sp_fs_join_path(spn_allocator, dir->path, sp_str_lit("spn.toml"));
      ASSERT_TRUE(sp_fs_exists(source_manifest));

      git_repo_commit_from_dir(dir->path, repo, dir->name);
      sp_str_t commit = git_repo_head(repo);

      sp_str_t line = build_release_json(entry->name, dir->name, repo, commit);
      sp_str_builder_append(&jsonl, line);
      sp_str_builder_new_line(&jsonl);
    }

    fixture_write_file(jsonl_path, sp_str_builder_to_str(&jsonl));
  }
}

static sp_str_t str_replace_all(sp_str_t str, sp_str_t needle, sp_str_t repl) {
  sp_str_builder_t b = SP_ZERO_INITIALIZE();
  while (true) {
    s32 at = sp_str_find(str, needle);
    if (at == SP_STR_NO_MATCH) {
      sp_str_builder_append(&b, str);
      break;
    }
    sp_str_builder_append(&b, sp_str(str.data, at));
    sp_str_builder_append(&b, repl);
    str = sp_str(str.data + at + needle.len, str.len - at - needle.len);
  }
  return sp_str_builder_to_str(&b);
}

// @spader use sp_template.h
void setup_fixture_source_repos(s32* result, fixture_t* fixture, sp_str_t project) {
  UTEST_RESULT(result);

  sp_str_t source = sp_fs_join_path(spn_allocator, project, sp_str_lit("source"));
  if (!sp_fs_exists(source)) {
    return;
  }

  ASSERT_TRUE(sp_fs_is_dir(source));

  struct { sp_str_t token; sp_str_t value; } subs[16];
  s32 num_subs = 0;

  sp_da(sp_fs_entry_t) entries = sp_fs_collect(spn_allocator, source);
  sp_da_for(entries, it) {
    sp_fs_entry_t* entry = &entries[it];
    if (!sp_fs_is_dir(entry->path)) {
      continue;
    }

    sp_str_t repo = tmpfs_get(&fixture->fs, sp_fs_join_path(spn_allocator, sp_str_lit("source"), entry->name));
    git_repo_init(repo);
    git_repo_commit_from_dir(entry->path, repo, sp_str_lit("source"));
    sp_str_t commit = git_repo_head(repo);
    sp_str_t url = sp_str_replace_c8(spn_allocator, repo, '\\', '/');

    ASSERT_TRUE(num_subs + 2 <= (s32)sp_carr_len(subs));
    subs[num_subs].token = sp_format("@{}.url@", SP_FMT_STR(entry->name));
    subs[num_subs].value = url;
    num_subs++;
    subs[num_subs].token = sp_format("@{}.commit@", SP_FMT_STR(entry->name));
    subs[num_subs].value = commit;
    num_subs++;
  }

  if (num_subs == 0) {
    return;
  }

  sp_da(sp_fs_entry_t) files = sp_fs_collect_recursive(spn_allocator, fixture->fs.root);
  sp_da_for(files, it) {
    sp_fs_entry_t* file = &files[it];
    if (sp_fs_is_dir(file->path)) {
      continue;
    }

    sp_str_t content = SP_ZERO_INITIALIZE();
    sp_io_read_file(spn_allocator, file->path, &content);

    bool changed = false;
    sp_for(s, num_subs) {
      if (sp_str_contains(content, subs[s].token)) {
        content = str_replace_all(content, subs[s].token, subs[s].value);
        changed = true;
      }
    }

    if (changed) {
      fixture_write_file(file->path, content);
    }
  }
}

void setup_fixture_envrc(tmpfs_t* fs, sp_str_t storage, sp_str_t toolchain, sp_str_t config) {
  sp_str_t path = tmpfs_get(fs, sp_str_lit(".envrc"));
  sp_str_t content = sp_format(
    "export SPN_STORAGE_DIR={}\n"
    "export SPN_TOOLCHAIN_DIR={}\n"
    "export SPN_CONFIG_DIR={}\n",
    SP_FMT_STR(storage),
    SP_FMT_STR(toolchain),
    SP_FMT_STR(config)
  );
  fixture_write_file(path, content);
}

void setup_fixture_config(tmpfs_t* fs, sp_str_t config_dir, sp_str_t index_dir, sp_str_t spn_dir) {
  sp_str_t spn_config_dir = sp_fs_join_path(spn_allocator, config_dir, sp_str_lit("spn"));
  sp_fs_create_dir(spn_config_dir);

  sp_str_t config_path = sp_fs_join_path(spn_allocator, spn_config_dir, sp_str_lit("spn.toml"));
  sp_str_t content = sp_format(
    "spn = \"{}\"\n"
    "\n"
    "[[index]]\n"
    "name = \"core\"\n"
    "url = \"{}\"\n"
    "protocol = \"filesystem\"\n",
    SP_FMT_STR(sp_str_replace_c8(spn_allocator, spn_dir, '\\', '/')),
    SP_FMT_STR(sp_str_replace_c8(spn_allocator, index_dir, '\\', '/'))
  );
  fixture_write_file(config_path, content);
}

void setup_e2e_config(tmpfs_t* fs, sp_str_t config_dir, sp_str_t spn_dir, sp_str_t index_url, sp_str_t index_rev) {
  sp_str_t spn_config_dir = sp_fs_join_path(spn_allocator, config_dir, sp_str_lit("spn"));
  sp_fs_create_dir(spn_config_dir);

  sp_str_t config_path = sp_fs_join_path(spn_allocator, spn_config_dir, sp_str_lit("spn.toml"));
  sp_str_t content = sp_format(
    "spn = \"{}\"\n"
    "\n"
    "[[index]]\n"
    "name = \"core\"\n"
    "url = \"{}\"\n"
    "protocol = \"git\"\n"
    "rev = \"{}\"\n",
    SP_FMT_STR(sp_str_replace_c8(spn_allocator, spn_dir, '\\', '/')),
    SP_FMT_STR(index_url),
    SP_FMT_STR(index_rev)
  );
  fixture_write_file(config_path, content);
}

void expect_exists(s32* utest_result, tmpfs_t* fs, sp_str_t path, bool expected, const c8* file, u32 line) {
  bool exists = sp_fs_exists(path);
  if (exists == expected) return;

  sp_str_builder_t b = SP_ZERO_INITIALIZE();
  sp_str_builder_append_fmt(&b, "{}:{}", SP_FMT_CSTR(file), SP_FMT_U32(line));

  b.indent.word = sp_format("{.red}", SP_FMT_CSTR("▐ "));
  sp_str_builder_indent(&b);

  if (fs) {
    sp_str_builder_new_line(&b);
    sp_str_builder_append_fmt(&b, "{.black} is the root", SP_FMT_STR(fs->root));

    path = sp_str_strip_left(path, fs->root);
    path = sp_str_concat(spn_allocator, sp_str_lit("$test"), path);
  }
  sp_str_builder_new_line(&b);
  if (expected) {
    sp_str_builder_append_fmt(&b, "{.black} does not exist", SP_FMT_STR(path));
  } else {
    sp_str_builder_append_fmt(&b, "{.black} exists (expected not to)", SP_FMT_STR(path));
  }

  SP_TEST_REPORT(sp_str_builder_to_str(&b));
  *utest_result = UTEST_TEST_FAILURE;
}

void fixture_copy_project(s32* utest_result, fixture_t* fixture, sp_str_t project, const c8* const* copy) {
  ASSERT_TRUE(sp_fs_exists(project));

  const c8* defaults [] = {
    "main.c",
    "spn.c",
    "spn.toml",
  };

  sp_carr_for(defaults, it) {
    sp_str_t from = sp_fs_join_path(spn_allocator, project, sp_str_view(defaults[it]));
    if (sp_fs_exists(from)) {
      sp_fs_copy(from, fixture->fs.root);
    }
  }

  if (copy) {
    for (u32 it = 0; copy[it]; it++) {
      copy_project_path(utest_result, &fixture->fs, project, sp_str_view(copy[it]));
    }
  }
}

void run_actions(s32* utest_result, fixture_t* fixture, const action_t* actions) {
  struct { sp_str_t path; sp_tm_epoch_t mtime; } mtime_snaps[8];
  s32 num_mtime_snaps = 0;

  sp_for(it, SPN_TEST_MAX_ACTIONS) {
    action_t action = actions[it];
    if (action.kind == ACTION_NONE) {
      break;
    }

    switch (action.kind) {
      case ACTION_NONE: {
        break;
      }
      case ACTION_CREATE_FILE: {
        tmpfs_create(&fixture->fs, action.create.file, action.create.content);
        break;
      }
      case ACTION_REMOVE_FILE: {
        sp_str_t path = tmpfs_get(&fixture->fs, sp_str_view(action.rm.file));
        sp_fs_remove_file(path);
        SP_EXPECT_NOT_EXISTS_TMPFS(&fixture->fs, path);
        break;
      }
      case ACTION_MOVE_FILE: {
        sp_str_t from = tmpfs_get(&fixture->fs, action.mv.from);
        sp_str_t to = tmpfs_get(&fixture->fs, action.mv.to);
        sp_str_t content = sp_zero; sp_io_read_file(spn_allocator, from, &content);

        tmpfs_create(&fixture->fs, action.mv.to, content);
        sp_fs_remove_file(from);

        SP_EXPECT_NOT_EXISTS_TMPFS(&fixture->fs, from);
        SP_EXPECT_EXISTS_TMPFS(&fixture->fs, to);
        break;
      }
      case ACTION_SUBPROCESS: {
        sp_ps_config_t config = action.process.config;
        if (sp_str_empty(config.cwd)) {
          config.cwd = fixture->fs.root;
        }

        sp_ps_output_t output = sp_ps_run(spn_allocator, config);
        EXPECT_EQ(action.process.rc, output.status.exit_code);
        break;
      }
      case ACTION_RUN_BIN: {
        sp_str_t bin = tmpfs_get(&fixture->fs, sp_format(
          "build/debug/store/bin/{}",
          SP_FMT_CSTR(action.bin.name)
        ));
        SP_EXPECT_EXISTS_TMPFS(&fixture->fs, bin);

        sp_ps_output_t output = sp_ps_run(spn_allocator, (sp_ps_config_t) {
          .command = bin,
          .cwd = fixture->fs.root,
        });

        EXPECT_EQ(action.bin.rc, output.status.exit_code);
        break;
      }
      case ACTION_VERIFY_EXISTS: {
        sp_str_t path = tmpfs_get(&fixture->fs, action.verify_exists.file);
        SP_EXPECT_EXISTS_TMPFS(&fixture->fs, path);
        break;
      }
      case ACTION_VERIFY_NOT_EXISTS: {
        sp_str_t path = tmpfs_get(&fixture->fs, action.verify_not_exists.file);
        SP_EXPECT_NOT_EXISTS_TMPFS(&fixture->fs, path);
        break;
      }
      case ACTION_VERIFY_INCLUDE: {
        sp_str_t path = tmpfs_get(
          &fixture->fs,
          sp_fs_join_path(spn_allocator, sp_str_lit("build/debug/store/include"), action.verify_include.file)
        );
        SP_EXPECT_EXISTS_TMPFS(&fixture->fs, path);
        break;
      }
      case ACTION_VERIFY_CONTENT: {
        sp_str_t path = tmpfs_get(&fixture->fs, action.verify_content.file);
        sp_str_t content = sp_zero; sp_io_read_file(spn_allocator, path, &content);
        EXPECT_TRUE(sp_str_equal(content, action.verify_content.content));
        break;
      }
      case ACTION_VERIFY_FILE_NONEMPTY: {
        sp_str_t path = tmpfs_get(&fixture->fs, action.verify_file_nonempty.file);
        SP_EXPECT_EXISTS_TMPFS(&fixture->fs, path);
        EXPECT_FALSE(spn_test_read_empty(path));
        break;
      }
      case ACTION_VERIFY_FILE_CONTAINS: {
        sp_str_t path = tmpfs_get(&fixture->fs, action.verify_file_contains.file);
        sp_str_t content = sp_zero; sp_io_read_file(spn_allocator, path, &content);
        EXPECT_TRUE(sp_str_contains(content, action.verify_file_contains.needle));
        break;
      }
      case ACTION_VERIFY_FILE_NOT_CONTAINS: {
        sp_str_t path = tmpfs_get(&fixture->fs, action.verify_file_not_contains.file);
        sp_str_t content = sp_zero; sp_io_read_file(spn_allocator, path, &content);
        EXPECT_FALSE(sp_str_contains(content, action.verify_file_not_contains.needle));
        break;
      }
      case ACTION_SNAPSHOT_MTIME: {
        sp_str_t path = tmpfs_get(&fixture->fs, action.snapshot_mtime.file);
        SP_EXPECT_EXISTS_TMPFS(&fixture->fs, path);
        ASSERT_TRUE(num_mtime_snaps < (s32)sp_carr_len(mtime_snaps));
        mtime_snaps[num_mtime_snaps].path = path;
        mtime_snaps[num_mtime_snaps].mtime = sp_fs_get_mod_time(path);
        num_mtime_snaps++;
        break;
      }
      case ACTION_VERIFY_MTIME_UNCHANGED:
      case ACTION_VERIFY_MTIME_CHANGED: {
        sp_str_t path = tmpfs_get(&fixture->fs, action.verify_mtime.file);
        sp_tm_epoch_t before = SP_ZERO_INITIALIZE();
        bool found = false;
        sp_for(s, (u32)num_mtime_snaps) {
          if (sp_str_equal(mtime_snaps[s].path, path)) {
            before = mtime_snaps[s].mtime;
            found = true;
          }
        }
        ASSERT_TRUE(found);
        sp_tm_epoch_t now = sp_fs_get_mod_time(path);
        bool unchanged = before.s == now.s && before.ns == now.ns;
        if (action.kind == ACTION_VERIFY_MTIME_UNCHANGED) {
          EXPECT_TRUE(unchanged);
        } else {
          EXPECT_FALSE(unchanged);
        }
        break;
      }
      case ACTION_REMOVE_DIR: {
        sp_str_t path = tmpfs_get(&fixture->fs, sp_str_view(action.rm.dir));
        sp_fs_remove_dir(path);
        SP_EXPECT_NOT_EXISTS_TMPFS(&fixture->fs, path);
        break;
      }
      case ACTION_RUN_CLI: {
        sp_ps_config_t config = {
          .command = fixture->paths.spn,
          .cwd = fixture->fs.root,
          .env = {
            .extra = {
              { sp_str_lit("SPN_STORAGE_DIR"), fixture->paths.storage },
              { sp_str_lit("SPN_TOOLCHAIN_DIR"), fixture->paths.toolchain },
              { sp_str_lit("SPN_CONFIG_DIR"), fixture->paths.config },
            },
          },
        };

        if (action.cli.cmd) {
          sp_ps_config_add_arg(spn_allocator, &config, sp_str_view(action.cli.cmd));
        }

        sp_carr_for(action.cli.args, arg_it) {
          const c8* arg = action.cli.args[arg_it];
          if (!arg) {
            break;
          }

          sp_ps_config_add_arg(spn_allocator, &config, sp_str_view(arg));
        }

        sp_ps_output_t output = sp_ps_run(spn_allocator, config);
        EXPECT_EQ(action.cli.rc, output.status.exit_code);
        break;
      }
      case ACTION_VERIFY_LOCKED: {
        sp_str_t path = tmpfs_get(&fixture->fs, sp_str_lit("spn.lock"));
        SP_EXPECT_EXISTS_TMPFS(&fixture->fs, path);

        sp_str_t lock = sp_zero; sp_io_read_file(spn_allocator, path, &lock);
        EXPECT_TRUE(sp_str_contains(lock, sp_str_lit("[[dep]]")));
        break;
      }
      case ACTION_VERIFY_PKG_LOCKED: {
        sp_str_t path = tmpfs_get(&fixture->fs, sp_str_lit("spn.lock"));
        SP_EXPECT_EXISTS_TMPFS(&fixture->fs, path);

        sp_str_t lock = sp_zero; sp_io_read_file(spn_allocator, path, &lock);
        sp_str_t needle = sp_format("name = \"{}\"", SP_FMT_CSTR(action.verify_locked.name));
        EXPECT_TRUE(sp_str_contains(lock, needle));
        break;
      }
    }
  }
}

// The toolchain cache is content-addressed and immutable, so every hermetic
// test can share one copy instead of re-downloading zig. Prefer the developer's
// global cache when it's already populated; otherwise use a fixed repo-level dir
// that CI can cache across runs.
static sp_str_t pick_shared_toolchain_dir(sp_str_t root) {
  sp_str_t global = sp_fs_join_path(spn_allocator, sp_fs_get_storage_path(spn_allocator), sp_str_lit("spn/cache/toolchain"));
  if (sp_fs_exists(global)) return global;
  return sp_fs_join_path(spn_allocator, root, sp_str_lit(".cache/toolchain"));
}

void run_test(s32* utest_result, fixture_t* fixture, test_t test) {
  fixture->paths.config = tmpfs_get(&fixture->fs, sp_str_lit(".home/config"));
  fixture->paths.storage = tmpfs_get(&fixture->fs, sp_str_lit(".home/storage"));
  fixture->paths.toolchain = pick_shared_toolchain_dir(fixture->paths.root);
  fixture->paths.include = sp_fs_join_path(spn_allocator, fixture->paths.storage, sp_str_lit("spn/include"));
  fixture->paths.index = sp_fs_join_path(spn_allocator, fixture->paths.storage, sp_str_lit("spn/packages"));
  sp_fs_create_dir(fixture->paths.config);
  sp_fs_create_dir(fixture->paths.storage);
  sp_fs_create_dir(fixture->paths.toolchain);
  sp_fs_create_dir(fixture->paths.include);
  sp_fs_create_dir(fixture->paths.index);
  setup_fixture_envrc(&fixture->fs, fixture->paths.storage, fixture->paths.toolchain, fixture->paths.config);
  setup_fixture_config(&fixture->fs, fixture->paths.config, fixture->paths.index, fixture->paths.root);

  sp_fs_copy(sp_fs_join_path(spn_allocator, fixture->paths.root, sp_str_lit("include/spn.h")), fixture->paths.include);

  if (test.project) {
    sp_str_t project = sp_fs_join_path(spn_allocator, fixture->paths.root, sp_str_view(test.project));
    fixture_copy_project(utest_result, fixture, project, test.copy);
    setup_fixture_index_from_remote(utest_result, &fixture->fs, fixture->paths.index, project);
    setup_fixture_source_repos(utest_result, fixture, project);
  }

  run_actions(utest_result, fixture, test.actions);
}
