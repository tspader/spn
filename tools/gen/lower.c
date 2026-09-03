#include "gen.h"

typedef enum {
  CARD_SCALAR,
  CARD_ARRAY,
  CARD_MAP,
} card_t;

typedef enum {
  VAL_STR,
  VAL_PRIM,
  VAL_ENUM,
  VAL_STRUCT,
} val_kind_t;

typedef struct {
  val_kind_t kind;
  gen_prim_t prim;
  gen_type_t* object;
} val_t;

gen_t* gen_new(sp_mem_t mem) {
  gen_t* g = sp_alloc_type(mem, gen_t);
  g->mem = mem;
  g->types = sp_da_new(mem, gen_type_t*);
  sp_str_ht_init(mem, g->visited);
  g->containers.array = sp_da_new(mem, gen_type_t*);
  g->containers.shorthand = sp_da_new(mem, gen_shorthand_t);
  g->containers.keyed = sp_da_new(mem, gen_keyed_t);
  g->containers.object = sp_da_new(mem, gen_type_t*);
  g->entries = sp_da_new(mem, gen_entry_t*);
  g->includes = sp_da_new(mem, sp_str_t);
  g->kinds = sp_da_new(mem, gen_kind_t);
  g->arms = sp_da_new(mem, gen_type_t*);
  g->prefix = sp_str_lit("spn_cg");
  return g;
}

gen_type_t* gen_find(gen_t* g, sp_str_t name) {
  sp_da_for(g->types, it) {
    if (sp_str_equal(g->types[it]->name, name)) {
      return g->types[it];
    }
  }
  return SP_NULLPTR;
}

static jtd_schema_t* deref(jtd_schema_t* schema) {
  while (schema && schema->form == JTD_FORM_REF) {
    schema = schema->as.ref.target;
  }
  return schema;
}

static bool fail(gen_t* g, sp_str_t type, sp_str_t key, sp_str_t what) {
  g->err = sp_fmt(g->mem, "{.cyan}.{.cyan}: {}", sp_fmt_str(type), sp_fmt_str(key), sp_fmt_str(what)).value;
  return false;
}

static bool fail_scalar(gen_t* g, sp_str_t type, sp_str_t key, jtd_type_t scalar) {
  return fail(g, type, key, sp_fmt(g->mem, "unsupported scalar type {.red}", sp_fmt_cstr(jtd_type_name(scalar))).value);
}

static bool fail_form(gen_t* g, sp_str_t type, sp_str_t key, jtd_form_t form) {
  return fail(g, type, key, sp_fmt(g->mem, "unsupported schema form {.red}", sp_fmt_cstr(jtd_form_name(form))).value);
}

static void add_include(gen_t* g, jtd_schema_t* schema) {
  sp_str_t include = jtd_metadata(schema, "include");
  if (sp_str_empty(include)) {
    return;
  }
  sp_da_for(g->includes, it) {
    if (sp_str_equal(g->includes[it], include)) {
      return;
    }
  }
  sp_da_push(g->includes, include);
}

static void add_array(gen_t* g, gen_type_t* object) {
  sp_da_for(g->containers.array, it) {
    if (g->containers.array[it] == object) {
      return;
    }
  }
  sp_da_push(g->containers.array, object);
}

static bool add_shorthand(gen_t* g, gen_type_t* object, sp_str_t field) {
  sp_da_for(g->containers.shorthand, it) {
    if (g->containers.shorthand[it].object == object) {
      return sp_str_equal(g->containers.shorthand[it].field, field);
    }
  }
  gen_shorthand_t shorthand = { .object = object, .field = field };
  sp_da_push(g->containers.shorthand, shorthand);
  return true;
}

static void add_keyed(gen_t* g, gen_type_t* object, sp_str_t key_field) {
  sp_da_for(g->containers.keyed, it) {
    if (g->containers.keyed[it].object == object) {
      return;
    }
  }
  gen_keyed_t keyed = { .object = object, .key_field = key_field };
  sp_da_push(g->containers.keyed, keyed);
}

static void add_object(gen_t* g, gen_type_t* object) {
  sp_da_for(g->containers.object, it) {
    if (g->containers.object[it] == object) {
      return;
    }
  }
  sp_da_push(g->containers.object, object);
}

static gen_entry_t* add_entry(gen_t* g, gen_entry_t entry) {
  sp_da_for(g->entries, it) {
    if (sp_str_equal(g->entries[it]->name, entry.name)) {
      return g->entries[it];
    }
  }
  gen_entry_t* added = sp_alloc_type(g->mem, gen_entry_t);
  *added = entry;
  sp_da_push(g->entries, added);
  return added;
}

static bool lower_type(gen_t* g, sp_str_t name, jtd_schema_t* schema, gen_type_t** lowered);

