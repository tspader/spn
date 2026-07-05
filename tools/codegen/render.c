#include "codegen.h"

static sp_str_t undecorated(gen_t* g, sp_str_t name) {
  return sp_fmt(g->mem, "spn_cg_{}", sp_fmt_str(name)).value;
}

static sp_str_t struct_type(gen_t* g, sp_str_t name) {
  return sp_fmt(g->mem, "spn_cg_{}_t", sp_fmt_str(name)).value;
}

static sp_str_t om_type(gen_t* g, sp_str_t name) {
  return sp_fmt(g->mem, "spn_cg_{}_om_t", sp_fmt_str(name)).value;
}

static bool field_named(field_t* field) {
  return !sp_str_empty(field->key_field);
}

static bool field_uses_opt(field_t* field) {
  return field->kind == FIELD_BOOL || field->kind == FIELD_ENUM;
}

static sp_str_t value_type(gen_t* g, field_t* field) {
  switch (field->kind) {
    case FIELD_STR:    return sp_str_lit("sp_str_t");
    case FIELD_BOOL:   return sp_str_lit("bool");
    case FIELD_ENUM:   return sp_fmt(g->mem, "spn_{}_t", sp_fmt_str(field->type_name)).value;
    case FIELD_STRUCT: return struct_type(g, field->type_name);
  }
  return sp_str_lit("");
}

static sp_str_t field_type(gen_t* g, field_t* field) {
  if (field_named(field)) {
    return om_type(g, field->type_name);
  }
  switch (field->card) {
    case CARD_ARRAY: return sp_fmt(g->mem, "sp_da({})", sp_fmt_str(value_type(g, field))).value;
    case CARD_MAP:   return sp_fmt(g->mem, "sp_da({})", sp_fmt_str(struct_type(g, field->entry))).value;
    case CARD_SCALAR: {
      if (!field->required && field_uses_opt(field)) {
        return sp_fmt(g->mem, "sp_opt({})", sp_fmt_str(value_type(g, field))).value;
      }
      return value_type(g, field);
    }
  }
  return sp_str_lit("");
}

static sp_str_t present_expr(gen_t* g, field_t* field, sp_str_t recv) {
  if (field_named(field)) {
    return sp_fmt(g->mem, "sp_om_size({}{}) > 0", sp_fmt_str(recv), sp_fmt_str(field->key)).value;
  }
  if (field->required) {
    return sp_str_lit("true");
  }
  if (field->card == CARD_ARRAY || field->card == CARD_MAP) {
    return sp_fmt(g->mem, "sp_da_size({}{}) > 0", sp_fmt_str(recv), sp_fmt_str(field->key)).value;
  }
  switch (field->kind) {
    case FIELD_STR: {
      return sp_fmt(g->mem, "!sp_str_empty({}{})", sp_fmt_str(recv), sp_fmt_str(field->key)).value;
    }
    case FIELD_BOOL:
    case FIELD_ENUM: {
      return sp_fmt(g->mem, "!sp_opt_is_null({}{})", sp_fmt_str(recv), sp_fmt_str(field->key)).value;
    }
    case FIELD_STRUCT: {
      type_t* object = gen_type(g, field->type_name);
      sp_str_t inner = sp_fmt(g->mem, "{}{}.", sp_fmt_str(recv), sp_fmt_str(field->key)).value;
      sp_io_dyn_mem_writer_t terms;
      sp_io_dyn_mem_writer_init(g->mem, &terms);
      sp_da_for(object->fields, it) {
        sp_fmt_io(&terms.base, it ? " || {}" : "{}", sp_fmt_str(present_expr(g, &object->fields[it], inner)));
      }
      return sp_fmt(g->mem, "({})", sp_fmt_str(sp_io_dyn_mem_writer_as_str(&terms))).value;
    }
  }
  return sp_str_lit("true");
}

