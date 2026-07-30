#include "sp.h"
#include "spit.h"
#include "harness.h"
#include "error/error.h"
#include "triple/triple.h"
#include "yyjson.h"

#define SPN_EVENT_NAME(kind, name, ...) [kind] = name,
static const c8* event_names[SPN_EVENT_COUNT] = {
  SPN_EVENT_LIST(SPN_EVENT_NAME)
};
#undef SPN_EVENT_NAME

#define expect_path(t, fixture, path) expect_exists(t, fixture, path, true, __FILE__, __LINE__)
#define expect_no_path(t, fixture, path) expect_exists(t, fixture, path, false, __FILE__, __LINE__)

static SP_THREAD_LOCAL sp_mem_heap_t* harness_heap;

static sp_mem_t harness_mem() {
  if (!harness_heap) {
    harness_heap = sp_mem_heap_new();
  }
  return sp_mem_heap_as_allocator(harness_heap);
}

static sp_str_t layout_path(const c8* triple, const c8* profile, sp_str_t rest) {
  sp_mem_t mem = harness_mem();
  sp_str_t path = sp_str_lit("build");
  if (triple) {
    path = sp_fs_join_path(mem, path, sp_str_view(triple));
  }
  path = sp_fs_join_path(mem, path, sp_str_view(profile));
  return sp_fs_join_path(mem, path, rest);
}

static sp_str_t layout_sub(const c8* dir, const c8* rest) {
  return sp_fs_join_path(harness_mem(), sp_str_view(dir), sp_str_view(rest));
}

static const c8* shared_lib_file(const c8* name) {
  sp_mem_t mem = harness_mem();
  return sp_str_to_cstr(mem, spn_triple_lib_file_name(mem, test_host(), sp_str_view(name), SP_OS_LIB_SHARED));
}

sp_str_t shared_lib(const c8* name) {
  sp_mem_t mem = harness_mem();
  return store_file(sp_str_to_cstr(mem, sp_fs_join_path(mem, sp_str_lit("lib"), sp_str_view(shared_lib_file(name)))));
}

sp_str_t profile_static_lib(const c8* profile, const c8* name) {
  sp_mem_t mem = harness_mem();
  return sp_fmt(mem,
    "build/{}/store/lib/{}",
    sp_fmt_cstr(profile),
    sp_fmt_str(spn_triple_lib_file_name(mem, test_host(), sp_cstr_as_str(name), SP_OS_LIB_STATIC))
  ).value;
}

sp_str_t static_lib(const c8* name) {
  return profile_static_lib("debug", name);
}

sp_str_t staged_lib(const c8* name) {
  return exe(shared_lib_file(name));
}

sp_str_t test_lib(const c8* name) {
  return test_exe(shared_lib_file(name));
}

static sp_str_t exe_file_name(const c8* name, const c8* triple) {
  spn_triple_t target = test_host();
  if (triple) {
    target = spn_triple_merge(target, spn_triple_from_str(sp_str_view(triple)));
  }
  if (sp_str_find_c8(sp_fs_get_name(sp_str_view(name)), '.') >= 0) {
    return sp_str_view(name);
  }
  return spn_triple_exe_file_name(harness_mem(), target, sp_str_view(name));
}

static sp_str_t profile_exe(const c8* profile, const c8* name) {
  return layout_path(SP_NULLPTR, profile, exe_file_name(name, SP_NULLPTR));
}

static const c8* store_rest(const c8* rest, const c8* triple) {
  if (sp_str_starts_with(sp_str_view(rest), sp_str_lit("bin/"))) {
    return sp_str_to_cstr(harness_mem(), exe_file_name(rest, triple));
  }
  return rest;
}

sp_str_t profile_store_file(const c8* profile, const c8* rest) {
  return layout_path(SP_NULLPTR, profile, layout_sub("store", store_rest(rest, SP_NULLPTR)));
}

sp_str_t exe(const c8* name) {
  return profile_exe("debug", name);
}

sp_str_t test_exe(const c8* name) {
  sp_str_t file = exe_file_name(name, SP_NULLPTR);
  return layout_path(SP_NULLPTR, "debug", layout_sub("test", sp_str_to_cstr(harness_mem(), file)));
}

sp_str_t target_exe(const c8* name, const c8* triple) {
  return layout_path(triple, "debug", exe_file_name(name, triple));
}

sp_str_t store_file(const c8* rest) {
  return profile_store_file("debug", rest);
}

sp_str_t work_file(const c8* rest) {
  return layout_path(SP_NULLPTR, "debug", layout_sub("work", rest));
}

sp_str_t target_store_file(const c8* rest, const c8* triple) {
  return layout_path(triple, "debug", layout_sub("store", store_rest(rest, triple)));
}

