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
    case CONVERSION_ENUM: return sp_fmt(g->mem, "spn_{}_t", sp_fmt_str(node->name)).value;
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

static bool field_by_pointer(field_t* field) {
  return field->card == CARD_SCALAR
    && field->node->kind == NODE_STRUCT
    && !field->required
    && field->node->as.type->has_required;
}

static sp_str_t get_struct_type(gen_t* g, field_t* field) {
  sp_str_t t = node_c_type(g, field->node);
  switch (field->card) {
    case CARD_ARRAY: return sp_fmt(g->mem, "sp_da({})", sp_fmt_str(t)).value;
    case CARD_MAP:   return sp_fmt(g->mem, "sp_da({})", sp_fmt_str(type_name(g, field->entry))).value;
    case CARD_SCALAR:
      if (field_by_pointer(field)) {
        return sp_fmt(g->mem, "{}*", sp_fmt_str(t)).value;
      }
      if (!field->required && field->node->use_optional) {
        return sp_fmt(g->mem, "sp_opt({})", sp_fmt_str(t)).value;
      }
      return t;
  }
  return sp_str_lit("");
}

static sp_str_t get_is_present(gen_t* g, field_t* field, sp_str_t recv) {
  if (field->required) {
    return sp_str_lit("true");
  }
  if (field->card == CARD_ARRAY || field->card == CARD_MAP) {
    return sp_fmt(g->mem, "sp_da_size({}{}) > 0", sp_fmt_str(recv), sp_fmt_str(field->key)).value;
  }
  if (field_by_pointer(field)) {
    return sp_fmt(g->mem, "{}{} != SP_NULLPTR", sp_fmt_str(recv), sp_fmt_str(field->key)).value;
  }
  switch (field->node->kind) {
    case NODE_STR: {
      return sp_fmt(g->mem, "!sp_str_empty({}{})", sp_fmt_str(recv), sp_fmt_str(field->key)).value;
    }
    case NODE_BOOL:
    case NODE_CONVERSION: {
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
  if (field->card == CARD_ARRAY) {
    bool object = field->node->kind == NODE_STRUCT;
    *read = object ? sp_str_lit("read/object_array") : sp_str_lit("read/str_array");
    *write = object ? sp_str_lit("write/object_array") : sp_str_lit("write/str_array");
    return;
  }
  if (field->card == CARD_MAP) {
    bool object = field->node->kind == NODE_STRUCT;
    *read = object ? sp_str_lit("read/map_object") : sp_str_lit("read/map_str");
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
      *read = field->required ? sp_str_lit("read/conv_required") : sp_str_lit("read/conv_optional");
      *write = field->required ? sp_str_lit("write/conv_required") : sp_str_lit("write/conv_optional");
      return;
    case NODE_STRUCT:
      *read = field->required ? sp_str_lit("read/object_required")
            : field_by_pointer(field) ? sp_str_lit("read/object_ptr") : sp_str_lit("read/object_optional");
      *write = field_by_pointer(field) ? sp_str_lit("write/object_ptr") : sp_str_lit("write/object");
      return;
  }
}

static void bind_conversion(gen_t* g, sp_template_scope_t* scope, node_t* node) {
  switch (node->as.conversion) {
    case CONVERSION_ENUM:
      sp_template_set(scope, sp_str_lit("from"), sp_fmt(g->mem, "spn_{}_from_str", sp_fmt_str(node->name)).value);
      sp_template_set(scope, sp_str_lit("to"), sp_fmt(g->mem, "spn_{}_to_str", sp_fmt_str(node->name)).value);
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
  }
  if (field->card == CARD_MAP) {
    sp_template_set(scope, sp_str_lit("entry_type"), type_name(g, field->entry));
  }

  sp_str_t read = sp_zero;
  sp_str_t write = sp_zero;
  field_templates(field, &read, &write);
  sp_template_set(scope, sp_str_lit("read"), read);
  sp_template_set(scope, sp_str_lit("write"), write);
  sp_template_set(scope, sp_str_lit("write_field"), field->required ? sp_str_lit("write_req") : sp_str_lit("write_opt"));
  if (!field->required) {
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

  sp_om_for(g->array_types, it) {
    type_t* type = *sp_str_om_at(g->array_types, it);
    sp_template_scope_t* scope = sp_template_scope_create(g->mem);
    sp_template_set(scope, sp_str_lit("object"), type->name);
    sp_template_set(scope, sp_str_lit("type"), type_name(g, type->name));
    render_try(render(out, reg, "read/array", scope));
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
