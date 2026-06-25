#include "codegen.h"

#define render_try(expr) do { render_result_t _result = (expr); if (_result.err) return _result; } while (0)

static sp_str_t undecorated_name(gen_t* g, sp_str_t name) {
  return sp_fmt(g->mem, "spn_cg_{}", sp_fmt_str(name)).value;
}

static sp_str_t type_name(gen_t* g, sp_str_t name) {
  return sp_fmt(g->mem, "spn_cg_{}_t", sp_fmt_str(name)).value;
}

type_t* find_type(gen_t* g, sp_str_t name) {
  if (!sp_str_om_has(g->types, name)) {
    return SP_NULLPTR;
  }
  return sp_str_om_get(g->types, name);
}

static sp_str_t conversion_c_type(gen_t* g, node_t* node) {
  switch (node->as.conversion) {
    case CONVERSION_ENUM:     return sp_fmt(g->mem, "spn_{}_t", sp_fmt_str(node->name)).value;
    case CONVERSION_LAUNCHER: return sp_str_lit("spn_toolchain_launcher_t");
    case CONVERSION_PATH:     return sp_str_lit("sp_str_t");
  }
  return sp_str_lit("");
}

sp_str_t node_c_type(gen_t* g, node_t* node) {
  switch (node->kind) {
    case NODE_STR:        return sp_str_lit("sp_str_t");
    case NODE_BOOL:       return sp_str_lit("bool");
    case NODE_CONVERSION: return conversion_c_type(g, node);
    case NODE_STRUCT:     return type_name(g, node->name);
  }
  return sp_str_lit("");
}

static bool field_is_named(field_t* field) {
  return !sp_str_empty(field->key_field);
}

static sp_str_t get_struct_type(gen_t* g, field_t* field) {
  sp_str_t t = node_c_type(g, field->node);
  if (field_is_named(field)) {
    return sp_fmt(g->mem, "spn_cg_{}_om_t", sp_fmt_str(field->node->name)).value;
  }
  switch (field->card) {
    case CARD_ARRAY: return sp_fmt(g->mem, "sp_da({})", sp_fmt_str(t)).value;
    case CARD_MAP:   return sp_fmt(g->mem, "sp_da({})", sp_fmt_str(type_name(g, field->entry))).value;
    case CARD_SCALAR:
      if (!field->required && field->node->use_optional) {
        return sp_fmt(g->mem, "sp_opt({})", sp_fmt_str(t)).value;
      }
      return t;
  }
  return sp_str_lit("");
}

static sp_str_t get_is_present(gen_t* g, field_t* field, sp_str_t recv) {
  if (field_is_named(field)) {
    return sp_fmt(g->mem, "sp_om_size({}{}) > 0", sp_fmt_str(recv), sp_fmt_str(field->key)).value;
  }
  if (field->required) {
    return sp_str_lit("true");
  }
  if (field->card == CARD_ARRAY || field->card == CARD_MAP) {
    return sp_fmt(g->mem, "sp_da_size({}{}) > 0", sp_fmt_str(recv), sp_fmt_str(field->key)).value;
  }
  switch (field->node->kind) {
    case NODE_STR: {
      return sp_fmt(g->mem, "!sp_str_empty({}{})", sp_fmt_str(recv), sp_fmt_str(field->key)).value;
    }
    case NODE_CONVERSION: {
      if (field->node->as.conversion == CONVERSION_LAUNCHER) {
        return sp_fmt(g->mem, "!sp_str_empty({}{}.program)", sp_fmt_str(recv), sp_fmt_str(field->key)).value;
      }
      if (field->node->as.conversion == CONVERSION_PATH) {
        return sp_fmt(g->mem, "!sp_str_empty({}{})", sp_fmt_str(recv), sp_fmt_str(field->key)).value;
      }
      return sp_fmt(g->mem, "!sp_opt_is_null({}{})", sp_fmt_str(recv), sp_fmt_str(field->key)).value;
    }
    case NODE_BOOL: {
      return sp_fmt(g->mem, "!sp_opt_is_null({}{})", sp_fmt_str(recv), sp_fmt_str(field->key)).value;
    }
    case NODE_STRUCT: {
      type_t* object = field->node->as.type;
      sp_str_t inner = sp_fmt(g->mem, "{}{}.", sp_fmt_str(recv), sp_fmt_str(field->key)).value;
      sp_io_dyn_mem_writer_t terms;
      sp_io_dyn_mem_writer_init(g->mem, &terms);
      sp_da_for(object->fields, it) {
        sp_fmt_io(&terms.base, it ? " || {}" : "{}", sp_fmt_str(get_is_present(g, &object->fields[it], inner)));
      }
      return sp_fmt(g->mem, "({})", sp_fmt_str(sp_io_dyn_mem_writer_as_str(&terms))).value;
    }
  }
  return sp_str_lit("true");
}