static bool lower_field(gen_t* g, gen_type_t* type, jtd_property_t property) {
  gen_field_t field = {
    .key = property.key,
    .name = property.key,
    .required = property.required,
  };

  sp_str_t rename = jtd_metadata(property.schema, "field");
  if (!sp_str_empty(rename)) {
    field.name = rename;
  }

  jtd_schema_t* value = property.schema;
  card_t card = CARD_SCALAR;
  if (property.schema->form == JTD_FORM_ELEMENTS) {
    card = CARD_ARRAY;
    value = property.schema->as.elements.schema;
  }
  else if (property.schema->form == JTD_FORM_VALUES) {
    card = CARD_MAP;
    value = property.schema->as.values.schema;
  }

  sp_str_t extern_name = jtd_metadata(value, "extern");
  if (!sp_str_empty(extern_name)) {
    if (value->form != JTD_FORM_EMPTY) {
      return fail(g, type->name, property.key, sp_str_lit("extern fields must use the empty form"));
    }
    if (card == CARD_MAP) {
      return fail(g, type->name, property.key, sp_str_lit("extern maps are not supported"));
    }
    if (card == CARD_ARRAY && g->format != GEN_FORMAT_ERRORS && g->format != GEN_FORMAT_EVENTS) {
      return fail(g, type->name, property.key, sp_str_lit("extern arrays are only supported in union schemas"));
    }
    field.kind = card == CARD_ARRAY ? FIELD_EXTERN_ARRAY : FIELD_EXTERN;
    field.as.ext = extern_name;
    add_include(g, value);
    sp_da_push(type->fields, field);
    return true;
  }

  sp_str_t name = value->form == JTD_FORM_REF ? value->as.ref.name : property.key;
  jtd_schema_t* target = deref(value);

  val_t val = sp_zero;
  if (target->form == JTD_FORM_ENUM) {
    val.kind = VAL_ENUM;
    add_include(g, target);
  }
  else if (target->form == JTD_FORM_TYPE && target->as.type == JTD_TYPE_STRING) {
    val.kind = VAL_STR;
  }
  else if (target->form == JTD_FORM_TYPE && target->as.type == JTD_TYPE_BOOLEAN) {
    val.kind = VAL_PRIM;
    val.prim = PRIM_BOOL;
  }
  else if (target->form == JTD_FORM_TYPE && target->as.type == JTD_TYPE_UINT32) {
    val.kind = VAL_PRIM;
    val.prim = PRIM_U32;
  }
  else if (target->form == JTD_FORM_TYPE && target->as.type == JTD_TYPE_UINT64) {
    val.kind = VAL_PRIM;
    val.prim = PRIM_U64;
  }
  else if (target->form == JTD_FORM_TYPE && target->as.type == JTD_TYPE_INT32) {
    val.kind = VAL_PRIM;
    val.prim = PRIM_S32;
  }
  else if (target->form == JTD_FORM_TYPE) {
    return fail_scalar(g, type->name, property.key, target->as.type);
  }
  else if (target->form == JTD_FORM_PROPERTIES) {
    val.kind = VAL_STRUCT;
    if (!lower_type(g, name, target, &val.object)) {
      return false;
    }
  }
  else {
    return fail_form(g, type->name, property.key, target->form);
  }

  switch (card) {
    case CARD_SCALAR: {
      switch (val.kind) {
        case VAL_STR: {
          field.kind = FIELD_STR;
          break;
        }
        case VAL_PRIM: {
          field.kind = FIELD_PRIM;
          field.as.prim = val.prim;
          break;
        }
        case VAL_ENUM: {
          field.kind = FIELD_ENUM;
          field.as.enumeration = name;
          break;
        }
        case VAL_STRUCT: {
          if (!gen_find(g, name)) {
            return fail(g, type->name, property.key, sp_str_lit("recursive type must be an array or map"));
          }
          field.kind = FIELD_OBJECT;
          field.as.object = val.object;
          add_object(g, val.object);
          break;
        }
      }
      break;
    }
    case CARD_ARRAY: {
      switch (val.kind) {
        case VAL_STR: {
          field.kind = FIELD_STR_ARRAY;
          break;
        }
        case VAL_PRIM: {
          return fail_form(g, type->name, property.key, target->form);
        }
        case VAL_ENUM: {
          field.kind = FIELD_ENUM_ARRAY;
          field.as.enumeration = name;
          break;
        }
        case VAL_STRUCT: {
          sp_str_t key_field = jtd_metadata(property.schema, "key");
          sp_str_t shorthand = jtd_metadata(property.schema, "shorthand");
          if (!sp_str_empty(key_field)) {
            field.kind = FIELD_KEYED;
            field.as.keyed.object = val.object;
            field.as.keyed.field = key_field;
            add_keyed(g, val.object, key_field);
          }
          else if (!sp_str_empty(shorthand)) {
            field.kind = FIELD_OBJECT_ARRAY;
            field.as.array.object = val.object;
            field.as.array.shorthand = shorthand;
            if (!add_shorthand(g, val.object, shorthand)) {
              return fail(g, type->name, property.key, sp_str_lit("object is used with two different shorthand fields"));
            }
          }
          else {
            field.kind = FIELD_OBJECT_ARRAY;
            field.as.array.object = val.object;
            add_array(g, val.object);
          }
          break;
        }
      }
      break;
    }
    case CARD_MAP: {
      if (val.kind != VAL_STR && val.kind != VAL_STRUCT) {
        return fail_form(g, type->name, property.key, target->form);
      }
      gen_entry_t entry = {
        .name = sp_fmt(g->mem, "{}_{}_entry", sp_fmt_str(type->name), sp_fmt_str(property.key)).value,
        .object = val.kind == VAL_STRUCT ? val.object : SP_NULLPTR,
        .shorthand = jtd_metadata(property.schema, "shorthand"),
      };
      field.kind = FIELD_MAP;
      field.as.map = add_entry(g, entry);
      break;
    }
  }

  sp_da_push(type->fields, field);
  return true;
}

