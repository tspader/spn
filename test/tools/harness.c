#include "sp.h"
#include "utest.h"
#include "test.h"
#include "action.h"
#include "harness.h"
#include "yyjson.h"

static sp_mem_t layout_mem(void) {
  static sp_mem_t mem;
  static bool init = false;
  if (!init) {
    mem = sp_mem_os_new();
    init = true;
  }
  return mem;
}

static sp_str_t layout_path(const c8* triple, const c8* profile, sp_str_t rest) {
  sp_mem_t mem = layout_mem();
  sp_str_t path = sp_str_lit("build");
  if (triple) {
    path = sp_fs_join_path(mem, path, sp_str_view(triple));
  }
  path = sp_fs_join_path(mem, path, sp_str_view(profile));
  return sp_fs_join_path(mem, path, rest);
}

static sp_str_t layout_sub(const c8* dir, const c8* rest) {
  return sp_fs_join_path(layout_mem(), sp_str_view(dir), sp_str_view(rest));
}

static const c8* shared_lib_file(const c8* name) {
  sp_mem_t mem = layout_mem();
  return sp_str_to_cstr(mem, sp_os_lib_to_file_name(mem, sp_str_view(name), SP_OS_LIB_SHARED));
}

sp_str_t shared_lib(const c8* name) {
  sp_mem_t mem = layout_mem();
  return store_file(sp_str_to_cstr(mem, sp_fs_join_path(mem, sp_str_lit("lib"), sp_str_view(shared_lib_file(name)))));
}

sp_str_t staged_lib(const c8* name) {
  return exe(shared_lib_file(name));
}

sp_str_t test_lib(const c8* name) {
  return test_exe(shared_lib_file(name));
}

sp_str_t profile_exe(const c8* profile, const c8* name) {
  return layout_path(SP_NULLPTR, profile, sp_str_view(name));
}

sp_str_t profile_store_file(const c8* profile, const c8* rest) {
  return layout_path(SP_NULLPTR, profile, layout_sub("store", rest));
}

sp_str_t exe(const c8* name) {
  return profile_exe("debug", name);
}

sp_str_t test_exe(const c8* name) {
  return layout_path(SP_NULLPTR, "debug", layout_sub("test", name));
}

sp_str_t target_exe(const c8* name, const c8* triple) {
  return layout_path(triple, "debug", sp_str_view(name));
}

sp_str_t store_file(const c8* rest) {
  return profile_store_file("debug", rest);
}

sp_str_t work_file(const c8* rest) {
  return layout_path(SP_NULLPTR, "debug", layout_sub("work", rest));
}

sp_str_t target_store_file(const c8* rest, const c8* triple) {
  return layout_path(triple, "debug", layout_sub("store", rest));
}

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

  sp_str_t from = sp_fs_join_path(fs->mem, project, relative);

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

typedef struct {
  sp_str_t token;
  sp_str_t value;
} fixture_sub_t;

static sp_str_t str_replace_all(sp_mem_t mem, sp_str_t str, sp_str_t needle, sp_str_t repl);

// Publish one committed manifest checkout into the fixture's filesystem
// index with the real spn binary, so fixture index entries are whatever
// publish actually produces
static void fixture_publish(s32* result, fixture_t* fixture, sp_str_t repo, sp_str_t url, sp_str_t rev) {
  UTEST_RESULT(result);
  sp_mem_t mem = fixture->fs.mem;

  sp_ps_config_t config = {
    .command = fixture->paths.spn,
    .cwd = repo,
    .io = {
      .in.mode = SP_PS_IO_MODE_NULL,
      .err.mode = SP_PS_IO_MODE_REDIRECT,
    },
    .env = {
      .extra = {
        { sp_str_lit("SPN_STORAGE_DIR"), fixture->paths.storage },
        { sp_str_lit("SPN_TOOLCHAIN_DIR"), fixture->paths.toolchain },
        { sp_str_lit("SPN_CONFIG_DIR"), fixture->paths.config },
      },
    },
  };
  sp_ps_config_add_arg(mem, &config, sp_str_lit("publish"));
  sp_ps_config_add_arg(mem, &config, sp_str_lit("--source-url"));
  sp_ps_config_add_arg(mem, &config, sp_str_replace_c8(mem, url, '\\', '/'));
  sp_ps_config_add_arg(mem, &config, sp_str_lit("--source-rev"));
  sp_ps_config_add_arg(mem, &config, rev);

  sp_ps_output_t output = sp_ps_run(mem, config);
  if (output.status.exit_code) {
    sp_log("publish failed in {}:\n{}", sp_fmt_str(repo), sp_fmt_str(output.out));
  }
  ASSERT_EQ(0, output.status.exit_code);
}

