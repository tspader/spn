#include "codegen.h"

static sp_str_t type_name(gen_t* g, sp_str_t name) {
  return sp_fmt(g->mem, "spn_cg_{}_t", sp_fmt_str(name)).value;
}

static bool has_required(jtd_schema_t* schema) {
  if (schema->form == JTD_FORM_REF) {
    return has_required(schema->as.ref.target);
  }
  if (schema->form == JTD_FORM_PROPERTIES) {
    return sp_da_size(schema->as.properties.required) > 0;
  }
  return false;
}

static jtd_schema_t* resolve_ref(jtd_schema_t* schema) {
  while (schema && schema->form == JTD_FORM_REF) {
    schema = schema->as.ref.target;
  }
  return schema;
}

static bool schema_is_conv(jtd_schema_t* schema) {
  if (!schema) {
    return false;
  }
  return schema->form == JTD_FORM_ENUM || jtd_metadata_has(schema, "as");
}

static conv_t* register_conv(gen_t* g, sp_str_t name, jtd_schema_t* schema) {
  if (sp_str_om_has(g->convs, name)) {
    return sp_str_om_get(g->convs, name);
  }

  conv_t conv = { .name = sp_str_copy(g->mem, name) };
  if (schema->form == JTD_FORM_ENUM) {
    conv.type = sp_fmt(g->mem, "spn_{}_t", sp_fmt_str(name)).value;
    conv.from = sp_fmt(g->mem, "spn_{}_from_str", sp_fmt_str(name)).value;
    conv.to = sp_fmt(g->mem, "spn_{}_to_str", sp_fmt_str(name)).value;
  } else {
    sp_str_t from = jtd_metadata(schema, "from");
    sp_str_t to = jtd_metadata(schema, "to");
    if (sp_str_empty(from) || sp_str_empty(to)) {
      return SP_NULLPTR;
    }
    conv.type = sp_str_copy(g->mem, jtd_metadata(schema, "as"));
    conv.from = sp_str_copy(g->mem, from);
    conv.to = sp_str_copy(g->mem, to);
  }

  sp_str_om_insert(g->convs, conv.name, conv);
  return sp_str_om_get(g->convs, conv.name);
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

walk_result_t register_type(gen_t* g, jtd_ref_t ref) {
  jtd_schema_t* schema = ref.target;

  if (sp_ht_getp(g->visited, ref.name)) {
    return (walk_result_t) { .err = WALK_OK };
  }
  sp_ht_insert(g->visited, ref.name, true);

  type_t type = {
    .name = sp_str_copy(g->mem, ref.name),
    .fields = sp_da_new(g->mem, field_t),
  };

  sp_da_for(schema->as.properties.all, it) {
    jtd_property_t property = schema->as.properties.all[it];
    jtd_schema_t* sub = property.schema;
    field_t field = sp_zero;
    field.key = property.key;
    field.required = property.required;

    jtd_schema_t* resolved = resolve_ref(sub);
    if (schema_is_conv(resolved)) {
      sp_str_t name = sub->form == JTD_FORM_REF ? sub->as.ref.name : property.key;
      field.conv = register_conv(g, name, resolved);
      if (!field.conv) {
        return (walk_result_t) { .err = WALK_ERR_CONV_BINDING, .type = ref.name, .key = property.key };
      }
      field.kind = FIELD_CONV;
      sp_da_push(type.fields, field);
      continue;
    }

    switch (sub->form) {
      case JTD_FORM_TYPE: {
        switch (sub->as.type) {
          case JTD_TYPE_STRING:  field.kind = FIELD_STR;  break;
          case JTD_TYPE_BOOLEAN: field.kind = FIELD_BOOL; break;
          default:
            return (walk_result_t) {
              .err = WALK_ERR_SCALAR_TYPE,
              .type = ref.name,
              .key = property.key,
              .as.scalar_type = sub->as.type,
            };
        }
        break;
      }
      case JTD_FORM_ELEMENTS: {
        jtd_schema_t* element = sub->as.elements.schema;
        if (element->form == JTD_FORM_TYPE && element->as.type == JTD_TYPE_STRING) {
          field.kind = FIELD_STR_ARRAY;
          break;
        }
        if (element->form == JTD_FORM_REF) {
          walk_result_t err = register_type(g, element->as.ref);
          if (err.err) return err;
          field.kind = FIELD_OBJECT_ARRAY;
          field.object = sp_str_copy(g->mem, element->as.ref.name);
          register_array_type(g, find_type(g, element->as.ref.name));
          break;
        }
        return (walk_result_t) {
          .err = WALK_ERR_ELEMENT_FORM,
          .type = ref.name,
          .key = property.key,
          .as.form = element->form,
        };
      }
      case JTD_FORM_VALUES: {
        sp_str_t map = sp_fmt(g->mem, "{}_{}", sp_fmt_str(ref.name), sp_fmt_str(property.key)).value;
        sp_str_t entry = sp_fmt(g->mem, "{}_entry", sp_fmt_str(map)).value;
        jtd_schema_t* value = sub->as.values.schema;

        if (value->form == JTD_FORM_TYPE && value->as.type == JTD_TYPE_STRING) {
          register_entry(g, entry, sp_str_lit("sp_str_t"));
          field.kind = FIELD_MAP_STR;
        } else if (value->form == JTD_FORM_REF) {
          walk_result_t err = register_type(g, value->as.ref);
          if (err.err) return err;
          register_entry(g, entry, type_name(g, value->as.ref.name));
          field.kind = FIELD_MAP_OBJECT;
          field.object = sp_str_copy(g->mem, value->as.ref.name);
        } else {
          return (walk_result_t) {
            .err = WALK_ERR_MAP_VALUE_FORM,
            .type = ref.name,
            .key = property.key,
            .as.form = value->form,
          };
        }

        field.entry = entry;
        break;
      }
      case JTD_FORM_REF: {
        jtd_ref_t field_ref = sub->as.ref;

        walk_result_t err = register_type(g, field_ref);
        if (err.err) return err;

        field.object = sp_str_copy(g->mem, field_ref.name);
        if (!property.required && has_required(field_ref.target)) {
          field.kind = FIELD_OBJECT_PTR;
        } else {
          field.kind = FIELD_OBJECT;
        }
        break;
      }
      default:
        return (walk_result_t) {
          .err = WALK_ERR_SCHEMA_FORM,
          .type = ref.name,
          .key = property.key,
          .as.form = sub->form,
        };
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
    case WALK_ERR_ELEMENT_FORM:
      return sp_fmt(mem, "{.cyan}.{.cyan}: unsupported array element form {.red}",
        sp_fmt_str(result.type), sp_fmt_str(result.key), sp_fmt_cstr(jtd_form_name(result.as.form))).value;
    case WALK_ERR_MAP_VALUE_FORM:
      return sp_fmt(mem, "{.cyan}.{.cyan}: unsupported map value form {.red}",
        sp_fmt_str(result.type), sp_fmt_str(result.key), sp_fmt_cstr(jtd_form_name(result.as.form))).value;
    case WALK_ERR_SCHEMA_FORM:
      return sp_fmt(mem, "{.cyan}.{.cyan}: unsupported schema form {.red}",
        sp_fmt_str(result.type), sp_fmt_str(result.key), sp_fmt_cstr(jtd_form_name(result.as.form))).value;
    case WALK_ERR_CONV_BINDING:
      return sp_fmt(mem, "{.cyan}.{.cyan}: converter requires metadata as/from/to",
        sp_fmt_str(result.type), sp_fmt_str(result.key)).value;
  }
  return sp_str_lit("unknown error");
}