static void write_file(sp_str_t path, sp_str_t content) {
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
  fixture->paths.spn = test_repo_path(mem, exe("spn"));
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

static sp_str_t str_replace_all(sp_mem_t mem, sp_str_t str, sp_str_t needle, sp_str_t repl);

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
  sp_ps_config_add_arg(mem, &config, sp_str_lit("--ci"));
  sp_ps_config_add_arg(mem, &config, sp_str_lit("publish"));
  sp_ps_config_add_arg(mem, &config, sp_str_lit("--source-url"));
  sp_ps_config_add_arg(mem, &config, sp_str_replace_c8(mem, url, '\\', '/'));
  sp_ps_config_add_arg(mem, &config, sp_str_lit("--source-rev"));
  sp_ps_config_add_arg(mem, &config, rev);

  sp_ps_output_t output = sp_ps_run(mem, config);
  sp_test_kv(t, "command", ps_command_line(mem, &config));
  sp_test_kv(t, "cwd", repo);
  sp_test_kv(t, "output", output.out);
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

  sp_da(sp_fs_entry_t) entries = sp_fs_collect(mem, remote);
  sp_da_for(entries, it) {
    sp_fs_entry_t* entry = &entries[it];
    if (!sp_fs_is_dir(entry->path)) {
      continue;
    }

    sp_da(sp_fs_entry_t) versions = sp_fs_collect(mem, entry->path);
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
        write_file(
          sp_fs_join_path(mem, sp_fs_join_path(mem, fixture->paths.index, ns->name), files[ft].name),
          content);
      }
    }
  }
  return SP_OK;
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

static sp_err_t setup_fixture_source_repos(sp_test_t* t, fixture_t* fixture, sp_str_t project) {
  sp_mem_t mem = fixture->mem;

  sp_str_t source = sp_fs_join_path(mem, project, sp_str_lit("source"));
  if (!sp_fs_exists(source)) {
    return SP_OK;
  }

  sp_must(t, sp_fs_is_dir(source));

  struct { sp_str_t token; sp_str_t value; } subs[16];
  s32 num_subs = 0;

  sp_da(sp_fs_entry_t) entries = sp_fs_collect(mem, source);
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

  sp_da(sp_fs_entry_t) files = sp_fs_collect_recursive(mem, fixture->root);
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
    "protocol = \"filesystem\"\n",
    sp_fmt_str(sp_str_replace_c8(mem, spn_dir, '\\', '/')),
    sp_fmt_str(sp_str_replace_c8(mem, index_dir, '\\', '/'))
  ).value;
  write_file(config_path, content);
}

sp_err_t expect_exists(sp_test_t* t, fixture_t* fixture, sp_str_t path, bool expected, const c8* file, u32 line) {
  bool exists = sp_fs_exists(path);
  if (exists == expected) return SP_OK;

  if (fixture) {
    sp_test_kv(t, "root", fixture->root);
    sp_str_t relative = sp_str_strip_left(path, fixture->root);
    if (relative.len != path.len) {
      path = sp_str_concat(harness_mem(), sp_str_lit("$test"), relative);
    }
  }
  sp_test_kv(t, "path", path);
  sp_test_record(t, (sp_test_failure_t) {
    .file = sp_cstr_as_str(file),
    .line = line,
    .expected = sp_cstr_as_str(expected ? "path exists" : "path does not exist"),
    .actual = sp_cstr_as_str(exists ? "it exists" : "it does not"),
  });
  return SP_ERR;
}

static bool event_matches(yyjson_val* line, const c8* event, const c8* key, const c8* value) {
  const c8* name = yyjson_get_str(yyjson_obj_get(line, "event"));
  if (!name || !sp_cstr_equal(name, event)) return false;
  if (!key) return true;

  yyjson_val* field = yyjson_obj_get(line, key);
  if (!field) {
    field = yyjson_obj_get(yyjson_obj_get(line, "data"), key);
  }

  if (yyjson_is_int(field)) {
    sp_str_t num = sp_fmt(sp_mem_get_scratch(), "{}", sp_fmt_int(yyjson_get_sint(field))).value;
    return sp_str_equal_cstr(num, value);
  }

  const c8* str = yyjson_get_str(field);
  return str && sp_cstr_equal(str, value);
}

