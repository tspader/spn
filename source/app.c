#include "app.h"
#include "event.h"
#include "resolve.h"
#include "external/cc.h"
#include "sp.h"
#include "sp/ht.h"
#include "sp/str.h"

void spn_app_update_lock_file(spn_app_t* app) {
  spn_lock_file_t lock = spn_build_lock_file();

  // Add top-level package's system_deps to lock
  sp_da_for(app->package.system_deps, i) {
    sp_ht_insert(lock.system_deps, sp_str_copy(app->package.system_deps[i]), true);
  }

  sp_da(sp_str_t) keys = SP_NULLPTR;
  sp_ht_collect_keys(lock.entries, keys);
  sp_dyn_array_sort(keys, sp_str_sort_kernel_alphabetical);

  spn_toml_writer_t toml = spn_toml_writer_new();

  spn_toml_begin_table_cstr(&toml, "spn");
  spn_toml_append_str_cstr(&toml, "version", sp_str_lit(SPN_VERSION));
  spn_toml_append_str_cstr(&toml, "commit", sp_str_lit(SPN_COMMIT));
  spn_toml_end_table(&toml);

  // Write [package] table with system_deps
  if (sp_ht_size(lock.system_deps)) {
    spn_toml_begin_table_cstr(&toml, "package");
    sp_da(sp_str_t) sys_deps = SP_NULLPTR;
    sp_ht_collect_keys(lock.system_deps, sys_deps);
    sp_dyn_array_sort(sys_deps, sp_str_sort_kernel_alphabetical);
    spn_toml_append_str_array_cstr(&toml, "system_deps", sys_deps);
    spn_toml_end_table(&toml);
  }

  spn_toml_begin_array_cstr(&toml, "dep");
  sp_dyn_array_for(keys, it) {
    spn_lock_entry_t* entry = sp_ht_getp(lock.entries, keys[it]);

    spn_toml_append_array_table(&toml);
    spn_toml_append_str_cstr(&toml, "name", entry->name);
    spn_toml_append_str_cstr(&toml, "version", spn_semver_to_str(entry->version));
    spn_toml_append_str_cstr(&toml, "commit", entry->commit);
    spn_toml_append_str_cstr(&toml, "kind", spn_package_kind_to_str(entry->kind));
    spn_toml_append_str_cstr(&toml, "visibility", spn_visibility_to_str(entry->visibility));

    if (sp_dyn_array_size(entry->deps)) {
      spn_toml_append_str_array_cstr(&toml, "deps", entry->deps);
    }
  }
  spn_toml_end_array(&toml);

  sp_str_t output = spn_toml_writer_write(&toml);
  sp_io_writer_t file = sp_io_writer_from_file(app->paths.lock, SP_IO_WRITE_MODE_OVERWRITE);
  sp_io_write_str(&file, output);
  sp_io_writer_close(&file);
}