void setup_fixture_index_from_remote(s32* result, fixture_t* fixture, sp_str_t project) {
  UTEST_RESULT(result);
  tmpfs_t* fs = &fixture->fs;
  sp_mem_t mem = fs->mem;

  sp_str_t remote = sp_fs_join_path(mem, project, sp_str_lit("remote"));
  if (!sp_fs_exists(remote)) {
    return;
  }

  ASSERT_TRUE(sp_fs_is_dir(remote));

  sp_str_t raw = sp_fs_join_path(mem, project, sp_str_lit("index"));

  sp_str_t recipes = sp_fs_join_path(mem, project, sp_str_lit("recipes"));
  sp_da(fixture_sub_t) subs = sp_da_new(mem, fixture_sub_t);

  sp_da(sp_fs_entry_t) entries = sp_fs_collect(mem, remote);
  sp_da_for(entries, it) {
    sp_fs_entry_t* entry = &entries[it];
    if (!sp_fs_is_dir(entry->path)) {
      continue;
    }

    sp_da(sp_fs_entry_t) versions = sp_fs_collect(mem, entry->path);
    ASSERT_FALSE(sp_da_empty(versions));
    sp_da_sort(versions, sort_dirs_by_name);

    sp_str_t repo = tmpfs_get(fs, sp_fs_join_path(mem, sp_str_lit("remote"), entry->name));
    git_repo_init(repo);
    sp_da_push(subs, ((fixture_sub_t) {
      .token = sp_fmt(mem, "@{}.url@", sp_fmt_str(entry->name)).value,
      .value = sp_str_replace_c8(mem, repo, '\\', '/'),
    }));

    // recipes/<pkg>/<version>/ holds a separate manifest repo for packages
    // whose recipe is published apart from their source
    sp_str_t recipe_versions = sp_fs_join_path(mem, recipes, entry->name);
    bool split = sp_fs_is_dir(recipe_versions);
    sp_str_t recipe_repo = sp_str_lit("");
    if (split) {
      recipe_repo = tmpfs_get(fs, sp_fs_join_path(mem, sp_str_lit("recipes"), entry->name));
      git_repo_init(recipe_repo);
    }

    sp_da_for(versions, v) {
      sp_fs_entry_t* dir = &versions[v];
      ASSERT_TRUE(sp_fs_is_dir(dir->path));

      git_repo_commit_from_dir(dir->path, repo, dir->name);
      sp_str_t commit = git_repo_head(repo);
      sp_da_push(subs, ((fixture_sub_t) {
        .token = sp_fmt(mem, "@{}.{}.commit@", sp_fmt_str(entry->name), sp_fmt_str(dir->name)).value,
        .value = commit,
      }));

      if (sp_fs_is_dir(raw)) {
        continue;
      }

      if (split) {
        sp_str_t recipe_dir = sp_fs_join_path(mem, recipe_versions, dir->name);
        sp_str_t recipe_manifest = sp_fs_join_path(mem, recipe_dir, sp_str_lit("spn.toml"));
        ASSERT_TRUE(sp_fs_exists(recipe_manifest));

        git_repo_commit_from_dir(recipe_dir, recipe_repo, dir->name);

        // The recipe manifest names its upstream with @<pkg>.url@ and
        // @<pkg>.commit@ tokens, which resolve to this version's source
        sp_str_t content = test_read_file(mem, recipe_manifest);
        content = str_replace_all(mem, content,
          sp_fmt(mem, "@{}.url@", sp_fmt_str(entry->name)).value,
          sp_str_replace_c8(mem, repo, '\\', '/'));
        content = str_replace_all(mem, content,
          sp_fmt(mem, "@{}.commit@", sp_fmt_str(entry->name)).value,
          commit);
        fixture_write_file(sp_fs_join_path(mem, recipe_repo, sp_str_lit("spn.toml")), content);
        git_repo_stage_all(recipe_repo);
        git_repo_commit(recipe_repo, dir->name);

        fixture_publish(result, fixture, recipe_repo, recipe_repo, git_repo_head(recipe_repo));
      }
      else {
        sp_str_t source_manifest = sp_fs_join_path(mem, dir->path, sp_str_lit("spn.toml"));
        ASSERT_TRUE(sp_fs_exists(source_manifest));

        fixture_publish(result, fixture, repo, repo, commit);
      }
    }
  }

  // A fixture with an index/ dir writes its entries verbatim (with repo
  // tokens substituted) instead of publishing: coverage for entries that
  // predate whatever publish currently emits
  if (sp_fs_is_dir(raw)) {
    sp_da(sp_fs_entry_t) namespaces = sp_fs_collect(mem, raw);
    sp_da_for(namespaces, nt) {
      sp_fs_entry_t* ns = &namespaces[nt];
      if (!sp_fs_is_dir(ns->path)) {
        continue;
      }
      sp_da(sp_fs_entry_t) files = sp_fs_collect(mem, ns->path);
      sp_da_for(files, ft) {
        sp_str_t content = test_read_file(mem, files[ft].path);
        sp_da_for(subs, st) {
          content = str_replace_all(mem, content, subs[st].token, subs[st].value);
        }
        fixture_write_file(
          sp_fs_join_path(mem, sp_fs_join_path(mem, fixture->paths.index, ns->name), files[ft].name),
          content);
      }
    }
  }
}