static u32 count_events(fixture_t* fixture, spn_build_event_kind_t kind, const c8* key, const c8* value) {
  const c8* event = event_names[kind];
  sp_mem_t mem = fixture->mem;
  sp_str_t path = sp_fs_join_path(mem, fixture->paths.storage, sp_str_lit("log/build.jsonl"));

  sp_str_t content = sp_zero;
  sp_io_read_file(mem, path, &content);

  u32 count = 0;
  sp_da(sp_str_t) lines = sp_str_split_c8(mem, content, '\n');
  sp_da_for(lines, it) {
    if (sp_str_empty(lines[it])) continue;

    yyjson_doc* doc = yyjson_read(lines[it].data, lines[it].len, 0);
    if (!doc) continue;

    if (event_matches(yyjson_doc_get_root(doc), event, key, value)) {
      count++;
    }
    yyjson_doc_free(doc);
  }
  return count;
}

static sp_err_t expect_event(sp_test_t* t, fixture_t* fixture, spn_build_event_kind_t kind, const c8* key, const c8* value, bool expected, const c8* file, u32 line) {
  const c8* event = event_names[kind];
  sp_mem_t mem = fixture->mem;
  sp_str_t path = sp_fs_join_path(mem, fixture->paths.storage, sp_str_lit("log/build.jsonl"));

  sp_str_t content = sp_zero;
  sp_io_read_file(mem, path, &content);

  bool found = false;
  sp_da(sp_str_t) lines = sp_str_split_c8(mem, content, '\n');
  sp_da_for(lines, it) {
    if (sp_str_empty(lines[it])) continue;

    yyjson_doc* doc = yyjson_read(lines[it].data, lines[it].len, 0);
    if (!doc) continue;

    found = event_matches(yyjson_doc_get_root(doc), event, key, value);
    yyjson_doc_free(doc);
    if (found) break;
  }

  if (found == expected) return SP_OK;

  sp_test_kv(t, "event", sp_str_view(event));
  if (key) {
    sp_test_kv(t, "field", sp_fmt(mem, "{} = {}",
      sp_fmt_cstr(key),
      sp_fmt_cstr(value)).value);
  }
  sp_test_kv(t, "log", path);
  sp_test_record(t, (sp_test_failure_t) {
    .file = sp_cstr_as_str(file),
    .line = line,
    .expected = sp_cstr_as_str(expected ? "event logged" : "event not logged"),
    .actual = sp_cstr_as_str(found ? "it was logged" : "it was not"),
  });
  return SP_ERR;
}

static sp_err_t expect_result(sp_test_t* t, fixture_t* fixture, spn_err_t err, const c8* file, u32 line) {
  sp_mem_t mem = fixture->mem;
  sp_str_t path = sp_fs_join_path(mem, fixture->paths.storage, sp_str_lit("log/build.jsonl"));

  sp_str_t content = sp_zero;
  sp_io_read_file(mem, path, &content);

  const c8* actual = SP_NULLPTR;
  sp_da(sp_str_t) lines = sp_str_split_c8(mem, content, '\n');
  sp_da_for(lines, it) {
    if (sp_str_empty(lines[it])) continue;

    yyjson_doc* doc = yyjson_read(lines[it].data, lines[it].len, 0);
    if (!doc) continue;

    yyjson_val* root = yyjson_doc_get_root(doc);
    const c8* name = yyjson_get_str(yyjson_obj_get(root, "event"));
    if (name && sp_cstr_equal(name, "result")) {
      const c8* code = yyjson_get_str(yyjson_obj_get(yyjson_obj_get(root, "data"), "err"));
      actual = code ? sp_str_to_cstr(mem, sp_str_view(code)) : SP_NULLPTR;
    }
    yyjson_doc_free(doc);
  }

  if (actual && sp_cstr_equal(actual, spn_err_to_str(err))) return SP_OK;

  sp_test_kv(t, "log", path);
  sp_test_record(t, (sp_test_failure_t) {
    .file = sp_cstr_as_str(file),
    .line = line,
    .expected = sp_cstr_as_str(spn_err_to_str(err)),
    .actual = actual ? sp_cstr_as_str(actual) : sp_str_lit("no result event"),
  });
  return SP_ERR;
}

