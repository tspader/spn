#include "jtd.h"
#include "yyjson.h"

const c8* jtd_err_name(jtd_err_t err) {
  switch (err) {
    case JTD_OK: return "ok";
    case JTD_ERR: return "unknown";
    case JTD_ERR_IO: return "io";
    case JTD_ERR_JSON: return "invalid-json";
    case JTD_ERR_SCHEMA_NOT_OBJECT: return "schema-not-object";
    case JTD_ERR_MULTIPLE_FORMS: return "multiple-forms";
    case JTD_ERR_TYPE_NOT_STRING: return "type-not-string";
    case JTD_ERR_UNKNOWN_TYPE: return "unknown-type";
    case JTD_ERR_ENUM_EMPTY: return "enum-empty";
    case JTD_ERR_ENUM_NOT_STRING: return "enum-not-string";
    case JTD_ERR_ENUM_DUPLICATE: return "enum-duplicate";
    case JTD_ERR_PROPERTIES_NOT_OBJECT: return "properties-not-object";
    case JTD_ERR_DISCRIMINATOR_NOT_STRING: return "discriminator-not-string";
    case JTD_ERR_MAPPING_NOT_OBJECT: return "mapping-not-object";
    case JTD_ERR_MAPPING_NOT_PROPERTIES: return "mapping-not-properties";
    case JTD_ERR_REF_NOT_STRING: return "ref-not-string";
    case JTD_ERR_METADATA_NOT_OBJECT: return "metadata-not-object";
    case JTD_ERR_NULLABLE_NOT_BOOL: return "nullable-not-bool";
    case JTD_ERR_ADDITIONAL_PROPERTIES_NOT_BOOL: return "additional-properties-not-bool";
    case JTD_ERR_ADDITIONAL_PROPERTIES_WITHOUT_PROPERTIES: return "additional-properties-without-properties";
    case JTD_ERR_PROPERTIES_DUPLICATE: return "properties-duplicate";
    case JTD_ERR_DISCRIMINATOR_TAG_REDEFINED: return "discriminator-tag-redefined";
    case JTD_ERR_DEFINITIONS_NOT_ROOT: return "definitions-not-root";
    case JTD_ERR_REF_UNRESOLVED: return "ref-unresolved";
    case JTD_ERR_REF_CYCLE: return "ref-cycle";
    case JTD_ERR_UNKNOWN_MEMBER: return "unknown-member";
    case JTD_ERR_UNRECOGNIZED_FORM: return "unrecognized-form";
  }
  return "unknown";
}

const c8* jtd_form_name(jtd_form_t form) {
  switch (form) {
    case JTD_FORM_EMPTY: return "empty";
    case JTD_FORM_TYPE: return "type";
    case JTD_FORM_ENUM: return "enum";
    case JTD_FORM_ELEMENTS: return "elements";
    case JTD_FORM_PROPERTIES: return "properties";
    case JTD_FORM_VALUES: return "values";
    case JTD_FORM_DISCRIMINATOR: return "discriminator";
    case JTD_FORM_REF: return "ref";
  }
  return "unknown";
}

const c8* jtd_type_name(jtd_type_t type) {
  switch (type) {
    case JTD_TYPE_BOOLEAN: return "boolean";
    case JTD_TYPE_FLOAT32: return "float32";
    case JTD_TYPE_FLOAT64: return "float64";
    case JTD_TYPE_INT8: return "int8";
    case JTD_TYPE_UINT8: return "uint8";
    case JTD_TYPE_INT16: return "int16";
    case JTD_TYPE_UINT16: return "uint16";
    case JTD_TYPE_INT32: return "int32";
    case JTD_TYPE_UINT32: return "uint32";
    case JTD_TYPE_INT64: return "int64";
    case JTD_TYPE_UINT64: return "uint64";
    case JTD_TYPE_STRING: return "string";
    case JTD_TYPE_TIMESTAMP: return "timestamp";
  }
  return "unknown";
}

static const struct {
  const c8* name;
  jtd_type_t type;
} JTD_PRIMITIVES[] = {
  { "boolean", JTD_TYPE_BOOLEAN },
  { "float32", JTD_TYPE_FLOAT32 },
  { "float64", JTD_TYPE_FLOAT64 },
  { "int8", JTD_TYPE_INT8 },
  { "uint8", JTD_TYPE_UINT8 },
  { "int16", JTD_TYPE_INT16 },
  { "uint16", JTD_TYPE_UINT16 },
  { "int32", JTD_TYPE_INT32 },
  { "uint32", JTD_TYPE_UINT32 },
  { "int64", JTD_TYPE_INT64 },
  { "uint64", JTD_TYPE_UINT64 },
  { "string", JTD_TYPE_STRING },
  { "timestamp", JTD_TYPE_TIMESTAMP },
};

