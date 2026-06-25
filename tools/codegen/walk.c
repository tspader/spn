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

static void register_entry(gen_t* g, sp_str_t name, sp_str_t value_type) {
  sp_da_for(g->entries, it) {
    if (sp_str_equal(g->entries[it].name, name)) {
      return;
    }
  }

  entry_t entry = sp_zero;
  entry.name = sp_str_copy(g->mem, name);
  entry.value_type = value_type;
  sp_da_push(g->entries, entry);
}

static walk_result_t resolve_conversion(gen_t* g, jtd_schema_t* schema, sp_str_t name, sp_str_t owner, sp_str_t key, node_t** out) {
  sp_str_t convert = jtd_metadata(schema, "convert");

  if (sp_str_equal_cstr(convert, "enum")) {
    sp_str_t named = jtd_metadata(schema, "enum");
    if (!sp_str_empty(named)) {
      name = named;
    }
  }
  else if (!sp_str_empty(convert)) {
    return (walk_result_t) { .err = WALK_ERR_CONV_UNKNOWN, .type = owner, .key = key, .name = convert };
  }

  *out = add_node(g, (node_t) {
    .kind = NODE_CONVERSION,
    .name = name,
    .use_optional = true,
    .as.conversion = CONVERSION_ENUM,
  });
  return OK;
}

static walk_result_t resolve_node(gen_t* g, jtd_schema_t* schema, sp_str_t name, sp_str_t owner, sp_str_t key, node_t** out) {
  schema = resolve_ref(schema);

  if (!sp_str_empty(jtd_metadata(schema, "convert")) || schema->form == JTD_FORM_ENUM) {
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

walk_result_t register_type(gen_t* g, sp_str_t name, jtd_schema_t* schema) {
  walk_result_t result = sp_zero;
  if (visit_type(g, name)) return result;

  type_t type = {
    .name = sp_str_copy(g->mem, name),
    .fields = sp_da_new(g->mem, field_t),
    .has_required = sp_da_size(schema->as.properties.required) > 0,
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
    node_t* node = SP_NULLPTR;
    result = resolve_node(g, value, value_name, name, property.key, &node);
    if (result.err) return result;

    if (card != CARD_SCALAR && node->kind != NODE_STR && node->kind != NODE_STRUCT) {
      return (walk_result_t) { .err = WALK_ERR_UNSUPPORTED_FORM, .type = name, .key = property.key, .as.form = value->form };
    }

    field_t field = {
      .key = property.key,
      .required = property.required,
      .card = card,
      .node = node,
    };

    if (card == CARD_ARRAY && node->kind == NODE_STRUCT) {
      register_array_type(g, node->as.type);
    }
    if (card == CARD_MAP) {
      field.entry = sp_fmt(g->mem, "{}_{}_entry", sp_fmt_str(name), sp_fmt_str(property.key)).value;
      register_entry(g, field.entry, node_c_type(g, node));
    }

    sp_da_push(type.fields, field);
  }

  sp_str_om_insert(g->types, type.name, type);
  return (walk_result_t) { .err = WALK_OK };
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