static void field_templates(field_t* field, sp_str_t* read, sp_str_t* write) {
  if (field_is_named(field)) {
    *read = sp_str_lit("read/named_field");
    *write = sp_str_lit("write/named");
    return;
  }
  if (field->card == CARD_ARRAY) {
    if (field->node->kind == NODE_CONVERSION && field->node->as.conversion == CONVERSION_PATH) {
      *read = sp_str_lit("read/path_array");
      *write = sp_str_lit("write/str_array");
      return;
    }
    bool object = field->node->kind == NODE_STRUCT;
    *read = object ? sp_str_lit("read/object_array") : sp_str_lit("read/str_array");
    *write = object ? sp_str_lit("write/object_array") : sp_str_lit("write/str_array");
    return;
  }
  if (field->card == CARD_MAP) {
    bool object = field->node->kind == NODE_STRUCT;
    *read = sp_str_lit("read/map_field");
    *write = object ? sp_str_lit("write/map_object") : sp_str_lit("write/map_str");
    return;
  }
  switch (field->node->kind) {
    case NODE_STR:
      *read = field->required ? sp_str_lit("read/str_required") : sp_str_lit("read/str_optional");
      *write = sp_str_lit("write/str");
      return;
    case NODE_BOOL:
      *read = field->required ? sp_str_lit("read/bool_required") : sp_str_lit("read/bool_optional");
      *write = field->required ? sp_str_lit("write/bool_required") : sp_str_lit("write/bool_optional");
      return;
    case NODE_CONVERSION:
      if (field->node->as.conversion == CONVERSION_LAUNCHER) {
        *read = sp_str_lit("read/launcher");
        *write = sp_str_lit("write/launcher");
        return;
      }
      if (field->node->as.conversion == CONVERSION_PATH) {
        *read = sp_str_lit("read/path");
        *write = sp_str_lit("write/str");
        return;
      }
      *read = field->required ? sp_str_lit("read/conv_required") : sp_str_lit("read/conv_optional");
      *write = field->required ? sp_str_lit("write/conv_required") : sp_str_lit("write/conv_optional");
      return;
    case NODE_STRUCT:
      *read = sp_str_lit("read/object_field");
      *write = sp_str_lit("write/object");
      return;
  }
}

static void bind_conversion(gen_t* g, sp_template_scope_t* scope, node_t* node) {
  switch (node->as.conversion) {
    case CONVERSION_ENUM:
      sp_template_set(scope, sp_str_lit("from"), sp_fmt(g->mem, "spn_{}_from_str", sp_fmt_str(node->name)).value);
      sp_template_set(scope, sp_str_lit("to"), sp_fmt(g->mem, "spn_{}_to_str", sp_fmt_str(node->name)).value);
      return;
    case CONVERSION_LAUNCHER:
      return;
    case CONVERSION_PATH:
      return;
  }
}

