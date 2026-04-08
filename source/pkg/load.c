#include "error/types.h"
#include "profile/types.h"

#include "enum/enum.h"
#include "external/tom.h"
#include "intern/intern.h"
#include "pkg/id.h"
#include "pkg/load.h"
#include "pkg/mutate.h"
#include "semver/convert.h"
#include "semver/parser.h"
#include "spn.h"
#include "target/mutate.h"
#include "toml.h"
#include "toolchain/types.h"

spn_dep_option_t parse_option(toml_table_t* toml, const c8* key) {
  toml_unparsed_t unparsed = toml_table_unparsed(toml, key);
  SP_ASSERT(unparsed);

  bool b;
  s64 s;
  c8* cstr;
  s32 len;

  if (!toml_value_string(unparsed, &cstr, &len)) {
    return (spn_dep_option_t) {
      .kind = SPN_DEP_OPTION_KIND_STR,
      .name = sp_str_from_cstr(key),
      .str = sp_str_from_cstr(cstr)
    };
  }

  if (!toml_value_int(unparsed, &s)) {
    return (spn_dep_option_t) {
      .kind = SPN_DEP_OPTION_KIND_S64,
      .name = sp_str_from_cstr(key),
      .s = s
    };
  }

  if (!toml_value_bool(unparsed, &b)) {
    return (spn_dep_option_t) {
      .kind = SPN_DEP_OPTION_KIND_BOOL,
      .name = sp_str_from_cstr(key),
      .b = b
    };
  }

  SP_UNREACHABLE_RETURN(SP_ZERO_STRUCT(spn_dep_option_t));
}

void spn_toml_append_option(spn_toml_writer_t* writer, sp_str_t key, spn_dep_option_t option) {
  switch (option.kind) {
    case SPN_DEP_OPTION_KIND_BOOL: {
      spn_toml_append_bool(writer, key, option.b);
      break;
    }
    case SPN_DEP_OPTION_KIND_S64: {
      spn_toml_append_s64(writer, key, option.s);
      break;
    }
    case SPN_DEP_OPTION_KIND_STR: {
      spn_toml_append_str(writer, key, option.str);
      break;
    }
    default: {
      SP_UNREACHABLE_CASE();
    }
  }
}

void spn_toml_append_option_cstr(spn_toml_writer_t* writer, const c8* key, spn_dep_option_t option) {
  spn_toml_append_option(writer, sp_str_view(key), option);
}


typedef struct {
  sp_str_t parent;
  bool has_index;
  u32 index;
} toml_path_t;

static toml_path_t spn_pkg_toml_path(sp_str_t parent) {
  return (toml_path_t) {
    .parent = parent,
    .has_index = false,
    .index = 0,
  };
}

static toml_path_t spn_pkg_toml_path_with_index(sp_str_t parent, u32 index) {
  return (toml_path_t) {
    .parent = parent,
    .has_index = true,
    .index = index,
  };
}

static sp_str_t spn_pkg_toml_path_field(toml_path_t path, const c8* key) {
  if (sp_str_empty(path.parent)) {
    return sp_str_view(key);
  }

  if (path.has_index) {
    return sp_format("{}[{}].{}", SP_FMT_STR(path.parent), SP_FMT_U32(path.index), SP_FMT_CSTR(key));
  }

  return sp_format("{}.{}", SP_FMT_STR(path.parent), SP_FMT_CSTR(key));
}

static sp_str_t spn_pkg_toml_path_index(sp_str_t parent, u32 index) {
  if (sp_str_empty(parent)) {
    return sp_format("[{}]", SP_FMT_U32(index));
  }

  return sp_format("{}[{}]", SP_FMT_STR(parent), SP_FMT_U32(index));
}

static sp_str_t spn_pkg_toml_path_array_item(toml_path_t path, const c8* key, u32 it) {
  if (path.has_index) {
    return sp_format("{}[{}].{}[{}]", SP_FMT_STR(path.parent), SP_FMT_U32(path.index), SP_FMT_CSTR(key), SP_FMT_U32(it));
  }

  if (sp_str_empty(path.parent)) {
    return sp_format("{}[{}]", SP_FMT_CSTR(key), SP_FMT_U32(it));
  }

  return sp_format("{}.{}[{}]", SP_FMT_STR(path.parent), SP_FMT_CSTR(key), SP_FMT_U32(it));
}