static bool lower_type(gen_t* g, sp_str_t name, jtd_schema_t* schema, gen_type_t** lowered) {
  schema = deref(schema);
  if (schema->form != JTD_FORM_PROPERTIES) {
    g->err = sp_fmt(g->mem, "{.cyan}: schema must be a properties form, got {.red}", sp_fmt_str(name), sp_fmt_cstr(jtd_form_name(schema->form))).value;
    return false;
  }

  gen_visit_t* visited = sp_ht_getp(g->visited, name);
  if (visited) {
    if (visited->schema != schema) {
      g->err = sp_fmt(g->mem, "{.cyan}: two distinct schemas produce the same type name", sp_fmt_str(name)).value;
      return false;
    }
    *lowered = visited->type;
    return true;
  }

  gen_type_t* type = sp_alloc_type(g->mem, gen_type_t);
  type->name = name;
  type->fields = sp_da_new(g->mem, gen_field_t);
  type->schema = schema;

  gen_visit_t visit = { .schema = schema, .type = type };
  sp_ht_insert(g->visited, name, visit);

  sp_da_for(schema->as.properties.all, it) {
    if (!lower_field(g, type, schema->as.properties.all[it])) {
      return false;
    }
  }

  sp_da_push(g->types, type);
  *lowered = type;
  return true;
}

bool gen_lower(gen_t* g, sp_str_t name, jtd_schema_t* schema) {
  gen_type_t* type = SP_NULLPTR;
  return lower_type(g, name, schema, &type);
}

static void add_arm(gen_t* g, gen_type_t* payload) {
  sp_da_for(g->arms, it) {
    if (g->arms[it] == payload) {
      return;
    }
  }
  sp_da_push(g->arms, payload);
}

static bool lower_metadata_enum(gen_t* g, jtd_schema_t* schema, sp_str_t tag, const c8* key, const c8* fallback, const c8** values, u32 num_values, const c8* constant, sp_str_t* lowered) {
  sp_str_t value = jtd_metadata(schema, key);
  if (sp_str_empty(value)) {
    value = sp_cstr_as_str(fallback);
  }
  sp_for(it, num_values) {
    if (sp_str_equal_cstr(value, values[it])) {
      *lowered = sp_fmt(g->mem, "{}_{}", sp_fmt_cstr(constant), sp_fmt_str(sp_str_to_upper(g->mem, value))).value;
      return true;
    }
  }
  g->err = sp_fmt(g->mem, "{.cyan}: unknown {} {.red}", sp_fmt_str(tag), sp_fmt_cstr(key), sp_fmt_str(value)).value;
  return false;
}

bool gen_lower_union(gen_t* g, jtd_schema_t* root) {
  if (root->form != JTD_FORM_DISCRIMINATOR) {
    g->err = sp_fmt(g->mem, "schema must be a discriminator form, got {.red}", sp_fmt_cstr(jtd_form_name(root->form))).value;
    return false;
  }

  sp_da_for(root->as.discriminator.mapping, it) {
    jtd_mapping_t* mapping = &root->as.discriminator.mapping[it];
    sp_str_t name = mapping->schema->form == JTD_FORM_REF ? mapping->schema->as.ref.name : mapping->tag;
    jtd_schema_t* payload = deref(mapping->schema);

    gen_kind_t kind = {
      .tag = mapping->tag,
    };
    if (sp_da_size(payload->as.properties.all)) {
      if (!lower_type(g, name, payload, &kind.payload)) {
        return false;
      }
      add_arm(g, kind.payload);
    }

    if (g->format == GEN_FORMAT_EVENTS) {
      kind.verb = jtd_metadata(mapping->schema, "verb");

      static const c8* verbosities[] = { "normal", "verbose", "debug" };
      if (!lower_metadata_enum(g, mapping->schema, kind.tag, "verbosity", "normal", verbosities, sp_carr_len(verbosities), "SPN_VERBOSITY", &kind.verbosity)) {
        return false;
      }

      static const c8* severities[] = { "info", "error", "fatal" };
      if (!lower_metadata_enum(g, mapping->schema, kind.tag, "severity", "info", severities, sp_carr_len(severities), "SPN_EVENT_SEVERITY", &kind.severity)) {
        return false;
      }
    }

    sp_da_push(g->kinds, kind);
  }

  return true;
}