static void bind_field(gen_t* g, sp_template_scope_t* scope, field_t* field) {
  node_t* node = field->node;
  sp_template_set(scope, sp_str_lit("key"), field->key);

  if (node->kind == NODE_CONVERSION) {
    bind_conversion(g, scope, node);
  }
  if (node->kind == NODE_STRUCT) {
    sp_template_set(scope, sp_str_lit("object"), node->name);
    sp_template_set(scope, sp_str_lit("type"), node_c_type(g, node));
    sp_template_set(scope, sp_str_lit("required"), field->required ? sp_str_lit("true") : sp_str_lit("false"));
  }
  if (field->card == CARD_MAP) {
    sp_template_set(scope, sp_str_lit("entry"), field->entry);
    sp_template_set(scope, sp_str_lit("entry_type"), type_name(g, field->entry));
  }
  if (field_is_named(field)) {
    sp_template_set(scope, sp_str_lit("key_field"), field->key_field);
  }

  sp_str_t read = sp_zero;
  sp_str_t write = sp_zero;
  field_templates(field, &read, &write);
  sp_template_set(scope, sp_str_lit("read"), read);
  sp_template_set(scope, sp_str_lit("write"), write);
  bool guarded = !field->required || field_is_named(field);
  sp_template_set(scope, sp_str_lit("write_field"), guarded ? sp_str_lit("write_opt") : sp_str_lit("write_req"));
  if (guarded) {
    sp_template_set(scope, sp_str_lit("present"), get_is_present(g, field, sp_str_lit("in->")));
  }
}

static render_result_t render(sp_io_writer_t* out, sp_template_registry_t* reg, const c8* key, sp_template_scope_t* scope) {
  sp_str_t source = sp_zero;
  if (!sp_template_get(reg, sp_cstr_as_str(key), &source)) {
    return (render_result_t) {
      .err = RENDER_ERR_TEMPLATE_MISSING,
      .tmpl = sp_cstr_as_str(key),
    };
  }

  sp_template_err_t err = sp_template_render(out, source, scope, reg);
  if (err) {
    return (render_result_t) {
      .err = RENDER_ERR_TEMPLATE_RENDER,
      .tmpl = sp_cstr_as_str(key),
      .code = err,
    };
  }

  return (render_result_t) { .err = RENDER_OK };
}

static render_result_t render_object_struct(gen_t* g, sp_io_writer_t* out, type_t* object, sp_template_registry_t* reg) {
  sp_template_scope_t* scope = sp_template_scope_create(g->mem);
  sp_template_set(scope, sp_str_lit("name"), object->name);
  sp_da_for(object->fields, it) {
    sp_template_scope_t* child = sp_template_push(scope, sp_str_lit("fields"));
    sp_template_set(child, sp_str_lit("type"), get_struct_type(g, &object->fields[it]));
    sp_template_set(child, sp_str_lit("name"), object->fields[it].key);
  }
  return render(out, reg, "struct", scope);
}

static render_result_t render_entry_struct(gen_t* g, sp_io_writer_t* out, entry_t* entry, sp_template_registry_t* reg) {
  sp_template_scope_t* scope = sp_template_scope_create(g->mem);
  sp_template_set(scope, sp_str_lit("name"), entry->name);

  sp_template_scope_t* key = sp_template_push(scope, sp_str_lit("fields"));
  sp_template_set(key, sp_str_lit("type"), sp_str_lit("sp_str_t"));
  sp_template_set(key, sp_str_lit("name"), sp_str_lit("key"));
  sp_template_scope_t* value = sp_template_push(scope, sp_str_lit("fields"));
  sp_template_set(value, sp_str_lit("type"), entry->value_type);
  sp_template_set(value, sp_str_lit("name"), sp_str_lit("value"));

  return render(out, reg, "struct", scope);
}

