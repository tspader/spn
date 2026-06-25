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

static sp_str_t get_struct_type(gen_t* g, field_t* field) {
  switch (field->kind) {
    case FIELD_STR:          return sp_str_lit("sp_str_t");
    case FIELD_BOOL:         return field->required ? sp_str_lit("bool") : sp_str_lit("sp_opt(bool)");
    case FIELD_CONV:         return field->required ? field->conv->type : sp_fmt(g->mem, "sp_opt({})", sp_fmt_str(field->conv->type)).value;
    case FIELD_STR_ARRAY:    return sp_str_lit("sp_da(sp_str_t)");
    case FIELD_OBJECT:       return type_name(g, field->object);
    case FIELD_OBJECT_PTR:   return sp_fmt(g->mem, "{}*", sp_fmt_str(type_name(g, field->object))).value;
    case FIELD_OBJECT_ARRAY: return sp_fmt(g->mem, "sp_da({})", sp_fmt_str(type_name(g, field->object))).value;
    case FIELD_MAP_STR:
    case FIELD_MAP_OBJECT:   return sp_fmt(g->mem, "sp_da({})", sp_fmt_str(type_name(g, field->entry))).value;
  }
  return sp_str_lit("");
}

static sp_str_t get_is_present(gen_t* g, field_t* field, sp_str_t recv) {
  if (field->required) {
    return sp_str_lit("true");
  }
  switch (field->kind) {
    case FIELD_STR: {
      return sp_fmt(g->mem, "!sp_str_empty({}{})", sp_fmt_str(recv), sp_fmt_str(field->key)).value;
    }
    case FIELD_BOOL:
    case FIELD_CONV: {
      return sp_fmt(g->mem, "!sp_opt_is_null({}{})", sp_fmt_str(recv), sp_fmt_str(field->key)).value;
    }
    case FIELD_STR_ARRAY:
    case FIELD_OBJECT_ARRAY:
    case FIELD_MAP_STR:
    case FIELD_MAP_OBJECT: {
      return sp_fmt(g->mem, "sp_da_size({}{}) > 0", sp_fmt_str(recv), sp_fmt_str(field->key)).value;
    }
    case FIELD_OBJECT_PTR: {
      return sp_fmt(g->mem, "{}{} != SP_NULLPTR", sp_fmt_str(recv), sp_fmt_str(field->key)).value;
    }
    case FIELD_OBJECT: {
      type_t* object = find_type(g, field->object);
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

static void bind_field(gen_t* g, sp_template_scope_t* scope, field_t* field) {
  sp_template_set(scope, sp_str_lit("key"), field->key);

  sp_str_t read = sp_zero;
  sp_str_t write = sp_zero;
  switch (field->kind) {
    case FIELD_STR: {
      read = field->required ? sp_str_lit("read/str_required") : sp_str_lit("read/str_optional");
      write = sp_str_lit("write/str");
      break;
    }
    case FIELD_BOOL: {
      read = field->required ? sp_str_lit("read/bool_required") : sp_str_lit("read/bool_optional");
      write = field->required ? sp_str_lit("write/bool_required") : sp_str_lit("write/bool_optional");
      break;
    }
    case FIELD_CONV: {
      read = field->required ? sp_str_lit("read/conv_required") : sp_str_lit("read/conv_optional");
      write = field->required ? sp_str_lit("write/conv_required") : sp_str_lit("write/conv_optional");
      sp_template_set(scope, sp_str_lit("from"), field->conv->from);
      sp_template_set(scope, sp_str_lit("to"), field->conv->to);
      break;
    }
    case FIELD_STR_ARRAY: {
      read = sp_str_lit("read/str_array");
      write = sp_str_lit("write/str_array");
      break;
    }
    case FIELD_OBJECT_ARRAY: {
      read = sp_str_lit("read/object_array");
      write = sp_str_lit("write/object_array");
      sp_template_set(scope, sp_str_lit("object"), field->object);
      sp_template_set(scope, sp_str_lit("type"), type_name(g, field->object));
      break;
    }
    case FIELD_OBJECT: {
      read = field->required ? sp_str_lit("read/object_required") : sp_str_lit("read/object_optional");
      write = sp_str_lit("write/object");
      sp_template_set(scope, sp_str_lit("object"), field->object);
      break;
    }
    case FIELD_OBJECT_PTR: {
      read = sp_str_lit("read/object_ptr");
      write = sp_str_lit("write/object_ptr");
      sp_template_set(scope, sp_str_lit("object"), field->object);
      sp_template_set(scope, sp_str_lit("type"), type_name(g, field->object));
      break;
    }
    case FIELD_MAP_STR: {
      read = sp_str_lit("read/map_str");
      write = sp_str_lit("write/map_str");
      sp_template_set(scope, sp_str_lit("entry_type"), type_name(g, field->entry));
      break;
    }
    case FIELD_MAP_OBJECT: {
      read = sp_str_lit("read/map_object");
      write = sp_str_lit("write/map_object");
      sp_template_set(scope, sp_str_lit("entry_type"), type_name(g, field->entry));
      sp_template_set(scope, sp_str_lit("object"), field->object);
      break;
    }
  }

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