static spn_toml_value_kind_t toml_kind_from_field(toml_table_t* table, const c8* key) {
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

static sp_str_t toml_kind_to_str(spn_toml_value_kind_t kind) {
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

// @spader
// These are for generating rich error messages, but I don't like how it's done. It always feels wrong
// to return purely aesthetic data far away from the place where we're building the mesage. We ought
// to be returning structured data here, at the site of the error, and mapping it onto readable
// strings at the place it's reported
static spn_err_union_t field_missing_error(sp_str_t path, sp_str_t expected) {
  return (spn_err_union_t) {
    .kind = SPN_ERR_MANIFEST_FIELD,
    .manifest_field = {
      .path = path,
      .expected = expected,
      .actual = sp_str_lit("missing"),
    },
  };
}

static spn_err_union_t field_type_error(sp_str_t path, sp_str_t expected, spn_toml_value_kind_t actual) {
  return (spn_err_union_t) {
    .kind = SPN_ERR_MANIFEST_FIELD,
    .manifest_field = {
      .path = path,
      .expected = expected,
      .actual = toml_kind_to_str(actual),
    },
  };
}

static spn_err_union_t ensure_unique_target_name(spn_pkg_info_t* pkg, toml_path_t path, sp_str_t name) {
  bool exists =
    sp_om_has(pkg->libs, name)    ||
    sp_om_has(pkg->exes, name)    ||
    sp_om_has(pkg->scripts, name) ||
    sp_om_has(pkg->tests, name);

  if (exists) {
    return (spn_err_union_t) {
      .kind = SPN_ERR_MANIFEST_FIELD,
      .manifest_field = {
        .path = spn_pkg_toml_path_field(path, "name"),
        .expected = sp_str_lit("unique target name"),
        .actual = sp_format("{} is already defined", SP_FMT_STR(name)),
      },
    };
  }

  return spn_result(SPN_OK);
}

static spn_err_union_t toml_get_table_required(toml_table_t* table, const c8* key, toml_path_t path, toml_table_t** out) {
  spn_toml_value_kind_t kind = toml_kind_from_field(table, key);
  if (kind == SPN_TOML_VALUE_KIND_NONE) {
    return field_missing_error(spn_pkg_toml_path_field(path, key), sp_str_lit("table"));
  }

  if (kind != SPN_TOML_VALUE_KIND_TABLE) {
    return field_type_error(spn_pkg_toml_path_field(path, key), sp_str_lit("table"), kind);
  }

  *out = toml_table_table(table, key);
  return spn_result(SPN_OK);
}

static spn_err_union_t toml_get_table_optional(toml_table_t* table, const c8* key, toml_path_t path, toml_table_t** out) {
  *out = SP_NULLPTR;

  spn_toml_value_kind_t kind = toml_kind_from_field(table, key);
  if (kind == SPN_TOML_VALUE_KIND_NONE) {
    return spn_result(SPN_OK);
  }

  if (kind != SPN_TOML_VALUE_KIND_TABLE) {
    return field_type_error(spn_pkg_toml_path_field(path, key), sp_str_lit("table"), kind);
  }

  *out = toml_table_table(table, key);
  return spn_result(SPN_OK);
}

static spn_err_union_t toml_get_array_required(toml_table_t* table, const c8* key, toml_path_t path, toml_array_t** out) {
  spn_toml_value_kind_t kind = toml_kind_from_field(table, key);
  if (kind == SPN_TOML_VALUE_KIND_NONE) {
    return field_missing_error(spn_pkg_toml_path_field(path, key), sp_str_lit("array"));
  }

  if (kind != SPN_TOML_VALUE_KIND_ARRAY) {
    return field_type_error(spn_pkg_toml_path_field(path, key), sp_str_lit("array"), kind);
  }

  *out = toml_table_array(table, key);
  return spn_result(SPN_OK);
}

static spn_err_union_t toml_get_array_optional(toml_table_t* table, const c8* key, toml_path_t path, toml_array_t** out) {
  *out = SP_NULLPTR;

  spn_toml_value_kind_t kind = toml_kind_from_field(table, key);
  if (kind == SPN_TOML_VALUE_KIND_NONE) {
    return spn_result(SPN_OK);
  }

  if (kind != SPN_TOML_VALUE_KIND_ARRAY) {
    return field_type_error(spn_pkg_toml_path_field(path, key), sp_str_lit("array"), kind);
  }

  *out = toml_table_array(table, key);
  return spn_result(SPN_OK);
}

// @spader
// A sketch to unfuck the errors a little
//
static spn_err_t get_bool_optional(toml_table_t* table, const c8* key, bool* b) {
  spn_toml_value_kind_t kind = toml_kind_from_field(table, key);
  switch (kind) {
    case SPN_TOML_VALUE_KIND_NONE: return SPN_OK;
    case SPN_TOML_VALUE_KIND_ARRAY:
    case SPN_TOML_VALUE_KIND_TABLE: return SPN_ERR_TOML_TYPE;
    case SPN_TOML_VALUE_KIND_SCALAR: {
      toml_value_t value = toml_table_bool(table, key);
      if (!value.ok) {
        return SPN_ERR_TOML_TYPE;
      }

      *b = value.u.b;
      return SPN_OK;
    }
  }

  sp_unreachable_return(SPN_ERROR);
}

static spn_err_t get_str_optional(toml_table_t* table, const c8* key, sp_str_t* str) {
  spn_toml_value_kind_t kind = toml_kind_from_field(table, key);
  switch (kind) {
    case SPN_TOML_VALUE_KIND_NONE: return SPN_OK;
    case SPN_TOML_VALUE_KIND_ARRAY:
    case SPN_TOML_VALUE_KIND_TABLE: return SPN_ERR_TOML_TYPE;
    case SPN_TOML_VALUE_KIND_SCALAR: {
      toml_value_t value = toml_table_string(table, key);
      if (!value.ok) {
        return SPN_ERR_TOML_TYPE;
      }

      *str = sp_str_view(value.u.s);
      return SPN_OK;
    }
  }
  sp_unreachable_return(SPN_ERROR);
}

static spn_err_t get_str_required(toml_table_t* table, const c8* key, sp_str_t* str) {
  spn_toml_value_kind_t kind = toml_kind_from_field(table, key);
  switch (kind) {
    case SPN_TOML_VALUE_KIND_NONE: return SPN_ERR_TOML_MISSING;
    case SPN_TOML_VALUE_KIND_ARRAY:
    case SPN_TOML_VALUE_KIND_TABLE: return SPN_ERR_TOML_TYPE;
    case SPN_TOML_VALUE_KIND_SCALAR: {
      toml_value_t value = toml_table_string(table, key);
      if (!value.ok) {
        return SPN_ERR_TOML_TYPE;
      }

      *str = sp_str_view(value.u.s);
      return SPN_OK;
    }
  }
  sp_unreachable_return(SPN_ERROR);
}

static spn_err_t get_table_optional(toml_table_t* toml, const c8* key, toml_table_t** table) {
  spn_toml_value_kind_t kind = toml_kind_from_field(toml, key);
  switch (kind) {
    case SPN_TOML_VALUE_KIND_NONE: return SPN_OK;
    case SPN_TOML_VALUE_KIND_SCALAR:
    case SPN_TOML_VALUE_KIND_ARRAY: return SPN_ERR_TOML_TYPE;
    case SPN_TOML_VALUE_KIND_TABLE: {
      *table = toml_table_table(toml, key);
      return SPN_OK;
    }
  }
  sp_unreachable_return(SPN_ERROR);
}

static spn_err_t get_table_required(toml_table_t* toml, const c8* key, toml_table_t** table) {
  spn_toml_value_kind_t kind = toml_kind_from_field(toml, key);
  switch (kind) {
    case SPN_TOML_VALUE_KIND_NONE:
    case SPN_TOML_VALUE_KIND_SCALAR:
    case SPN_TOML_VALUE_KIND_ARRAY: return SPN_ERR_TOML_TYPE;
    case SPN_TOML_VALUE_KIND_TABLE: {
      *table = toml_table_table(toml, key);
      return SPN_OK;
    }
  }
  sp_unreachable_return(SPN_ERROR);
}

static spn_err_t get_arr_optional(toml_table_t* table, const c8* key, toml_array_t** arr) {
  spn_toml_value_kind_t kind = toml_kind_from_field(table, key);
  switch (kind) {
    case SPN_TOML_VALUE_KIND_NONE: return SPN_OK;
    case SPN_TOML_VALUE_KIND_SCALAR:
    case SPN_TOML_VALUE_KIND_TABLE: return SPN_ERR_TOML_TYPE;
    case SPN_TOML_VALUE_KIND_ARRAY: {
      *arr = toml_table_array(table, key);
      return SPN_OK;
    }
  }
  sp_unreachable_return(SPN_ERROR);
}

static spn_err_t get_arr_required(toml_table_t* table, const c8* key, toml_array_t** arr) {
  spn_toml_value_kind_t kind = toml_kind_from_field(table, key);
  switch (kind) {
    case SPN_TOML_VALUE_KIND_NONE:
    case SPN_TOML_VALUE_KIND_SCALAR:
    case SPN_TOML_VALUE_KIND_TABLE: return SPN_ERR_TOML_TYPE;
    case SPN_TOML_VALUE_KIND_ARRAY: {
      *arr = toml_table_array(table, key);
      return SPN_OK;
    }
  }
  sp_unreachable_return(SPN_ERROR);
}

static spn_err_union_t toml_get_str_required(toml_table_t* table, const c8* key, toml_path_t path, sp_str_t* out) {
  spn_toml_value_kind_t kind = toml_kind_from_field(table, key);
  if (kind == SPN_TOML_VALUE_KIND_NONE) {
    return field_missing_error(spn_pkg_toml_path_field(path, key), sp_str_lit("string"));
  }

  if (kind != SPN_TOML_VALUE_KIND_SCALAR) {
    return field_type_error(spn_pkg_toml_path_field(path, key), sp_str_lit("string"), kind);
  }

  toml_value_t value = toml_table_string(table, key);
  if (!value.ok) {
    return field_type_error(spn_pkg_toml_path_field(path, key), sp_str_lit("string"), SPN_TOML_VALUE_KIND_SCALAR);
  }

  *out = sp_str_view(value.u.s);
  return spn_result(SPN_OK);
}

static spn_err_union_t toml_get_str_optional(toml_table_t* table, const c8* key, toml_path_t path, sp_str_t* out) {
  spn_toml_value_kind_t kind = toml_kind_from_field(table, key);
  if (kind == SPN_TOML_VALUE_KIND_NONE) {
    return spn_result(SPN_OK);
  }

  if (kind != SPN_TOML_VALUE_KIND_SCALAR) {
    return field_type_error(spn_pkg_toml_path_field(path, key), sp_str_lit("string"), kind);
  }

  toml_value_t value = toml_table_string(table, key);
  if (!value.ok) {
    return field_type_error(spn_pkg_toml_path_field(path, key), sp_str_lit("string"), SPN_TOML_VALUE_KIND_SCALAR);
  }

  *out = sp_str_view(value.u.s);
  return spn_result(SPN_OK);
}

static spn_err_union_t toml_get_array_table_required(toml_array_t* array, u32 it, sp_str_t parent, toml_table_t** out) {
  *out = toml_array_table(array, it);
  if (!*out) {
    return field_type_error(spn_pkg_toml_path_index(parent, it), sp_str_lit("table"), SPN_TOML_VALUE_KIND_SCALAR);
  }

  return spn_result(SPN_OK);
}

static spn_err_union_t toml_get_array_string_required(toml_array_t* array, u32 it, toml_path_t path, const c8* key, sp_str_t* out) {
  toml_value_t value = toml_array_string(array, it);
  if (!value.ok) {
    return field_type_error(spn_pkg_toml_path_array_item(path, key, it), sp_str_lit("string"), SPN_TOML_VALUE_KIND_SCALAR);
  }

  *out = sp_str_view(value.u.s);
  return spn_result(SPN_OK);
}

static spn_err_t toml_get_array_string_required_2(toml_array_t* array, u32 it, sp_str_t* str) {
  toml_value_t value = toml_array_string(array, it);
  if (!value.ok) {
    return SPN_ERR_TOML_TYPE;
  }

  *str = sp_str_view(value.u.s);
  return SPN_OK;

}

static spn_err_union_t load_target(
  toml_table_t* table,
  toml_path_t path,
  spn_target_info_t* target
) {
  toml_array_t* source = toml_table_array(table, "source");
  spn_toml_arr_for(source, s) {
    sp_str_t value = SP_ZERO_INITIALIZE();
    spn_try_union(toml_get_array_string_required(source, s, path, "source", &value));
    spn_target_add_source_ex(target, value);
  }

  toml_array_t* include = toml_table_array(table, "include");
  spn_toml_arr_for(include, i) {
    sp_str_t value = SP_ZERO_INITIALIZE();
    spn_try_union(toml_get_array_string_required(include, i, path, "include", &value));
    spn_target_add_include_ex(target, value);
  }

  toml_array_t* define = toml_table_array(table, "define");
  spn_toml_arr_for(define, d) {
    sp_str_t value = SP_ZERO_INITIALIZE();
    spn_try_union(toml_get_array_string_required(define, d, path, "define", &value));
    spn_target_add_define_ex(target, value);
  }

  toml_array_t* deps = toml_table_array(table, "deps");
  spn_toml_arr_for(deps, it) {
    sp_str_t value = SP_ZERO_INITIALIZE();
    spn_try_union(toml_get_array_string_required(deps, it, path, "deps", &value));
    spn_target_add_dep_ex(target, value);
  }


  return spn_result(SPN_OK);
}

static spn_err_union_t load_deps(toml_table_t* toml, spn_pkg_info_t* pkg, spn_visibility_t vis, sp_str_t root) {
  if (!toml) {
    return spn_result(SPN_OK);
  }

  const c8* key = SP_NULLPTR;
  spn_toml_for(toml, n, key) {
    sp_str_t version = SP_ZERO_INITIALIZE();
    spn_try_as_union(get_str_required(toml, key, &version));

    sp_str_t prefix = sp_str_lit("file://");
    if (sp_str_starts_with(version, prefix)) {
      sp_str_t path = sp_str_strip_left(version, prefix);
      if (!sp_str_starts_with(path, sp_str_lit("/"))) {
        path = sp_fs_join_path(root, path);
      }

      path = sp_fs_normalize_path(path);
      version = sp_format("file://{}", SP_FMT_STR(path));
    }

    spn_pkg_id_t id = spn_qualified_name_to_pkg_id(spn_intern_cstr(key));
    sp_str_t qualified = spn_pkg_id_to_qualified_name(id);
    sp_str_t file_prefix = sp_str_lit("file://");
    if (sp_str_starts_with(version, file_prefix)) {
      spn_requested_pkg_t req = {
        .id = id,
        .visibility = vis,
        .kind = SPN_PACKAGE_KIND_FILE,
        .file = version,
      };
      sp_ht_insert(pkg->deps, qualified, req);
    }
    else {
      spn_requested_pkg_t req = {
        .id = id,
        .visibility = vis,
        .kind = SPN_PACKAGE_KIND_INDEX,
        .range = spn_semver_parse_range(version),
      };
      sp_ht_insert(pkg->deps, qualified, req);
    }
  }

  return spn_result(SPN_OK);
}

static spn_toolchain_launcher_t split_launcher(sp_str_t str) {
  spn_toolchain_launcher_t launcher = sp_zero_initialize();
  if (sp_str_empty(str)) return launcher;

  if (sp_str_contains(str, sp_str_lit(" "))) {
    sp_da(sp_str_t) parts = sp_str_split_c8(str, ' ');
    launcher.program = spn_intern(parts[0]);
    for (u32 i = 1; i < sp_da_size(parts); i++) {
      sp_da_push(launcher.args, spn_intern(parts[i]));
    }
  }
  else {
    launcher.program = spn_intern(str);
  }

  return launcher;
}


static spn_err_t load_triple_array(toml_array_t* toml, spn_triple_t* triples, u32 len) {
  spn_toml_arr_for(toml, it) {
    toml_table_t* triple = toml_array_table(toml, it);

    sp_str_t arch = sp_zero_initialize();
    spn_try(get_str_optional(triple, "arch", &arch));
    sp_str_t os = sp_zero_initialize();
    spn_try(get_str_optional(triple, "os", &os));
    sp_str_t abi = sp_zero_initialize();
    spn_try(get_str_optional(triple, "abi", &abi));

    triples[it] = (spn_triple_t) {
      .arch = spn_arch_from_str(arch),
      .os = spn_os_from_str(os),
      .abi = spn_abi_from_str(abi),
    };
  }

  return SPN_OK;
}

static spn_err_t load_profile(toml_table_t* toml, spn_pkg_info_t* pkg, sp_str_t name) {
  sp_str_t linkage = sp_str_lit("");
  sp_str_t standard = sp_str_lit("");
  sp_str_t toolchain = sp_str_lit("");
  sp_str_t mode = sp_str_lit("");
  sp_str_t os = sp_str_lit("");
  sp_str_t arch = sp_str_lit("");
  sp_str_t abi = sp_str_lit("");

  spn_try(get_str_optional(toml, "linkage", &linkage));
  spn_try(get_str_optional(toml, "standard", &standard));
  spn_try(get_str_optional(toml, "toolchain", &toolchain));
  spn_try(get_str_optional(toml, "mode", &mode));
  spn_try(get_str_optional(toml, "os", &os));
  spn_try(get_str_optional(toml, "arch", &arch));
  spn_try(get_str_optional(toml, "abi", &abi));

  spn_pkg_add_profile_ex(pkg, (spn_profile_info_t) {
    .name = spn_intern(name),
    .toolchain = spn_intern(toolchain),
    .linkage = sp_str_empty(linkage) ? 0 : spn_pkg_linkage_from_str(linkage),
    .standard = sp_str_empty(standard) ? 0 : spn_c_standard_from_str(standard),
    .mode = sp_str_empty(mode) ? 0 : spn_dep_build_mode_from_str(mode),
    .os = sp_str_empty(os) ? 0 : spn_os_from_str(os),
    .arch = sp_str_empty(arch) ? 0 : spn_arch_from_str(arch),
    .abi = sp_str_empty(abi) ? 0 : spn_abi_from_str(abi),
  });

  return SPN_OK;
}

spn_err_union_t spn_index_load(toml_table_t* toml, sp_str_t parent, u32 index, spn_index_info_t* result) {
  toml_path_t path = spn_pkg_toml_path_with_index(parent, index);
  sp_str_t name = SP_ZERO_INITIALIZE();
  sp_str_t url = SP_ZERO_INITIALIZE();
  sp_str_t protocol_str = SP_ZERO_INITIALIZE();

  spn_try_union(toml_get_str_required(toml, "name", path, &name));
  spn_try_union(toml_get_str_required(toml, "url", path, &url));
  spn_try_union(toml_get_str_required(toml, "protocol", path, &protocol_str));

  *result = (spn_index_info_t) {
    .name = name,
    .url = url,
    .protocol = spn_index_protocol_from_str(protocol_str),
  };

  return spn_result(SPN_OK);
}

spn_err_union_t spn_pkg_load(spn_pkg_info_t* pkg, sp_str_t manifest_path) {
  struct {
    toml_table_t* manifest;
    toml_table_t* package;
    toml_array_t* lib;
    toml_array_t* bin;
    toml_array_t* script;
    toml_array_t* test;
    toml_table_t* deps;
    toml_table_t* profile;
    toml_array_t* toolchain;
    toml_array_t* index;
    toml_table_t* options;
    toml_table_t* config;
  } toml = SP_ZERO_INITIALIZE();

  bool parse_error = false;
  toml.manifest = spn_toml_parse_ex(manifest_path, &parse_error);
  if (!toml.manifest) {
    if (parse_error) {
      return (spn_err_union_t) {
        .kind = SPN_ERR_MANIFEST_PARSE,
        .manifest_parse.path = manifest_path,
      };
    }

    return spn_result(SPN_ERROR);
  }

  toml_path_t root_path = spn_pkg_toml_path(sp_str_lit(""));
  toml_path_t package_path = spn_pkg_toml_path(sp_str_lit("package"));
  sp_str_t manifest_dir = sp_fs_parent_path(manifest_path);

  spn_try_union(toml_get_table_required(toml.manifest, "package", root_path, &toml.package));
  spn_try_union(toml_get_array_optional(toml.manifest, "lib", root_path, &toml.lib));
  spn_try_union(toml_get_array_optional(toml.manifest, "bin", root_path, &toml.bin));
  spn_try_union(toml_get_array_optional(toml.manifest, "script", root_path, &toml.script));
  spn_try_union(toml_get_array_optional(toml.manifest, "test", root_path, &toml.test));
  spn_try_union(toml_get_table_optional(toml.manifest, "profile", root_path, &toml.profile));
  spn_try_union(toml_get_array_optional(toml.manifest, "toolchain", root_path, &toml.toolchain));
  spn_try_union(toml_get_array_optional(toml.manifest, "index", root_path, &toml.index));
  spn_try_union(toml_get_table_optional(toml.manifest, "deps", root_path, &toml.deps));
  spn_try_union(toml_get_table_optional(toml.manifest, "options", root_path, &toml.options));
  spn_try_union(toml_get_table_optional(toml.manifest, "config", root_path, &toml.config));

  spn_err_t result = SPN_OK;

  sp_str_t package_namespace = sp_str_lit("");
  spn_try_union(toml_get_str_optional(toml.package, "namespace", package_path, &package_namespace));

  // spn_try_map_union(
  //   get_str_required(toml.package, "name", &package_name),
  //   it,
  //   add_error_path(it, package_path, "name")
  // );
  sp_str_t name = SP_ZERO_INITIALIZE();
  spn_try_union(toml_get_str_required(toml.package, "name", package_path, &name));

  sp_str_t url = sp_str_lit("");
  spn_try_union(toml_get_str_optional(toml.package, "url", package_path, &url));

  sp_str_t author = sp_str_lit("");
  spn_try_union(toml_get_str_optional(toml.package, "author", package_path, &author));

  sp_str_t maintainer = sp_str_lit("");
  spn_try_union(toml_get_str_optional(toml.package, "maintainer", package_path, &maintainer));

  sp_str_t version = SP_ZERO_INITIALIZE();
  spn_try_union(toml_get_str_required(toml.package, "version", package_path, &version));

  sp_str_t commit = sp_str_lit("");
  spn_try_union(toml_get_str_optional(toml.package, "commit", package_path, &commit));

  spn_pkg_init(pkg, name);
  pkg->namespace = package_namespace;
  pkg->qualified = spn_pkg_canonicalize_pair(pkg->namespace, pkg->name);
  spn_pkg_set_url_ex(pkg, url);
  spn_pkg_set_author_ex(pkg, author);
  spn_pkg_set_maintainer_ex(pkg, maintainer);
  spn_pkg_add_version_ex(pkg, spn_semver_from_str(version), commit);

  toml_array_t* include = toml_table_array(toml.package, "include");
  spn_toml_arr_for(include, it) {
    sp_str_t value = SP_ZERO_INITIALIZE();
    spn_try_union(toml_get_array_string_required(include, it, package_path, "include", &value));
    spn_pkg_add_include_ex(pkg, sp_fs_join_path(manifest_dir, value));
  }

  toml_array_t* define = toml_table_array(toml.package, "define");
  spn_toml_arr_for(define, it) {
    sp_str_t value = SP_ZERO_INITIALIZE();
    spn_try_union(toml_get_array_string_required(define, it, package_path, "define", &value));
    spn_pkg_add_define_ex(pkg, value);
  }

  toml_array_t* system_deps = toml_table_array(toml.package, "system_deps");
  spn_toml_arr_for(system_deps, it) {
    sp_str_t value = SP_ZERO_INITIALIZE();
    spn_try_union(toml_get_array_string_required(system_deps, it, package_path, "system_deps", &value));
    spn_pkg_add_system_dep_ex(pkg, value);
  }

  spn_toml_arr_for(toml.lib, n) {
    toml_table_t* it = SP_NULLPTR;
    spn_try_union(toml_get_array_table_required(toml.lib, n, sp_str_lit("lib"), &it));
    toml_path_t path = spn_pkg_toml_path_with_index(sp_str_lit("lib"), n);

    sp_str_t name = SP_ZERO_INITIALIZE();
    toml_array_t* kinds = SP_NULLPTR;
    spn_try_union(toml_get_str_required(it, "name", path, &name));
    spn_try_union(toml_get_array_required(it, "kinds", path, &kinds));

    spn_linkage_set_t linkages = SP_ZERO_INITIALIZE();
    spn_toml_arr_for(kinds, k) {
      sp_str_t kind = SP_ZERO_INITIALIZE();
      spn_try_union(toml_get_array_string_required(kinds, k, path, "kinds", &kind));
      spn_linkage_set_add(&linkages, spn_pkg_linkage_from_str(kind));
    }

    spn_try_union(ensure_unique_target_name(pkg, path, name));
    spn_linkage_t linkage = spn_linkage_set_default(linkages);
    spn_target_info_t* lib = spn_pkg_add_lib_ex(pkg, name, linkage);
    lib->linkages = linkages;
    spn_try_union(load_target(it, path, lib));
  }

  spn_toml_arr_for(toml.toolchain, n) {
    toml_table_t* it = SP_NULLPTR;
    spn_try_union(toml_get_array_table_required(toml.toolchain, n, sp_str_lit("toolchain"), &it));
    toml_path_t path = spn_pkg_toml_path_with_index(sp_str_lit("profile"), n);

    spn_toolchain_info_t toolchain = sp_zero_initialize();
    sp_str_t driver = sp_zero_initialize();

    spn_try_as_union(get_str_optional(it, "name", &toolchain.name));
    toolchain.name = spn_intern(toolchain.name);
    spn_try_as_union(get_str_optional(it, "url", &toolchain.url));
    toolchain.url = spn_intern(toolchain.url);
    sp_str_t compiler_str = sp_zero_initialize();
    sp_str_t linker_str = sp_zero_initialize();
    sp_str_t archiver_str = sp_zero_initialize();
    spn_try_as_union(get_str_optional(it, "compiler", &compiler_str));
    spn_try_as_union(get_str_optional(it, "linker", &linker_str));
    spn_try_as_union(get_str_optional(it, "archiver", &archiver_str));
    toolchain.compiler = split_launcher(compiler_str);
    toolchain.linker = split_launcher(linker_str);
    toolchain.archiver = split_launcher(archiver_str);
    spn_try_as_union(get_str_optional(it, "sysroot", &toolchain.sysroot));
    toolchain.sysroot = spn_intern(toolchain.sysroot);
    spn_try_as_union(get_bool_optional(it, "export", &toolchain.export));
    spn_try_as_union(get_str_optional(it, "driver", &driver));
    toolchain.driver = spn_cc_driver_from_str(driver);

    sp_str_t package = sp_zero_initialize();
    spn_try_as_union(get_str_optional(it, "package", &package));

    if (!sp_str_empty(package)) {
      // INDEX toolchain: references a dependency
      sp_str_t version = sp_str_lit("*");
      spn_try_as_union(get_str_optional(it, "version", &version));

      spn_pkg_add_toolchain(pkg, (spn_toolchain_entry_t) {
        .name = package,
        .kind = SPN_TOOLCHAIN_INDEX,
        .request = {
          .package = spn_pkg_canonicalize_name(package),
          .range = spn_semver_parse_range(version)
        },
      });
    }
    else {
      // INLINE toolchain: defined in this manifest
      if (sp_str_empty(toolchain.name))              return spn_result(SPN_ERROR);
      if (sp_str_empty(toolchain.compiler.program))  return spn_result(SPN_ERROR);
      if (sp_str_empty(toolchain.linker.program))    return spn_result(SPN_ERROR);
      if (sp_str_empty(toolchain.archiver.program))  return spn_result(SPN_ERROR);
      if (toolchain.driver == SPN_CC_DRIVER_NONE)    return spn_result(SPN_ERROR);

      toml_array_t* hosts = SP_NULLPTR;
      spn_try_as_union(get_arr_required(it, "host", &hosts));
      spn_try_as_union(load_triple_array(hosts, toolchain.hosts, SPN_TOOLCHAIN_MAX_HOSTS));

      toml_array_t* target = SP_NULLPTR;
      spn_try_as_union(get_arr_required(it, "target", &target));
      spn_try_as_union(load_triple_array(target, toolchain.targets, SPN_TOOLCHAIN_MAX_TARGETS));

      spn_try_as_union(spn_pkg_add_toolchain(pkg, (spn_toolchain_entry_t) {
        .name = toolchain.name,
        .kind = SPN_TOOLCHAIN_INLINE,
        .info = toolchain,
      }));
    }
  }

  if (toml.profile) {
    const c8* key = SP_NULLPTR;
    spn_toml_for(toml.profile, it, key) {
      toml_table_t* profile = SP_NULLPTR;
      spn_try_as_union(get_table_required(toml.profile, key, &profile));
      spn_try_as_union(load_profile(profile, pkg, sp_str_view(key)));
    }
  }

  spn_toml_arr_for(toml.index, n) {
    toml_table_t* it = SP_NULLPTR;
    spn_try_union(toml_get_array_table_required(toml.index, n, sp_str_lit("index"), &it));

    spn_index_info_t index = SP_ZERO_INITIALIZE();
    spn_try_union(spn_index_load(it, sp_str_lit("index"), n, &index));
    index.kind = SPN_INDEX_WORKSPACE;
    sp_om_insert(pkg->indexes, index.name, index);
  }

  spn_toml_arr_for(toml.bin, n) {
    toml_table_t* it = SP_NULLPTR;
    spn_try_union(toml_get_array_table_required(toml.bin, n, sp_str_lit("bin"), &it));
    toml_path_t path = spn_pkg_toml_path_with_index(sp_str_lit("bin"), n);

    sp_str_t name = SP_ZERO_INITIALIZE();
    spn_try_union(toml_get_str_required(it, "name", path, &name));
    spn_try_union(ensure_unique_target_name(pkg, path, name));

    spn_target_info_t* bin = spn_pkg_add_exe_ex(pkg, name);
    spn_try_union(load_target(it, path, bin));

    sp_str_t kind = sp_str_lit("");
    spn_try_union(toml_get_str_optional(it, "kind", path, &kind));
    if (!sp_str_empty(kind)) {
      spn_target_set_visibility(bin, spn_visibility_from_str(kind));
    }
  }

  spn_toml_arr_for(toml.script, n) {
    toml_table_t* it = SP_NULLPTR;
    spn_try_union(toml_get_array_table_required(toml.script, n, sp_str_lit("script"), &it));
    toml_path_t path = spn_pkg_toml_path_with_index(sp_str_lit("script"), n);

    sp_str_t name = SP_ZERO_INITIALIZE();
    spn_try_union(toml_get_str_required(it, "name", path, &name));
    spn_try_union(ensure_unique_target_name(pkg, path, name));

    spn_target_info_t* script = spn_pkg_add_script_ex(pkg, name);
    spn_try_union(load_target(it, path, script));
  }

  spn_toml_arr_for(toml.test, n) {
    toml_table_t* it = SP_NULLPTR;
    spn_try_union(toml_get_array_table_required(toml.test, n, sp_str_lit("test"), &it));
    toml_path_t path = spn_pkg_toml_path_with_index(sp_str_lit("test"), n);

    sp_str_t name = SP_ZERO_INITIALIZE();
    spn_try_union(toml_get_str_required(it, "name", path, &name));
    spn_try_union(ensure_unique_target_name(pkg, path, name));

    spn_target_info_t* test = spn_pkg_add_test_ex(pkg, name);
    spn_try_union(load_target(it, path, test));
  }

  if (toml.deps) {
    struct {
      toml_table_t* package;
      toml_table_t* test;
      toml_table_t* build;
    } deps = SP_ZERO_INITIALIZE();
    toml_path_t deps_path = spn_pkg_toml_path(sp_str_lit("deps"));

    spn_try_union(toml_get_table_optional(toml.deps, "package", deps_path, &deps.package));
    spn_try_union(toml_get_table_optional(toml.deps, "test", deps_path, &deps.test));
    spn_try_union(toml_get_table_optional(toml.deps, "build", deps_path, &deps.build));

    spn_try_union(load_deps(deps.package, pkg, SPN_VISIBILITY_PUBLIC, manifest_dir));
    spn_try_union(load_deps(deps.test, pkg, SPN_VISIBILITY_TEST, manifest_dir));
    spn_try_union(load_deps(deps.build, pkg, SPN_VISIBILITY_BUILD, manifest_dir));
  }

  if (toml.options) {
    const c8* key = SP_NULLPTR;
    spn_toml_for(toml.options, n, key) {
      spn_dep_option_t option = parse_option(toml.options, key);
      sp_ht_insert(pkg->options, option.name, option);
    }
  }

  if (toml.config) {
    sp_da(toml_table_t*) configs = SP_ZERO_INITIALIZE();
    toml_path_t config_path = spn_pkg_toml_path(sp_str_lit("config"));
    const c8* key = SP_NULLPTR;
    spn_toml_for(toml.config, n, key) {
      toml_table_t* config = SP_NULLPTR;
      spn_try_union(toml_get_table_required(toml.config, key, config_path, &config));
      sp_da_push(configs, config);
    }

    sp_da_for(configs, it) {
      toml_table_t* config = configs[it];
      sp_str_t name = sp_str_from_cstr(config->key);

      spn_dep_options_t options = SP_NULLPTR;
      sp_ht_set_fns(options, sp_ht_on_hash_str_key, sp_ht_on_compare_str_key);

      const c8* option_key = SP_NULLPTR;
      spn_toml_for(config, n, option_key) {
        spn_dep_option_t option = parse_option(config, option_key);
        sp_ht_insert(options, option.name, option);
      }

      sp_ht_insert(pkg->config, name, options);
    }
  }

  return spn_result(SPN_OK);
}