typedef struct {
  sp_mem_arena_t* temp_arena;
  sp_mem_t result;
  sp_mem_t temp;
  jtd_result_t* out;
} jtd_ctx_t;

static sp_str_t jtd_yj_str(sp_mem_t mem, yyjson_val* v) {
  return sp_str_copy(mem, sp_str((c8*)yyjson_get_str(v), (u32)yyjson_get_len(v)));
}

static sp_str_t jtd_path_escape(sp_mem_t mem, sp_str_t seg) {
  u32 len = seg.len;
  sp_for(it, seg.len) {
    if (seg.data[it] == '~' || seg.data[it] == '/') {
      len++;
    }
  }

  if (len == seg.len) {
    return seg;
  }

  c8* data = sp_alloc(mem, len);
  u32 pos = 0;
  sp_for(it, seg.len) {
    switch (seg.data[it]) {
      case '~': {
        data[pos++] = '~';
        data[pos++] = '0';
        break;
      }
      case '/': {
        data[pos++] = '~';
        data[pos++] = '1';
        break;
      }
      default: {
        data[pos++] = seg.data[it];
        break;
      }
    }
  }

  return sp_str(data, len);
}

static sp_str_t jtd_path_seg(sp_mem_t mem, sp_str_t base, sp_str_t seg) {
  return sp_fmt(mem, "{}/{}", sp_fmt_str(base), sp_fmt_str(jtd_path_escape(mem, seg))).value;
}

static sp_str_t jtd_path_seg_cstr(sp_mem_t mem, sp_str_t base, const c8* seg) {
  return jtd_path_seg(mem, base, sp_str_view(seg));
}

static sp_str_t jtd_path_seg_cstr_cstr(sp_mem_t mem, sp_str_t base, const c8* a, const c8* b) {
  return jtd_path_seg_cstr(mem, jtd_path_seg_cstr(mem, base, a), b);
}

static sp_str_t jtd_path_seg_cstr_str(sp_mem_t mem, sp_str_t base, const c8* a, sp_str_t b) {
  return jtd_path_seg(mem, jtd_path_seg_cstr(mem, base, a), b);
}

static bool jtd_diag(sp_mem_t mem, jtd_diagnostic_t* diag, jtd_err_t code, sp_str_t path) {
  if (diag) {
    *diag = sp_zero_s(jtd_diagnostic_t);
    diag->code = code;
    diag->path = sp_str_copy(mem, path);
  }
  return false;
}

static bool jtd_diag_str(sp_mem_t mem, jtd_diagnostic_t* diag, jtd_err_t code, sp_str_t path, sp_str_t str) {
  jtd_diag(mem, diag, code, path);
  if (diag) {
    sp_str_t v = sp_str_copy(mem, str);
    switch (code) {
      case JTD_ERR_UNKNOWN_TYPE: diag->as.unknown_type.name = v; break;
      case JTD_ERR_ENUM_DUPLICATE: diag->as.enum_duplicate.value = v; break;
      case JTD_ERR_REF_UNRESOLVED: diag->as.ref_unresolved.name = v; break;
      case JTD_ERR_REF_CYCLE: diag->as.ref_cycle.name = v; break;
      case JTD_ERR_UNKNOWN_MEMBER: diag->as.unknown_member.key = v; break;
      default: break;
    }
  }
  return false;
}

static bool jtd_fail(jtd_ctx_t* ctx, jtd_err_t code, sp_str_t path) {
  return jtd_diag(ctx->result, &ctx->out->diag, code, path);
}

static bool jtd_fail_str(jtd_ctx_t* ctx, jtd_err_t code, sp_str_t path, sp_str_t str) {
  return jtd_diag_str(ctx->result, &ctx->out->diag, code, path, str);
}

static jtd_schema_t* jtd_parse_schema(jtd_ctx_t* ctx, yyjson_val* v, sp_str_t path);

static bool jtd_parse_shared(jtd_ctx_t* ctx, jtd_schema_t* s, yyjson_val* v, sp_str_t path) {
  yyjson_val* metadata = yyjson_obj_get(v, "metadata");
  if (metadata) {
    if (!yyjson_is_obj(metadata)) {
      return jtd_fail(ctx, JTD_ERR_METADATA_NOT_OBJECT, jtd_path_seg_cstr(ctx->temp, path, "metadata"));
    }

    s->metadata = sp_da_new(ctx->result, jtd_metadata_t);
    size_t idx, max;
    yyjson_val *k, *mv;
    yyjson_obj_foreach(metadata, idx, max, k, mv) {
      sp_str_t value = sp_zero;
      if (yyjson_is_str(mv)) {
        value = jtd_yj_str(ctx->result, mv);
      }
      else if (yyjson_is_bool(mv)) {
        value = yyjson_get_bool(mv) ? sp_str_lit("true") : sp_str_lit("false");
      }
      else {
        continue;
      }

      jtd_metadata_t entry = { .key = jtd_yj_str(ctx->result, k), .value = value };
      sp_da_push(s->metadata, entry);
    }
  }

  yyjson_val* nullable = yyjson_obj_get(v, "nullable");
  if (nullable) {
    if (!yyjson_is_bool(nullable)) {
      return jtd_fail(ctx, JTD_ERR_NULLABLE_NOT_BOOL, path);
    }
    s->nullable = yyjson_get_bool(nullable);
  }

  return true;
}