static void field_templates(field_t* field, sp_str_t* read, sp_str_t* write) {
  if (field_named(field)) {
    *read = sp_str_lit("read/named_field");
    *write = sp_str_lit("write/named");
    return;
  }
  if (field->card == CARD_ARRAY) {
    bool object = field->kind == FIELD_STRUCT;
    *read = object ? sp_str_lit("read/object_array") : sp_str_lit("read/str_array");
    *write = object ? sp_str_lit("write/object_array") : sp_str_lit("write/str_array");
    return;
  }
  if (field->card == CARD_MAP) {
    *read = sp_str_lit("read/map_field");
    *write = field->kind == FIELD_STRUCT ? sp_str_lit("write/map_object") : sp_str_lit("write/map_str");
    return;
  }
  switch (field->kind) {
    case FIELD_STR: {
      *read = field->required ? sp_str_lit("read/str_required") : sp_str_lit("read/str_optional");
      *write = sp_str_lit("write/str");
      return;
    }
    case FIELD_BOOL: {
      *read = field->required ? sp_str_lit("read/bool_required") : sp_str_lit("read/bool_optional");
      *write = field->required ? sp_str_lit("write/bool_required") : sp_str_lit("write/bool_optional");
      return;
    }
    case FIELD_ENUM: {
      *read = field->required ? sp_str_lit("read/conv_required") : sp_str_lit("read/conv_optional");
      *write = field->required ? sp_str_lit("write/conv_required") : sp_str_lit("write/conv_optional");
      return;
    }
    case FIELD_STRUCT: {
      *read = sp_str_lit("read/object_field");
      *write = sp_str_lit("write/object");
      return;
    }
  }
}

static void bind_field(gen_t* g, sp_template_scope_t* scope, field_t* field) {
  sp_template_set(scope, sp_str_lit("key"), field->key);

  if (field->kind == FIELD_ENUM) {
    sp_template_set(scope, sp_str_lit("from"), sp_fmt(g->mem, "spn_{}_from_str", sp_fmt_str(field->type_name)).value);
    sp_template_set(scope, sp_str_lit("to"), sp_fmt(g->mem, "spn_{}_to_str", sp_fmt_str(field->type_name)).value);
  }
  if (field->kind == FIELD_STRUCT) {
    sp_template_set(scope, sp_str_lit("object"), field->type_name);
    sp_template_set(scope, sp_str_lit("type"), struct_type(g, field->type_name));
    sp_template_set(scope, sp_str_lit("required"), field->required ? sp_str_lit("true") : sp_str_lit("false"));
  }
  if (field->card == CARD_MAP) {
    sp_template_set(scope, sp_str_lit("entry"), field->entry);
    sp_template_set(scope, sp_str_lit("entry_type"), struct_type(g, field->entry));
  }
  if (field_named(field)) {
    sp_template_set(scope, sp_str_lit("key_field"), field->key_field);
  }

  sp_str_t read = sp_zero;
  sp_str_t write = sp_zero;
  field_templates(field, &read, &write);
  sp_template_set(scope, sp_str_lit("read"), read);
  sp_template_set(scope, sp_str_lit("write"), write);

  bool guarded = !field->required || field_named(field);
  sp_template_set(scope, sp_str_lit("write_field"), guarded ? sp_str_lit("write_opt") : sp_str_lit("write_req"));
  if (guarded) {
    sp_template_set(scope, sp_str_lit("present"), present_expr(g, field, sp_str_lit("in->")));
  }
}

bool cg_render(sp_mem_t mem, sp_str_t* err, sp_io_writer_t* io, sp_template_registry_t* reg, const c8* name, sp_template_scope_t* scope) {
  sp_str_t source = sp_zero;
  if (!sp_template_get(reg, sp_cstr_as_str(name), &source)) {
    *err = sp_fmt(mem, "failed to find template {.cyan}", sp_fmt_cstr(name)).value;
    return false;
  }

  sp_template_err_t result = sp_template_render(io, source, scope, reg);
  if (result) {
    *err = sp_fmt(mem, "failed to render template {.cyan} with code {.red}", sp_fmt_cstr(name), sp_fmt_int(result)).value;
    return false;
  }

  return true;
}

static bool gen_render(gen_t* g, sp_io_writer_t* io, sp_template_registry_t* reg, const c8* name, sp_template_scope_t* scope) {
  return cg_render(g->mem, &g->err, io, reg, name, scope);
}

