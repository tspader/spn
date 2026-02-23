#include "pkg.h"

#include "cc.h"
#include "ctx.h"
#include "intern.h"
#include "registry.h"
#include "resolve.h"

sp_str_t spn_package_kind_to_str(spn_pkg_kind_t kind) {
  switch (kind) {
    SPN_PACKAGE_KIND(SP_X_NAMED_ENUM_CASE_TO_STRING_LOWER)
  }

  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

spn_pkg_kind_t spn_package_kind_from_str(sp_str_t str) {
  SPN_PACKAGE_KIND(SP_X_NAMED_ENUM_STR_TO_ENUM)
  SP_UNREACHABLE_RETURN(SPN_PACKAGE_KIND_NONE);
}

spn_pkg_dir_t spn_cache_dir_kind_from_str(sp_str_t str) {
  if (sp_str_equal_cstr(str, "")) {
    return SPN_DIR_STORE;
  }
  if (sp_str_equal_cstr(str, "cache")) {
    return SPN_DIR_CACHE;
  }
  if (sp_str_equal_cstr(str, "store")) {
    return SPN_DIR_STORE;
  }
  if (sp_str_equal_cstr(str, "include")) {
    return SPN_DIR_INCLUDE;
  }
  if (sp_str_equal_cstr(str, "vendor")) {
    return SPN_DIR_VENDOR;
  }
  if (sp_str_equal_cstr(str, "lib")) {
    return SPN_DIR_LIB;
  }
  if (sp_str_equal_cstr(str, "source")) {
    return SPN_DIR_SOURCE;
  }
  if (sp_str_equal_cstr(str, "work")) {
    return SPN_DIR_WORK;
  }
  if (sp_str_equal_cstr(str, "project")) {
    return SPN_DIR_PROJECT;
  }

  SP_FATAL("Unknown dir kind {:fg brightyellow}; options are [cache, store, include, vendor, lib, source, work]", SP_FMT_STR(str));
  SP_UNREACHABLE_RETURN(SPN_DIR_CACHE);
}

void spn_pkg_init(spn_pkg_t* pkg, sp_str_t name) {
  pkg->arena = sp_mem_arena_new(4096);
  pkg->name = spn_intern(name);
  pkg->paths.cache.source = sp_fs_join_path(spn_ctx_source_cache_root(), pkg->name);
  pkg->paths.cache.work = sp_fs_join_path(spn_ctx_build_cache_root(), pkg->name);
  pkg->paths.cache.store = sp_fs_join_path(spn_ctx_store_cache_root(), pkg->name);

  sp_ht_set_fns(pkg->deps, sp_ht_on_hash_str_key, sp_ht_on_compare_str_key);
  sp_ht_set_fns(pkg->options, sp_ht_on_hash_str_key, sp_ht_on_compare_str_key);
  sp_ht_set_fns(pkg->config, sp_ht_on_hash_str_key, sp_ht_on_compare_str_key);
}

void spn_pkg_set_index(spn_pkg_t* pkg, sp_str_t path) {
  sp_context_push_arena(pkg->arena);
  pkg->kind = SPN_PACKAGE_KIND_INDEX;
  pkg->paths.root = sp_str_copy(path);
  pkg->paths.manifest = sp_fs_join_path(pkg->paths.root, sp_str_lit("spn.toml"));
  sp_context_pop();
}

void spn_pkg_set_manifest(spn_pkg_t* pkg, sp_str_t path) {
  sp_context_push_arena(pkg->arena);
  pkg->kind = SPN_PACKAGE_KIND_WORKSPACE;
  pkg->paths.manifest = path;
  pkg->paths.root = sp_fs_parent_path(path);
  pkg->paths.script = sp_fs_join_path(pkg->paths.root, SP_LIT("spn.c"));
  pkg->paths.metadata = sp_fs_join_path(pkg->paths.root, SP_LIT("spn.meta.toml"));
  sp_context_pop();
}

spn_pkg_t spn_pkg_new(sp_str_t name) {
  spn_pkg_t pkg = SP_ZERO_INITIALIZE();
  spn_pkg_init(&pkg, name);
  return pkg;
}

static spn_pkg_t spn_pkg_from_bare_default(sp_str_t path, sp_str_t name) {
  spn_pkg_t pkg = spn_pkg_new(name);
  spn_pkg_set_manifest(&pkg, sp_fs_join_path(path, SP_LIT("spn.toml")));
  return pkg;
}

spn_pkg_t spn_pkg_from_default(sp_str_t path, sp_str_t name) {
  spn_pkg_t pkg = spn_pkg_from_bare_default(path, name);
  spn_pkg_add_version(&pkg, "0.1.0", "");
  spn_profile_t* profile = spn_pkg_add_profile(&pkg, "debug");
  spn_profile_set_cc(profile, SPN_CC_CLANG);
  spn_profile_set_standard(profile, SPN_C99);
  spn_profile_set_mode(profile, SPN_DEP_BUILD_MODE_DEBUG);

  return pkg;
}

void spn_pkg_from_index(spn_pkg_t* pkg, sp_str_t path) {
  SP_ASSERT(sp_fs_exists(path));
  SP_ASSERT(sp_fs_is_dir(path));

  spn_pkg_set_index(pkg, path);
  if (sp_fs_exists(pkg->paths.manifest)) {
    spn_pkg_from_manifest(pkg, pkg->paths.manifest);
  }
}

void spn_pkg_from_manifest(spn_pkg_t* pkg, sp_str_t manifest) {
  SP_ASSERT(sp_fs_exists(manifest));
  SP_ASSERT(!sp_fs_is_dir(manifest));

  spn_pkg_load(pkg, manifest);
  spn_pkg_set_manifest(pkg, manifest);
}

static void spn_pkg_load_deps(toml_table_t* toml, spn_pkg_t* package, spn_visibility_t visibility) {
  if (!toml) {
    return;
  }

  const c8* key = SP_NULLPTR;
  spn_toml_for(toml, n, key) {
    sp_str_t version = spn_toml_str(toml, key);
    spn_pkg_add_dep(package, sp_str_view(key), version, visibility);
  }
}

void spn_pkg_load(spn_pkg_t* pkg, sp_str_t manifest_path) {
  struct {
    toml_table_t* manifest;
    toml_table_t* package;
    toml_array_t* lib;
    toml_array_t* bin;
    toml_array_t* test;
    toml_table_t* deps;
    toml_array_t* profile;
    toml_array_t* registry;
    toml_table_t* options;
    toml_table_t* config;
  } toml = SP_ZERO_INITIALIZE();
  toml.manifest = spn_toml_parse(manifest_path);
  toml.package = toml_table_table(toml.manifest, "package");
  toml.lib = toml_table_array(toml.manifest, "lib");
  toml.bin = toml_table_array(toml.manifest, "bin");
  toml.test = toml_table_array(toml.manifest, "test");
  toml.profile = toml_table_array(toml.manifest, "profile");
  toml.registry = toml_table_array(toml.manifest, "registry");
  toml.deps = toml_table_table(toml.manifest, "deps");
  toml.options = toml_table_table(toml.manifest, "options");
  toml.config = toml_table_table(toml.manifest, "config");

  spn_pkg_init(pkg, spn_toml_str(toml.package, "name"));
  spn_pkg_set_url(pkg, spn_toml_cstr_opt(toml.package, "url", ""));
  spn_pkg_set_author(pkg, spn_toml_cstr_opt(toml.package, "author", ""));
  spn_pkg_set_maintainer(pkg, spn_toml_cstr_opt(toml.package, "maintainer", ""));

  const c8* version = spn_toml_cstr(toml.package, "version");
  const c8* commit = spn_toml_cstr_opt(toml.package, "commit", "");
  spn_pkg_add_version(pkg, version, commit);

  toml_array_t* include = toml_table_array(toml.package, "include");
  spn_toml_arr_for(include, it) {
    sp_str_t path = sp_fs_join_path(spn_ctx_project_root(), spn_toml_arr_str(include, it));
    spn_pkg_add_include_ex(pkg, path);
  }

  toml_array_t* define = toml_table_array(toml.package, "define");
  spn_toml_arr_for(define, it) {
    spn_pkg_add_define(pkg, spn_toml_arr_cstr(define, it));
  }

  toml_array_t* system_deps = toml_table_array(toml.package, "system_deps");
  spn_toml_arr_for(system_deps, it) {
    spn_pkg_add_system_dep(pkg, spn_toml_arr_cstr(system_deps, it));
  }

  spn_toml_arr_for(toml.lib, n) {
    toml_table_t* it = toml_array_table(toml.lib, n);
    sp_str_t kind_str = spn_toml_str_opt(it, "kind", "static");
    spn_linkage_t kind = spn_pkg_linkage_from_str(kind_str);
    spn_target_t* lib = spn_pkg_add_lib(pkg, spn_toml_cstr(it, "name"), kind);

    toml_array_t* source = toml_table_array(it, "source");
    spn_toml_arr_for(source, s) {
      spn_target_add_source_ex(lib, spn_toml_arr_str(source, s));
    }

    toml_array_t* include_arr = toml_table_array(it, "include");
    spn_toml_arr_for(include_arr, i) {
      spn_target_add_include_ex(lib, spn_toml_arr_str(include_arr, i));
    }

    toml_array_t* define_arr = toml_table_array(it, "define");
    spn_toml_arr_for(define_arr, d) {
      spn_target_add_define_ex(lib, spn_toml_arr_str(define_arr, d));
    }
  }

  spn_toml_arr_for(toml.profile, n) {
    toml_table_t* it = toml_array_table(toml.profile, n);

    spn_profile_t* profile = spn_pkg_add_profile(pkg, spn_toml_cstr(it, "name"));
    spn_profile_set_cc(profile, spn_cc_kind_from_str(spn_toml_str_opt(it, "cc", "gcc")));
    spn_profile_set_linkage(profile, spn_pkg_linkage_from_str(spn_toml_str_opt(it, "linkage", "shared")));
    spn_profile_set_libc(profile, spn_libc_kind_from_str(spn_toml_str_opt(it, "libc", "gnu")));
    spn_profile_set_standard(profile, spn_c_standard_from_str(spn_toml_str_opt(it, "standard", "c99")));
    spn_profile_set_mode(profile, spn_dep_build_mode_from_str(spn_toml_str_opt(it, "mode", "debug")));
  }

  spn_toml_arr_for(toml.registry, n) {
    toml_table_t* it = toml_array_table(toml.registry, n);
    spn_pkg_add_registry(pkg, spn_toml_cstr(it, "name"), spn_toml_cstr(it, "location"));
  }

  spn_toml_arr_for(toml.bin, n) {
    toml_table_t* it = toml_array_table(toml.bin, n);
    spn_target_t* bin = spn_pkg_add_exe(pkg, spn_toml_cstr(it, "name"));

    toml_array_t* source = toml_table_array(it, "source");
    spn_toml_arr_for(source, s) {
      spn_target_add_source_ex(bin, spn_toml_arr_str(source, s));
    }

    toml_array_t* include_arr = toml_table_array(it, "include");
    spn_toml_arr_for(include_arr, i) {
      spn_target_add_include_ex(bin, spn_toml_arr_str(include_arr, i));
    }

    toml_array_t* define_arr = toml_table_array(it, "define");
    spn_toml_arr_for(define_arr, d) {
      spn_target_add_define_ex(bin, spn_toml_arr_str(define_arr, d));
    }

    toml_value_t kind = toml_table_string(it, "kind");
    if (kind.ok) {
      spn_target_set_visibility(bin, spn_visibility_from_str(sp_str_view(kind.u.s)));
    }
  }

  spn_toml_arr_for(toml.test, n) {
    toml_table_t* it = toml_array_table(toml.test, n);
    spn_target_t* test = spn_pkg_add_test(pkg, spn_toml_cstr(it, "name"));

    toml_array_t* source = toml_table_array(it, "source");
    spn_toml_arr_for(source, s) {
      spn_target_add_source_ex(test, spn_toml_arr_str(source, s));
    }

    toml_array_t* include_arr = toml_table_array(it, "include");
    spn_toml_arr_for(include_arr, i) {
      spn_target_add_include_ex(test, spn_toml_arr_str(include_arr, i));
    }

    toml_array_t* define_arr = toml_table_array(it, "define");
    spn_toml_arr_for(define_arr, d) {
      spn_target_add_define_ex(test, spn_toml_arr_str(define_arr, d));
    }
  }

  if (toml.deps) {
    spn_pkg_load_deps(toml_table_table(toml.deps, "package"), pkg, SPN_VISIBILITY_PUBLIC);
    spn_pkg_load_deps(toml_table_table(toml.deps, "test"), pkg, SPN_VISIBILITY_TEST);
    spn_pkg_load_deps(toml_table_table(toml.deps, "build"), pkg, SPN_VISIBILITY_BUILD);
  }

  if (toml.options) {
    const c8* key = SP_NULLPTR;
    spn_toml_for(toml.options, n, key) {
      spn_dep_option_t option = spn_dep_option_from_toml(toml.options, key);
      sp_ht_insert(pkg->options, option.name, option);
    }
  }

  if (toml.config) {
    sp_da(toml_table_t*) configs = SP_ZERO_INITIALIZE();
    const c8* key = SP_NULLPTR;
    spn_toml_for(toml.config, n, key) {
      sp_da_push(configs, toml_table_table(toml.config, key));
    }

    sp_da_for(configs, it) {
      toml_table_t* config = configs[it];

      sp_str_t name = sp_str_from_cstr(config->key);

      spn_dep_options_t options = SP_NULLPTR;
      sp_ht_set_fns(options, sp_ht_on_hash_str_key, sp_ht_on_compare_str_key);

      const c8* option_key = SP_NULLPTR;
      spn_toml_for(config, n, option_key) {
        spn_dep_option_t option = spn_dep_option_from_toml(config, option_key);
        sp_ht_insert(options, option.name, option);
      }

      sp_ht_insert(pkg->config, name, options);
    }
  }
}

void spn_pkg_set_name(spn_pkg_t* pkg, const c8* name) {
  spn_pkg_set_name_ex(pkg, sp_str_view(name));
}

void spn_pkg_set_name_ex(spn_pkg_t* pkg, sp_str_t name) {
  pkg->name = spn_intern(name);
}

void spn_pkg_set_repo(spn_pkg_t* pkg, const c8* repo) {
  spn_pkg_set_repo_ex(pkg, sp_str_view(repo));
}

void spn_pkg_set_repo_ex(spn_pkg_t* pkg, sp_str_t repo) {
  sp_context_push_arena(pkg->arena);
  pkg->repo = sp_str_copy(repo);
  sp_context_pop();
}

void spn_pkg_set_url(spn_pkg_t* pkg, const c8* url) {
  spn_pkg_set_url_ex(pkg, sp_str_view(url));
}

void spn_pkg_set_url_ex(spn_pkg_t* pkg, sp_str_t url) {
  sp_context_push_arena(pkg->arena);
  pkg->url = sp_str_copy(url);
  sp_context_pop();
}

void spn_pkg_set_author(spn_pkg_t* pkg, const c8* author) {
  spn_pkg_set_author_ex(pkg, sp_str_view(author));
}

void spn_pkg_set_author_ex(spn_pkg_t* pkg, sp_str_t author) {
  sp_context_push_arena(pkg->arena);
  pkg->author = sp_str_copy(author);
  sp_context_pop();
}

void spn_pkg_set_maintainer(spn_pkg_t* pkg, const c8* maintainer) {
  spn_pkg_set_maintainer_ex(pkg, sp_str_view(maintainer));
}

void spn_pkg_set_maintainer_ex(spn_pkg_t* pkg, sp_str_t maintainer) {
  sp_context_push_arena(pkg->arena);
  pkg->maintainer = sp_str_copy(maintainer);
  sp_context_pop();
}

void spn_pkg_add_version(spn_pkg_t* pkg, const c8* version, const c8* commit) {
  spn_pkg_add_version_ex(pkg, spn_semver_from_str(sp_str_view(version)), sp_str_view(commit));
}

void spn_pkg_add_version_ex(spn_pkg_t* pkg, spn_semver_t version, sp_str_t commit) {
  if (spn_semver_is_empty(pkg->version)) {
    pkg->version = version;
  }

  sp_context_push_arena(pkg->arena);
  sp_ht_insert(pkg->metadata, version, ((spn_pkg_metadata_t) {
    .version = version,
    .commit = sp_str_copy(commit)
  }));
  sp_da_push(pkg->versions, version);
  sp_context_pop();
}

void spn_pkg_add_include(spn_pkg_t* pkg, const c8* include) {
  spn_pkg_add_include_ex(pkg, sp_str_view(include));
}

void spn_pkg_add_include_ex(spn_pkg_t* pkg, sp_str_t path) {
  sp_context_push_arena(pkg->arena);
  sp_da_push(pkg->include, sp_str_copy(path));
  sp_context_pop();
}

void spn_pkg_add_define(spn_pkg_t* pkg, const c8* define) {
  spn_pkg_add_define_ex(pkg, sp_str_view(define));
}

void spn_pkg_add_define_ex(spn_pkg_t* pkg, sp_str_t define) {
  sp_context_push_arena(pkg->arena);
  sp_da_push(pkg->define, sp_str_copy(define));
  sp_context_pop();
}

void spn_pkg_add_system_dep(spn_pkg_t* pkg, const c8* dep) {
  spn_pkg_add_system_dep_ex(pkg, sp_str_view(dep));
}

void spn_pkg_add_system_dep_ex(spn_pkg_t* pkg, sp_str_t dep) {
  sp_context_push_arena(pkg->arena);
  sp_da_push(pkg->system_deps, sp_str_copy(dep));
  sp_context_pop();
}

void spn_pkg_add_linkage(spn_pkg_t* pkg, spn_linkage_t linkage) {
  (void)pkg;
  (void)linkage;
}

void spn_pkg_add_dep_latest(spn_pkg_t* pkg, sp_str_t name, spn_visibility_t visibility) {
  spn_pkg_add_dep(pkg, name, sp_str_lit(""), visibility);
}

void spn_pkg_add_dep(spn_pkg_t* pkg, sp_str_t name, sp_str_t version, spn_visibility_t visibility) {
  sp_require(pkg);

  spn_pkg_req_t req = {
    .name = spn_intern(name),
    .visibility = visibility,
  };

  if (sp_str_empty(version)) {
    req.kind = SPN_PACKAGE_KIND_INDEX;

    spn_pkg_t* dep = spn_ctx_ensure_package(req);
    if (!dep) {
      spn_ctx_bail_on_missing_package(name);
    }

    if (sp_da_empty(dep->versions)) {
      SP_FATAL("{:fg brightcyan} has no known versions", SP_FMT_STR(dep->name));
    }

    spn_semver_parsed_t parsed = {
      .version = *sp_da_back(dep->versions),
      .components = { true, true, true }
    };
    req.range = spn_semver_caret_to_range(parsed);
  }
  else {
    req = spn_pkg_req_from_str(version);
    req.name = spn_intern(name);
    req.visibility = visibility;
  }

  sp_ht_insert(pkg->deps, req.name, req);
}

spn_profile_t* spn_pkg_add_profile(spn_pkg_t* pkg, const c8* name) {
  spn_profile_t profile = {
    .name = spn_intern_cstr(name),
    .cc.exe = spn_intern_cstr("gcc"),
    .cc.kind = SPN_CC_GCC,
    .linkage = SPN_LIB_KIND_SHARED,
    .libc = SPN_LIBC_GNU,
    .standard = SPN_C99,
    .mode = SPN_DEP_BUILD_MODE_DEBUG,
    .kind = SPN_PROFILE_USER,
  };

  return spn_pkg_add_profile_ex(pkg, profile);
}

spn_profile_t* spn_pkg_add_profile_ex(spn_pkg_t* pkg, spn_profile_t profile) {
  sp_om_insert(pkg->profiles, profile.name, profile);
  return sp_om_get(pkg->profiles, profile.name);
}

spn_target_t* spn_pkg_add_exe(spn_pkg_t* pkg, const c8* name) {
  return spn_pkg_add_exe_ex(pkg, spn_intern_cstr(name));
}

spn_target_t* spn_pkg_add_exe_ex(spn_pkg_t* pkg, sp_str_t name) {
  spn_target_t exe = {
    .name = spn_intern(name),
    .kind = SPN_TARGET_EXE,
    .pkg = pkg,
    .visibility = SPN_VISIBILITY_PUBLIC,
  };
  sp_om_insert(pkg->exes, exe.name, exe);
  return sp_om_get(pkg->exes, exe.name);
}

spn_target_t* spn_pkg_add_test(spn_pkg_t* pkg, const c8* name) {
  return spn_pkg_add_test_ex(pkg, spn_intern_cstr(name));
}

spn_target_t* spn_pkg_add_test_ex(spn_pkg_t* pkg, sp_str_t name) {
  spn_target_t test = {
    .name = spn_intern(name),
    .kind = SPN_TARGET_EXE,
    .pkg = pkg,
    .visibility = SPN_VISIBILITY_TEST,
  };
  sp_om_insert(pkg->tests, test.name, test);
  return sp_om_get(pkg->tests, test.name);
}

spn_target_t* spn_pkg_add_lib(spn_pkg_t* pkg, const c8* name, spn_linkage_t kind) {
  return spn_pkg_add_lib_ex(pkg, spn_intern_cstr(name), kind);
}

spn_target_t* spn_pkg_add_lib_ex(spn_pkg_t* pkg, sp_str_t name, spn_linkage_t kind) {
  spn_target_t lib = {
    .name = spn_intern(name),
    .kind = spn_pkg_linkage_to_target_kind(kind),
    .pkg = pkg,
    .visibility = SPN_VISIBILITY_PUBLIC,
  };
  sp_om_insert(pkg->libs, lib.name, lib);
  return sp_om_get(pkg->libs, lib.name);
}

bool spn_pkg_has_lib_kind(spn_pkg_t* pkg, spn_linkage_t kind) {
  if (kind == SPN_LIB_KIND_SOURCE) {
    return sp_om_empty(pkg->libs);
  }
  spn_target_kind_t target_kind = spn_pkg_linkage_to_target_kind(kind);
  sp_om_for(pkg->libs, it) {
    spn_target_t* lib = sp_om_at(pkg->libs, it);
    if (lib->kind == target_kind) {
      return true;
    }
  }
  return false;
}

spn_registry_t* spn_pkg_add_registry(spn_pkg_t* pkg, const c8* name, const c8* location) {
  return spn_pkg_add_registry_ex(pkg, spn_intern_cstr(name), spn_intern_cstr(location));
}

spn_registry_t* spn_pkg_add_registry_ex(spn_pkg_t* pkg, sp_str_t name, sp_str_t location) {
  spn_registry_t registry = {
    .name = spn_intern(name),
    .location = spn_intern(location),
    .kind = SPN_PACKAGE_KIND_WORKSPACE
  };
  sp_om_insert(pkg->registries, registry.name, registry);
  return sp_om_get(pkg->registries, registry.name);
}

sp_str_t spn_pkg_get_url(spn_pkg_t* pkg) {
  return pkg->url;
}

spn_target_t* spn_pkg_get_target(spn_pkg_t* pkg, const c8* name) {
  return spn_pkg_get_target_ex(pkg, sp_str_view(name));
}

spn_target_t* spn_pkg_get_target_ex(spn_pkg_t* pkg, sp_str_t name) {
  if (sp_om_has(pkg->exes, name)) return sp_om_get(pkg->exes, name);
  if (sp_om_has(pkg->tests, name)) return sp_om_get(pkg->tests, name);
  if (sp_om_has(pkg->libs, name)) return sp_om_get(pkg->libs, name);

  return SP_NULLPTR;
}

spn_profile_t* spn_pkg_get_default_profile(spn_pkg_t* pkg) {
  sp_om_for(pkg->profiles, it) {
    return sp_om_at(pkg->profiles, it);
  }

  sp_unreachable_return(SP_NULLPTR);
}

spn_profile_t* spn_pkg_get_profile_or_default(spn_pkg_t* pkg, sp_str_t name) {
  sp_require_as_null(!sp_om_empty(pkg->profiles));

  spn_profile_t* profile = sp_om_get(pkg->profiles, name);
  if (profile) {
    return profile;
  }

  return spn_pkg_get_default_profile(pkg);
}