static bool jtd_schema_key(sp_str_t key) {
  static const c8* keys[] = {
    "metadata",
    "nullable",
    "type",
    "enum",
    "elements",
    "properties",
    "optionalProperties",
    "additionalProperties",
    "values",
    "discriminator",
    "mapping",
    "ref",
  };

  sp_carr_for(keys, it) {
    if (sp_str_equal_cstr(key, keys[it])) {
      return true;
    }
  }

  return false;
}

static bool jtd_root_key(sp_str_t key) {
  return jtd_schema_key(key) || sp_str_equal_cstr(key, "definitions");
}

static bool jtd_validate_keys(jtd_ctx_t* ctx, yyjson_val* v, sp_str_t path) {
  size_t idx, max;
  yyjson_val *k, *mv;

  yyjson_obj_foreach(v, idx, max, k, mv) {
    sp_str_t key = jtd_yj_str(ctx->temp, k);
    if (!jtd_schema_key(key)) {
      return jtd_fail_str(ctx, JTD_ERR_UNKNOWN_MEMBER, jtd_path_seg(ctx->temp, path, key), key);
    }
  }

  return true;
}

static bool jtd_validate_root_keys(jtd_ctx_t* ctx, yyjson_val* v, sp_str_t path) {
  size_t idx, max;
  yyjson_val *k, *mv;

  yyjson_obj_foreach(v, idx, max, k, mv) {
    sp_str_t key = jtd_yj_str(ctx->temp, k);
    if (!jtd_root_key(key)) {
      return jtd_fail_str(ctx, JTD_ERR_UNKNOWN_MEMBER, jtd_path_seg(ctx->temp, path, key), key);
    }
  }

  return true;
}

static jtd_schema_t* jtd_parse_properties(jtd_ctx_t* ctx, jtd_schema_t* s, yyjson_val* props, yyjson_val* object, yyjson_val* additional, sp_str_t path) {
  s->form = JTD_FORM_PROPERTIES;
  s->as.properties.required = sp_da_new(ctx->result, jtd_property_t);
  s->as.properties.optional = sp_da_new(ctx->result, jtd_property_t);
  s->as.properties.all = sp_da_new(ctx->result, jtd_property_t);

  if (additional && !yyjson_is_bool(additional)) {
    jtd_fail(ctx, JTD_ERR_ADDITIONAL_PROPERTIES_NOT_BOOL, jtd_path_seg_cstr(ctx->temp, path, "additionalProperties"));
    return SP_NULLPTR;
  }

  if (additional && !props && !object) {
    jtd_fail(ctx, JTD_ERR_ADDITIONAL_PROPERTIES_WITHOUT_PROPERTIES, jtd_path_seg_cstr(ctx->temp, path, "additionalProperties"));
    return SP_NULLPTR;
  }

  size_t idx, max;
  yyjson_val *k, *mv;

  if (props) {
    if (!yyjson_is_obj(props)) {
      jtd_fail(ctx, JTD_ERR_PROPERTIES_NOT_OBJECT, path);
      return SP_NULLPTR;
    }

    yyjson_obj_foreach(props, idx, max, k, mv) {
      sp_str_t key = jtd_yj_str(ctx->result, k);
      jtd_schema_t* child = jtd_parse_schema(ctx, mv, jtd_path_seg_cstr_str(ctx->temp, path, "properties", key));
      if (!child) return SP_NULLPTR;
      jtd_property_t p = { .key = key, .schema = child, .required = true };
      sp_da_push(s->as.properties.required, p);
      sp_da_push(s->as.properties.all, p);
    }
  }

  if (object) {
    if (!yyjson_is_obj(object)) {
      jtd_fail(ctx, JTD_ERR_PROPERTIES_NOT_OBJECT, path);
      return SP_NULLPTR;
    }

    yyjson_obj_foreach(object, idx, max, k, mv) {
      sp_str_t key = jtd_yj_str(ctx->result, k);
      sp_da_for(s->as.properties.required, it) {
        if (sp_str_equal(key, s->as.properties.required[it].key)) {
          jtd_fail(ctx, JTD_ERR_PROPERTIES_DUPLICATE, jtd_path_seg_cstr_str(ctx->temp, path, "optionalProperties", key));
          return SP_NULLPTR;
        }
      }
      jtd_schema_t* child = jtd_parse_schema(ctx, mv, jtd_path_seg_cstr_str(ctx->temp, path, "optionalProperties", key));
      if (!child) return SP_NULLPTR;
      jtd_property_t p = { .key = key, .schema = child, .required = false };
      sp_da_push(s->as.properties.optional, p);
      sp_da_push(s->as.properties.all, p);
    }
  }

  if (additional) {
    s->as.properties.additional = yyjson_get_bool(additional);
  }

  return s;
}

