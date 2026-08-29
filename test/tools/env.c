#include "env.h"
#include "caps.h"
#include "triple/triple.h"

void write_file(sp_str_t path, sp_str_t content) {
  sp_str_t parent = sp_fs_parent_path(path);
  if (!sp_str_empty(parent)) {
    sp_fs_create_dir(parent);
  }

  sp_io_file_writer_t f = sp_zero;
  sp_io_file_writer_from_path(&f, path);
  sp_io_write_str(&f.base, content, SP_NULLPTR);
  sp_io_file_writer_close(&f);
}

static void fixture_setup_paths(fixture_t* fixture) {
  sp_mem_t mem = fixture->mem;
  fixture->paths.root = test_repo_root(mem);
#if defined(SPN_TEST_BIN)
  fixture->paths.spn = test_repo_path(mem, sp_str_lit(SPN_TEST_BIN));
#else
  sp_str_t test_dir = sp_fs_parent_path(sp_fs_get_exe_path(mem));
  fixture->paths.spn = sp_fs_join_path(mem, sp_fs_parent_path(test_dir), spn_triple_exe_file_name(mem, test_host(), sp_str_lit("spn")));
#endif
}

fixture_t fixture_new(sp_test_t* t) {
  fixture_t fixture = {
    .mem = sp_test_arena(t),
    .root = sp_test_dir(t),
  };
  fixture_setup_paths(&fixture);
  return fixture;
}

sp_err_t fixture_init(sp_test_t* t, fixture_t* fixture) {
  *fixture = fixture_new(t);
  sp_must(t, sp_fs_exists(fixture->paths.spn));
  return SP_OK;
}

sp_str_t fixture_path(fixture_t* fixture, sp_str_t relative) {
  return sp_fs_join_path(fixture->mem, fixture->root, relative);
}

void fixture_create(fixture_t* fixture, sp_str_t relative, sp_str_t content) {
  write_file(fixture_path(fixture, relative), content);
}

static sp_err_t copy_project_path(sp_test_t* t, fixture_t* fixture, sp_str_t project, sp_str_t relative) {
  sp_str_t from = sp_fs_join_path(fixture->mem, project, relative);

  if (sp_fs_is_glob(from)) {
    sp_must(t, sp_fs_exists(sp_fs_parent_path(from)));
  } else {
    sp_must(t, sp_fs_exists(from));
  }

  sp_str_t to = fixture->root;

  sp_str_t parent = sp_fs_parent_path(relative);
  if (!sp_str_empty(parent)) {
    to = fixture_path(fixture, parent);
    sp_fs_create_dir(to);
  }

  sp_fs_copy(from, to);
  return SP_OK;
}

static s32 sort_dirs_by_name(const void* a, const void* b) {
  const sp_fs_entry_t* lhs = (const sp_fs_entry_t*)a;
  const sp_fs_entry_t* rhs = (const sp_fs_entry_t*)b;
  return sp_str_sort_kernel_alphabetical(&lhs->name, &rhs->name);
}

typedef struct {
  sp_str_t token;
  sp_str_t value;
} fixture_sub_t;

static sp_str_t ps_command_line(sp_mem_t mem, const sp_ps_config_t* config) {
  sp_da(sp_str_t) parts = sp_da_new(mem, sp_str_t);
  sp_da_push(parts, config->command);
  sp_carr_for(config->args, it) {
    if (sp_str_empty(config->args[it])) break;
    sp_da_push(parts, config->args[it]);
  }
  sp_da_for(config->dyn_args, it) {
    sp_da_push(parts, config->dyn_args[it]);
  }
  return sp_str_join_n(mem, parts, sp_da_size(parts), sp_str_lit(" "));
}