static sp_err_t expect_cc_arg(sp_test_t* t, fixture_t* fixture, const action_t* action, const c8* file, u32 line) {
  sp_mem_t mem = fixture->mem;
  bool expected = action->kind == ACTION_VERIFY_CC_ARG;

  sp_str_t path = fixture_path(fixture, sp_str_lit("compile_commands.json"));
  sp_str_t content = test_read_file(mem, path);

  sp_str_t list = sp_str_lit("");
  sp_carr_for(action->verify_cc_arg, it) {
    if (!action->verify_cc_arg[it]) {
      break;
    }
    list = sp_fmt(mem, "{} {.quote}", sp_fmt_str(list), sp_fmt_str(sp_str_view(action->verify_cc_arg[it]))).value;
  }

  yyjson_doc* doc = yyjson_read(content.data, content.len, 0);
  yyjson_val* root = doc ? yyjson_doc_get_root(doc) : SP_NULLPTR;

  u32 num = (u32)yyjson_arr_size(root);
  bool ok = num || !expected;
  sp_str_t offender = sp_zero;

  sp_for(it, num) {
    yyjson_val* entry = yyjson_arr_get(root, it);
    yyjson_val* arguments = yyjson_obj_get(entry, "arguments");

    bool found = false;
    u32 num_args = (u32)yyjson_arr_size(arguments);
    sp_for(ai, num_args) {
      const c8* arg = yyjson_get_str(yyjson_arr_get(arguments, ai));
      if (!arg) {
        continue;
      }
      sp_carr_for(action->verify_cc_arg, alt) {
        if (!action->verify_cc_arg[alt]) {
          break;
        }
        if (sp_cstr_equal(arg, action->verify_cc_arg[alt])) {
          found = true;
        }
      }
    }

    if (found != expected) {
      ok = false;
      const c8* source = yyjson_get_str(yyjson_obj_get(entry, "file"));
      offender = source ? sp_str_view(source) : sp_str_lit("(unknown)");
    }
  }

  if (doc) {
    yyjson_doc_free(doc);
  }
  if (ok) {
    return SP_OK;
  }

  sp_test_kv(t, "args", list);
  sp_test_kv(t, "path", path);
  if (offender.len) {
    sp_test_kv(t, "entry", offender);
  }
  sp_test_record(t, (sp_test_failure_t) {
    .file = sp_cstr_as_str(file),
    .line = line,
    .expected = sp_cstr_as_str(expected ? "compile argument present in every entry" : "compile argument absent from every entry"),
    .actual = sp_cstr_as_str(num ? (expected ? "an entry is missing it" : "an entry has it") : "no compile entries"),
  });
  return SP_ERR;
}

static sp_err_t expect_command_cc(sp_test_t* t, fixture_t* fixture, command_cc_t expected) {
  sp_mem_t mem = fixture->mem;
  sp_str_t path = fixture_path(fixture, sp_str_lit("compile_commands.json"));
  sp_str_t content = test_read_file(mem, path);

  sp_str_t list = sp_str_lit("");
  sp_carr_for(expected.args, it) {
    if (!expected.args[it]) {
      break;
    }
    list = sp_fmt(mem, "{} {.quote}", sp_fmt_str(list), sp_fmt_str(sp_str_view(expected.args[it]))).value;
  }

  yyjson_doc* doc = yyjson_read(content.data, content.len, 0);
  yyjson_val* root = doc ? yyjson_doc_get_root(doc) : SP_NULLPTR;

  u32 num = (u32)yyjson_arr_size(root);
  u32 matches = 0;
  sp_for(it, num) {
    yyjson_val* entry = yyjson_arr_get(root, it);
    yyjson_val* arguments = yyjson_obj_get(entry, "arguments");

    bool found = false;
    u32 num_args = (u32)yyjson_arr_size(arguments);
    sp_for(ai, num_args) {
      const c8* arg = yyjson_get_str(yyjson_arr_get(arguments, ai));
      if (!arg) {
        continue;
      }
      sp_carr_for(expected.args, alt) {
        if (!expected.args[alt]) {
          break;
        }
        if (sp_str_contains(sp_str_view(arg), sp_str_view(expected.args[alt]))) {
          found = true;
        }
      }
    }
    if (found) {
      matches++;
    }
  }

  if (doc) {
    yyjson_doc_free(doc);
  }

  bool ok = expected.absent ? matches == 0 : matches > 0;
  if (ok) {
    return SP_OK;
  }

  sp_test_kv(t, "args", list);
  sp_test_kv(t, "path", path);
  sp_test_record(t, (sp_test_failure_t) {
    .file = sp_cstr_as_str(__FILE__),
    .line = (u32)__LINE__,
    .expected = sp_cstr_as_str(expected.absent ? "no compile entry has a matching argument" : "a compile entry has a matching argument"),
    .actual = sp_cstr_as_str(expected.absent ? "an entry has one" : (num ? "no entry has one" : "no compile entries")),
  });
  return SP_ERR;
}