static jtd_schema_t* jtd_parse_schema_object(jtd_ctx_t* ctx, yyjson_val* v, sp_str_t path) {
  jtd_schema_t* s = sp_alloc_type(ctx->result, jtd_schema_t);
  *s = sp_zero_s(jtd_schema_t);

  if (!jtd_parse_shared(ctx, s, v, path)) {
    return SP_NULLPTR;
  }

  struct {
    yyjson_val* type;
    yyjson_val* enumeration;
    yyjson_val* elements;
    struct {
      yyjson_val* base;
      yyjson_val* optional;
      yyjson_val* additional;
    } props;
    yyjson_val* values;
    yyjson_val* discriminator;
    yyjson_val* mapping;
    yyjson_val* ref;
  } o = {
    .type = yyjson_obj_get(v, "type"),
    .enumeration = yyjson_obj_get(v, "enum"),
    .elements = yyjson_obj_get(v, "elements"),
    .props = {
      .base = yyjson_obj_get(v, "properties"),
      .optional = yyjson_obj_get(v, "optionalProperties"),
      .additional = yyjson_obj_get(v, "additionalProperties"),
    },
    .values = yyjson_obj_get(v, "values"),
    .discriminator = yyjson_obj_get(v, "discriminator"),
    .mapping = yyjson_obj_get(v, "mapping"),
    .ref = yyjson_obj_get(v, "ref"),
  };

  u32 forms =
    (o.type != SP_NULLPTR) +
    (o.enumeration != SP_NULLPTR) +
    (o.elements != SP_NULLPTR) +
    (o.props.base || o.props.optional || o.props.additional) +
    (o.values != SP_NULLPTR) +
    (o.discriminator || o.mapping) +
    (o.ref != SP_NULLPTR);

  if (forms > 1) {
    jtd_fail(ctx, JTD_ERR_MULTIPLE_FORMS, path);
    return SP_NULLPTR;
  }

  if (forms == 0) {
    s->form = JTD_FORM_EMPTY;
    return s;
  }

  if (o.type) {
    if (!yyjson_is_str(o.type)) { jtd_fail(ctx, JTD_ERR_TYPE_NOT_STRING, path); return SP_NULLPTR; }
    sp_str_t name = sp_str((c8*)yyjson_get_str(o.type), (u32)yyjson_get_len(o.type));
    sp_carr_for(JTD_PRIMITIVES, i) {
      if (sp_str_equal_cstr(name, JTD_PRIMITIVES[i].name)) {
        s->form = JTD_FORM_TYPE;
        s->as.type = JTD_PRIMITIVES[i].type;
        return s;
      }
    }
    jtd_fail_str(ctx, JTD_ERR_UNKNOWN_TYPE, path, name);
    return SP_NULLPTR;
  }

  if (o.enumeration) {
    if (!yyjson_is_arr(o.enumeration)) { jtd_fail(ctx, JTD_ERR_ENUM_NOT_STRING, path); return SP_NULLPTR; }
    s->form = JTD_FORM_ENUM;
    s->as.enumeration.values = sp_da_new(ctx->result, sp_str_t);

    size_t idx, max;
    yyjson_val* ev;
    yyjson_arr_foreach(o.enumeration, idx, max, ev) {
      if (!yyjson_is_str(ev)) { jtd_fail(ctx, JTD_ERR_ENUM_NOT_STRING, path); return SP_NULLPTR; }
      sp_str_t val = jtd_yj_str(ctx->result, ev);
      sp_da_for(s->as.enumeration.values, j) {
        if (sp_str_equal(val, s->as.enumeration.values[j])) {
          jtd_fail_str(ctx, JTD_ERR_ENUM_DUPLICATE, path, val);
          return SP_NULLPTR;
        }
      }
      sp_da_push(s->as.enumeration.values, val);
    }

    if (sp_da_size(s->as.enumeration.values) == 0) {
      jtd_fail(ctx, JTD_ERR_ENUM_EMPTY, path);
      return SP_NULLPTR;
    }
    return s;
  }

  if (o.elements) {
    s->form = JTD_FORM_ELEMENTS;
    jtd_schema_t* child = jtd_parse_schema(ctx, o.elements, jtd_path_seg_cstr(ctx->temp, path, "elements"));
    if (!child) return SP_NULLPTR;
    s->as.elements.schema = child;
    return s;
  }

  if (o.props.base || o.props.optional || o.props.additional) {
    return jtd_parse_properties(ctx, s, o.props.base, o.props.optional, o.props.additional, path);
  }

  if (o.values) {
    s->form = JTD_FORM_VALUES;
    jtd_schema_t* child = jtd_parse_schema(ctx, o.values, jtd_path_seg_cstr(ctx->temp, path, "values"));
    if (!child) return SP_NULLPTR;
    s->as.values.schema = child;
    return s;
  }

  if (o.discriminator || o.mapping) {
    s->form = JTD_FORM_DISCRIMINATOR;
    if (!o.discriminator || !yyjson_is_str(o.discriminator)) { jtd_fail(ctx, JTD_ERR_DISCRIMINATOR_NOT_STRING, path); return SP_NULLPTR; }
    if (!o.mapping || !yyjson_is_obj(o.mapping)) { jtd_fail(ctx, JTD_ERR_MAPPING_NOT_OBJECT, path); return SP_NULLPTR; }

    s->as.discriminator.tag = jtd_yj_str(ctx->result, o.discriminator);
    s->as.discriminator.mapping = sp_da_new(ctx->result, jtd_mapping_t);

    size_t idx, max;
    yyjson_val *k, *mv;
    yyjson_obj_foreach(o.mapping, idx, max, k, mv) {
      sp_str_t tag = jtd_yj_str(ctx->result, k);
      sp_str_t cpath = jtd_path_seg_cstr_str(ctx->temp, path, "mapping", tag);
      jtd_schema_t* child = jtd_parse_schema(ctx, mv, cpath);
      if (!child) return SP_NULLPTR;
      if (child->form != JTD_FORM_PROPERTIES || child->nullable) {
        jtd_fail(ctx, JTD_ERR_MAPPING_NOT_PROPERTIES, cpath);
        return SP_NULLPTR;
      }
      sp_da_for(child->as.properties.required, it) {
        jtd_property_t* p = &child->as.properties.required[it];
        if (sp_str_equal(p->key, s->as.discriminator.tag)) {
          jtd_fail(ctx, JTD_ERR_DISCRIMINATOR_TAG_REDEFINED, jtd_path_seg_cstr_str(ctx->temp, cpath, "properties", p->key));
          return SP_NULLPTR;
        }
      }
      sp_da_for(child->as.properties.optional, it) {
        jtd_property_t* p = &child->as.properties.optional[it];
        if (sp_str_equal(p->key, s->as.discriminator.tag)) {
          jtd_fail(ctx, JTD_ERR_DISCRIMINATOR_TAG_REDEFINED, jtd_path_seg_cstr_str(ctx->temp, cpath, "optionalProperties", p->key));
          return SP_NULLPTR;
        }
      }
      jtd_mapping_t m = { .tag = tag, .schema = child };
      sp_da_push(s->as.discriminator.mapping, m);
    }
    return s;
  }

  if (o.ref) {
    if (!yyjson_is_str(o.ref)) { jtd_fail(ctx, JTD_ERR_REF_NOT_STRING, path); return SP_NULLPTR; }
    s->form = JTD_FORM_REF;
    s->as.ref.name = jtd_yj_str(ctx->result, o.ref);
    return s;
  }

  jtd_fail(ctx, JTD_ERR_UNRECOGNIZED_FORM, path);
  return SP_NULLPTR;
}