static sp_err_t fixture_publish(sp_test_t* t, fixture_t* fixture, sp_str_t repo, sp_str_t url, sp_str_t rev) {
  sp_mem_t mem = fixture->mem;

  sp_ps_config_t config = {
    .command = fixture->paths.spn,
    .cwd = repo,
    .io = {
      .in.mode = SP_PS_IO_MODE_NULL,
      .err.mode = SP_PS_IO_MODE_CREATE,
    },
    .env = {
      .extra = {
        { sp_str_lit("SPN_STORAGE_DIR"), fixture->paths.storage },
        { sp_str_lit("SPN_TOOLCHAIN_DIR"), fixture->paths.toolchain },
        { sp_str_lit("SPN_CONFIG_DIR"), fixture->paths.config },
      },
    },
  };
  sp_ps_config_add_arg(mem, &config, sp_str_lit("-o"));
  sp_ps_config_add_arg(mem, &config, sp_str_lit("json"));
  sp_ps_config_add_arg(mem, &config, sp_str_lit("publish"));
  sp_ps_config_add_arg(mem, &config, sp_str_lit("--source-url"));
  sp_ps_config_add_arg(mem, &config, sp_str_replace_c8(mem, url, '\\', '/'));
  sp_ps_config_add_arg(mem, &config, sp_str_lit("--source-rev"));
  sp_ps_config_add_arg(mem, &config, rev);

  sp_ps_output_t output = sp_ps_run(mem, config);
  fixture->events = output.out;
  sp_test_kv(t, "command", ps_command_line(mem, &config));
  sp_test_kv(t, "cwd", repo);
  sp_test_kv(t, "stdout", output.out);
  sp_test_kv(t, "stderr", output.err);
  sp_must_eq(t, 0, output.status.exit_code);
  return SP_OK;
}

static sp_err_t setup_fixture_index_from_remote(sp_test_t* t, fixture_t* fixture, sp_str_t project) {
  sp_mem_t mem = fixture->mem;

  sp_str_t remote = sp_fs_join_path(mem, project, sp_str_lit("remote"));
  if (!sp_fs_exists(remote)) {
    return SP_OK;
  }

  sp_must(t, sp_fs_is_dir(remote));

  sp_str_t raw = sp_fs_join_path(mem, project, sp_str_lit("index"));

  sp_str_t recipes = sp_fs_join_path(mem, project, sp_str_lit("recipes"));
  sp_da(fixture_sub_t) subs = sp_da_new(mem, fixture_sub_t);

  sp_da(sp_fs_entry_t) entries = sp_zero;
  sp_fs_collect(mem, remote, &entries);
  sp_da_for(entries, it) {
    sp_fs_entry_t* entry = &entries[it];
    if (!sp_fs_is_dir(entry->path)) {
      continue;
    }

    sp_da(sp_fs_entry_t) versions = sp_zero;
    sp_fs_collect(mem, entry->path, &versions);
    sp_must(t, !sp_da_empty(versions));
    sp_da_sort(versions, sort_dirs_by_name);

    sp_str_t repo = fixture_path(fixture, sp_fs_join_path(mem, sp_str_lit("remote"), entry->name));
    git_repo_init(repo);
    sp_da_push(subs, ((fixture_sub_t) {
      .token = sp_fmt(mem, "@{}.url@", sp_fmt_str(entry->name)).value,
      .value = sp_str_replace_c8(mem, repo, '\\', '/'),
    }));

    sp_str_t recipe_versions = sp_fs_join_path(mem, recipes, entry->name);
    bool split = sp_fs_is_dir(recipe_versions);
    sp_str_t recipe_repo = sp_str_lit("");
    if (split) {
      recipe_repo = fixture_path(fixture, sp_fs_join_path(mem, sp_str_lit("recipes"), entry->name));
      git_repo_init(recipe_repo);
    }

    sp_da_for(versions, v) {
      sp_fs_entry_t* dir = &versions[v];
      sp_must(t, sp_fs_is_dir(dir->path));

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
        sp_must(t, sp_fs_exists(recipe_manifest));

        git_repo_commit_from_dir(recipe_dir, recipe_repo, dir->name);

        sp_str_t content = test_read_file(mem, recipe_manifest);
        content = str_replace_all(mem, content,
          sp_fmt(mem, "@{}.url@", sp_fmt_str(entry->name)).value,
          sp_str_replace_c8(mem, repo, '\\', '/'));
        content = str_replace_all(mem, content,
          sp_fmt(mem, "@{}.commit@", sp_fmt_str(entry->name)).value,
          commit);
        write_file(sp_fs_join_path(mem, recipe_repo, sp_str_lit("spn.toml")), content);
        git_repo_stage_all(recipe_repo);
        git_repo_commit(recipe_repo, dir->name);

        sp_try(fixture_publish(t, fixture, recipe_repo, recipe_repo, git_repo_head(recipe_repo)));
      }
      else {
        sp_str_t source_manifest = sp_fs_join_path(mem, dir->path, sp_str_lit("spn.toml"));
        sp_must(t, sp_fs_exists(source_manifest));

        sp_try(fixture_publish(t, fixture, repo, repo, commit));
      }
    }
  }

  if (sp_fs_is_dir(raw)) {
    sp_da(sp_fs_entry_t) namespaces = sp_zero;
    sp_fs_collect(mem, raw, &namespaces);
    sp_da_for(namespaces, nt) {
      sp_fs_entry_t* ns = &namespaces[nt];
      if (!sp_fs_is_dir(ns->path)) {
        continue;
      }
      sp_da(sp_fs_entry_t) files = sp_zero;
      sp_fs_collect(mem, ns->path, &files);
      sp_da_for(files, ft) {
        sp_str_t content = test_read_file(mem, files[ft].path);
        sp_da_for(subs, st) {
          content = str_replace_all(mem, content, subs[st].token, subs[st].value);
        }
        write_file(
          sp_fs_join_path(mem, sp_fs_join_path(mem, fixture->paths.index, ns->name), files[ft].name),
          content);
      }
    }
    git_repo_stage_all(fixture->paths.index);
    git_repo_commit(fixture->paths.index, sp_str_lit("seed"));
  }
  return SP_OK;
}