static void bind_struct_field(sp_template_scope_t* scope, sp_str_t type, sp_str_t name) {
  sp_template_scope_t* child = sp_template_push(scope, sp_str_lit("fields"));
  sp_template_set(child, sp_str_lit("type"), type);
  sp_template_set(child, sp_str_lit("name"), name);
}

static bool render_struct(gen_t* g, sp_io_writer_t* io, sp_template_registry_t* reg, type_t* type) {
  sp_template_scope_t* scope = sp_template_scope_create(g->mem);
  sp_template_set(scope, sp_str_lit("name"), type->name);
  sp_da_for(type->fields, it) {
    field_t* field = &type->fields[it];
    bind_struct_field(scope, field_type(g, field), field->key);
  }
  return gen_render(g, io, reg, "struct", scope);
}

static bool render_entry_struct(gen_t* g, sp_io_writer_t* io, sp_template_registry_t* reg, entry_t* entry) {
  sp_template_scope_t* scope = sp_template_scope_create(g->mem);
  sp_template_set(scope, sp_str_lit("name"), entry->name);
  bind_struct_field(scope, sp_str_lit("sp_str_t"), sp_str_lit("key"));
  bind_struct_field(scope, entry->kind == FIELD_STRUCT ? struct_type(g, entry->object) : sp_str_lit("sp_str_t"), sp_str_lit("value"));
  return gen_render(g, io, reg, "struct", scope);
}

static bool render_object(gen_t* g, sp_io_writer_t* io, sp_template_registry_t* reg, type_t* type, const c8* tmpl) {
  sp_template_scope_t* scope = sp_template_scope_create(g->mem);
  sp_template_set(scope, sp_str_lit("name"), type->name);
  sp_template_set(scope, sp_str_lit("type"), struct_type(g, type->name));
  sp_da_for(type->fields, it) {
    bind_field(g, sp_template_push(scope, sp_str_lit("fields")), &type->fields[it]);
  }
  return gen_render(g, io, reg, tmpl, scope);
}

static bool render_types(gen_t* g, sp_io_writer_t* io, sp_template_registry_t* reg) {
  sp_template_scope_t* tags = sp_template_scope_create(g->mem);
  sp_om_for(g->types, it) {
    type_t* type = sp_str_om_at(g->types, it);
    if (type->shared) {
      continue;
    }
    sp_template_set(sp_template_push(tags, sp_str_lit("tags")), sp_str_lit("name"), undecorated(g, type->name));
  }
  sp_da_for(g->entries, it) {
    sp_template_set(sp_template_push(tags, sp_str_lit("tags")), sp_str_lit("name"), undecorated(g, g->entries[it].name));
  }
  if (!gen_render(g, io, reg, "tags", tags)) {
    return false;
  }
  sp_fmt_io(io, "\n");

  sp_da_for(g->containers.map, it) {
    sp_template_scope_t* scope = sp_template_scope_create(g->mem);
    sp_template_set(scope, sp_str_lit("object"), g->containers.map[it].object);
    sp_template_set(scope, sp_str_lit("type"), struct_type(g, g->containers.map[it].object));
    if (!gen_render(g, io, reg, "named_type", scope)) {
      return false;
    }
  }
  sp_fmt_io(io, "\n");

  sp_om_for(g->types, it) {
    type_t* type = sp_str_om_at(g->types, it);
    if (type->shared) {
      continue;
    }
    if (!render_struct(g, io, reg, type)) {
      return false;
    }
  }
  sp_da_for(g->entries, it) {
    if (!render_entry_struct(g, io, reg, &g->entries[it])) {
      return false;
    }
  }
  return true;
}

static sp_template_scope_t* includes_scope(gen_t* g, sp_template_scope_t* scope) {
  sp_template_list(scope, sp_str_lit("includes"));
  sp_da_for(g->includes, it) {
    sp_template_set(sp_template_push(scope, sp_str_lit("includes")), sp_str_lit("name"), g->includes[it]);
  }
  return scope;
}

bool render_common(gen_t* g, sp_io_writer_t* io, sp_template_registry_t* reg) {
  if (!gen_render(g, io, reg, "common", includes_scope(g, sp_template_scope_create(g->mem)))) {
    return false;
  }
  if (!render_types(g, io, reg)) {
    return false;
  }
  sp_fmt_io(io, "\n#endif\n");
  return true;
}

