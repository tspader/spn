#include "codegen.h"

#define OK (walk_result_t) { .err = WALK_OK }

static jtd_schema_t* resolve_ref(jtd_schema_t* schema) {
  while (schema && schema->form == JTD_FORM_REF) {
    schema = schema->as.ref.target;
  }
  return schema;
}

static node_t* add_node(gen_t* g, node_t node) {
  if (sp_str_om_has(g->nodes, node.name)) {
    return sp_str_om_get(g->nodes, node.name);
  }
  node.name = sp_str_copy(g->mem, node.name);
  sp_str_om_insert(g->nodes, node.name, node);
  return sp_str_om_get(g->nodes, node.name);
}

static void register_array_type(gen_t* g, type_t* type) {
  sp_str_om_insert(g->array_types, type->name, type);
}

static void register_om_type(gen_t* g, type_t* type, sp_str_t key_field) {
  sp_str_om_insert(g->om_types, type->name, ((om_type_t) { .type = type, .key_field = key_field }));
}

static void register_object_read(gen_t* g, sp_str_t object, sp_str_t owner) {
  sp_da_for(g->object_reads, it) {
    if (sp_str_equal(g->object_reads[it].object, object)) {
      return;
    }
  }

  object_read_t read = sp_zero;
  read.object = sp_str_copy(g->mem, object);
  read.owner = sp_str_copy(g->mem, owner);
  sp_da_push(g->object_reads, read);
}

static void register_entry(gen_t* g, sp_str_t name, sp_str_t value_type, sp_str_t object) {
  sp_da_for(g->entries, it) {
    if (sp_str_equal(g->entries[it].name, name)) {
      return;
    }
  }

  entry_t entry = sp_zero;
  entry.name = sp_str_copy(g->mem, name);
  entry.value_type = value_type;
  entry.object = object;
  sp_da_push(g->entries, entry);
}

static void register_validator(gen_t* g, sp_str_t fn, field_t* field, sp_str_t owner) {
  sp_da_for(g->validators, it) {
    if (sp_str_equal(g->validators[it].fn, fn)) {
      return;
    }
  }

  validator_t validator = sp_zero;
  validator.fn = sp_str_copy(g->mem, fn);
  validator.field = field;
  validator.owner = sp_str_copy(g->mem, owner);
  sp_da_push(g->validators, validator);
}

static converter_t converter_make(gen_t* g, jtd_schema_t* schema, sp_str_t name) {
  sp_str_t type = jtd_metadata(schema, "type");
  if (!sp_str_empty(type)) {
    return (converter_t) {
      .c_type = type,
      .present = jtd_metadata(schema, "present"),
      .custom = true,
    };
  }

  return (converter_t) {
    .c_type = sp_fmt(g->mem, "spn_{}_t", sp_fmt_str(name)).value,
    .from = sp_fmt(g->mem, "spn_{}_from_str", sp_fmt_str(name)).value,
    .to = sp_fmt(g->mem, "spn_{}_to_str", sp_fmt_str(name)).value,
  };
}

static walk_result_t resolve_conversion(gen_t* g, jtd_schema_t* schema, sp_str_t name, sp_str_t owner, sp_str_t key, node_t** out) {
  (void)owner;
  (void)key;

  converter_t conv = converter_make(g, schema, name);
  *out = add_node(g, (node_t) {
    .kind = NODE_CONVERSION,
    .name = name,
    .use_optional = !conv.custom,
    .as.conv = conv,
  });
  return OK;
}

static walk_result_t resolve_node(gen_t* g, jtd_schema_t* schema, sp_str_t name, sp_str_t owner, sp_str_t key, node_t** out) {
  schema = resolve_ref(schema);

  if (schema->form == JTD_FORM_ENUM || !sp_str_empty(jtd_metadata(schema, "type"))) {
    return resolve_conversion(g, schema, name, owner, key, out);
  }

  if (schema->form == JTD_FORM_TYPE) {
    if (schema->as.type == JTD_TYPE_STRING) {
      *out = add_node(g, (node_t) { .kind = NODE_STR, .name = sp_str_lit("str") });
      return OK;
    }
    if (schema->as.type == JTD_TYPE_BOOLEAN) {
      *out = add_node(g, (node_t) { .kind = NODE_BOOL, .name = sp_str_lit("bool"), .use_optional = true });
      return OK;
    }
    return (walk_result_t) { .err = WALK_ERR_SCALAR_TYPE, .type = owner, .key = key, .as.scalar_type = schema->as.type };
  }
  else if (schema->form == JTD_FORM_PROPERTIES) {
    walk_result_t err = register_type(g, name, schema);
    if (err.err) return err;
    *out = add_node(g, (node_t) {
      .kind = NODE_STRUCT,
      .name = name,
      .as.type = find_type(g, name),
    });
    return OK;
  }

  return (walk_result_t) { .err = WALK_ERR_UNSUPPORTED_FORM, .type = owner, .key = key, .as.form = schema->form };
}

static bool visit_type(gen_t* g, sp_str_t name) {
  if (sp_ht_getp(g->visited, name)) {
    return true;
  }
  sp_ht_insert(g->visited, name, true);
  return false;
}