static jtd_schema_t* jtd_parse_schema(jtd_ctx_t* ctx, yyjson_val* v, sp_str_t path) {
  if (!yyjson_is_obj(v)) {
    jtd_fail(ctx, JTD_ERR_SCHEMA_NOT_OBJECT, path);
    return SP_NULLPTR;
  }

  if (yyjson_obj_get(v, "definitions")) {
    jtd_fail(ctx, JTD_ERR_DEFINITIONS_NOT_ROOT, jtd_path_seg_cstr(ctx->temp, path, "definitions"));
    return SP_NULLPTR;
  }

  if (!jtd_validate_keys(ctx, v, path)) {
    return SP_NULLPTR;
  }

  return jtd_parse_schema_object(ctx, v, path);
}

static jtd_schema_t* jtd_parse_root_schema(jtd_ctx_t* ctx, yyjson_val* v, sp_str_t path) {
  if (!yyjson_is_obj(v)) {
    jtd_fail(ctx, JTD_ERR_SCHEMA_NOT_OBJECT, path);
    return SP_NULLPTR;
  }

  if (!jtd_validate_root_keys(ctx, v, path)) {
    return SP_NULLPTR;
  }

  return jtd_parse_schema_object(ctx, v, path);
}

static bool jtd_walk_schema(sp_mem_t mem, jtd_schema_t* s, sp_str_t path, jtd_visit_fn fn, void* user) {
  if (!fn(s, path, user)) return false;

  switch (s->form) {
    case JTD_FORM_PROPERTIES: {
      sp_da_for(s->as.properties.required, i) {
        jtd_property_t* p = &s->as.properties.required[i];
        if (!jtd_walk_schema(mem, p->schema, jtd_path_seg_cstr_str(mem, path, "properties", p->key), fn, user)) return false;
      }
      sp_da_for(s->as.properties.optional, i) {
        jtd_property_t* p = &s->as.properties.optional[i];
        if (!jtd_walk_schema(mem, p->schema, jtd_path_seg_cstr_str(mem, path, "optionalProperties", p->key), fn, user)) return false;
      }
      return true;
    }
    case JTD_FORM_DISCRIMINATOR: {
      sp_da_for(s->as.discriminator.mapping, i) {
        jtd_mapping_t* m = &s->as.discriminator.mapping[i];
        if (!jtd_walk_schema(mem, m->schema, jtd_path_seg_cstr_str(mem, path, "mapping", m->tag), fn, user)) return false;
      }
      return true;
    }
    case JTD_FORM_ELEMENTS: return jtd_walk_schema(mem, s->as.elements.schema, jtd_path_seg_cstr(mem, path, "elements"), fn, user);
    case JTD_FORM_VALUES: return jtd_walk_schema(mem, s->as.values.schema, jtd_path_seg_cstr(mem, path, "values"), fn, user);
    case JTD_FORM_EMPTY:
    case JTD_FORM_TYPE:
    case JTD_FORM_ENUM:
    case JTD_FORM_REF:
      return true;
  }
  return true;
}