static sp_template_scope_t* root_scope(gen_t* g) {
  sp_template_scope_t* root = sp_template_scope_create(g->mem);
  sp_template_set(root, sp_str_lit("root_name"), g->root->name);
  sp_template_set(root, sp_str_lit("guard"), sp_str_to_upper(g->mem, g->root->name));
  return includes_scope(g, root);
}

bool render_decls(gen_t* g, sp_io_writer_t* io, sp_template_registry_t* reg) {
  sp_template_scope_t* root = root_scope(g);
  if (!gen_render(g, io, reg, "header", root)) {
    return false;
  }
  if (!render_types(g, io, reg)) {
    return false;
  }
  sp_fmt_io(io, "\n");
  if (!gen_render(g, io, reg, "root_decl", root)) {
    return false;
  }

  sp_fmt_io(io, "\n#endif\n");
  return true;
}

bool render_impl(gen_t* g, sp_io_writer_t* io, sp_template_registry_t* reg) {
  sp_template_scope_t* root = root_scope(g);
  if (!gen_render(g, io, reg, "header_impl", root)) {
    return false;
  }

  sp_template_scope_t* objects = sp_template_scope_create(g->mem);
  sp_om_for(g->types, it) {
    type_t* type = sp_str_om_at(g->types, it);
    sp_template_scope_t* child = sp_template_push(objects, sp_str_lit("objects"));
    sp_template_set(child, sp_str_lit("name"), type->name);
    sp_template_set(child, sp_str_lit("type"), struct_type(g, type->name));
  }
  if (!gen_render(g, io, reg, "forward", objects)) {
    return false;
  }
  sp_fmt_io(io, "\n");

  sp_da_for(g->containers.array, it) {
    sp_template_scope_t* scope = sp_template_scope_create(g->mem);
    sp_template_set(scope, sp_str_lit("object"), g->containers.array[it]);
    sp_template_set(scope, sp_str_lit("type"), struct_type(g, g->containers.array[it]));
    if (!gen_render(g, io, reg, "read/array", scope)) {
      return false;
    }
  }
  sp_da_for(g->containers.map, it) {
    om_type_t* om = &g->containers.map[it];
    sp_template_scope_t* scope = sp_template_scope_create(g->mem);
    sp_template_set(scope, sp_str_lit("object"), om->object);
    sp_template_set(scope, sp_str_lit("type"), struct_type(g, om->object));
    sp_template_set(scope, sp_str_lit("key_field"), om->key_field);
    if (!gen_render(g, io, reg, "read/named", scope)) {
      return false;
    }
  }
  sp_da_for(g->containers.object, it) {
    sp_template_scope_t* scope = sp_template_scope_create(g->mem);
    sp_template_set(scope, sp_str_lit("object"), g->containers.object[it]);
    sp_template_set(scope, sp_str_lit("type"), struct_type(g, g->containers.object[it]));
    if (!gen_render(g, io, reg, "read/object", scope)) {
      return false;
    }
  }
  sp_da_for(g->entries, it) {
    entry_t* entry = &g->entries[it];
    sp_template_scope_t* scope = sp_template_scope_create(g->mem);
    sp_template_set(scope, sp_str_lit("entry"), entry->name);
    sp_template_set(scope, sp_str_lit("entry_type"), struct_type(g, entry->name));
    if (entry->kind == FIELD_STRUCT) {
      sp_template_set(scope, sp_str_lit("object"), entry->object);
    }
    if (!gen_render(g, io, reg, entry->kind == FIELD_STRUCT ? "read/map_object" : "read/map_str", scope)) {
      return false;
    }
  }
  sp_fmt_io(io, "\n");

  sp_om_for(g->types, it) {
    type_t* type = sp_str_om_at(g->types, it);
    if (!render_object(g, io, reg, type, "read")) {
      return false;
    }
    if (!render_object(g, io, reg, type, "write")) {
      return false;
    }
  }

  if (!gen_render(g, io, reg, "root_read", root)) {
    return false;
  }
  sp_fmt_io(io, "\n");
  return gen_render(g, io, reg, "root_write", root);
}