sp_str_t str_replace_all(sp_mem_t mem, sp_str_t str, sp_str_t needle, sp_str_t repl) {
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

static sp_err_t setup_fixture_source_repos(sp_test_t* t, fixture_t* fixture, sp_str_t project) {
  sp_mem_t mem = fixture->mem;

  sp_str_t source = sp_fs_join_path(mem, project, sp_str_lit("source"));
  if (!sp_fs_exists(source)) {
    return SP_OK;
  }

  sp_must(t, sp_fs_is_dir(source));

  struct { sp_str_t token; sp_str_t value; } subs[16];
  s32 num_subs = 0;

  subs[num_subs].token = sp_str_lit("@fixture.dir@");
  subs[num_subs].value = sp_str_replace_c8(mem, fixture->root, '\\', '/');
  num_subs++;

  sp_da(sp_fs_entry_t) entries = sp_zero;
  sp_fs_collect(mem, source, &entries);
  sp_da_for(entries, it) {
    sp_fs_entry_t* entry = &entries[it];
    if (!sp_fs_is_dir(entry->path)) {
      continue;
    }

    sp_str_t repo = fixture_path(fixture, sp_fs_join_path(mem, sp_str_lit("source"), entry->name));
    git_repo_init(repo);
    git_repo_commit_from_dir(entry->path, repo, sp_str_lit("source"));
    sp_str_t commit = git_repo_head(repo);
    sp_str_t url = sp_str_replace_c8(mem, repo, '\\', '/');

    sp_must(t, num_subs + 2 <= (s32)sp_carr_len(subs));
    subs[num_subs].token = sp_fmt(mem, "@{}.url@", sp_fmt_str(entry->name)).value;
    subs[num_subs].value = url;
    num_subs++;
    subs[num_subs].token = sp_fmt(mem, "@{}.commit@", sp_fmt_str(entry->name)).value;
    subs[num_subs].value = commit;
    num_subs++;
  }

  if (num_subs == 0) {
    return SP_OK;
  }

  sp_da(sp_fs_entry_t) files = sp_zero;
  sp_fs_collect_recursive(mem, fixture->root, &files);
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
      write_file(file->path, content);
    }
  }
  return SP_OK;
}