static sp_str_t str_replace_all(sp_mem_t mem, sp_str_t str, sp_str_t needle, sp_str_t repl) {
  sp_io_dyn_mem_writer_t b = sp_zero;
  sp_io_dyn_mem_writer_init(mem, &b);
  while (true) {
    s32 at = sp_str_find(str, needle);
    if (at == SP_STR_NO_MATCH) {
      sp_io_write_str(&b.base, str, SP_NULLPTR);
      break;
    }
    sp_io_write_str(&b.base, sp_str(str.data, at), SP_NULLPTR);
    sp_io_write_str(&b.base, repl, SP_NULLPTR);
    str = sp_str(str.data + at + needle.len, str.len - at - needle.len);
  }
  return sp_io_dyn_mem_writer_take_str(&b);
}

// @spader use sp_template.h
void setup_fixture_source_repos(s32* result, fixture_t* fixture, sp_str_t project) {
  UTEST_RESULT(result);
  sp_mem_t mem = fixture->fs.mem;

  sp_str_t source = sp_fs_join_path(mem, project, sp_str_lit("source"));
  if (!sp_fs_exists(source)) {
    return;
  }

  ASSERT_TRUE(sp_fs_is_dir(source));

  struct { sp_str_t token; sp_str_t value; } subs[16];
  s32 num_subs = 0;

  sp_da(sp_fs_entry_t) entries = sp_fs_collect(mem, source);
  sp_da_for(entries, it) {
    sp_fs_entry_t* entry = &entries[it];
    if (!sp_fs_is_dir(entry->path)) {
      continue;
    }

    sp_str_t repo = tmpfs_get(&fixture->fs, sp_fs_join_path(mem, sp_str_lit("source"), entry->name));
    git_repo_init(repo);
    git_repo_commit_from_dir(entry->path, repo, sp_str_lit("source"));
    sp_str_t commit = git_repo_head(repo);
    sp_str_t url = sp_str_replace_c8(mem, repo, '\\', '/');

    ASSERT_TRUE(num_subs + 2 <= (s32)sp_carr_len(subs));
    subs[num_subs].token = sp_fmt(mem, "@{}.url@", sp_fmt_str(entry->name)).value;
    subs[num_subs].value = url;
    num_subs++;
    subs[num_subs].token = sp_fmt(mem, "@{}.commit@", sp_fmt_str(entry->name)).value;
    subs[num_subs].value = commit;
    num_subs++;
  }

  if (num_subs == 0) {
    return;
  }

  sp_da(sp_fs_entry_t) files = sp_fs_collect_recursive(mem, fixture->fs.root);
  sp_da_for(files, it) {
    sp_fs_entry_t* file = &files[it];
    if (sp_fs_is_dir(file->path)) {
      continue;
    }

    sp_str_t content = sp_zero;
    sp_io_read_file(mem, file->path, &content);

    bool changed = false;
    sp_for(s, num_subs) {
      if (sp_str_contains(content, subs[s].token)) {
        content = str_replace_all(mem, content, subs[s].token, subs[s].value);
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
  sp_str_t content = sp_fmt(
    fs->mem,
    "export SPN_STORAGE_DIR={}\n"
    "export SPN_TOOLCHAIN_DIR={}\n"
    "export SPN_CONFIG_DIR={}\n",
    sp_fmt_str(storage),
    sp_fmt_str(toolchain),
    sp_fmt_str(config)
  ).value;
  fixture_write_file(path, content);
}

void setup_fixture_config(tmpfs_t* fs, sp_str_t config_dir, sp_str_t index_dir, sp_str_t spn_dir) {
  sp_mem_t mem = fs->mem;
  sp_str_t spn_config_dir = sp_fs_join_path(mem, config_dir, sp_str_lit("spn"));
  sp_fs_create_dir(spn_config_dir);

  sp_str_t config_path = sp_fs_join_path(mem, spn_config_dir, sp_str_lit("spn.toml"));
  sp_str_t content = sp_fmt(
    mem,
    "spn = \"{}\"\n"
    "\n"
    "[[index]]\n"
    "name = \"core\"\n"
    "url = \"{}\"\n"
    "protocol = \"filesystem\"\n",
    sp_fmt_str(sp_str_replace_c8(mem, spn_dir, '\\', '/')),
    sp_fmt_str(sp_str_replace_c8(mem, index_dir, '\\', '/'))
  ).value;
  fixture_write_file(config_path, content);
}

void setup_e2e_config(tmpfs_t* fs, sp_str_t config_dir, sp_str_t spn_dir, sp_str_t index_url, sp_str_t index_rev) {
  sp_mem_t mem = fs->mem;
  sp_str_t spn_config_dir = sp_fs_join_path(mem, config_dir, sp_str_lit("spn"));
  sp_fs_create_dir(spn_config_dir);

  sp_str_t config_path = sp_fs_join_path(mem, spn_config_dir, sp_str_lit("spn.toml"));
  sp_str_t content = sp_fmt(
    mem,
    "spn = \"{}\"\n"
    "\n"
    "[[index]]\n"
    "name = \"core\"\n"
    "url = \"{}\"\n"
    "protocol = \"git\"\n"
    "rev = \"{}\"\n",
    sp_fmt_str(sp_str_replace_c8(mem, spn_dir, '\\', '/')),
    sp_fmt_str(index_url),
    sp_fmt_str(index_rev)
  ).value;
  fixture_write_file(config_path, content);
}

void expect_exists(s32* utest_result, tmpfs_t* fs, sp_str_t path, bool expected, const c8* file, u32 line) {
  bool exists = sp_fs_exists(path);
  if (exists == expected) return;

  sp_mem_t mem = sp_mem_get_scratch();
  sp_str_t bar = sp_fmt(mem, "{.red}", sp_fmt_cstr("▐ ")).value;

  sp_io_dyn_mem_writer_t b = sp_zero;
  sp_io_dyn_mem_writer_init(mem, &b);
  sp_fmt_io(&b.base, "{}:{}", sp_fmt_cstr(file), sp_fmt_uint(line));

  if (fs) {
    sp_fmt_io(&b.base, "\n{}{.cyan} is the root", sp_fmt_str(bar), sp_fmt_str(fs->root));

    path = sp_str_strip_left(path, fs->root);
    path = sp_str_concat(mem, sp_str_lit("$test"), path);
  }
  if (expected) {
    sp_fmt_io(&b.base, "\n{}{.cyan} does not exist", sp_fmt_str(bar), sp_fmt_str(path));
  } else {
    sp_fmt_io(&b.base, "\n{}{.cyan} exists (expected not to)", sp_fmt_str(bar), sp_fmt_str(path));
  }

  SP_TEST_REPORT_STR(sp_io_dyn_mem_writer_as_str(&b));
  *utest_result = UTEST_TEST_FAILURE;
}

static bool event_matches(yyjson_val* line, const c8* event, const c8* key, const c8* value) {
  const c8* name = yyjson_get_str(yyjson_obj_get(line, "event"));
  if (!name || !sp_cstr_equal(name, event)) return false;
  if (!key) return true;

  yyjson_val* field = yyjson_obj_get(line, key);
  if (!field) {
    field = yyjson_obj_get(yyjson_obj_get(line, "data"), key);
  }

  const c8* str = yyjson_get_str(field);
  return str && sp_cstr_equal(str, value);
}

static void expect_event(s32* utest_result, fixture_t* fixture, action_t action, bool expected, const c8* file, u32 line) {
  sp_mem_t mem = fixture->fs.mem;
  sp_str_t path = sp_fs_join_path(mem, fixture->paths.storage, sp_str_lit("log/build.jsonl"));

  sp_str_t content = sp_zero;
  sp_io_read_file(mem, path, &content);

  bool found = false;
  sp_da(sp_str_t) lines = sp_str_split_c8(mem, content, '\n');
  sp_da_for(lines, it) {
    if (sp_str_empty(lines[it])) continue;

    yyjson_doc* doc = yyjson_read(lines[it].data, lines[it].len, 0);
    if (!doc) continue;

    found = event_matches(yyjson_doc_get_root(doc), action.verify_event.event, action.verify_event.key, action.verify_event.value);
    yyjson_doc_free(doc);
    if (found) break;
  }

  if (found == expected) return;

  sp_io_dyn_mem_writer_t b = sp_zero;
  sp_io_dyn_mem_writer_init(mem, &b);
  sp_fmt_io(&b.base, "{}:{}\n", sp_fmt_cstr(file), sp_fmt_uint(line));
  sp_fmt_io(&b.base, "{.red}event {.cyan}", sp_fmt_cstr("▐ "), sp_fmt_cstr(action.verify_event.event));
  if (action.verify_event.key) {
    sp_fmt_io(&b.base, " with {.cyan} = {.cyan}", sp_fmt_cstr(action.verify_event.key), sp_fmt_cstr(action.verify_event.value));
  }
  if (expected) {
    sp_fmt_io(&b.base, " not found in {.cyan}", sp_fmt_str(path));
  } else {
    sp_fmt_io(&b.base, " found in {.cyan} (expected not to be)", sp_fmt_str(path));
  }

  UTEST_PRINTF("{}\n", sp_fmt_str(sp_io_dyn_mem_writer_as_str(&b)));
  *utest_result = UTEST_TEST_FAILURE;
}

void fixture_copy_project(s32* utest_result, fixture_t* fixture, sp_str_t project, const c8* const* copy) {
  ASSERT_TRUE(sp_fs_exists(project));

  const c8* defaults [] = {
    "main.c",
    "spn.c",
    "spn.toml",
    "configure.c",
    "build.c",
  };

  sp_carr_for(defaults, it) {
    sp_str_t from = sp_fs_join_path(fixture->fs.mem, project, sp_str_view(defaults[it]));
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
  sp_mem_t mem = fixture->fs.mem;

  struct { sp_str_t path; sp_tm_epoch_t mtime; } mtime_snaps[8];
  s32 num_mtime_snaps = 0;
  sp_str_t cli_output = sp_zero;

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
        sp_str_t content = test_read_file(mem, from);

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

        sp_ps_output_t output = sp_ps_run(mem, config);
        EXPECT_EQ(action.process.rc, output.status.exit_code);
        break;
      }
      case ACTION_RUN_BIN:
      case ACTION_RUN_TEST: {
        sp_str_t staged = action.kind == ACTION_RUN_TEST ? test_exe(action.bin.name) : exe(action.bin.name);
        sp_str_t bin = tmpfs_get(&fixture->fs, staged);
        SP_EXPECT_EXISTS_TMPFS(&fixture->fs, bin);

        sp_ps_output_t output = sp_ps_run(mem, (sp_ps_config_t) {
          .command = bin,
          .cwd = fixture->fs.root,
          .io = {
            .in.mode = SP_PS_IO_MODE_NULL,
            .err.mode = SP_PS_IO_MODE_REDIRECT,
          },
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
          sp_fs_join_path(mem, store_file("include"), action.verify_include.file)
        );
        SP_EXPECT_EXISTS_TMPFS(&fixture->fs, path);
        break;
      }
      case ACTION_VERIFY_CONTENT: {
        sp_str_t path = tmpfs_get(&fixture->fs, action.verify_content.file);
        sp_str_t content = test_read_file(mem, path);
        EXPECT_TRUE(sp_str_equal(content, action.verify_content.content));
        break;
      }
      case ACTION_VERIFY_FILE_NONEMPTY: {
        sp_str_t path = tmpfs_get(&fixture->fs, action.verify_file_nonempty.file);
        SP_EXPECT_EXISTS_TMPFS(&fixture->fs, path);
        EXPECT_FALSE(test_read_empty(mem, path));
        break;
      }
      case ACTION_VERIFY_JSON: {
        sp_str_t path = tmpfs_get(&fixture->fs, action.verify_json.file);
        SP_EXPECT_EXISTS_TMPFS(&fixture->fs, path);
        sp_str_t content = test_read_file(mem, path);
        yyjson_doc* doc = yyjson_read(content.data, content.len, 0);
        EXPECT_TRUE(doc != NULL);
        yyjson_doc_free(doc);
        break;
      }
      case ACTION_VERIFY_FILE_CONTAINS: {
        sp_str_t path = tmpfs_get(&fixture->fs, action.verify_file_contains.file);
        sp_str_t content = test_read_file(mem, path);
        EXPECT_TRUE(sp_str_contains(content, action.verify_file_contains.needle));
        break;
      }
      case ACTION_VERIFY_FILE_NOT_CONTAINS: {
        sp_str_t path = tmpfs_get(&fixture->fs, action.verify_file_not_contains.file);
        sp_str_t content = test_read_file(mem, path);
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
        sp_tm_epoch_t before = sp_zero;
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
          .io = {
            .in.mode = SP_PS_IO_MODE_NULL,
            .err.mode = SP_PS_IO_MODE_REDIRECT,
          },
          .env = {
            .extra = {
              { sp_str_lit("SPN_STORAGE_DIR"), fixture->paths.storage },
              { sp_str_lit("SPN_TOOLCHAIN_DIR"), fixture->paths.toolchain },
              { sp_str_lit("SPN_CONFIG_DIR"), fixture->paths.config },
              { sp_str_lit("SPN_PATCH_DIR"), fixture->paths.patches },
            },
          },
        };

        u32 env_slot = 0;
        while (env_slot < sp_carr_len(config.env.extra) && !sp_str_empty(config.env.extra[env_slot].key)) env_slot++;
        sp_carr_for(action.cli.env, env_it) {
          const c8* var = action.cli.env[env_it];
          if (!var || env_slot >= sp_carr_len(config.env.extra)) break;
          sp_str_pair_t pair = sp_str_cleave_c8(sp_str_view(var), '=');
          config.env.extra[env_slot++] = (sp_env_var_t) { .key = pair.first, .value = pair.second };
        }

        if (action.cli.cmd) {
          sp_ps_config_add_arg(mem, &config, sp_str_view(action.cli.cmd));
        }

        sp_carr_for(action.cli.args, arg_it) {
          const c8* arg = action.cli.args[arg_it];
          if (!arg) {
            break;
          }

          sp_ps_config_add_arg(mem, &config, sp_str_view(arg));
        }

        sp_ps_output_t output = sp_ps_run(mem, config);
        EXPECT_EQ(action.cli.rc, output.status.exit_code);
        cli_output = output.out;
        break;
      }
      case ACTION_VERIFY_CLI_CONTAINS: {
        EXPECT_TRUE(sp_str_contains(cli_output, action.verify_cli.needle));
        break;
      }
      case ACTION_VERIFY_CLI_NOT_CONTAINS: {
        EXPECT_FALSE(sp_str_contains(cli_output, action.verify_cli.needle));
        break;
      }
      case ACTION_VERIFY_EVENT:
      case ACTION_VERIFY_NO_EVENT: {
        expect_event(utest_result, fixture, action, action.kind == ACTION_VERIFY_EVENT, __FILE__, __LINE__);
        break;
      }
      case ACTION_VERIFY_LOCKED: {
        sp_str_t path = tmpfs_get(&fixture->fs, sp_str_lit("spn.lock"));
        SP_EXPECT_EXISTS_TMPFS(&fixture->fs, path);

        sp_str_t lock = test_read_file(mem, path);
        EXPECT_TRUE(sp_str_contains(lock, sp_str_lit("[[dep]]")));
        break;
      }
      case ACTION_VERIFY_PKG_LOCKED: {
        sp_str_t path = tmpfs_get(&fixture->fs, sp_str_lit("spn.lock"));
        SP_EXPECT_EXISTS_TMPFS(&fixture->fs, path);

        sp_str_t lock = test_read_file(mem, path);
        sp_str_t needle = sp_fmt(mem, "name = \"{}\"", sp_fmt_cstr(action.verify_locked.name)).value;
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
static sp_str_t pick_shared_toolchain_dir(sp_mem_t mem, sp_str_t root) {
  sp_str_t global = sp_fs_join_path(mem, sp_fs_get_storage_path(mem), sp_str_lit("spn/cache/toolchain"));
  if (sp_fs_exists(global)) return global;
  return sp_fs_join_path(mem, root, sp_str_lit(".cache/toolchain"));
}

void fixture_setup_paths(fixture_t* fixture) {
  // CMake passes the binary as a repo-relative compile definition; under spn,
  // fall back to its store. The root always comes from walking up the exe path
  sp_mem_t mem = sp_mem_os_new();
  fixture->paths.root = test_repo_root(mem);
#if defined(SPN_TEST_BIN)
  fixture->paths.spn = test_repo_path(mem, sp_str_lit(SPN_TEST_BIN));
#else
  fixture->paths.spn = test_repo_path(mem, exe("spn"));
#endif
}

void run_test(s32* utest_result, fixture_t* fixture, test_t test) {
  sp_mem_t mem = fixture->fs.mem;

  fixture->paths.config = tmpfs_get(&fixture->fs, sp_str_lit(".home/config"));
  fixture->paths.storage = tmpfs_get(&fixture->fs, sp_str_lit(".home/storage"));
  fixture->paths.patches = tmpfs_get(&fixture->fs, sp_str_lit("patches"));
  fixture->paths.toolchain = pick_shared_toolchain_dir(mem, fixture->paths.root);
  fixture->paths.include = sp_fs_join_path(mem, fixture->paths.storage, sp_str_lit("spn/include"));
  fixture->paths.index = sp_fs_join_path(mem, fixture->paths.storage, sp_str_lit("spn/packages"));
  sp_fs_create_dir(fixture->paths.config);
  sp_fs_create_dir(fixture->paths.storage);
  sp_fs_create_dir(fixture->paths.toolchain);
  sp_fs_create_dir(fixture->paths.include);
  sp_fs_create_dir(fixture->paths.index);
  setup_fixture_envrc(&fixture->fs, fixture->paths.storage, fixture->paths.toolchain, fixture->paths.config);
  setup_fixture_config(&fixture->fs, fixture->paths.config, fixture->paths.index, fixture->paths.root);

  sp_fs_copy(sp_fs_join_path(mem, fixture->paths.root, sp_str_lit("include/spn.h")), fixture->paths.include);

  if (test.project) {
    sp_str_t project = sp_fs_join_path(mem, fixture->paths.root, sp_str_view(test.project));
    fixture_copy_project(utest_result, fixture, project, test.copy);
    setup_fixture_index_from_remote(utest_result, fixture, project);
    setup_fixture_source_repos(utest_result, fixture, project);
  }

  run_actions(utest_result, fixture, test.actions);
}
