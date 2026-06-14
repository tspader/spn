#include "jtd.h"

const c8* jtd_err_name(jtd_err_t err) {
  switch (err) {
    case JTD_OK:                          return "ok";
    case JTD_ERR_JSON:                    return "invalid-json";
    case JTD_ERR_SCHEMA_NOT_OBJECT:       return "schema-not-object";
    case JTD_ERR_MULTIPLE_FORMS:          return "multiple-forms";
    case JTD_ERR_INVALID_TYPE:            return "invalid-type";
    case JTD_ERR_ENUM_EMPTY:              return "enum-empty";
    case JTD_ERR_ENUM_NOT_STRING:         return "enum-not-string";
    case JTD_ERR_ENUM_DUPLICATE:          return "enum-duplicate";
    case JTD_ERR_ELEMENTS_NOT_SCHEMA:     return "elements-not-schema";
    case JTD_ERR_VALUES_NOT_SCHEMA:       return "values-not-schema";
    case JTD_ERR_PROPERTIES_NOT_OBJECT:   return "properties-not-object";
    case JTD_ERR_DISCRIMINATOR_NOT_STRING:return "discriminator-not-string";
    case JTD_ERR_MAPPING_NOT_OBJECT:      return "mapping-not-object";
    case JTD_ERR_MAPPING_NOT_PROPERTIES:  return "mapping-not-properties";
    case JTD_ERR_REF_NOT_STRING:          return "ref-not-string";
    case JTD_ERR_METADATA_NOT_OBJECT:     return "metadata-not-object";
    case JTD_ERR_NULLABLE_NOT_BOOL:       return "nullable-not-bool";
    case JTD_ERR_ADDITIONAL_PROPERTIES_NOT_BOOL: return "additional-properties-not-bool";
    case JTD_ERR_ADDITIONAL_PROPERTIES_WITHOUT_PROPERTIES: return "additional-properties-without-properties";
    case JTD_ERR_PROPERTIES_DUPLICATE:    return "properties-duplicate";
    case JTD_ERR_DISCRIMINATOR_TAG_REDEFINED: return "discriminator-tag-redefined";
    case JTD_ERR_DEFINITIONS_NOT_ROOT:    return "definitions-not-root";
    case JTD_ERR_REF_UNRESOLVED:          return "ref-unresolved";
    case JTD_ERR_REF_CYCLE:               return "ref-cycle";
    case JTD_ERR_UNSUPPORTED:             return "unsupported";
  }
  return "unknown";
}

const c8* jtd_form_name(jtd_form_t form) {
  switch (form) {
    case JTD_FORM_EMPTY:         return "empty";
    case JTD_FORM_TYPE:          return "type";
    case JTD_FORM_ENUM:          return "enum";
    case JTD_FORM_ELEMENTS:      return "elements";
    case JTD_FORM_PROPERTIES:    return "properties";
    case JTD_FORM_VALUES:        return "values";
    case JTD_FORM_DISCRIMINATOR: return "discriminator";
    case JTD_FORM_REF:           return "ref";
  }
  return "unknown";
}

const c8* jtd_type_name(jtd_type_t type) {
  switch (type) {
    case JTD_TYPE_BOOLEAN:   return "boolean";
    case JTD_TYPE_FLOAT32:   return "float32";
    case JTD_TYPE_FLOAT64:   return "float64";
    case JTD_TYPE_INT8:      return "int8";
    case JTD_TYPE_UINT8:     return "uint8";
    case JTD_TYPE_INT16:     return "int16";
    case JTD_TYPE_UINT16:    return "uint16";
    case JTD_TYPE_INT32:     return "int32";
    case JTD_TYPE_UINT32:    return "uint32";
    case JTD_TYPE_STRING:    return "string";
    case JTD_TYPE_TIMESTAMP: return "timestamp";
  }
  return "unknown";
}