static void setup_fixture_envrc(fixture_t* fixture, sp_str_t storage, sp_str_t toolchain, sp_str_t config) {
  sp_str_t path = fixture_path(fixture, sp_str_lit(".envrc"));
  sp_str_t content = sp_fmt(
    fixture->mem,
    "export SPN_STORAGE_DIR={}\n"
    "export SPN_TOOLCHAIN_DIR={}\n"
    "export SPN_CONFIG_DIR={}\n",
    sp_fmt_str(storage),
    sp_fmt_str(toolchain),
    sp_fmt_str(config)
  ).value;
  write_file(path, content);
}

static void setup_fixture_config(fixture_t* fixture, sp_str_t config_dir, sp_str_t index_dir, sp_str_t spn_dir) {
  sp_mem_t mem = fixture->mem;
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
    "protocol = \"git\"\n",
    sp_fmt_str(sp_str_replace_c8(mem, spn_dir, '\\', '/')),
    sp_fmt_str(sp_str_replace_c8(mem, index_dir, '\\', '/'))
  ).value;
  write_file(config_path, content);
}

static sp_str_t pick_shared_toolchain_dir(sp_mem_t mem, sp_str_t root) {
  sp_str_t global = sp_fs_join_path(mem, sp_fs_get_storage_path(mem), sp_str_lit("spn/cache/toolchain"));
  if (sp_fs_exists(global)) return global;
  return sp_fs_join_path(mem, root, sp_str_lit(".cache/toolchain"));
}

static sp_err_t fixture_copy_project(sp_test_t* t, fixture_t* fixture, sp_str_t project, const c8* const* copy) {
  sp_must(t, sp_fs_exists(project));

  const c8* defaults [] = {
    "main.c",
    "spn.c",
    "spn.toml",
    "configure.c",
    "build.c",
  };

  sp_carr_for(defaults, it) {
    sp_str_t from = sp_fs_join_path(fixture->mem, project, sp_str_view(defaults[it]));
    if (sp_fs_exists(from)) {
      sp_fs_copy(from, fixture->root);
    }
  }

  if (copy) {
    for (u32 it = 0; copy[it]; it++) {
      sp_try(copy_project_path(t, fixture, project, sp_str_view(copy[it])));
    }
  }
  return SP_OK;
}