void spn_app_write_manifest(spn_pkg_t* pkg, sp_str_t path) {
  spn_toml_writer_t toml = spn_toml_writer_new();

  spn_toml_begin_table_cstr(&toml, "package");
  spn_toml_append_str_cstr(&toml, "name", pkg->name);
  spn_toml_append_str_cstr(&toml, "version", spn_semver_to_str(pkg->version));
  if (!sp_str_empty(pkg->url)) {
    spn_toml_append_str_cstr(&toml, "url", pkg->url);
  }
  if (!sp_str_empty(pkg->author)) {
    spn_toml_append_str_cstr(&toml, "author", pkg->author);
  }
  if (!sp_str_empty(pkg->maintainer)) {
    spn_toml_append_str_cstr(&toml, "maintainer", pkg->maintainer);
  }
  if (!sp_dyn_array_empty(pkg->include)) {
    spn_toml_append_str_array_cstr(&toml, "include", pkg->include);
  }
  if (!sp_dyn_array_empty(pkg->system_deps)) {
    spn_toml_append_str_array_cstr(&toml, "system_deps", pkg->system_deps);
  }
  if (!sp_dyn_array_empty(pkg->define)) {
    spn_toml_append_str_array_cstr(&toml, "define", pkg->define);
  }
  spn_toml_end_table(&toml);

  if (sp_ht_size(pkg->deps)) {
    // Write package deps
    bool has_package_deps = false;
    sp_ht_for_kv(pkg->deps, it) {
      if (it.val->visibility == SPN_VISIBILITY_PUBLIC) {
        has_package_deps = true;
        break;
      }
    }
    if (has_package_deps) {
      spn_toml_begin_table_cstr(&toml, "deps.package");
      sp_ht_for_kv(pkg->deps, it) {
        if (it.val->visibility == SPN_VISIBILITY_PUBLIC) {
          spn_toml_append_str(&toml, *it.key, spn_pkg_req_to_str(*it.val));
        }
      }
      spn_toml_end_table(&toml);
    }

    // Write build deps
    bool has_build_deps = false;
    sp_ht_for_kv(pkg->deps, it) {
      if (it.val->visibility == SPN_VISIBILITY_BUILD) {
        has_build_deps = true;
        break;
      }
    }
    if (has_build_deps) {
      spn_toml_begin_table_cstr(&toml, "deps.build");
      sp_ht_for_kv(pkg->deps, it) {
        if (it.val->visibility == SPN_VISIBILITY_BUILD) {
          spn_toml_append_str(&toml, *it.key, spn_pkg_req_to_str(*it.val));
        }
      }
      spn_toml_end_table(&toml);
    }

    // Write test deps
    bool has_test_deps = false;
    sp_ht_for_kv(pkg->deps, it) {
      if (it.val->visibility == SPN_VISIBILITY_TEST) {
        has_test_deps = true;
        break;
      }
    }
    if (has_test_deps) {
      spn_toml_begin_table_cstr(&toml, "deps.test");
      sp_ht_for_kv(pkg->deps, it) {
        if (it.val->visibility == SPN_VISIBILITY_TEST) {
          spn_toml_append_str(&toml, *it.key, spn_pkg_req_to_str(*it.val));
        }
      }
      spn_toml_end_table(&toml);
    }
  }

  if (!sp_om_empty(pkg->profiles)) {
    spn_toml_begin_array_cstr(&toml, "profile");
    sp_om_for(pkg->profiles, it) {
      spn_profile_t* profile = sp_om_at(pkg->profiles, it);
      if (profile->kind != SPN_PROFILE_BUILTIN) {
        spn_toml_append_array_table(&toml);
        spn_toml_append_str_cstr(&toml, "name", profile->name);
        spn_toml_append_str_cstr(&toml, "cc", profile->cc.exe);
        spn_toml_append_str_cstr(&toml, "linkage", spn_pkg_linkage_to_str(profile->linkage));
        spn_toml_append_str_cstr(&toml, "libc", spn_libc_kind_to_str(profile->libc));
        spn_toml_append_str_cstr(&toml, "standard", spn_c_standard_to_str(profile->standard));
        spn_toml_append_str_cstr(&toml, "mode", spn_dep_build_mode_to_str(profile->mode));
      }
    }
    spn_toml_end_array(&toml);
  }

  if (!sp_om_empty(pkg->libs)) {
    spn_toml_begin_array_cstr(&toml, "lib");
    sp_om_for(pkg->libs, it) {
      spn_target_t* lib = sp_om_at(pkg->libs, it);
      spn_toml_append_array_table(&toml);
      spn_toml_append_str_cstr(&toml, "name", lib->name);

      spn_linkage_t linkage = spn_target_kind_to_pkg_linkage(lib->kind);
      spn_toml_append_str_cstr(&toml, "kind", spn_pkg_linkage_to_str(linkage));

      if (sp_dyn_array_size(lib->source)) {
        spn_toml_append_str_array_cstr(&toml, "source", lib->source);
      }
      if (sp_dyn_array_size(lib->include)) {
        spn_toml_append_str_array_cstr(&toml, "include", lib->include);
      }
      if (sp_dyn_array_size(lib->define)) {
        spn_toml_append_str_array_cstr(&toml, "define", lib->define);
      }
    }
    spn_toml_end_array(&toml);
  }

  if (!sp_om_empty(pkg->exes)) {
    spn_toml_begin_array_cstr(&toml, "bin");
    sp_om_for(pkg->exes, it) {
      spn_target_t* bin = sp_om_at(pkg->exes, it);
      spn_toml_append_array_table(&toml);
      spn_toml_append_str_cstr(&toml, "name", bin->name);

      if (bin->visibility != SPN_VISIBILITY_PUBLIC) {
        spn_toml_append_str_cstr(&toml, "kind", spn_visibility_to_str(bin->visibility));
      }
      if (sp_dyn_array_size(bin->source)) {
        spn_toml_append_str_array_cstr(&toml, "source", bin->source);
      }
      if (sp_dyn_array_size(bin->include)) {
        spn_toml_append_str_array_cstr(&toml, "include", bin->include);
      }
      if (sp_dyn_array_size(bin->define)) {
        spn_toml_append_str_array_cstr(&toml, "define", bin->define);
      }
    }
    spn_toml_end_array(&toml);
  }

  if (!sp_om_empty(pkg->tests)) {
    spn_toml_begin_array_cstr(&toml, "test");
    sp_om_for(pkg->tests, it) {
      spn_target_t* test = sp_om_at(pkg->tests, it);

      spn_toml_append_array_table(&toml);
      spn_toml_append_str_cstr(&toml, "name", test->name);

      if (sp_dyn_array_size(test->source)) {
        spn_toml_append_str_array_cstr(&toml, "source", test->source);
      }
      if (sp_dyn_array_size(test->include)) {
        spn_toml_append_str_array_cstr(&toml, "include", test->include);
      }
      if (sp_dyn_array_size(test->define)) {
        spn_toml_append_str_array_cstr(&toml, "define", test->define);
      }
    }
    spn_toml_end_array(&toml);
  }

  if (sp_ht_size(pkg->options)) {
    spn_toml_begin_table_cstr(&toml, "options");
    sp_ht_for_kv(pkg->options, it) {
      spn_toml_append_option(&toml, *it.key, *it.val);
    }
    spn_toml_end_table(&toml);
  }

  if (sp_ht_size(pkg->config)) {
    spn_toml_begin_table_cstr(&toml, "config");

    sp_ht_for_kv(pkg->config, it) {
      spn_toml_begin_table(&toml, *it.key);
      sp_ht_for_kv(*it.val, n) {
        spn_toml_append_option(&toml, *n.key, *n.val);
      }
      spn_toml_end_table(&toml);
    }

    spn_toml_end_table(&toml);
  }

  if (!sp_om_empty(pkg->indexes)) {
    spn_toml_begin_array_cstr(&toml, "index");
    sp_om_for(pkg->indexes, it) {
      spn_index_t* index = sp_om_at(pkg->indexes, it);

      spn_toml_append_array_table(&toml);

      spn_toml_append_str_cstr(&toml, "name", index->name);
      spn_toml_append_str_cstr(&toml, "location", index->location);
    }
    spn_toml_end_array(&toml);
  }

  sp_str_t output = spn_toml_writer_write(&toml);
  output = sp_str_trim_right(output);
  sp_io_writer_t file = sp_io_writer_from_file(path, SP_IO_WRITE_MODE_OVERWRITE);
  sp_io_write_str(&file, output);
  sp_io_writer_close(&file);
}

