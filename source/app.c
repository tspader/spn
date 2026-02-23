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

  // Now that we know all the registries, discover all packages
  sp_dyn_array_push(app->search, spn_index_get_path(&spn.index));

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

spn_err_t spn_app_add_pkg_constraints(spn_app_t* app, spn_pkg_t* pkg) {
  spn_resolver_t* resolver = app->resolver;

  if (sp_ht_key_exists(resolver->visited, pkg->name)) {
    spn_push_event_ex((spn_build_event_t) {
      .kind = SPN_EVENT_ERR_CIRCULAR_DEP,
      .circular = {
        .pkg = pkg
      }
    });

    return SPN_ERROR;
  }

  // system deps
  sp_dyn_array_for(pkg->system_deps, i) {
    sp_str_t sys_dep = pkg->system_deps[i];
    bool found = false;
    sp_dyn_array_for(resolver->system_deps, j) {
      if (sp_str_equal(resolver->system_deps[j], sys_dep)) { found = true; break; }
    }
    if (!found) sp_dyn_array_push(resolver->system_deps, sys_dep);
  }

  // prevent circular deps by marking this dep until we're done with the subtree
  sp_str_ht_insert(resolver->visited, pkg->name, true);

  sp_ht_for_kv(pkg->deps, it) {
    spn_pkg_req_t request = *it.val;
    sp_require_as(!sp_str_empty(request.name), SPN_ERROR);

    spn_pkg_t* dep = spn_app_ensure_package(app, request);
    if (!dep) {
      spn_push_event_ex((spn_build_event_t) {
        .kind = SPN_EVENT_ERR_UNKNOWN_PKG,
        .unknown = {
          .request = request
        }
      });

      return SPN_ERROR;
    }

    // recurse
    sp_try(spn_app_add_pkg_constraints(app, dep));

    // add the dependency itself
    if (!sp_ht_key_exists(resolver->ranges, dep->name)) {
      sp_str_ht_insert(resolver->ranges, dep->name, SP_NULLPTR);
    }
    sp_da(spn_resolve_range_t)* ranges = sp_str_ht_get(resolver->ranges, dep->name);

    // collect the range of versions which satisfy the request
    spn_resolve_range_t range = {
      .source = request
    };

    switch (request.kind) {
      case SPN_PACKAGE_KIND_FILE: {
        u32 num_versions = sp_dyn_array_size(dep->versions);
        if (num_versions != 1) {
          SP_FATAL(
            "Local dependency {:fg brightcyan} has {} versions",
            SP_FMT_STR(dep->name),
            SP_FMT_U32(num_versions)
          );
        }
        sp_opt_set(range.low, 0);
        sp_opt_set(range.high, 0);

        break;
      }
      case SPN_PACKAGE_KIND_INDEX: {
        spn_semver_t low = request.range.low.version;
        spn_semver_t high = request.range.high.version;

        sp_dyn_array_for(dep->versions, n) {
          spn_semver_t version = dep->versions[n];

          if (!range.low.some) {
            if (spn_semver_satisfies(version, low, request.range.low.op)) {
              sp_opt_set(range.low, n);
            }
          }

          if (spn_semver_satisfies(version, high, request.range.high.op)) {
            sp_opt_set(range.high, n);
          }
        }

        break;
      }
      case SPN_PACKAGE_KIND_ROOT:
      case SPN_PACKAGE_KIND_WORKSPACE: {
        SP_BROKEN();
        break;
      }
      case SPN_PACKAGE_KIND_NONE: {
        SP_UNREACHABLE_CASE();
      }
    }

    sp_dyn_array_push(*ranges, range);
  }

  sp_str_ht_erase(resolver->visited, pkg->name);

  return SPN_OK;
}