sp_err_t prepare_test(sp_test_t* t, fixture_t* fixture, const c8* project, const c8* const* copy) {
  sp_mem_t mem = fixture->mem;

  fixture->paths.config = fixture_path(fixture, sp_str_lit(".home/config"));
  fixture->paths.storage = fixture_path(fixture, sp_str_lit(".home/storage"));
  fixture->paths.patches = fixture_path(fixture, sp_str_lit("patches"));
  fixture->paths.toolchain = pick_shared_toolchain_dir(mem, fixture->paths.root);
  fixture->paths.include = sp_fs_join_path(mem, fixture->paths.storage, sp_str_lit("spn/include"));
  fixture->paths.index = sp_fs_join_path(mem, fixture->paths.storage, sp_str_lit("spn/packages"));
  sp_fs_create_dir(fixture->paths.config);
  sp_fs_create_dir(fixture->paths.storage);
  sp_fs_create_dir(fixture->paths.toolchain);
  sp_fs_create_dir(fixture->paths.include);
  sp_fs_create_dir(fixture->paths.index);
  git_repo_init(fixture->paths.index);
  git_repo_git(fixture->paths.index, sp_str_lit("symbolic-ref"), sp_str_lit("HEAD"), sp_str_lit("refs/heads/main"));
  git_repo_git(fixture->paths.index, sp_str_lit("config"), sp_str_lit("receive.denyCurrentBranch"), sp_str_lit("updateInstead"));
  git_repo_commit(fixture->paths.index, sp_str_lit("init"));
  setup_fixture_envrc(fixture, fixture->paths.storage, fixture->paths.toolchain, fixture->paths.config);
  setup_fixture_config(fixture, fixture->paths.config, fixture->paths.index, fixture->paths.root);

  sp_fs_copy(sp_fs_join_path(mem, fixture->paths.root, sp_str_lit("include/spn.h")), fixture->paths.include);
  sp_str_t include_spn = sp_fs_join_path(mem, fixture->paths.include, sp_str_lit("spn"));
  sp_fs_create_dir(include_spn);
  sp_fs_copy(sp_fs_join_path(mem, fixture->paths.root, sp_str_lit("include/spn/core.h")), include_spn);
  sp_fs_copy(sp_fs_join_path(mem, fixture->paths.root, sp_str_lit("include/spn/err.h")), include_spn);

  if (project) {
    sp_str_t path = sp_fs_join_path(mem, fixture->paths.root, sp_str_view(project));
    sp_try(fixture_copy_project(t, fixture, path, copy));
    sp_try(setup_fixture_index_from_remote(t, fixture, path));
    sp_try(setup_fixture_source_repos(t, fixture, path));

    sp_str_t workspace_index = fixture_path(fixture, sp_str_lit("index"));
    if (sp_fs_is_dir(workspace_index)) {
      git_repo_init(workspace_index);
      git_repo_stage_all(workspace_index);
      git_repo_commit(workspace_index, sp_str_lit("seed"));
    }
  }
  return SP_OK;
}

sp_ps_output_t run_spn_command(sp_test_t* t, fixture_t* fixture, const c8* output_mode, const c8* const* args, const c8* const* env) {
  sp_mem_t mem = fixture->mem;
  sp_ps_config_t config = {
    .command = fixture->paths.spn,
    .cwd = fixture->root,
    .io = {
      .in.mode = SP_PS_IO_MODE_NULL,
      .err.mode = SP_PS_IO_MODE_CREATE,
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
  while (env_slot < sp_carr_len(config.env.extra) && !sp_str_empty(config.env.extra[env_slot].key)) {
    env_slot++;
  }
  if (env) {
    sp_for(it, SPN_TEST_COMMAND_MAX_ENV) {
      const c8* var = env[it];
      if (!var || env_slot >= sp_carr_len(config.env.extra)) {
        break;
      }
      sp_str_pair_t pair = sp_str_cleave_c8(sp_str_view(var), '=');
      config.env.extra[env_slot++] = (sp_env_var_t) { .key = pair.first, .value = pair.second };
    }
  }

  if (output_mode) {
    sp_ps_config_add_arg(mem, &config, sp_str_lit("-o"));
    sp_ps_config_add_arg(mem, &config, sp_cstr_as_str(output_mode));
  }

  if (args) {
    sp_for(it, SPN_TEST_COMMAND_MAX_ARGS) {
      if (!args[it]) {
        break;
      }
      sp_ps_config_add_arg(mem, &config, sp_str_view(args[it]));
    }
    const test_toolchain_t* toolchain = test_toolchain();
    bool takes_toolchain = sp_cstr_equal(args[0], "build") || sp_cstr_equal(args[0], "test");
    if (takes_toolchain && !sp_cstr_equal(toolchain->name, "zig")) {
      sp_ps_config_add_arg(mem, &config, sp_str_lit("--toolchain"));
      sp_ps_config_add_arg(mem, &config, sp_cstr_as_str(toolchain->name));
    }
  }

  sp_ps_output_t output = sp_ps_run(mem, config);
  fixture->events = output.out;
  sp_test_kv(t, "command", ps_command_line(mem, &config));
  sp_test_kv(t, "stdout", output.out);
  sp_test_kv(t, "stderr", output.err);
  return output;
}