static render_result_t render_object(gen_t* g, sp_io_writer_t* out, type_t* object, sp_template_registry_t* reg) {
  sp_template_scope_t* scope = sp_template_scope_create(g->mem);
  sp_template_set(scope, sp_str_lit("name"), object->name);
  sp_template_set(scope, sp_str_lit("type"), type_name(g, object->name));

  sp_io_dyn_mem_writer_t validators;
  sp_io_dyn_mem_writer_init(g->mem, &validators);
  sp_da_for(object->fields, it) {
    field_t* field = &object->fields[it];
    if (!sp_str_empty(field->validate)) {
      sp_fmt_io(&validators.base, "  {}(ctx, \"{}\", &out->{});\n",
        sp_fmt_str(field->validate), sp_fmt_str(field->key), sp_fmt_str(field->key));
    }
  }
  if (!sp_str_empty(object->validate)) {
    sp_fmt_io(&validators.base, "  {}(ctx, out);\n", sp_fmt_str(object->validate));
  }
  sp_template_set(scope, sp_str_lit("object_validate"), sp_io_dyn_mem_writer_as_str(&validators));

  sp_da_for(object->fields, it) {
    sp_template_scope_t* child = sp_template_push(scope, sp_str_lit("fields"));
    bind_field(g, child, &object->fields[it]);
  }

  render_try(render(out, reg, "read", scope));
  return render(out, reg, "write", scope);
}