spn_pkg_t* spn_app_find_package(spn_app_t* app, sp_str_t name) {
  return sp_om_get(app->cache, name);
}

spn_pkg_t* spn_app_find_package_from_request(spn_app_t* app, spn_pkg_req_t request) {
  spn_pkg_t* package = spn_app_find_package(app, request.name);
  if (package->kind != request.kind) {
    return SP_NULLPTR;
  }

  return package;
}

spn_pkg_t* spn_app_ensure_package(spn_app_t* app, spn_pkg_req_t request) {
  sp_str_t name = spn_intern(request.name);

  if (!sp_om_has(app->cache, name)) {
    sp_om_insert(app->cache, name, SP_ZERO_STRUCT(spn_pkg_t));
    spn_pkg_t* pkg = sp_om_get(app->cache, name);
    spn_pkg_init(pkg, name);

    switch (request.kind) {
      case SPN_PACKAGE_KIND_FILE: {
        sp_str_t prefix = sp_str_lit("file://");
        sp_str_t manifest = {
          .data = request.file.data + prefix.len,
          .len = request.file.len - prefix.len
        };
        spn_pkg_from_manifest(pkg, manifest);

        break;
      }
      case SPN_PACKAGE_KIND_INDEX: {
        sp_str_t* path = sp_str_ht_get(app->registry, name);
        if (!path) return SP_NULLPTR;

        spn_pkg_from_index(pkg, *path);

        break;
      }
      case SPN_PACKAGE_KIND_ROOT:
      case SPN_PACKAGE_KIND_WORKSPACE: {
        SP_FATAL("unimplemented find_package");
        break;
      }
      case SPN_PACKAGE_KIND_NONE: {
        SP_UNREACHABLE_RETURN(SP_NULLPTR);
      }
    }
  }

  return spn_app_find_package_from_request(app, request);
}

void spn_app_init(spn_app_t* app) {
  app->resolver = sp_alloc_type(spn_resolver_t);
}

