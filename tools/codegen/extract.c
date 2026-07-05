#include "codegen.h"

gen_t* gen_new(sp_mem_t mem) {
  gen_t* gen = sp_alloc_type(mem, gen_t);
  gen->mem = mem;
  sp_str_om_init(gen->types);
  gen->entries = sp_da_new(mem, entry_t);
  gen->containers.array = sp_da_new(mem, sp_str_t);
  gen->containers.map = sp_da_new(mem, om_type_t);
  gen->containers.object = sp_da_new(mem, sp_str_t);
  gen->includes = sp_da_new(mem, sp_str_t);
  sp_str_ht_init(mem, gen->visited);
  return gen;
}

type_t* gen_type(gen_t* g, sp_str_t name) {
  if (!sp_str_om_has(g->types, name)) {
    return SP_NULLPTR;
  }
  return sp_str_om_get(g->types, name);
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

static void add_unique(sp_da(sp_str_t) * strs, sp_str_t value) {
  sp_da_for(*strs, it) {
    if (sp_str_equal((*strs)[it], value)) {
      return;
    }
  }
  sp_da_push(*strs, value);
}

static void add_om_type(gen_t* g, sp_str_t object, sp_str_t key_field) {
  sp_da_for(g->containers.map, it) {
    if (sp_str_equal(g->containers.map[it].object, object)) {
      return;
    }
  }
  om_type_t om = { .object = object, .key_field = key_field };
  sp_da_push(g->containers.map, om);
}

static void add_entry(gen_t* g, entry_t entry) {
  sp_da_for(g->entries, it) {
    if (sp_str_equal(g->entries[it].name, entry.name)) {
      return;
    }
  }
  sp_da_push(g->entries, entry);
}

static bool extract_field(gen_t* g, type_t* type, jtd_property_t property) {
  field_t field = {
    .key = property.key,
    .required = property.required,
    .card = CARD_SCALAR,
  };

  jtd_schema_t* value = property.schema;
  if (property.schema->form == JTD_FORM_ELEMENTS) {
    field.card = CARD_ARRAY;
    value = property.schema->as.elements.schema;
  }
  else if (property.schema->form == JTD_FORM_VALUES) {
    field.card = CARD_MAP;
    value = property.schema->as.values.schema;
  }

  sp_str_t name = value->form == JTD_FORM_REF ? value->as.ref.name : property.key;
  jtd_schema_t* target = deref(value);

  if (target->form == JTD_FORM_ENUM) {
    field.kind = FIELD_ENUM;
    field.type_name = name;
    sp_str_t include = jtd_metadata(target, "include");
    if (!sp_str_empty(include)) {
      add_unique(&g->includes, include);
    }
  }
  else if (target->form == JTD_FORM_TYPE && target->as.type == JTD_TYPE_STRING) {
    field.kind = FIELD_STR;
  }
  else if (target->form == JTD_FORM_TYPE && target->as.type == JTD_TYPE_BOOLEAN) {
    field.kind = FIELD_BOOL;
  }
  else if (target->form == JTD_FORM_TYPE) {
    return fail_scalar(g, type->name, property.key, target->as.type);
  }
  else if (target->form == JTD_FORM_PROPERTIES) {
    if (!gen_extract(g, name, target)) {
      return false;
    }
    field.kind = FIELD_STRUCT;
    field.type_name = name;
  }
  else {
    return fail_form(g, type->name, property.key, target->form);
  }

  if (field.card == CARD_MAP && field.kind != FIELD_STR && field.kind != FIELD_STRUCT) {
    return fail_form(g, type->name, property.key, target->form);
  }
  if (field.card == CARD_ARRAY && field.kind == FIELD_BOOL) {
    return fail_form(g, type->name, property.key, target->form);
  }

  if (field.card == CARD_ARRAY && field.kind == FIELD_STRUCT) {
    field.key_field = jtd_metadata(property.schema, "key");
    if (sp_str_empty(field.key_field)) {
      add_unique(&g->containers.array, field.type_name);
    }
    else {
      add_om_type(g, field.type_name, field.key_field);
    }
  }

  if (field.card == CARD_SCALAR && field.kind == FIELD_STRUCT) {
    if (!gen_type(g, field.type_name)) {
      return fail(g, type->name, property.key, sp_str_lit("recursive type must be an array or map"));
    }
    add_unique(&g->containers.object, field.type_name);
  }

  if (field.card == CARD_MAP) {
    field.entry = sp_fmt(g->mem, "{}_{}_entry", sp_fmt_str(type->name), sp_fmt_str(property.key)).value;
    entry_t entry = {
      .name = field.entry,
      .kind = field.kind,
      .object = field.type_name,
    };
    add_entry(g, entry);
  }

  sp_da_push(type->fields, field);
  return true;
}

bool gen_extract(gen_t* g, sp_str_t name, jtd_schema_t* schema) {
  schema = deref(schema);
  if (schema->form != JTD_FORM_PROPERTIES) {
    g->err = sp_fmt(g->mem, "{.cyan}: schema must be a properties form, got {.red}", sp_fmt_str(name), sp_fmt_cstr(jtd_form_name(schema->form))).value;
    return false;
  }

  jtd_schema_t** visited = sp_ht_getp(g->visited, name);
  if (visited) {
    if (*visited != schema) {
      g->err = sp_fmt(g->mem, "{.cyan}: two distinct schemas produce the same type name", sp_fmt_str(name)).value;
      return false;
    }
    return true;
  }
  sp_ht_insert(g->visited, name, schema);

  type_t type = {
    .name = name,
    .fields = sp_da_new(g->mem, field_t),
    .schema = schema,
  };

  sp_da_for(schema->as.properties.all, it) {
    if (!extract_field(g, &type, schema->as.properties.all[it])) {
      return false;
    }
  }

  sp_str_om_insert(g->types, type.name, type);
  return true;
}