void jtd_walk(sp_mem_t mem, const jtd_result_t* result, jtd_visit_fn fn, void* user) {
  if (!result->root) return;
  if (!jtd_walk_schema(mem, result->root, sp_str_lit("#"), fn, user)) return;
  sp_da_for(result->definitions, i) {
    jtd_definition_t* d = &result->definitions[i];
    if (!jtd_walk_schema(mem, d->schema, jtd_path_seg(mem, sp_str_lit("#/definitions"), d->name), fn, user)) return;
  }
}

sp_str_t jtd_metadata(const jtd_schema_t* schema, const c8* key) {
  if (!schema) return sp_zero_s(sp_str_t);
  sp_da_for(schema->metadata, it) {
    if (sp_str_equal_cstr(schema->metadata[it].key, key)) {
      return schema->metadata[it].value;
    }
  }
  return sp_zero_s(sp_str_t);
}

bool jtd_metadata_has(const jtd_schema_t* schema, const c8* key) {
  if (!schema) return false;
  sp_da_for(schema->metadata, it) {
    if (sp_str_equal_cstr(schema->metadata[it].key, key)) {
      return true;
    }
  }
  return false;
}

jtd_schema_t* jtd_definition(const jtd_result_t* result, sp_str_t name) {
  sp_da_for(result->definitions, i) {
    if (sp_str_equal(result->definitions[i].name, name)) {
      return result->definitions[i].schema;
    }
  }
  return SP_NULLPTR;
}

jtd_schema_t* jtd_resolve(const jtd_result_t* result, jtd_schema_t* schema) {
  if (!schema) return schema;
  if (schema->form != JTD_FORM_REF) return schema;
  if (schema->as.ref.target) return schema->as.ref.target;
  return jtd_definition(result, schema->as.ref.name);
}

typedef struct {
  jtd_schema_t* schema;
  sp_str_t path;
  bool found;
} jtd_find_path_t;

static bool jtd_find_path_visit(jtd_schema_t* schema, sp_str_t path, void* user) {
  jtd_find_path_t* ctx = (jtd_find_path_t*)user;
  if (schema == ctx->schema) {
    ctx->path = path;
    ctx->found = true;
    return false;
  }
  return true;
}

static sp_str_t jtd_schema_path(sp_mem_t mem, const jtd_result_t* result, jtd_schema_t* schema) {
  if (!result) return sp_str_lit("#");
  if (!schema) return sp_str_lit("#");

  jtd_find_path_t ctx = { .schema = schema };
  jtd_walk(mem, result, jtd_find_path_visit, &ctx);
  if (!ctx.found) return sp_str_lit("#");
  return ctx.path;
}

