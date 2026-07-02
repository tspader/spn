#include "sp.h"
#include "sp/macro.h"
#include "error/types.h"
#include "pkg/types.h"

#include "enum/enum.h"
#include "external/tom.h"
#include "pkg/load.h"
#include "spn.h"
#include "toml.h"

typedef struct {
  sp_str_t parent;
  bool has_index;
  u32 index;
} toml_path_t;

static toml_path_t spn_pkg_toml_path_with_index(sp_str_t parent, u32 index) {
  return (toml_path_t) {
    .parent = parent,
    .has_index = true,
    .index = index,
  };
}

static sp_str_t spn_pkg_toml_path_field(sp_mem_t mem, toml_path_t path, const c8* key) {
  if (sp_str_empty(path.parent)) {
    return sp_str_view(key);
  }

  if (path.has_index) {
    return sp_fmt(mem, "{}[{}].{}", SP_FMT_STR(path.parent), SP_FMT_U32(path.index), SP_FMT_CSTR(key)).value;
  }

  return sp_fmt(mem, "{}.{}", SP_FMT_STR(path.parent), SP_FMT_CSTR(key)).value;
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

static spn_err_union_t toml_get_str_required(sp_mem_t mem, toml_table_t* table, const c8* key, toml_path_t path, sp_str_t* out) {
  spn_toml_value_kind_t kind = toml_kind_from_field(table, key);
  if (kind == SPN_TOML_VALUE_KIND_NONE) {
    return field_missing_error(spn_pkg_toml_path_field(mem, path, key), sp_str_lit("string"));
  }

  if (kind != SPN_TOML_VALUE_KIND_SCALAR) {
    return field_type_error(spn_pkg_toml_path_field(mem, path, key), sp_str_lit("string"), kind);
  }

  toml_value_t value = toml_table_string(table, key);
  if (!value.ok) {
    return field_type_error(spn_pkg_toml_path_field(mem, path, key), sp_str_lit("string"), SPN_TOML_VALUE_KIND_SCALAR);
  }

  *out = sp_str_view(value.u.s);
  return spn_result(SPN_OK);
}

static spn_err_union_t toml_get_str_optional(sp_mem_t mem, toml_table_t* table, const c8* key, toml_path_t path, sp_str_t* out) {
  spn_toml_value_kind_t kind = toml_kind_from_field(table, key);
  if (kind == SPN_TOML_VALUE_KIND_NONE) {
    return spn_result(SPN_OK);
  }

  if (kind != SPN_TOML_VALUE_KIND_SCALAR) {
    return field_type_error(spn_pkg_toml_path_field(mem, path, key), sp_str_lit("string"), kind);
  }

  toml_value_t value = toml_table_string(table, key);
  if (!value.ok) {
    return field_type_error(spn_pkg_toml_path_field(mem, path, key), sp_str_lit("string"), SPN_TOML_VALUE_KIND_SCALAR);
  }

  *out = sp_str_view(value.u.s);
  return spn_result(SPN_OK);
}

spn_err_union_t spn_index_load(sp_mem_t mem, toml_table_t* toml, sp_str_t parent, u32 index, spn_index_info_t* result) {
  toml_path_t path = spn_pkg_toml_path_with_index(parent, index);
  sp_str_t name = SP_ZERO_INITIALIZE();
  sp_str_t url = SP_ZERO_INITIALIZE();
  sp_str_t rev = SP_ZERO_INITIALIZE();
  sp_str_t protocol_str = SP_ZERO_INITIALIZE();

  spn_try_union(toml_get_str_required(mem, toml, "name", path, &name));
  spn_try_union(toml_get_str_required(mem, toml, "url", path, &url));
  spn_try_union(toml_get_str_required(mem, toml, "protocol", path, &protocol_str));
  spn_try_union(toml_get_str_optional(mem, toml, "rev", path, &rev));

  *result = (spn_index_info_t) {
    .name = name,
    .url = url,
    .rev = rev,
    .protocol = spn_index_protocol_from_str(protocol_str),
  };

  return spn_result(SPN_OK);
}

spn_pkg_tree_t spn_pkg_manifest_source_tree(spn_pkg_info_t* info) {
  spn_pkg_metadata_t* meta = sp_ht_getp(info->metadata, info->version);
  if (!sp_str_empty(info->url) && meta && !sp_str_empty(meta->commit)) {
    return (spn_pkg_tree_t) {
      .kind = SPN_PKG_TREE_GIT,
      .git = { .url = info->url, .rev = meta->commit },
    };
  }

  return (spn_pkg_tree_t) { .kind = SPN_PKG_TREE_NONE };
}