void spn_app_resolve_from_lock_file(spn_app_t* app) {
  SP_ASSERT(app->lock.some);

  spn_lock_file_t* lock = &app->lock.value;
  sp_ht_for_kv(lock->entries, it) {
    spn_lock_entry_t* entry = it.val;

    spn_pkg_req_t request = {
      .name = *it.key,
      .kind = entry->kind,
      .visibility = entry->visibility,
    };

    if (request.kind == SPN_PACKAGE_KIND_INDEX) {
      request.range = (spn_semver_range_t) {
        .low = { .version = entry->version, .op = SPN_SEMVER_OP_EQ },
        .high = { .version = entry->version, .op = SPN_SEMVER_OP_EQ },
        .mod = SPN_SEMVER_MOD_CMP
      };
    }
    else if (request.kind == SPN_PACKAGE_KIND_FILE) {
      spn_pkg_req_t* dep = sp_ht_getp(app->package.deps, request.name);
      SP_ASSERT(dep);
      SP_ASSERT(dep->kind == SPN_PACKAGE_KIND_FILE);
      request.file = dep->file;
    }

    spn_pkg_t* pkg = spn_app_ensure_package(app, request);

    sp_str_ht_insert(app->resolver->resolved, entry->name, ((spn_resolved_pkg_t) {
      .pkg = pkg,
      .kind = request.kind,
      .version = entry->version
    }));
  }

  sp_ht_for_kv(lock->system_deps, it) {
    sp_da_push(app->resolver->system_deps, *it.key);
  }
}

spn_err_t spn_app_resolve_from_solver(spn_app_t* app) {
  sp_try(spn_app_add_pkg_constraints(app, &app->package));

  sp_str_ht_for_kv(app->resolver->ranges, it) {
    sp_str_t name = *it.key;
    sp_da(spn_resolve_range_t) ranges = *it.val;
    if (sp_da_empty(ranges)) {
      return SPN_ERROR;
    }

    spn_pkg_req_t req_low, req_high = SP_ZERO_INITIALIZE();
    u32 low = 0, high = SP_LIMIT_U32_MAX;
    sp_dyn_array_for(ranges, n) {
      spn_resolve_range_t range = ranges[n];
      SP_ASSERT(range.low.some);
      SP_ASSERT(range.high.some);

      if (sp_opt_get(range.low) >= low) {
        low = sp_opt_get(range.low);
        req_low = range.source;
      }
      if (sp_opt_get(range.high) <= high) {
        high = sp_opt_get(range.high);
        req_high = range.source;
      }
    }

    if (low > high) {
      sp_str_builder_t builder = SP_ZERO_INITIALIZE();
      sp_str_builder_append_fmt(&builder, "{:fg brightcyan} cannot be resolved:", SP_FMT_STR(name));
      sp_str_builder_indent(&builder);
      sp_str_builder_new_line(&builder);
      sp_str_builder_append_fmt(&builder, "{:fg brightcyan} requires {:fg brightred}", SP_FMT_STR(req_low.name), SP_FMT_STR(spn_semver_range_to_str(req_low.range)));
      sp_str_builder_new_line(&builder);
      sp_str_builder_append_fmt(&builder, "{:fg brightcyan} requires {:fg brightred}", SP_FMT_STR(req_high.name), SP_FMT_STR(spn_semver_range_to_str(req_high.range)));

      SP_FATAL("{}", SP_FMT_STR(sp_str_builder_to_str(&builder)));
    }


    spn_pkg_t* pkg = spn_app_ensure_package(app, req_high);
    sp_str_ht_insert(app->resolver->resolved, name, ((spn_resolved_pkg_t) {
      .pkg = pkg,
      .version = pkg->versions[high],
      .kind = req_high.kind,
    }));
  }

  return SPN_OK;
}

void spn_app_resolve(spn_app_t* app) {
  spn_resolver_init(app->resolver, &app->package, &app->cache);

  switch (app->lock.some) {
    case SP_OPT_SOME: {
      spn_event_buffer_push_ctx(spn.events, &spn_session_find_root(&app->session)->ctx, (spn_build_event_t) {
        .kind = SPN_EVENT_RESOLVE,
        .resolve = {
          .strategy = SPN_RESOLVE_STRATEGY_LOCK_FILE
        }
      });

      spn_app_resolve_from_lock_file(app);
      break;
    }
    case SP_OPT_NONE: {
      spn_event_buffer_push_ctx(spn.events, &spn_session_find_root(&app->session)->ctx, (spn_build_event_t) {
        .kind = SPN_EVENT_RESOLVE,
        .resolve = {
          .strategy = SPN_RESOLVE_STRATEGY_SOLVER
        }
      });

      spn_app_resolve_from_solver(app);
      break;
    }
  }
}