jtd_schema_t* jtd_resolve_deep(sp_mem_t mem, const jtd_result_t* result, jtd_schema_t* schema, jtd_diagnostic_t* diag) {
  if (!schema || schema->form != JTD_FORM_REF) {
    return schema;
  }

  sp_da(jtd_schema_t*) seen = sp_da_new(mem, jtd_schema_t*);
  jtd_schema_t* current = schema;
  while (current && current->form == JTD_FORM_REF) {
    sp_da_for(seen, it) {
      if (seen[it] == current) {
        jtd_diag_str(mem, diag, JTD_ERR_REF_CYCLE, jtd_schema_path(mem, result, current), current->as.ref.name);
        return SP_NULLPTR;
      }
    }

    sp_da_push(seen, current);
    jtd_schema_t* next = current->as.ref.target ? current->as.ref.target : jtd_definition(result, current->as.ref.name);
    if (!next) {
      jtd_diag_str(mem, diag, JTD_ERR_REF_UNRESOLVED, jtd_schema_path(mem, result, current), current->as.ref.name);
      return SP_NULLPTR;
    }

    current = next;
  }

  return current;
}

typedef struct {
  jtd_ctx_t* ctx;
  bool ok;
} jtd_link_t;

static bool jtd_link_visit(jtd_schema_t* s, sp_str_t path, void* user) {
  jtd_link_t* c = (jtd_link_t*)user;
  if (s->form != JTD_FORM_REF) {
    return true;
  }
  jtd_schema_t* target = jtd_definition(c->ctx->out, s->as.ref.name);
  if (!target) {
    c->ok = jtd_fail_str(c->ctx, JTD_ERR_REF_UNRESOLVED, path, s->as.ref.name);
    return false;
  }
  s->as.ref.target = target;
  return true;
}

static bool jtd_link_refs(jtd_ctx_t* ctx) {
  jtd_link_t c = { .ctx = ctx, .ok = true };
  jtd_walk(ctx->temp, ctx->out, jtd_link_visit, &c);
  return c.ok;
}

static bool jtd_run(jtd_ctx_t* ctx, yyjson_val* root) {
  if (!yyjson_is_obj(root)) {
    return jtd_fail(ctx, JTD_ERR_SCHEMA_NOT_OBJECT, sp_str_lit("#"));
  }

  yyjson_val* defs = yyjson_obj_get(root, "definitions");
  if (defs) {
    if (!yyjson_is_obj(defs)) {
      return jtd_fail(ctx, JTD_ERR_SCHEMA_NOT_OBJECT, sp_str_lit("#/definitions"));
    }
    size_t idx, max;
    yyjson_val *k, *v;
    yyjson_obj_foreach(defs, idx, max, k, v) {
      sp_str_t name = jtd_yj_str(ctx->result, k);
      jtd_schema_t* s = jtd_parse_schema(ctx, v, jtd_path_seg(ctx->temp, sp_str_lit("#/definitions"), name));
      if (!s) return false;
      jtd_definition_t def = { .name = name, .schema = s };
      sp_da_push(ctx->out->definitions, def);
    }
  }

  ctx->out->root = jtd_parse_root_schema(ctx, root, sp_str_lit("#"));
  if (!ctx->out->root) return false;

  return jtd_link_refs(ctx);
}

static jtd_result_t jtd_parse_val(sp_mem_t mem, yyjson_val* root) {
  jtd_result_t out = sp_zero_s(jtd_result_t);
  out.arena = sp_mem_arena_new(mem);

  jtd_ctx_t ctx = sp_zero;
  ctx.temp_arena = sp_mem_arena_new(mem);
  ctx.result = sp_mem_arena_as_allocator(out.arena);
  ctx.temp = sp_mem_arena_as_allocator(ctx.temp_arena);
  ctx.out = &out;

  out.definitions = sp_da_new(ctx.result, jtd_definition_t);
  out.ok = jtd_run(&ctx, root);

  sp_mem_arena_destroy(ctx.temp_arena);
  return out;
}

jtd_err_t jtd_parse_file(sp_mem_t mem, sp_str_t file, jtd_result_t* jtd) {
  sp_str_t json = sp_zero;
  if (sp_io_read_file(mem, file, &json)) {
    *jtd = sp_zero_s(jtd_result_t);
    jtd->arena = sp_mem_arena_new(mem);
    jtd_diag(sp_mem_arena_as_allocator(jtd->arena), &jtd->diag, JTD_ERR_IO, file);
    return JTD_ERR_IO;
  }

  *jtd = jtd_parse(mem, json);
  return jtd->ok ? JTD_OK : JTD_ERR;
}