render_result_t render_file(gen_t* g, sp_io_writer_t* out, sp_template_registry_t* reg) {
  render_try(render(out, reg, "header", sp_template_scope_create(g->mem)));

  sp_template_scope_t* tags = sp_template_scope_create(g->mem);
  sp_om_for(g->types, it) {
    sp_template_scope_t* child = sp_template_push(tags, sp_str_lit("tags"));
    sp_template_set(child, sp_str_lit("name"), undecorated_name(g, sp_str_om_at(g->types, it)->name));
  }
  sp_da_for(g->entries, it) {
    sp_template_scope_t* child = sp_template_push(tags, sp_str_lit("tags"));
    sp_template_set(child, sp_str_lit("name"), undecorated_name(g, g->entries[it].name));
  }
  render_try(render(out, reg, "tags", tags));
  sp_fmt_io(out, "\n");

  sp_om_for(g->om_types, it) {
    om_type_t* om = sp_str_om_at(g->om_types, it);
    sp_template_scope_t* scope = sp_template_scope_create(g->mem);
    sp_template_set(scope, sp_str_lit("object"), om->type->name);
    sp_template_set(scope, sp_str_lit("type"), type_name(g, om->type->name));
    render_try(render(out, reg, "named_type", scope));
  }
  sp_fmt_io(out, "\n");

  sp_om_for(g->types, it) {
    render_try(render_object_struct(g, out, sp_str_om_at(g->types, it), reg));
  }
  sp_da_for(g->entries, it) {
    render_try(render_entry_struct(g, out, &g->entries[it], reg));
  }

  sp_template_scope_t* objects = sp_template_scope_create(g->mem);
  sp_om_for(g->types, it) {
    type_t* type = sp_str_om_at(g->types, it);
    sp_template_scope_t* child = sp_template_push(objects, sp_str_lit("objects"));
    sp_template_set(child, sp_str_lit("name"), type->name);
    sp_template_set(child, sp_str_lit("type"), type_name(g, type->name));
  }
  render_try(render(out, reg, "forward", objects));
  sp_fmt_io(out, "\n");

  sp_str_om(u8) seen_validators = SP_NULLPTR;
  sp_str_om_init(seen_validators);
  sp_template_scope_t* validators = sp_template_scope_create(g->mem);
  bool any_validators = false;
  sp_om_for(g->types, it) {
    type_t* type = sp_str_om_at(g->types, it);
    if (!sp_str_empty(type->validate) && !sp_str_om_has(seen_validators, type->validate)) {
      sp_str_om_insert(seen_validators, type->validate, 1);
      any_validators = true;
      sp_template_scope_t* child = sp_template_push(validators, sp_str_lit("validators"));
      sp_template_set(child, sp_str_lit("fn"), type->validate);
      sp_template_set(child, sp_str_lit("params"),
        sp_fmt(g->mem, "{}* value", sp_fmt_str(type_name(g, type->name))).value);
    }
    sp_da_for(type->fields, f) {
      field_t* field = &type->fields[f];
      if (!sp_str_empty(field->validate) && !sp_str_om_has(seen_validators, field->validate)) {
        sp_str_om_insert(seen_validators, field->validate, 1);
        any_validators = true;
        sp_template_scope_t* child = sp_template_push(validators, sp_str_lit("validators"));
        sp_template_set(child, sp_str_lit("fn"), field->validate);
        sp_template_set(child, sp_str_lit("params"),
          sp_fmt(g->mem, "const c8* key, {}* value", sp_fmt_str(get_struct_type(g, field))).value);
      }
    }
  }
  if (any_validators) {
    render_try(render(out, reg, "validators", validators));
    sp_fmt_io(out, "\n");
  }

  sp_om_for(g->array_types, it) {
    type_t* type = *sp_str_om_at(g->array_types, it);
    sp_template_scope_t* scope = sp_template_scope_create(g->mem);
    sp_template_set(scope, sp_str_lit("object"), type->name);
    sp_template_set(scope, sp_str_lit("type"), type_name(g, type->name));
    render_try(render(out, reg, "read/array", scope));
  }
  sp_om_for(g->om_types, it) {
    om_type_t* om = sp_str_om_at(g->om_types, it);
    sp_template_scope_t* scope = sp_template_scope_create(g->mem);
    sp_template_set(scope, sp_str_lit("object"), om->type->name);
    sp_template_set(scope, sp_str_lit("type"), type_name(g, om->type->name));
    sp_template_set(scope, sp_str_lit("key_field"), om->key_field);
    render_try(render(out, reg, "read/named", scope));
  }
  sp_om_for(g->object_types, it) {
    type_t* type = *sp_str_om_at(g->object_types, it);
    sp_template_scope_t* scope = sp_template_scope_create(g->mem);
    sp_template_set(scope, sp_str_lit("object"), type->name);
    sp_template_set(scope, sp_str_lit("type"), type_name(g, type->name));
    render_try(render(out, reg, "read/object", scope));
  }
  sp_da_for(g->entries, it) {
    entry_t* entry = &g->entries[it];
    bool object = !sp_str_empty(entry->object);
    sp_template_scope_t* scope = sp_template_scope_create(g->mem);
    sp_template_set(scope, sp_str_lit("entry"), entry->name);
    sp_template_set(scope, sp_str_lit("entry_type"), type_name(g, entry->name));
    if (object) {
      sp_template_set(scope, sp_str_lit("object"), entry->object);
    }
    render_try(render(out, reg, object ? "read/map_object" : "read/map_str", scope));
  }
  sp_fmt_io(out, "\n");

  sp_om_for(g->types, it) {
    render_try(render_object(g, out, sp_str_om_at(g->types, it), reg));
  }

  sp_template_scope_t* root = sp_template_scope_create(g->mem);
  sp_template_set(root, sp_str_lit("root_type"), type_name(g, g->root->name));
  sp_template_set(root, sp_str_lit("root_name"), g->root->name);
  return render(out, reg, "root", root);
}

sp_str_t render_result_to_str(sp_mem_t mem, render_result_t result) {
  switch (result.err) {
    case RENDER_OK:
      return sp_str_lit("ok");
    case RENDER_ERR_TEMPLATE_MISSING:
      return sp_fmt(mem, "failed to find template {.cyan}", sp_fmt_str(result.tmpl)).value;
    case RENDER_ERR_TEMPLATE_RENDER:
      return sp_fmt(mem, "failed to render template {.cyan} with code {.red}",
        sp_fmt_str(result.tmpl), sp_fmt_int(result.code)).value;
  }
  return sp_str_lit("unknown error");
}
