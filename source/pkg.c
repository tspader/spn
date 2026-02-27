#include "pkg.h"

#include "app.h"
#include "ctx.h"
#include "event.h"
#include "intern.h"
#include "index.h"
#include "resolve.h"
#include "external/cc.h"
#include "sp/str.h"

extern spn_app_t app;

static spn_err_t spn_pkg_push_manifest_field_err(spn_pkg_t* pkg, sp_str_t path, sp_str_t expected, sp_str_t actual) {
  spn_event_buffer_push_ex(spn.events, pkg, SP_NULLPTR, (spn_build_event_t) {
    .kind = SPN_EVENT_ERR,
    .err = {
      .code = SPN_ERROR,
      .kind = SPN_ERR_KIND_MANIFEST_FIELD,
      .manifest_field = {
        .path = path,
        .expected = expected,
        .actual = actual,
      },
    },
  });

  return SPN_ERROR;
}

static spn_toml_value_kind_t spn_pkg_toml_table_value_kind(toml_table_t* table, const c8* key) {
  if (toml_table_array(table, key)) {
    return SPN_TOML_VALUE_KIND_ARRAY;
  }

  if (toml_table_table(table, key)) {
    return SPN_TOML_VALUE_KIND_TABLE;
  }

  if (toml_table_unparsed(table, key)) {
    return SPN_TOML_VALUE_KIND_SCALAR;
  }

  return SPN_TOML_VALUE_KIND_NONE;
}

static sp_str_t spn_pkg_toml_value_kind_to_str(spn_toml_value_kind_t kind) {
  switch (kind) {
    case SPN_TOML_VALUE_KIND_NONE: {
      return sp_str_lit("missing");
    }
    case SPN_TOML_VALUE_KIND_SCALAR: {
      return sp_str_lit("scalar");
    }
    case SPN_TOML_VALUE_KIND_ARRAY: {
      return sp_str_lit("array");
    }
    case SPN_TOML_VALUE_KIND_TABLE: {
      return sp_str_lit("table");
    }
  }

  SP_UNREACHABLE_RETURN(sp_str_lit("unknown"));
}

static spn_err_t spn_pkg_toml_validate_field_missing(spn_pkg_t* pkg, sp_str_t path, sp_str_t expected) {
  return spn_pkg_push_manifest_field_err(pkg, path, expected, sp_str_lit("missing"));
}

static spn_err_t spn_pkg_toml_validate_field_type(spn_pkg_t* pkg, sp_str_t path, sp_str_t expected, spn_toml_value_kind_t actual) {
  return spn_pkg_push_manifest_field_err(pkg, path, expected, spn_pkg_toml_value_kind_to_str(actual));
}

static spn_err_t spn_pkg_toml_get_table_strict(spn_pkg_t* pkg, toml_table_t* table, const c8* key, bool required, sp_str_t field, toml_table_t** out) {
  spn_toml_value_kind_t kind = spn_pkg_toml_table_value_kind(table, key);
  if (kind == SPN_TOML_VALUE_KIND_NONE) {
    if (required) {
      return spn_pkg_toml_validate_field_missing(pkg, field, sp_str_lit("table"));
    }

    *out = SP_NULLPTR;
    return SPN_OK;
  }

  if (kind != SPN_TOML_VALUE_KIND_TABLE) {
    return spn_pkg_toml_validate_field_type(pkg, field, sp_str_lit("table"), kind);
  }

  *out = toml_table_table(table, key);
  return SPN_OK;
}