jtd_result_t jtd_parse(sp_mem_t mem, sp_str_t json) {
  yyjson_doc* doc = yyjson_read(json.data, json.len, 0);
  if (!doc) {
    jtd_result_t out = sp_zero_s(jtd_result_t);
    out.arena = sp_mem_arena_new(mem);
    out.ok = false;
    jtd_diag(sp_mem_arena_as_allocator(out.arena), &out.diag, JTD_ERR_JSON, sp_str_lit("#"));
    return out;
  }

  jtd_result_t out = jtd_parse_val(mem, yyjson_doc_get_root(doc));
  yyjson_doc_free(doc);
  return out;
}

void jtd_free(jtd_result_t* result) {
  if (result && result->arena) {
    sp_mem_arena_destroy(result->arena);
    *result = sp_zero_s(jtd_result_t);
  }
}

sp_str_t jtd_diagnostic_message(sp_mem_t mem, const jtd_diagnostic_t* diag) {
  sp_str_t detail;
  switch (diag->code) {
    case JTD_OK: detail = sp_str_lit("ok"); break;
    case JTD_ERR: detail = sp_str_lit("unknown JTD error"); break;
    case JTD_ERR_IO: detail = sp_str_lit("failed to open file"); break;
    case JTD_ERR_JSON: detail = sp_str_lit("input is not valid JSON"); break;
    case JTD_ERR_SCHEMA_NOT_OBJECT: detail = sp_str_lit("schema must be an object"); break;
    case JTD_ERR_MULTIPLE_FORMS: detail = sp_str_lit("schema uses keywords from more than one form"); break;
    case JTD_ERR_TYPE_NOT_STRING: detail = sp_str_lit("type must be a string"); break;
    case JTD_ERR_UNKNOWN_TYPE: detail = sp_fmt(mem, "unknown type {}", sp_fmt_str(diag->as.unknown_type.name)).value; break;
    case JTD_ERR_ENUM_EMPTY: detail = sp_str_lit("enum must be non-empty"); break;
    case JTD_ERR_ENUM_NOT_STRING: detail = sp_str_lit("enum must be an array of strings"); break;
    case JTD_ERR_ENUM_DUPLICATE: detail = sp_fmt(mem, "duplicate enum value {}", sp_fmt_str(diag->as.enum_duplicate.value)).value; break;
    case JTD_ERR_PROPERTIES_NOT_OBJECT: detail = sp_str_lit("properties must be an object"); break;
    case JTD_ERR_DISCRIMINATOR_NOT_STRING: detail = sp_str_lit("discriminator must be a string"); break;
    case JTD_ERR_MAPPING_NOT_OBJECT: detail = sp_str_lit("mapping must be an object"); break;
    case JTD_ERR_MAPPING_NOT_PROPERTIES: detail = sp_str_lit("mapping value must be a non-nullable properties schema"); break;
    case JTD_ERR_REF_NOT_STRING: detail = sp_str_lit("ref must be a string"); break;
    case JTD_ERR_METADATA_NOT_OBJECT: detail = sp_str_lit("metadata must be an object"); break;
    case JTD_ERR_NULLABLE_NOT_BOOL: detail = sp_str_lit("nullable must be a boolean"); break;
    case JTD_ERR_ADDITIONAL_PROPERTIES_NOT_BOOL: detail = sp_str_lit("additionalProperties must be a boolean"); break;
    case JTD_ERR_ADDITIONAL_PROPERTIES_WITHOUT_PROPERTIES: detail = sp_str_lit("additionalProperties requires properties or optionalProperties"); break;
    case JTD_ERR_PROPERTIES_DUPLICATE: detail = sp_str_lit("property cannot be both required and optional"); break;
    case JTD_ERR_DISCRIMINATOR_TAG_REDEFINED: detail = sp_str_lit("mapping schema cannot redefine discriminator tag"); break;
    case JTD_ERR_DEFINITIONS_NOT_ROOT: detail = sp_str_lit("definitions is only valid at the document root"); break;
    case JTD_ERR_REF_UNRESOLVED: detail = sp_fmt(mem, "ref to unknown definition {}", sp_fmt_str(diag->as.ref_unresolved.name)).value; break;
    case JTD_ERR_REF_CYCLE: detail = sp_fmt(mem, "ref cycle through {}", sp_fmt_str(diag->as.ref_cycle.name)).value; break;
    case JTD_ERR_UNKNOWN_MEMBER: detail = sp_fmt(mem, "unknown schema member {}", sp_fmt_str(diag->as.unknown_member.key)).value; break;
    case JTD_ERR_UNRECOGNIZED_FORM: detail = sp_str_lit("unrecognized schema form"); break;
  }
  return sp_fmt(mem, "{} at {}: {}", sp_fmt_cstr(jtd_err_name(diag->code)), sp_fmt_str(diag->path), sp_fmt_str(detail)).value;
}