static walk_result_t register_type_into(gen_t* g, sp_str_t name, jtd_schema_t* schema, sp_str_t owner) {
  walk_result_t result = sp_zero;
  if (visit_type(g, name)) return result;

  type_t type = {
    .name = sp_str_copy(g->mem, name),
    .fields = sp_da_new(g->mem, field_t),
    .validate = jtd_metadata(schema, "validate"),
  };

  sp_da_for(schema->as.properties.all, it) {
    jtd_property_t property = schema->as.properties.all[it];
    jtd_schema_t* sub = property.schema;

    cardinality_t card;
    jtd_schema_t* value;
    if (sub->form == JTD_FORM_ELEMENTS) {
      card = CARD_ARRAY;
      value = sub->as.elements.schema;
    } else if (sub->form == JTD_FORM_VALUES) {
      card = CARD_MAP;
      value = sub->as.values.schema;
    } else {
      card = CARD_SCALAR;
      value = sub;
    }

    sp_str_t value_name = value->form == JTD_FORM_REF ? value->as.ref.name : property.key;
    bool flatten = card == CARD_SCALAR
      && !sp_str_empty(jtd_metadata(sub, "flatten"))
      && resolve_ref(value)->form == JTD_FORM_PROPERTIES;

    node_t* node = SP_NULLPTR;
    if (flatten) {
      result = register_type_into(g, value_name, resolve_ref(value), name);
      if (result.err) return result;
      type_t* target = sp_str_om_get(g->flatten_types, value_name);
      node = add_node(g, (node_t) {
        .kind = NODE_STRUCT,
        .name = value_name,
        .as.type = target,
      });
      sp_da_push(g->flattens, ((flatten_t) { target, sp_str_copy(g->mem, name) }));
      register_object_read(g, value_name, name);
    } else {
      result = resolve_node(g, value, value_name, name, property.key, &node);
      if (result.err) return result;
    }

    bool path_element = card == CARD_ARRAY && node->kind == NODE_CONVERSION && node->as.conv.custom;
    if (card != CARD_SCALAR && node->kind != NODE_STR && node->kind != NODE_STRUCT && !path_element) {
      return (walk_result_t) { .err = WALK_ERR_UNSUPPORTED_FORM, .type = name, .key = property.key, .as.form = value->form };
    }

    sp_str_t key_field = card == CARD_ARRAY ? jtd_metadata(sub, "key") : sp_str_lit("");

    field_t field = {
      .key = property.key,
      .required = property.required,
      .flatten = flatten,
      .card = card,
      .node = node,
      .key_field = key_field,
      .validate = jtd_metadata(sub, "validate"),
      .compute = jtd_metadata(sub, "compute"),
    };

    if (card == CARD_ARRAY && node->kind == NODE_STRUCT) {
      if (!sp_str_empty(key_field)) {
        register_om_type(g, node->as.type, key_field);
      } else {
        register_array_type(g, node->as.type);
      }
    }
    if (card == CARD_SCALAR && node->kind == NODE_STRUCT && !flatten) {
      register_object_read(g, node->as.type->name, node->as.type->name);
    }
    if (card == CARD_MAP) {
      field.entry = sp_fmt(g->mem, "{}_{}_entry", sp_fmt_str(name), sp_fmt_str(property.key)).value;
      sp_str_t object = node->kind == NODE_STRUCT ? node->name : sp_str_lit("");
      register_entry(g, field.entry, node_c_type(g, node), object);
    }

    sp_da_push(type.fields, field);
  }

  bool flattened = !sp_str_equal(owner, name);
  type_t* stored;
  if (flattened) {
    sp_str_om_insert(g->flatten_types, type.name, type);
    stored = sp_str_om_get(g->flatten_types, type.name);
  } else {
    sp_str_om_insert(g->types, type.name, type);
    stored = sp_str_om_get(g->types, type.name);
  }

  if (!sp_str_empty(stored->validate)) {
    register_validator(g, stored->validate, SP_NULLPTR, owner);
  }
  sp_da_for(stored->fields, f) {
    field_t* field = &stored->fields[f];
    if (!sp_str_empty(field->validate)) {
      register_validator(g, field->validate, field, sp_str_lit(""));
    }
    if (!sp_str_empty(field->compute)) {
      register_validator(g, field->compute, SP_NULLPTR, owner);
    }
  }

  return (walk_result_t) { .err = WALK_OK };
}

walk_result_t register_type(gen_t* g, sp_str_t name, jtd_schema_t* schema) {
  return register_type_into(g, name, schema, name);
}

sp_str_t walk_result_to_str(sp_mem_t mem, walk_result_t result) {
  switch (result.err) {
    case WALK_OK:
      return sp_str_lit("ok");
    case WALK_ERR_SCALAR_TYPE:
      return sp_fmt(mem, "{.cyan}.{.cyan}: unsupported scalar type {.red}",
        sp_fmt_str(result.type), sp_fmt_str(result.key), sp_fmt_cstr(jtd_type_name(result.as.scalar_type))).value;
    case WALK_ERR_CONV_UNKNOWN:
      return sp_fmt(mem, "{.cyan}.{.cyan}: unknown conversion {.red}",
        sp_fmt_str(result.type), sp_fmt_str(result.key), sp_fmt_str(result.name)).value;
    case WALK_ERR_UNSUPPORTED_FORM:
      return sp_fmt(mem, "{.cyan}.{.cyan}: unsupported schema form {.red}",
        sp_fmt_str(result.type), sp_fmt_str(result.key), sp_fmt_cstr(jtd_form_name(result.as.form))).value;
  }
  return sp_str_lit("unknown error");
}