static spn_err_t spn_pkg_toml_get_array_strict(spn_pkg_t* pkg, toml_table_t* table, const c8* key, bool required, sp_str_t field, toml_array_t** out) {
  spn_toml_value_kind_t kind = spn_pkg_toml_table_value_kind(table, key);
  if (kind == SPN_TOML_VALUE_KIND_NONE) {
    if (required) {
      return spn_pkg_toml_validate_field_missing(pkg, field, sp_str_lit("array"));
    }

    *out = SP_NULLPTR;
    return SPN_OK;
  }

  if (!required && kind == SPN_TOML_VALUE_KIND_TABLE) {
    return spn_pkg_toml_validate_field_type(pkg, field, sp_str_lit("array"), kind);
  }

  if (kind != SPN_TOML_VALUE_KIND_ARRAY) {
    return spn_pkg_toml_validate_field_type(pkg, field, sp_str_lit("array"), kind);
  }

  *out = toml_table_array(table, key);
  return SPN_OK;
}

static spn_err_t spn_pkg_toml_get_string_strict(spn_pkg_t* pkg, toml_table_t* table, const c8* key, bool required, sp_str_t field, const c8* fallback, sp_str_t* out) {
  spn_toml_value_kind_t kind = spn_pkg_toml_table_value_kind(table, key);
  if (kind == SPN_TOML_VALUE_KIND_NONE) {
    if (required) {
      return spn_pkg_toml_validate_field_missing(pkg, field, sp_str_lit("string"));
    }

    *out = sp_str_view(fallback);
    return SPN_OK;
  }

  if (kind != SPN_TOML_VALUE_KIND_SCALAR) {
    return spn_pkg_toml_validate_field_type(pkg, field, sp_str_lit("string"), kind);
  }

  toml_value_t value = toml_table_string(table, key);
  if (!value.ok) {
    return spn_pkg_toml_validate_field_type(pkg, field, sp_str_lit("string"), SPN_TOML_VALUE_KIND_SCALAR);
  }

  *out = sp_str_view(value.u.s);
  return SPN_OK;
}

static spn_err_t spn_pkg_toml_array_get_string_strict(spn_pkg_t* pkg, toml_array_t* array, u32 it, sp_str_t field, sp_str_t* out) {
  toml_value_t value = toml_array_string(array, it);
  if (!value.ok) {
    return spn_pkg_toml_validate_field_type(pkg, field, sp_str_lit("string"), SPN_TOML_VALUE_KIND_SCALAR);
  }

  *out = sp_str_view(value.u.s);
  return SPN_OK;
}

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
  if      (sp_str_equal_cstr(str, ""))         return SPN_DIR_STORE;
  else if (sp_str_equal_cstr(str, "cache"))    return SPN_DIR_CACHE;
  else if (sp_str_equal_cstr(str, "store"))    return SPN_DIR_STORE;
  else if (sp_str_equal_cstr(str, "include"))  return SPN_DIR_INCLUDE;
  else if (sp_str_equal_cstr(str, "vendor"))   return SPN_DIR_VENDOR;
  else if (sp_str_equal_cstr(str, "lib"))      return SPN_DIR_LIB;
  else if (sp_str_equal_cstr(str, "source"))   return SPN_DIR_SOURCE;
  else if (sp_str_equal_cstr(str, "work"))     return SPN_DIR_WORK;
  else if (sp_str_equal_cstr(str, "project"))  return SPN_DIR_PROJECT;

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
  pkg->paths.metadata = sp_fs_join_path(pkg->paths.root, sp_str_lit("metadata.toml"));
  pkg->paths.script = sp_fs_join_path(pkg->paths.root, sp_str_lit("spn.c"));
  sp_context_pop();
}

void spn_pkg_set_manifest(spn_pkg_t* pkg, sp_str_t path) {
  sp_context_push_arena(pkg->arena);
  pkg->kind = SPN_PACKAGE_KIND_FILE;
  pkg->paths.manifest = sp_str_copy(path);
  pkg->paths.root = sp_fs_parent_path(path);
  pkg->paths.script = sp_fs_join_path(pkg->paths.root, SP_LIT("spn.c"));
  pkg->paths.metadata = sp_fs_join_path(pkg->paths.root, SP_LIT("metadata.toml"));
  sp_context_pop();
}

spn_pkg_t spn_pkg_new(sp_str_t name) {
  spn_pkg_t pkg = SP_ZERO_INITIALIZE();
  spn_pkg_init(&pkg, name);
  return pkg;
}