spn_app_t spn_app_init_and_write(sp_str_t path, sp_str_t name, spn_app_init_mode_t mode) {
  sp_str_t paths [] = {
    sp_fs_join_path(path, sp_str_lit("spn.toml")),
    sp_fs_join_path(path, sp_str_lit("spn.c")),
  };
  sp_carr_for(paths, it) {
    if (sp_fs_exists(paths[it])) {
      SP_FATAL("{:fg brightcyan} already exists; bailing", SP_FMT_STR(paths[it]));
    }
  }

  spn_app_t app = SP_ZERO_INITIALIZE();
  spn_app_init(&app);

  switch (mode) {
    case SPN_APP_INIT_NORMAL: {
      app.package = spn_pkg_from_default(path, name);

      sp_str_t main = sp_fs_join_path(path, sp_str_lit("main.c"));
      sp_io_writer_t io = sp_io_writer_from_file(main, SP_IO_WRITE_MODE_OVERWRITE);

      sp_str_t content = sp_str_lit(
        "#define SP_IMPLEMENTATION\n"
        "#include \"sp.h\"\n"
        "\n"
        "s32 main(s32 num_args, const c8** args) {\n"
        "  SP_LOG(\"hello, {:fg brightcyan}\", SP_FMT_CSTR(\"world\"));\n"
        "  SP_EXIT_SUCCESS();\n"
        "}\n"
      );

      if (sp_io_write_str(&io, content) != content.len) {
        SP_FATAL("Failed to write {:fg brightyellow}", SP_FMT_STR(main));
      }

      sp_io_writer_close(&io);

      spn_app_write_manifest(&app.package, app.package.paths.manifest);

      break;
    }
    case SPN_APP_INIT_BARE: {
      app.package = spn_pkg_new(name);
      spn_pkg_set_manifest(&app.package, sp_fs_join_path(path, SP_LIT("spn.toml")));
      spn_app_write_manifest(&app.package, app.package.paths.manifest);

      break;
    }
  }

  return app;
}

void spn_app_load(spn_app_t* app, sp_str_t manifest_path) {
  // Load the top level package
  if (sp_fs_exists(manifest_path)) {
    spn_pkg_from_manifest(&app->package, manifest_path);
  }

  app->paths.dir = app->package.paths.root;
  app->paths.lock = sp_fs_join_path(app->paths.dir, SP_LIT("spn.lock"));

  sp_dyn_array_for(spn.indexes, it) {
    spn_index_t* index = &spn.indexes[it];
    sp_dyn_array_push(app->search, spn_index_get_path(index));
  }

  sp_om_for(app->package.indexes, it) {
    spn_index_t* index = sp_om_at(app->package.indexes, it);
    sp_dyn_array_push(app->search, spn_index_get_path(index));
  }

  sp_dyn_array_for(app->search, i) {
    sp_str_t path = app->search[i];
    if (!sp_fs_exists(path)) continue;
    if (!sp_fs_is_dir(path)) {
      SP_FATAL(
        "{:fg brightcyan} is on the search path, but it's not a directory",
        SP_FMT_STR(path)
      );
    }

    sp_da(sp_os_dir_ent_t) entries = sp_fs_collect(path);
    sp_dyn_array_for(entries, i) {
      sp_os_dir_ent_t entry = entries[i];
      sp_str_t stem = sp_fs_get_stem(entry.file_path);
      sp_str_ht_insert(app->registry, stem, entry.file_path);
    }
  }

  // Load the lock file
  if (sp_fs_exists(app->paths.lock)) {
    sp_opt_set(app->lock, spn_lock_file_load(app->paths.lock));
  }

  // apply any defaults
  if (sp_om_empty(app->package.profiles)) {
    spn_profile_t profiles [] = {
      {
        .name = sp_str_lit("debug"),
        .linkage  = SPN_LIB_KIND_SHARED,
        .libc     = SPN_LIBC_GNU,
        .standard = SPN_C11,
        .mode     = SPN_DEP_BUILD_MODE_DEBUG,
        .kind     = SPN_PROFILE_BUILTIN,
        .cc = {
          .kind = SPN_CC_GCC,
          .exe = sp_str_lit("gcc")
        },
      },
      {
        .name     = sp_str_lit("release"),
        .linkage  = SPN_LIB_KIND_SHARED,
        .libc     = SPN_LIBC_GNU,
        .standard = SPN_C11,
        .mode     = SPN_DEP_BUILD_MODE_RELEASE,
        .kind     = SPN_PROFILE_BUILTIN,
        .cc = {
          .kind = SPN_CC_GCC,
          .exe = sp_str_lit("gcc")
        },
      }
    };
    sp_carr_for(profiles, it) {
      spn_pkg_add_profile_ex(&app->package, profiles[it]);
    }
  }
}