static sp_ps_output_t run_spn_command(sp_test_t* t, fixture_t* fixture, const c8* const* args, const c8* const* env) {
  sp_mem_t mem = fixture->mem;
  sp_ps_config_t config = {
    .command = fixture->paths.spn,
    .cwd = fixture->root,
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
  sp_ps_config_add_arg(mem, &config, sp_str_lit("--ci"));

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

  if (args) {
    sp_for(it, SPN_TEST_COMMAND_MAX_ARGS) {
      if (!args[it]) {
        break;
      }
      sp_ps_config_add_arg(mem, &config, sp_str_view(args[it]));
    }
  }

  sp_ps_output_t output = sp_ps_run(mem, config);
  sp_test_kv(t, "command", ps_command_line(mem, &config));
  sp_test_kv(t, "output", output.out);
  return output;
}

static sp_ps_output_t run_fixture_bin(fixture_t* fixture, sp_str_t path) {
  return sp_ps_run(fixture->mem, (sp_ps_config_t) {
    .command = path,
    .cwd = fixture->root,
    .env = {
      .extra = {
        { sp_str_lit("ASAN_OPTIONS"), sp_str_lit("abort_on_error=0:exitcode=1") },
        { sp_str_lit("UBSAN_OPTIONS"), sp_str_lit("halt_on_error=1:abort_on_error=0:exitcode=1") },
      },
    },
    .io = {
      .in.mode = SP_PS_IO_MODE_NULL,
      .err.mode = SP_PS_IO_MODE_REDIRECT,
    },
  });
}

static sp_err_t run_command_bin(sp_test_t* t, fixture_t* fixture, command_bin_t bin) {
  sp_str_t staged = bin.path;
  if (sp_str_empty(staged)) {
    staged = bin.profile ? profile_exe(bin.profile, bin.name) : exe(bin.name);
  }
  sp_str_t path = fixture_path(fixture, staged);
  expect_path(t, fixture, path);

  if (bin.build_only) {
    return SP_OK;
  }

  sp_ps_output_t output = run_fixture_bin(fixture, path);

  sp_test_kv(t, "command", path);
  sp_test_kv(t, "output", output.out);
  sp_expect_eq(t, bin.rc, output.status.exit_code);

  sp_carr_for(bin.contains, it) {
    if (!bin.contains[it]) {
      break;
    }
    sp_test_kv(t, "needle", sp_str_view(bin.contains[it]));
    sp_expect(t, sp_str_contains(output.out, sp_str_view(bin.contains[it])));
  }
  return SP_OK;
}

static void expect_command_file(sp_test_t* t, fixture_t* fixture, command_file_t expected) {
  sp_str_t path = fixture_path(fixture, expected.file);
  sp_str_t content = test_read_file(fixture->mem, path);
  if (expected.content) {
    sp_test_kv(t, "path", path);
    sp_test_kv(t, "expected", sp_str_view(expected.content));
    sp_test_kv(t, "content", content);
    sp_expect(t, sp_str_equal(content, sp_str_view(expected.content)));
  }
  sp_carr_for(expected.contains, it) {
    if (!expected.contains[it]) {
      break;
    }
    sp_test_kv(t, "path", path);
    sp_test_kv(t, "needle", sp_str_view(expected.contains[it]));
    sp_test_kv(t, "content", content);
    sp_expect(t, sp_str_contains(content, sp_str_view(expected.contains[it])));
  }
  sp_carr_for(expected.excludes, it) {
    if (!expected.excludes[it]) {
      break;
    }
    sp_test_kv(t, "path", path);
    sp_test_kv(t, "needle", sp_str_view(expected.excludes[it]));
    sp_test_kv(t, "content", content);
    sp_expect(t, !sp_str_contains(content, sp_str_view(expected.excludes[it])));
  }
  if (expected.json) {
    yyjson_doc* doc = yyjson_read(content.data, content.len, 0);
    sp_test_kv(t, "path", path);
    sp_test_kv(t, "content", content);
    sp_expect(t, doc != SP_NULLPTR);
    yyjson_doc_free(doc);
  }
}

static void expect_command_lock(sp_test_t* t, fixture_t* fixture, command_expect_t expected) {
  sp_str_t path = fixture_path(fixture, sp_str_lit("spn.lock"));
  expect_path(t, fixture, path);
  sp_str_t lock = test_read_file(fixture->mem, path);
  if (expected.lock) {
    sp_test_kv(t, "lock", lock);
    sp_expect(t, sp_str_contains(lock, sp_str_lit("[[dep]]")));
  }
  sp_carr_for(expected.packages, it) {
    if (!expected.packages[it]) {
      break;
    }
    sp_str_t needle = sp_fmt(fixture->mem, "name = \"{}\"", sp_fmt_cstr(expected.packages[it])).value;
    sp_test_kv(t, "needle", needle);
    sp_test_kv(t, "lock", lock);
    sp_expect(t, sp_str_contains(lock, needle));
  }
}

sp_err_t run_command(sp_test_t* t, fixture_t* fixture, command_test_t test) {
  if (test.project) {
    sp_try(prepare_test(t, fixture, test.project, test.copy));
  }
  sp_ps_output_t output = run_spn_command(t, fixture, test.args, test.env);
  sp_expect_eq(t, test.expect.rc, output.status.exit_code);

  sp_carr_for(test.expect.contains, it) {
    if (!test.expect.contains[it]) {
      break;
    }
    sp_test_kv(t, "needle", sp_str_view(test.expect.contains[it]));
    sp_test_kv(t, "output", output.out);
    sp_expect(t, sp_str_contains(output.out, sp_str_view(test.expect.contains[it])));
  }

  sp_carr_for(test.expect.excludes, it) {
    if (!test.expect.excludes[it]) {
      break;
    }
    sp_test_kv(t, "needle", sp_str_view(test.expect.excludes[it]));
    sp_test_kv(t, "output", output.out);
    sp_expect(t, !sp_str_contains(output.out, sp_str_view(test.expect.excludes[it])));
  }

  sp_carr_for(test.expect.files, it) {
    command_file_t expected = test.expect.files[it];
    if (sp_str_empty(expected.file)) {
      break;
    }
    expect_command_file(t, fixture, expected);
  }

  sp_carr_for(test.expect.events, it) {
    command_event_t expected = test.expect.events[it];
    if (!expected.event) {
      break;
    }
    expect_event(t, fixture, expected.event, expected.key, expected.value, !expected.absent, __FILE__, __LINE__);
  }

  sp_carr_for(test.expect.cc, it) {
    if (!test.expect.cc[it].args[0]) {
      break;
    }
    expect_command_cc(t, fixture, test.expect.cc[it]);
  }

  sp_carr_for(test.expect.exists, it) {
    if (sp_str_empty(test.expect.exists[it])) {
      break;
    }
    sp_str_t path = fixture_path(fixture, test.expect.exists[it]);
    expect_path(t, fixture, path);
  }

  sp_carr_for(test.expect.missing, it) {
    if (sp_str_empty(test.expect.missing[it])) {
      break;
    }
    sp_str_t path = fixture_path(fixture, test.expect.missing[it]);
    expect_no_path(t, fixture, path);
  }

  if (test.expect.lock || test.expect.packages[0]) {
    expect_command_lock(t, fixture, test.expect);
  }

  if (test.expect.bin.name || !sp_str_empty(test.expect.bin.path)) {
    sp_try(run_command_bin(t, fixture, test.expect.bin));
  }
  return SP_OK;
}

sp_err_t run_command_test(sp_test_t* t, command_test_t test) {
  fixture_t fixture = sp_zero;
  sp_try(fixture_init(t, &fixture));
  if (!test.project) {
    sp_try(prepare_test(t, &fixture, SP_NULLPTR, SP_NULLPTR));
  }
  return run_command(t, &fixture, test);
}

static sp_err_t apply_rebuild_change(sp_test_t* t, fixture_t* fixture, rebuild_change_t change) {
  sp_carr_for(change.remove_files, it) {
    if (sp_str_empty(change.remove_files[it])) {
      break;
    }
    sp_str_t path = fixture_path(fixture, change.remove_files[it]);
    sp_fs_remove_file(path);
    expect_no_path(t, fixture, path);
  }

  sp_carr_for(change.moves, it) {
    if (sp_str_empty(change.moves[it].from)) {
      break;
    }
    sp_str_t from = fixture_path(fixture, change.moves[it].from);
    sp_str_t to = fixture_path(fixture, change.moves[it].to);
    sp_str_t content = test_read_file(fixture->mem, from);
    fixture_create(fixture, change.moves[it].to, content);
    sp_fs_remove_file(from);
    expect_no_path(t, fixture, from);
    expect_path(t, fixture, to);
  }

  sp_carr_for(change.writes, it) {
    if (sp_str_empty(change.writes[it].file)) {
      break;
    }
    fixture_create(fixture, change.writes[it].file, change.writes[it].content);
  }

  sp_carr_for(change.remove_dirs, it) {
    if (sp_str_empty(change.remove_dirs[it])) {
      break;
    }
    sp_str_t path = fixture_path(fixture, change.remove_dirs[it]);
    sp_fs_remove_dir(path);
    expect_no_path(t, fixture, path);
  }
  return SP_OK;
}

sp_err_t run_rebuild_test(sp_test_t* t, rebuild_test_t test) {
  fixture_t fixture = sp_zero;
  sp_try(fixture_init(t, &fixture));

  sp_try(prepare_test(t, &fixture, test.project, test.copy));
  sp_try(run_command(t, &fixture, test.first));

  sp_tm_epoch_t mtimes[SPN_TEST_REBUILD_MAX_WATCHES] = sp_zero;
  sp_carr_for(test.watches, it) {
    if (sp_str_empty(test.watches[it].file)) {
      break;
    }
    sp_str_t path = fixture_path(&fixture, test.watches[it].file);
    expect_path(t, &fixture, path);
    mtimes[it] = sp_fs_get_mod_time(path);
  }

  sp_carr_for(test.rebuilds, it) {
    if (!test.rebuilds[it].command.args[0]) {
      break;
    }
    sp_try(apply_rebuild_change(t, &fixture, test.rebuilds[it].change));
    sp_try(run_command(t, &fixture, test.rebuilds[it].command));
  }

  sp_carr_for(test.watches, it) {
    rebuild_watch_t watch = test.watches[it];
    if (sp_str_empty(watch.file)) {
      break;
    }
    sp_str_t path = fixture_path(&fixture, watch.file);
    sp_tm_epoch_t now = sp_fs_get_mod_time(path);
    bool unchanged = mtimes[it].s == now.s && mtimes[it].ns == now.ns;
    if (watch.mtime == REBUILD_MTIME_UNCHANGED) {
      sp_expect(t, unchanged);
    }
    if (watch.mtime == REBUILD_MTIME_CHANGED) {
      sp_expect(t, !unchanged);
    }
  }
  return SP_OK;
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

sp_err_t run_actions(sp_test_t* t, fixture_t* fixture, const action_t* actions) {
  sp_mem_t mem = fixture->mem;

  sp_for(it, SPN_TEST_MAX_ACTIONS) {
    action_t action = actions[it];
    if (action.kind == ACTION_NONE) {
      break;
    }

    sp_test_kv_clear(t, SP_NULLPTR);

    switch (action.kind) {
      case ACTION_NONE: {
        break;
      }
      case ACTION_CREATE_FILE: {
        fixture_create(fixture, action.create.file, action.create.content);
        break;
      }
      case ACTION_RUN_BIN:
      case ACTION_RUN_TEST: {
        sp_str_t staged_bin = action.kind == ACTION_RUN_TEST ? test_exe(action.bin.name) : exe(action.bin.name);
        sp_str_t bin = fixture_path(fixture, staged_bin);
        expect_path(t, fixture, bin);

        sp_ps_output_t output = run_fixture_bin(fixture, bin);

        sp_test_kv(t, "command", bin);
        sp_test_kv(t, "output", output.out);
        sp_expect_eq(t, action.bin.rc, output.status.exit_code);
        break;
      }
      case ACTION_VERIFY_EXISTS: {
        sp_str_t path = fixture_path(fixture, action.exists);
        expect_path(t, fixture, path);
        break;
      }
      case ACTION_VERIFY_NOT_EXISTS: {
        sp_str_t path = fixture_path(fixture, action.exists);
        expect_no_path(t, fixture, path);
        break;
      }
      case ACTION_VERIFY_DIR_COUNT: {
        sp_str_t path = fixture_path(fixture, sp_str_view(action.verify_dir_count.dir));
        sp_da(sp_fs_entry_t) entries = sp_fs_collect(mem, path);
        u32 dirs = 0;
        sp_da_for(entries, et) {
          if (entries[et].kind == SP_FS_KIND_DIR) {
            dirs++;
          }
        }
        sp_test_kv(t, "path", path);
        sp_expect_eq(t, action.verify_dir_count.count, dirs);
        break;
      }
      case ACTION_VERIFY_INCLUDE: {
        sp_str_t path = fixture_path(
          fixture,
          sp_fs_join_path(mem, store_file("include"), action.verify_include.file)
        );
        expect_path(t, fixture, path);
        break;
      }
      case ACTION_VERIFY_FILE_CONTAINS: {
        sp_str_t path = fixture_path(fixture, action.verify_file_contains.file);
        sp_str_t content = test_read_file(mem, path);
        sp_test_kv(t, "path", path);
        sp_test_kv(t, "needle", action.verify_file_contains.needle);
        sp_test_kv(t, "content", content);
        sp_expect(t, sp_str_contains(content, action.verify_file_contains.needle));
        break;
      }
      case ACTION_VERIFY_FILE_NOT_CONTAINS: {
        sp_str_t path = fixture_path(fixture, action.verify_file_not_contains.file);
        sp_str_t content = test_read_file(mem, path);
        sp_test_kv(t, "path", path);
        sp_test_kv(t, "needle", action.verify_file_not_contains.needle);
        sp_test_kv(t, "content", content);
        sp_expect(t, !sp_str_contains(content, action.verify_file_not_contains.needle));
        break;
      }
      case ACTION_REMOVE_DIR: {
        sp_str_t path = fixture_path(fixture, sp_str_view(action.rm.dir));
        sp_fs_remove_dir(path);
        expect_no_path(t, fixture, path);
        break;
      }
      case ACTION_RUN_CLI: {
        const c8* args[SPN_TEST_COMMAND_MAX_ARGS] = { action.cli.cmd };
        sp_carr_for(action.cli.args, it) {
          if (!action.cli.args[it]) {
            break;
          }
          args[it + 1] = action.cli.args[it];
        }
        sp_ps_output_t output = run_spn_command(t, fixture, args, action.cli.env);
        sp_expect_eq(t, action.cli.rc, output.status.exit_code);
        break;
      }
      case ACTION_VERIFY_CC_ARG:
      case ACTION_VERIFY_NO_CC_ARG: {
        expect_cc_arg(t, fixture, &action, __FILE__, __LINE__);
        break;
      }
      case ACTION_VERIFY_EVENT:
      case ACTION_VERIFY_NO_EVENT: {
        expect_event(t, fixture, action.verify_event.event, action.verify_event.key, action.verify_event.value, action.kind == ACTION_VERIFY_EVENT, __FILE__, __LINE__);
        break;
      }
      case ACTION_VERIFY_RESULT: {
        expect_result(t, fixture, action.verify_result.err, __FILE__, __LINE__);
        break;
      }
      case ACTION_VERIFY_EVENT_COUNT: {
        u32 count = count_events(fixture, action.verify_event_count.event, action.verify_event_count.key, action.verify_event_count.value);
        sp_test_kv(t, "event", sp_str_view(event_names[action.verify_event_count.event]));
        sp_expect_eq(t, action.verify_event_count.count, count);
        break;
      }
      case ACTION_VERIFY_LOCKED: {
        sp_str_t path = fixture_path(fixture, sp_str_lit("spn.lock"));
        expect_path(t, fixture, path);

        sp_str_t lock = test_read_file(mem, path);
        sp_test_kv(t, "lock", lock);
        sp_expect(t, sp_str_contains(lock, sp_str_lit("[[dep]]")));
        break;
      }
      case ACTION_VERIFY_PKG_LOCKED: {
        sp_str_t path = fixture_path(fixture, sp_str_lit("spn.lock"));
        expect_path(t, fixture, path);

        sp_str_t lock = test_read_file(mem, path);
        sp_str_t needle = sp_fmt(mem, "name = \"{}\"", sp_fmt_cstr(action.verify_locked.name)).value;
        sp_test_kv(t, "needle", needle);
        sp_test_kv(t, "lock", lock);
        sp_expect(t, sp_str_contains(lock, needle));
        break;
      }
    }
  }
  return SP_OK;
}

static sp_str_t pick_shared_toolchain_dir(sp_mem_t mem, sp_str_t root) {
  sp_str_t global = sp_fs_join_path(mem, sp_fs_get_storage_path(mem), sp_str_lit("spn/cache/toolchain"));
  if (sp_fs_exists(global)) return global;
  return sp_fs_join_path(mem, root, sp_str_lit(".cache/toolchain"));
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
  setup_fixture_envrc(fixture, fixture->paths.storage, fixture->paths.toolchain, fixture->paths.config);
  setup_fixture_config(fixture, fixture->paths.config, fixture->paths.index, fixture->paths.root);

  sp_fs_copy(sp_fs_join_path(mem, fixture->paths.root, sp_str_lit("include/spn.h")), fixture->paths.include);

  if (project) {
    sp_str_t path = sp_fs_join_path(mem, fixture->paths.root, sp_str_view(project));
    sp_try(fixture_copy_project(t, fixture, path, copy));
    sp_try(setup_fixture_index_from_remote(t, fixture, path));
    sp_try(setup_fixture_source_repos(t, fixture, path));
  }
  return SP_OK;
}

sp_err_t run_test(sp_test_t* t, test_t test) {
  fixture_t fixture = sp_zero;
  sp_try(fixture_init(t, &fixture));

  sp_str_t blocked = test_when_blocked(&test.when);
  if (blocked.len) {
    return sp_test_skip(t, "{}", sp_fmt_str(blocked));
  }

  if (!test_when_runs(&test.when)) {
    u32 kept = 0;
    sp_carr_for(test.actions, it) {
      action_t action = test.actions[it];
      if (action.kind == ACTION_RUN_BIN || action.kind == ACTION_RUN_TEST) {
        continue;
      }
      test.actions[kept++] = action;
    }
    while (kept < SPN_TEST_MAX_ACTIONS) {
      test.actions[kept++] = (action_t) { .kind = ACTION_NONE };
    }
  }

  sp_try(prepare_test(t, &fixture, test.project, test.copy));
  return run_actions(t, &fixture, test.actions);
}