spn_pkg_t spn_pkg_from_bare_default(sp_str_t path, sp_str_t name) {
  spn_pkg_t package = spn_pkg_new(name);
  spn_pkg_set_manifest(&package, sp_fs_join_path(path, sp_str_lit("spn.toml")));
  package.version = (spn_semver_t){0, 1, 0};
  sp_dyn_array_push(package.versions, package.version);
  return package;
}

spn_pkg_t spn_pkg_from_default(sp_str_t path, sp_str_t name) {
  spn_pkg_t pkg = spn_pkg_new(name);
  spn_pkg_set_manifest(&pkg, sp_fs_join_path(path, sp_str_lit("spn.toml")));
  spn_pkg_add_dep_latest(&pkg, sp_str_lit("sp"), SPN_VISIBILITY_PUBLIC);
  spn_pkg_set_repo(&pkg, "");
  spn_pkg_add_version(&pkg, "0.1.0", "");

  spn_target_t* bin = spn_pkg_add_exe_ex(&pkg, pkg.name);
  spn_target_add_source_ex(bin, sp_str_lit("main.c"));

  return pkg;
}

spn_err_t spn_pkg_from_index(spn_pkg_t* pkg, sp_str_t path) {
  sp_str_t manifest = sp_fs_join_path(path, sp_str_lit("spn.toml"));
  SP_ASSERT(sp_fs_exists(manifest));

  sp_try(spn_pkg_load(pkg, manifest));
  spn_pkg_set_index(pkg, path);

  bool parse_error = false;
  toml_table_t* metadata = spn_toml_parse_ex(pkg->paths.metadata, &parse_error);
  if (parse_error) {
    spn_event_buffer_push_ex(spn.events, pkg, SP_NULLPTR, (spn_build_event_t) {
      .kind = SPN_EVENT_ERR,
      .err = {
        .code = SPN_ERROR,
        .kind = SPN_ERR_KIND_MANIFEST_PARSE,
        .manifest_parse = {
          .path = pkg->paths.metadata,
        },
      },
    });
  }

  if (metadata) {
    toml_array_t* versions = toml_table_array(metadata, "versions");

    const c8* key = SP_NULLPTR;
    spn_toml_arr_for(versions, it) {
      toml_table_t* entry = toml_array_table(versions, it);

      spn_semver_t version = spn_semver_from_str(spn_toml_str(entry, "version"));
      spn_pkg_add_version_ex(pkg, version, spn_toml_str(entry, "commit"));
    }

    sp_dyn_array_sort(pkg->versions, spn_semver_sort_kernel);
  }
  return SPN_OK;
}

spn_err_t spn_pkg_from_manifest(spn_pkg_t* pkg, sp_str_t manifest) {
  SP_ASSERT(sp_fs_exists(manifest));

  sp_try(spn_pkg_load(pkg, manifest));
  spn_pkg_set_manifest(pkg, manifest);
  return SPN_OK;
}

spn_err_t spn_pkg_load_deps(toml_table_t* toml, spn_pkg_t* package, spn_visibility_t visibility, sp_str_t path) {
  if (!toml) {
    return SPN_OK;
  }

  const c8 *key = SP_NULLPTR;
  spn_toml_for(toml, n, key) {
    sp_str_t field = sp_format("{}.{}", SP_FMT_STR(path), SP_FMT_CSTR(key));
    sp_str_t version = SP_ZERO_INITIALIZE();
    sp_try(spn_pkg_toml_get_string_strict(package, toml, key, true, field, "", &version));
    spn_pkg_add_dep(package, sp_str_view(key), version, visibility);
  }

  return SPN_OK;
}