static const struct {
  const c8*  name;
  jtd_type_t type;
} JTD_PRIMITIVES[] = {
  { "boolean",   JTD_TYPE_BOOLEAN   },
  { "float32",   JTD_TYPE_FLOAT32   },
  { "float64",   JTD_TYPE_FLOAT64   },
  { "int8",      JTD_TYPE_INT8      },
  { "uint8",     JTD_TYPE_UINT8     },
  { "int16",     JTD_TYPE_INT16     },
  { "uint16",    JTD_TYPE_UINT16    },
  { "int32",     JTD_TYPE_INT32     },
  { "uint32",    JTD_TYPE_UINT32    },
  { "string",    JTD_TYPE_STRING    },
  { "timestamp", JTD_TYPE_TIMESTAMP },
};

static sp_str_t jtd_yj_str(yyjson_val* v) {
  return sp_str_copy(spn_allocator, sp_str((c8*)yyjson_get_str(v), (u32)yyjson_get_len(v)));
}

static sp_str_t jtd_path_escape(sp_str_t seg) {
  u32 len = seg.len;
  sp_for(it, seg.len) {
    if (seg.data[it] == '~' || seg.data[it] == '/') {
      len++;
    }
  }

  if (len == seg.len) {
    return seg;
  }

  c8* data = sp_alloc(spn_allocator, len);
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

static sp_str_t jtd_path_seg(sp_str_t base, sp_str_t seg) {
  return sp_format("{}/{}", SP_FMT_STR(base), SP_FMT_STR(jtd_path_escape(seg)));
}

static bool jtd_fail(jtd_diagnostic_t* diag, jtd_err_t code, sp_str_t path, sp_str_t msg) {
  if (diag) {
    diag->code    = code;
    diag->path    = path;
    diag->message = sp_format(
      "{} at {}: {}",
      SP_FMT_CSTR(jtd_err_name(code)), SP_FMT_STR(path), SP_FMT_STR(msg));
  }
  return false;
}

static jtd_schema_t* jtd_parse_schema(yyjson_val* v, sp_str_t path, jtd_diagnostic_t* diag);

static bool jtd_parse_shared(jtd_schema_t* s, yyjson_val* v, sp_str_t path, jtd_diagnostic_t* diag) {
  yyjson_val* metadata = yyjson_obj_get(v, "metadata");
  if (metadata && !yyjson_is_obj(metadata)) {
    return jtd_fail(diag, JTD_ERR_METADATA_NOT_OBJECT, jtd_path_seg(path, sp_str_lit("metadata")), sp_str_lit("metadata must be an object"));
  }

  yyjson_val* nullable = yyjson_obj_get(v, "nullable");
  if (nullable) {
    if (!yyjson_is_bool(nullable)) {
      return jtd_fail(diag, JTD_ERR_NULLABLE_NOT_BOOL, path, sp_str_lit("nullable must be a boolean"));
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

static bool jtd_validate_keys(yyjson_val* v, sp_str_t path, jtd_diagnostic_t* diag) {
  size_t idx, max;
  yyjson_val *k, *mv;

  yyjson_obj_foreach(v, idx, max, k, mv) {
    sp_str_t key = jtd_yj_str(k);
    if (!jtd_schema_key(key)) {
      return jtd_fail(diag, JTD_ERR_UNSUPPORTED, jtd_path_seg(path, key), sp_format("unknown schema member {}", SP_FMT_STR(key)));
    }
  }

  return true;
}

static bool jtd_validate_root_keys(yyjson_val* v, sp_str_t path, jtd_diagnostic_t* diag) {
  size_t idx, max;
  yyjson_val *k, *mv;

  yyjson_obj_foreach(v, idx, max, k, mv) {
    sp_str_t key = jtd_yj_str(k);
    if (!jtd_root_key(key)) {
      return jtd_fail(diag, JTD_ERR_UNSUPPORTED, jtd_path_seg(path, key), sp_format("unknown schema member {}", SP_FMT_STR(key)));
    }
  }

  return true;
}

static jtd_schema_t* jtd_parse_properties(jtd_schema_t* s, yyjson_val* props, yyjson_val* oprops, yyjson_val* aprops, sp_str_t path, jtd_diagnostic_t* diag) {
  s->form = JTD_FORM_PROPERTIES;
  s->as.properties.required = SP_NULLPTR;
  s->as.properties.optional = SP_NULLPTR;

  if (aprops && !yyjson_is_bool(aprops)) {
    jtd_fail(diag, JTD_ERR_ADDITIONAL_PROPERTIES_NOT_BOOL, jtd_path_seg(path, sp_str_lit("additionalProperties")), sp_str_lit("additionalProperties must be a boolean"));
    return SP_NULLPTR;
  }

  if (aprops && !props && !oprops) {
    jtd_fail(diag, JTD_ERR_ADDITIONAL_PROPERTIES_WITHOUT_PROPERTIES, jtd_path_seg(path, sp_str_lit("additionalProperties")), sp_str_lit("additionalProperties requires properties or optionalProperties"));
    return SP_NULLPTR;
  }

  size_t idx, max;
  yyjson_val *k, *mv;

  if (props) {
    if (!yyjson_is_obj(props)) { jtd_fail(diag, JTD_ERR_PROPERTIES_NOT_OBJECT, path, sp_str_lit("properties must be an object")); return SP_NULLPTR; }
    yyjson_obj_foreach(props, idx, max, k, mv) {
      sp_str_t key = jtd_yj_str(k);
      jtd_schema_t* child = jtd_parse_schema(mv, jtd_path_seg(jtd_path_seg(path, sp_str_lit("properties")), key), diag);
      if (!child) return SP_NULLPTR;
      jtd_property_t p = { .key = key, .schema = child };
      sp_da_push(s->as.properties.required, p);
    }
  }

  if (oprops) {
    if (!yyjson_is_obj(oprops)) { jtd_fail(diag, JTD_ERR_PROPERTIES_NOT_OBJECT, path, sp_str_lit("optionalProperties must be an object")); return SP_NULLPTR; }
    yyjson_obj_foreach(oprops, idx, max, k, mv) {
      sp_str_t key = jtd_yj_str(k);
      sp_da_for(s->as.properties.required, it) {
        if (sp_str_equal(key, s->as.properties.required[it].key)) {
          jtd_fail(diag, JTD_ERR_PROPERTIES_DUPLICATE, jtd_path_seg(jtd_path_seg(path, sp_str_lit("optionalProperties")), key), sp_str_lit("property cannot be both required and optional"));
          return SP_NULLPTR;
        }
      }
      jtd_schema_t* child = jtd_parse_schema(mv, jtd_path_seg(jtd_path_seg(path, sp_str_lit("optionalProperties")), key), diag);
      if (!child) return SP_NULLPTR;
      jtd_property_t p = { .key = key, .schema = child };
      sp_da_push(s->as.properties.optional, p);
    }
  }

  if (aprops) {
    s->as.properties.additional = yyjson_get_bool(aprops);
  }

  return s;
}

static jtd_schema_t* jtd_parse_schema_object(yyjson_val* v, sp_str_t path, jtd_diagnostic_t* diag) {
  jtd_schema_t* s = sp_alloc_type(spn_allocator, jtd_schema_t);
  *s = SP_ZERO_STRUCT(jtd_schema_t);

  if (!jtd_parse_shared(s, v, path, diag)) {
    return SP_NULLPTR;
  }

  yyjson_val* k_type     = yyjson_obj_get(v, "type");
  yyjson_val* k_enum     = yyjson_obj_get(v, "enum");
  yyjson_val* k_elements = yyjson_obj_get(v, "elements");
  yyjson_val* k_props    = yyjson_obj_get(v, "properties");
  yyjson_val* k_oprops   = yyjson_obj_get(v, "optionalProperties");
  yyjson_val* k_aprops   = yyjson_obj_get(v, "additionalProperties");
  yyjson_val* k_values   = yyjson_obj_get(v, "values");
  yyjson_val* k_disc     = yyjson_obj_get(v, "discriminator");
  yyjson_val* k_mapping  = yyjson_obj_get(v, "mapping");
  yyjson_val* k_ref      = yyjson_obj_get(v, "ref");

  u32 forms =
    (k_type != SP_NULLPTR) +
    (k_enum != SP_NULLPTR) +
    (k_elements != SP_NULLPTR) +
    (k_props || k_oprops || k_aprops) +
    (k_values != SP_NULLPTR) +
    (k_disc || k_mapping) +
    (k_ref != SP_NULLPTR);

  if (forms > 1) {
    jtd_fail(diag, JTD_ERR_MULTIPLE_FORMS, path, sp_str_lit("schema uses keywords from more than one form"));
    return SP_NULLPTR;
  }

  if (forms == 0) {
    s->form = JTD_FORM_EMPTY;
    return s;
  }

  if (k_type) {
    if (!yyjson_is_str(k_type)) { jtd_fail(diag, JTD_ERR_INVALID_TYPE, path, sp_str_lit("type must be a string")); return SP_NULLPTR; }
    sp_str_t name = sp_str((c8*)yyjson_get_str(k_type), (u32)yyjson_get_len(k_type));
    sp_carr_for(JTD_PRIMITIVES, i) {
      if (sp_str_equal_cstr(name, JTD_PRIMITIVES[i].name)) {
        s->form = JTD_FORM_TYPE;
        s->as.type = JTD_PRIMITIVES[i].type;
        return s;
      }
    }
    jtd_fail(diag, JTD_ERR_INVALID_TYPE, path, sp_format("unknown type {}", SP_FMT_STR(name)));
    return SP_NULLPTR;
  }

  if (k_enum) {
    if (!yyjson_is_arr(k_enum)) { jtd_fail(diag, JTD_ERR_ENUM_NOT_STRING, path, sp_str_lit("enum must be an array of strings")); return SP_NULLPTR; }
    s->form = JTD_FORM_ENUM;
    s->as.enumeration.values = SP_NULLPTR;

    size_t idx, max;
    yyjson_val* ev;
    yyjson_arr_foreach(k_enum, idx, max, ev) {
      if (!yyjson_is_str(ev)) { jtd_fail(diag, JTD_ERR_ENUM_NOT_STRING, path, sp_str_lit("enum values must be strings")); return SP_NULLPTR; }
      sp_str_t val = jtd_yj_str(ev);
      sp_da_for(s->as.enumeration.values, j) {
        if (sp_str_equal(val, s->as.enumeration.values[j])) {
          jtd_fail(diag, JTD_ERR_ENUM_DUPLICATE, path, sp_format("duplicate enum value {}", SP_FMT_STR(val)));
          return SP_NULLPTR;
        }
      }
      sp_da_push(s->as.enumeration.values, val);
    }

    if (sp_da_size(s->as.enumeration.values) == 0) {
      jtd_fail(diag, JTD_ERR_ENUM_EMPTY, path, sp_str_lit("enum must be non-empty"));
      return SP_NULLPTR;
    }
    return s;
  }

  if (k_elements) {
    s->form = JTD_FORM_ELEMENTS;
    jtd_schema_t* child = jtd_parse_schema(k_elements, jtd_path_seg(path, sp_str_lit("elements")), diag);
    if (!child) return SP_NULLPTR;
    s->as.elements.schema = child;
    return s;
  }

  if (k_props || k_oprops || k_aprops) {
    return jtd_parse_properties(s, k_props, k_oprops, k_aprops, path, diag);
  }

  if (k_values) {
    s->form = JTD_FORM_VALUES;
    jtd_schema_t* child = jtd_parse_schema(k_values, jtd_path_seg(path, sp_str_lit("values")), diag);
    if (!child) return SP_NULLPTR;
    s->as.values.schema = child;
    return s;
  }

  if (k_disc || k_mapping) {
    s->form = JTD_FORM_DISCRIMINATOR;
    if (!k_disc || !yyjson_is_str(k_disc)) { jtd_fail(diag, JTD_ERR_DISCRIMINATOR_NOT_STRING, path, sp_str_lit("discriminator must be a string")); return SP_NULLPTR; }
    if (!k_mapping || !yyjson_is_obj(k_mapping)) { jtd_fail(diag, JTD_ERR_MAPPING_NOT_OBJECT, path, sp_str_lit("mapping must be an object")); return SP_NULLPTR; }

    s->as.discriminator.tag = jtd_yj_str(k_disc);
    s->as.discriminator.mapping = SP_NULLPTR;

    size_t idx, max;
    yyjson_val *k, *mv;
    yyjson_obj_foreach(k_mapping, idx, max, k, mv) {
      sp_str_t tag = jtd_yj_str(k);
      sp_str_t cpath = jtd_path_seg(jtd_path_seg(path, sp_str_lit("mapping")), tag);
      jtd_schema_t* child = jtd_parse_schema(mv, cpath, diag);
      if (!child) return SP_NULLPTR;
      if (child->form != JTD_FORM_PROPERTIES || child->nullable) {
        jtd_fail(diag, JTD_ERR_MAPPING_NOT_PROPERTIES, cpath, sp_str_lit("mapping value must be a non-nullable properties schema"));
        return SP_NULLPTR;
      }
      sp_da_for(child->as.properties.required, it) {
        jtd_property_t* p = &child->as.properties.required[it];
        if (sp_str_equal(p->key, s->as.discriminator.tag)) {
          jtd_fail(diag, JTD_ERR_DISCRIMINATOR_TAG_REDEFINED, jtd_path_seg(jtd_path_seg(cpath, sp_str_lit("properties")), p->key), sp_str_lit("mapping schema cannot redefine discriminator tag"));
          return SP_NULLPTR;
        }
      }
      sp_da_for(child->as.properties.optional, it) {
        jtd_property_t* p = &child->as.properties.optional[it];
        if (sp_str_equal(p->key, s->as.discriminator.tag)) {
          jtd_fail(diag, JTD_ERR_DISCRIMINATOR_TAG_REDEFINED, jtd_path_seg(jtd_path_seg(cpath, sp_str_lit("optionalProperties")), p->key), sp_str_lit("mapping schema cannot redefine discriminator tag"));
          return SP_NULLPTR;
        }
      }
      jtd_mapping_t m = { .tag = tag, .schema = child };
      sp_da_push(s->as.discriminator.mapping, m);
    }
    return s;
  }

  if (k_ref) {
    if (!yyjson_is_str(k_ref)) { jtd_fail(diag, JTD_ERR_REF_NOT_STRING, path, sp_str_lit("ref must be a string")); return SP_NULLPTR; }
    s->form = JTD_FORM_REF;
    s->as.ref.name = jtd_yj_str(k_ref);
    return s;
  }

  jtd_fail(diag, JTD_ERR_UNSUPPORTED, path, sp_str_lit("unrecognized schema form"));
  return SP_NULLPTR;
}

static jtd_schema_t* jtd_parse_schema(yyjson_val* v, sp_str_t path, jtd_diagnostic_t* diag) {
  if (!yyjson_is_obj(v)) {
    jtd_fail(diag, JTD_ERR_SCHEMA_NOT_OBJECT, path, sp_str_lit("schema must be an object"));
    return SP_NULLPTR;
  }

  if (yyjson_obj_get(v, "definitions")) {
    jtd_fail(diag, JTD_ERR_DEFINITIONS_NOT_ROOT, jtd_path_seg(path, sp_str_lit("definitions")), sp_str_lit("definitions is only valid at the document root"));
    return SP_NULLPTR;
  }

  if (!jtd_validate_keys(v, path, diag)) {
    return SP_NULLPTR;
  }

  return jtd_parse_schema_object(v, path, diag);
}

static jtd_schema_t* jtd_parse_root_schema(yyjson_val* v, sp_str_t path, jtd_diagnostic_t* diag) {
  if (!yyjson_is_obj(v)) {
    jtd_fail(diag, JTD_ERR_SCHEMA_NOT_OBJECT, path, sp_str_lit("schema must be an object"));
    return SP_NULLPTR;
  }

  if (!jtd_validate_root_keys(v, path, diag)) {
    return SP_NULLPTR;
  }

  return jtd_parse_schema_object(v, path, diag);
}

static bool jtd_walk_schema(jtd_schema_t* s, sp_str_t path, jtd_visit_fn fn, void* user) {
  if (!fn(s, path, user)) return false;

  switch (s->form) {
    case JTD_FORM_ELEMENTS:
      return jtd_walk_schema(s->as.elements.schema, jtd_path_seg(path, sp_str_lit("elements")), fn, user);

    case JTD_FORM_VALUES:
      return jtd_walk_schema(s->as.values.schema, jtd_path_seg(path, sp_str_lit("values")), fn, user);

    case JTD_FORM_PROPERTIES: {
      sp_da_for(s->as.properties.required, i) {
        jtd_property_t* p = &s->as.properties.required[i];
        if (!jtd_walk_schema(p->schema, jtd_path_seg(jtd_path_seg(path, sp_str_lit("properties")), p->key), fn, user)) return false;
      }
      sp_da_for(s->as.properties.optional, i) {
        jtd_property_t* p = &s->as.properties.optional[i];
        if (!jtd_walk_schema(p->schema, jtd_path_seg(jtd_path_seg(path, sp_str_lit("optionalProperties")), p->key), fn, user)) return false;
      }
      return true;
    }

    case JTD_FORM_DISCRIMINATOR: {
      sp_da_for(s->as.discriminator.mapping, i) {
        jtd_mapping_t* m = &s->as.discriminator.mapping[i];
        if (!jtd_walk_schema(m->schema, jtd_path_seg(jtd_path_seg(path, sp_str_lit("mapping")), m->tag), fn, user)) return false;
      }
      return true;
    }

    case JTD_FORM_EMPTY:
    case JTD_FORM_TYPE:
    case JTD_FORM_ENUM:
    case JTD_FORM_REF:
      return true;
  }
  return true;
}

void jtd_walk(const jtd_root_t* root, jtd_visit_fn fn, void* user) {
  if (!root->root) return;
  if (!jtd_walk_schema(root->root, sp_str_lit("#"), fn, user)) return;
  sp_da_for(root->definitions, i) {
    jtd_definition_t* d = &root->definitions[i];
    if (!jtd_walk_schema(d->schema, jtd_path_seg(sp_str_lit("#/definitions"), d->name), fn, user)) return;
  }
}

jtd_schema_t* jtd_definition(const jtd_root_t* root, sp_str_t name) {
  sp_da_for(root->definitions, i) {
    if (sp_str_equal(root->definitions[i].name, name)) {
      return root->definitions[i].schema;
    }
  }
  return SP_NULLPTR;
}

jtd_schema_t* jtd_resolve(const jtd_root_t* root, jtd_schema_t* schema) {
  if (!schema || schema->form != JTD_FORM_REF) return schema;
  return jtd_definition(root, schema->as.ref.name);
}

typedef struct {
  jtd_schema_t* schema;
  sp_str_t      path;
  bool          found;
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

static sp_str_t jtd_schema_path(const jtd_root_t* root, jtd_schema_t* schema) {
  if (!root || !schema) {
    return sp_str_lit("#");
  }

  jtd_find_path_t ctx = { .schema = schema };
  jtd_walk(root, jtd_find_path_visit, &ctx);
  if (ctx.found) {
    return ctx.path;
  }
  return sp_str_lit("#");
}

jtd_schema_t* jtd_resolve_deep(const jtd_root_t* root, jtd_schema_t* schema, jtd_diagnostic_t* diag) {
  if (!schema || schema->form != JTD_FORM_REF) {
    return schema;
  }

  sp_da(jtd_schema_t*) seen = SP_NULLPTR;
  jtd_schema_t* current = schema;
  while (current && current->form == JTD_FORM_REF) {
    sp_da_for(seen, it) {
      if (seen[it] == current) {
        jtd_fail(diag, JTD_ERR_REF_CYCLE, jtd_schema_path(root, current),
          sp_format("ref cycle through {}", SP_FMT_STR(current->as.ref.name)));
        return SP_NULLPTR;
      }
    }

    sp_da_push(seen, current);
    jtd_schema_t* next = jtd_definition(root, current->as.ref.name);
    if (!next) {
      jtd_fail(diag, JTD_ERR_REF_UNRESOLVED, jtd_schema_path(root, current),
        sp_format("ref to unknown definition {}", SP_FMT_STR(current->as.ref.name)));
      return SP_NULLPTR;
    }

    current = next;
  }

  return current;
}

typedef struct {
  const jtd_root_t* root;
  jtd_diagnostic_t* diag;
  bool ok;
} jtd_refcheck_t;

static bool jtd_refcheck_visit(jtd_schema_t* s, sp_str_t path, void* user) {
  jtd_refcheck_t* c = (jtd_refcheck_t*)user;
  if (s->form == JTD_FORM_REF && !jtd_definition(c->root, s->as.ref.name)) {
    c->ok = jtd_fail(c->diag, JTD_ERR_REF_UNRESOLVED, path,
      sp_format("ref to unknown definition {}", SP_FMT_STR(s->as.ref.name)));
    return false;
  }
  return true;
}

static bool jtd_validate_refs(const jtd_root_t* root, jtd_diagnostic_t* diag) {
  jtd_refcheck_t c = { .root = root, .diag = diag, .ok = true };
  jtd_walk(root, jtd_refcheck_visit, &c);
  return c.ok;
}

bool jtd_parse_val(yyjson_val* root, jtd_root_t* out, jtd_diagnostic_t* diag) {
  *out = (jtd_root_t){0};
  out->definitions = SP_NULLPTR;

  if (!yyjson_is_obj(root)) {
    return jtd_fail(diag, JTD_ERR_SCHEMA_NOT_OBJECT, sp_str_lit("#"), sp_str_lit("root schema must be an object"));
  }

  yyjson_val* defs = yyjson_obj_get(root, "definitions");
  if (defs) {
    if (!yyjson_is_obj(defs)) {
      return jtd_fail(diag, JTD_ERR_SCHEMA_NOT_OBJECT, sp_str_lit("#/definitions"), sp_str_lit("definitions must be an object"));
    }
    size_t idx, max;
    yyjson_val *k, *v;
    yyjson_obj_foreach(defs, idx, max, k, v) {
      sp_str_t name = jtd_yj_str(k);
      jtd_schema_t* s = jtd_parse_schema(v, jtd_path_seg(sp_str_lit("#/definitions"), name), diag);
      if (!s) return false;
      jtd_definition_t def = { .name = name, .schema = s };
      sp_da_push(out->definitions, def);
    }
  }

  out->root = jtd_parse_root_schema(root, sp_str_lit("#"), diag);
  if (!out->root) return false;

  return jtd_validate_refs(out, diag);
}

bool jtd_parse(sp_str_t json, jtd_root_t* out, jtd_diagnostic_t* diag) {
  yyjson_doc* doc = yyjson_read(json.data, json.len, 0);
  if (!doc) {
    *out = (jtd_root_t){0};
    return jtd_fail(diag, JTD_ERR_JSON, sp_str_lit("#"), sp_str_lit("input is not valid JSON"));
  }

  bool ok = jtd_parse_val(yyjson_doc_get_root(doc), out, diag);
  yyjson_doc_free(doc);
  return ok;
}