spn_err_t spn_pkg_load(spn_pkg_t* pkg, sp_str_t manifest_path) {
  struct {
    toml_table_t* manifest;
    toml_table_t* package;
    toml_array_t* lib;
    toml_array_t* bin;
    toml_array_t* test;
    toml_table_t* deps;
    toml_array_t* profile;
    toml_array_t* index;
    toml_table_t* options;
    toml_table_t* config;
  } toml = SP_ZERO_INITIALIZE();
#define SPN_PKG_TRY(expr) sp_try_as((expr), SPN_ERROR)

  bool parse_error = false;
  toml.manifest = spn_toml_parse_ex(manifest_path, &parse_error);
  if (!toml.manifest) {
    if (parse_error) {
      spn_event_buffer_push_ex(spn.events, pkg, SP_NULLPTR, (spn_build_event_t) {
        .kind = SPN_EVENT_ERR,
        .err = {
          .code = SPN_ERROR,
          .kind = SPN_ERR_KIND_MANIFEST_PARSE,
          .manifest_parse = {
            .path = manifest_path,
          },
        },
      });
    }

    return SPN_ERROR;
  }

  SPN_PKG_TRY(spn_pkg_toml_get_table_strict(pkg, toml.manifest, "package", true, sp_str_lit("package"), &toml.package));
  SPN_PKG_TRY(spn_pkg_toml_get_array_strict(pkg, toml.manifest, "lib", false, sp_str_lit("lib"), &toml.lib));
  SPN_PKG_TRY(spn_pkg_toml_get_array_strict(pkg, toml.manifest, "bin", false, sp_str_lit("bin"), &toml.bin));
  SPN_PKG_TRY(spn_pkg_toml_get_array_strict(pkg, toml.manifest, "test", false, sp_str_lit("test"), &toml.test));
  SPN_PKG_TRY(spn_pkg_toml_get_array_strict(pkg, toml.manifest, "profile", false, sp_str_lit("profile"), &toml.profile));
  SPN_PKG_TRY(spn_pkg_toml_get_array_strict(pkg, toml.manifest, "index", false, sp_str_lit("index"), &toml.index));
  SPN_PKG_TRY(spn_pkg_toml_get_table_strict(pkg, toml.manifest, "deps", false, sp_str_lit("deps"), &toml.deps));
  SPN_PKG_TRY(spn_pkg_toml_get_table_strict(pkg, toml.manifest, "options", false, sp_str_lit("options"), &toml.options));
  SPN_PKG_TRY(spn_pkg_toml_get_table_strict(pkg, toml.manifest, "config", false, sp_str_lit("config"), &toml.config));

  sp_str_t package_name = SP_ZERO_INITIALIZE();
  SPN_PKG_TRY(spn_pkg_toml_get_string_strict(pkg, toml.package, "name", true, sp_str_lit("package.name"), "", &package_name));

  sp_str_t package_url = SP_ZERO_INITIALIZE();
  SPN_PKG_TRY(spn_pkg_toml_get_string_strict(pkg, toml.package, "url", false, sp_str_lit("package.url"), "", &package_url));

  sp_str_t package_author = SP_ZERO_INITIALIZE();
  SPN_PKG_TRY(spn_pkg_toml_get_string_strict(pkg, toml.package, "author", false, sp_str_lit("package.author"), "", &package_author));

  sp_str_t package_maintainer = SP_ZERO_INITIALIZE();
  SPN_PKG_TRY(spn_pkg_toml_get_string_strict(pkg, toml.package, "maintainer", false, sp_str_lit("package.maintainer"), "", &package_maintainer));

  sp_str_t package_version = SP_ZERO_INITIALIZE();
  SPN_PKG_TRY(spn_pkg_toml_get_string_strict(pkg, toml.package, "version", true, sp_str_lit("package.version"), "", &package_version));

  sp_str_t package_commit = SP_ZERO_INITIALIZE();
  SPN_PKG_TRY(spn_pkg_toml_get_string_strict(pkg, toml.package, "commit", false, sp_str_lit("package.commit"), "", &package_commit));

  spn_pkg_init(pkg, package_name);
  spn_pkg_set_url_ex(pkg, package_url);
  spn_pkg_set_author_ex(pkg, package_author);
  spn_pkg_set_maintainer_ex(pkg, package_maintainer);
  spn_pkg_add_version_ex(pkg, spn_semver_from_str(package_version), package_commit);

  toml_array_t* include = toml_table_array(toml.package, "include");
  spn_toml_arr_for(include, it) {
    sp_str_t field = sp_format("package.include[{}]", SP_FMT_U32(it));
    sp_str_t value = SP_ZERO_INITIALIZE();
    SPN_PKG_TRY(spn_pkg_toml_array_get_string_strict(pkg, include, it, field, &value));

    sp_str_t path = sp_fs_join_path(spn_ctx_project_root(), value);
    spn_pkg_add_include_ex(pkg, path);
  }

  toml_array_t* define = toml_table_array(toml.package, "define");
  spn_toml_arr_for(define, it) {
    sp_str_t field = sp_format("package.define[{}]", SP_FMT_U32(it));
    sp_str_t value = SP_ZERO_INITIALIZE();
    SPN_PKG_TRY(spn_pkg_toml_array_get_string_strict(pkg, define, it, field, &value));
    spn_pkg_add_define_ex(pkg, value);
  }

  toml_array_t* system_deps = toml_table_array(toml.package, "system_deps");
  spn_toml_arr_for(system_deps, it) {
    sp_str_t field = sp_format("package.system_deps[{}]", SP_FMT_U32(it));
    sp_str_t value = SP_ZERO_INITIALIZE();
    SPN_PKG_TRY(spn_pkg_toml_array_get_string_strict(pkg, system_deps, it, field, &value));
    spn_pkg_add_system_dep_ex(pkg, value);
  }

  spn_toml_arr_for(toml.lib, n) {
    toml_table_t* it = toml_array_table(toml.lib, n);
    if (!it) {
      SPN_PKG_TRY(spn_pkg_toml_validate_field_type(pkg, sp_format("lib[{}]", SP_FMT_U32(n)), sp_str_lit("table"), SPN_TOML_VALUE_KIND_SCALAR));
    }

    sp_str_t name = SP_ZERO_INITIALIZE();
    SPN_PKG_TRY(spn_pkg_toml_get_string_strict(pkg, it, "name", true, sp_format("lib[{}].name", SP_FMT_U32(n)), "", &name));

    sp_str_t kind_str = SP_ZERO_INITIALIZE();
    SPN_PKG_TRY(spn_pkg_toml_get_string_strict(pkg, it, "kind", false, sp_format("lib[{}].kind", SP_FMT_U32(n)), "static", &kind_str));
    spn_linkage_t kind = spn_pkg_linkage_from_str(kind_str);
    spn_target_t* lib = spn_pkg_add_lib_ex(pkg, name, kind);

    toml_array_t* source = toml_table_array(it, "source");
    spn_toml_arr_for(source, s) {
      sp_str_t field = sp_format("lib[{}].source[{}]", SP_FMT_U32(n), SP_FMT_U32(s));
      sp_str_t value = SP_ZERO_INITIALIZE();
      SPN_PKG_TRY(spn_pkg_toml_array_get_string_strict(pkg, source, s, field, &value));
      spn_target_add_source_ex(lib, value);
    }

    toml_array_t* include_arr = toml_table_array(it, "include");
    spn_toml_arr_for(include_arr, i) {
      sp_str_t field = sp_format("lib[{}].include[{}]", SP_FMT_U32(n), SP_FMT_U32(i));
      sp_str_t value = SP_ZERO_INITIALIZE();
      SPN_PKG_TRY(spn_pkg_toml_array_get_string_strict(pkg, include_arr, i, field, &value));
      spn_target_add_include_ex(lib, value);
    }

    toml_array_t* define_arr = toml_table_array(it, "define");
    spn_toml_arr_for(define_arr, d) {
      sp_str_t field = sp_format("lib[{}].define[{}]", SP_FMT_U32(n), SP_FMT_U32(d));
      sp_str_t value = SP_ZERO_INITIALIZE();
      SPN_PKG_TRY(spn_pkg_toml_array_get_string_strict(pkg, define_arr, d, field, &value));
      spn_target_add_define_ex(lib, value);
    }
  }

  spn_toml_arr_for(toml.profile, n) {
    toml_table_t* it = toml_array_table(toml.profile, n);
    if (!it) {
      SPN_PKG_TRY(spn_pkg_toml_validate_field_type(pkg, sp_format("profile[{}]", SP_FMT_U32(n)), sp_str_lit("table"), SPN_TOML_VALUE_KIND_SCALAR));
    }

    sp_str_t profile_name = SP_ZERO_INITIALIZE();
    SPN_PKG_TRY(spn_pkg_toml_get_string_strict(pkg, it, "name", true, sp_format("profile[{}].name", SP_FMT_U32(n)), "", &profile_name));

    sp_str_t profile_cc = SP_ZERO_INITIALIZE();
    SPN_PKG_TRY(spn_pkg_toml_get_string_strict(pkg, it, "cc", false, sp_format("profile[{}].cc", SP_FMT_U32(n)), "gcc", &profile_cc));

    sp_str_t profile_linkage = SP_ZERO_INITIALIZE();
    SPN_PKG_TRY(spn_pkg_toml_get_string_strict(pkg, it, "linkage", false, sp_format("profile[{}].linkage", SP_FMT_U32(n)), "shared", &profile_linkage));

    sp_str_t profile_libc = SP_ZERO_INITIALIZE();
    SPN_PKG_TRY(spn_pkg_toml_get_string_strict(pkg, it, "libc", false, sp_format("profile[{}].libc", SP_FMT_U32(n)), "gnu", &profile_libc));

    sp_str_t profile_standard = SP_ZERO_INITIALIZE();
    SPN_PKG_TRY(spn_pkg_toml_get_string_strict(pkg, it, "standard", false, sp_format("profile[{}].standard", SP_FMT_U32(n)), "c99", &profile_standard));

    sp_str_t profile_mode = SP_ZERO_INITIALIZE();
    SPN_PKG_TRY(spn_pkg_toml_get_string_strict(pkg, it, "mode", false, sp_format("profile[{}].mode", SP_FMT_U32(n)), "debug", &profile_mode));

    spn_profile_t* profile = spn_pkg_add_profile_ex(pkg, (spn_profile_t) {
      .name = profile_name,
      .cc = { .exe = profile_cc, .kind = spn_cc_kind_from_str(profile_cc) },
      .linkage = spn_pkg_linkage_from_str(profile_linkage),
      .libc = spn_libc_kind_from_str(profile_libc),
      .standard = spn_c_standard_from_str(profile_standard),
      .mode = spn_dep_build_mode_from_str(profile_mode),
      .kind = SPN_PROFILE_USER,
    });
    (void)profile;
  }

  spn_toml_arr_for(toml.index, n) {
    toml_table_t* it = toml_array_table(toml.index, n);
    if (!it) {
      SPN_PKG_TRY(spn_pkg_toml_validate_field_type(pkg, sp_format("index[{}]", SP_FMT_U32(n)), sp_str_lit("table"), SPN_TOML_VALUE_KIND_SCALAR));
    }

    sp_str_t index_name = SP_ZERO_INITIALIZE();
    SPN_PKG_TRY(spn_pkg_toml_get_string_strict(pkg, it, "name", true, sp_format("index[{}].name", SP_FMT_U32(n)), "", &index_name));

    sp_str_t index_location = SP_ZERO_INITIALIZE();
    SPN_PKG_TRY(spn_pkg_toml_get_string_strict(pkg, it, "location", true, sp_format("index[{}].location", SP_FMT_U32(n)), "", &index_location));

    spn_pkg_add_index_ex(pkg, index_name, index_location);
  }

  spn_toml_arr_for(toml.bin, n) {
    toml_table_t* it = toml_array_table(toml.bin, n);
    if (!it) {
      SPN_PKG_TRY(spn_pkg_toml_validate_field_type(pkg, sp_format("bin[{}]", SP_FMT_U32(n)), sp_str_lit("table"), SPN_TOML_VALUE_KIND_SCALAR));
    }

    sp_str_t bin_name = SP_ZERO_INITIALIZE();
    SPN_PKG_TRY(spn_pkg_toml_get_string_strict(pkg, it, "name", true, sp_format("bin[{}].name", SP_FMT_U32(n)), "", &bin_name));

    spn_target_t* bin = spn_pkg_add_exe_ex(pkg, bin_name);

    toml_array_t* source = toml_table_array(it, "source");
    spn_toml_arr_for(source, s) {
      sp_str_t field = sp_format("bin[{}].source[{}]", SP_FMT_U32(n), SP_FMT_U32(s));
      sp_str_t value = SP_ZERO_INITIALIZE();
      SPN_PKG_TRY(spn_pkg_toml_array_get_string_strict(pkg, source, s, field, &value));
      spn_target_add_source_ex(bin, value);
    }

    toml_array_t* include_arr = toml_table_array(it, "include");
    spn_toml_arr_for(include_arr, i) {
      sp_str_t field = sp_format("bin[{}].include[{}]", SP_FMT_U32(n), SP_FMT_U32(i));
      sp_str_t value = SP_ZERO_INITIALIZE();
      SPN_PKG_TRY(spn_pkg_toml_array_get_string_strict(pkg, include_arr, i, field, &value));
      spn_target_add_include_ex(bin, value);
    }

    toml_array_t* define_arr = toml_table_array(it, "define");
    spn_toml_arr_for(define_arr, d) {
      sp_str_t field = sp_format("bin[{}].define[{}]", SP_FMT_U32(n), SP_FMT_U32(d));
      sp_str_t value = SP_ZERO_INITIALIZE();
      SPN_PKG_TRY(spn_pkg_toml_array_get_string_strict(pkg, define_arr, d, field, &value));
      spn_target_add_define_ex(bin, value);
    }

    sp_str_t kind = SP_ZERO_INITIALIZE();
    SPN_PKG_TRY(spn_pkg_toml_get_string_strict(pkg, it, "kind", false, sp_format("bin[{}].kind", SP_FMT_U32(n)), "", &kind));
    if (!sp_str_empty(kind)) {
      spn_target_set_visibility(bin, spn_visibility_from_str(kind));
    }
  }

  spn_toml_arr_for(toml.test, n) {
    toml_table_t* it = toml_array_table(toml.test, n);
    if (!it) {
      SPN_PKG_TRY(spn_pkg_toml_validate_field_type(pkg, sp_format("test[{}]", SP_FMT_U32(n)), sp_str_lit("table"), SPN_TOML_VALUE_KIND_SCALAR));
    }

    sp_str_t test_name = SP_ZERO_INITIALIZE();
    SPN_PKG_TRY(spn_pkg_toml_get_string_strict(pkg, it, "name", true, sp_format("test[{}].name", SP_FMT_U32(n)), "", &test_name));

    spn_target_t* test = spn_pkg_add_test_ex(pkg, test_name);

    toml_array_t* source = toml_table_array(it, "source");
    spn_toml_arr_for(source, s) {
      sp_str_t field = sp_format("test[{}].source[{}]", SP_FMT_U32(n), SP_FMT_U32(s));
      sp_str_t value = SP_ZERO_INITIALIZE();
      SPN_PKG_TRY(spn_pkg_toml_array_get_string_strict(pkg, source, s, field, &value));
      spn_target_add_source_ex(test, value);
    }

    toml_array_t* include_arr = toml_table_array(it, "include");
    spn_toml_arr_for(include_arr, i) {
      sp_str_t field = sp_format("test[{}].include[{}]", SP_FMT_U32(n), SP_FMT_U32(i));
      sp_str_t value = SP_ZERO_INITIALIZE();
      SPN_PKG_TRY(spn_pkg_toml_array_get_string_strict(pkg, include_arr, i, field, &value));
      spn_target_add_include_ex(test, value);
    }

    toml_array_t* define_arr = toml_table_array(it, "define");
    spn_toml_arr_for(define_arr, d) {
      sp_str_t field = sp_format("test[{}].define[{}]", SP_FMT_U32(n), SP_FMT_U32(d));
      sp_str_t value = SP_ZERO_INITIALIZE();
      SPN_PKG_TRY(spn_pkg_toml_array_get_string_strict(pkg, define_arr, d, field, &value));
      spn_target_add_define_ex(test, value);
    }
  }

  if (toml.deps) {
    toml_table_t* package_deps = SP_NULLPTR;
    toml_table_t* test_deps = SP_NULLPTR;
    toml_table_t* build_deps = SP_NULLPTR;

    SPN_PKG_TRY(spn_pkg_toml_get_table_strict(pkg, toml.deps, "package", false, sp_str_lit("deps.package"), &package_deps));
    SPN_PKG_TRY(spn_pkg_toml_get_table_strict(pkg, toml.deps, "test", false, sp_str_lit("deps.test"), &test_deps));
    SPN_PKG_TRY(spn_pkg_toml_get_table_strict(pkg, toml.deps, "build", false, sp_str_lit("deps.build"), &build_deps));

    SPN_PKG_TRY(spn_pkg_load_deps(package_deps, pkg, SPN_VISIBILITY_PUBLIC, sp_str_lit("deps.package")));
    SPN_PKG_TRY(spn_pkg_load_deps(test_deps, pkg, SPN_VISIBILITY_TEST, sp_str_lit("deps.test")));
    SPN_PKG_TRY(spn_pkg_load_deps(build_deps, pkg, SPN_VISIBILITY_BUILD, sp_str_lit("deps.build")));
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
      toml_table_t* config = SP_NULLPTR;
      SPN_PKG_TRY(spn_pkg_toml_get_table_strict(pkg, toml.config, key, true, sp_format("config.{}", SP_FMT_CSTR(key)), &config));
      sp_da_push(configs, config);
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

#undef SPN_PKG_TRY

  return SPN_OK;
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

    // @spader: Return an error, log higher up
    if (!dep) {
      sp_str_t prefix = sp_str_lit("  > ");
      sp_str_t color = sp_str_lit("brightcyan");

      sp_da(sp_str_t) search = app.search;
      search = sp_str_map(search, sp_dyn_array_size(search), &color, sp_str_map_kernel_colorize);
      search = sp_str_map(search, sp_dyn_array_size(search), &prefix, sp_str_map_kernel_prepend);

      SP_FATAL(
        "Could not find {:fg yellow} on search path: \n{}",
        SP_FMT_STR(name),
        SP_FMT_STR(sp_str_join_n(search, sp_dyn_array_size(search), sp_str_lit("\n")))
      );
    }

    if (sp_dyn_array_empty(dep->versions)) {
      SP_FATAL("{:fg brightcyan} has no known versions", SP_FMT_STR(dep->name));
    }

    spn_semver_parsed_t parsed = {
      .version = *sp_dyn_array_back(dep->versions),
      .components = { true, true, true }
    };
    req.range = spn_semver_caret_to_range(parsed);
  } else {
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

spn_index_t* spn_pkg_add_index(spn_pkg_t* pkg, const c8* name, const c8* location) {
  return spn_pkg_add_index_ex(pkg, spn_intern_cstr(name), spn_intern_cstr(location));
}

spn_index_t* spn_pkg_add_index_ex(spn_pkg_t* pkg, sp_str_t name, sp_str_t location) {
  spn_index_t index = {
    .name = spn_intern(name),
    .location = spn_intern(location),
    .kind = SPN_INDEX_WORKSPACE
  };
  sp_om_insert(pkg->indexes, index.name, index);
  return sp_om_get(pkg->indexes, index.name);
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
