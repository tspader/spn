#define SP_TEMPLATE_IMPLEMENTATION
#include "gen.h"

typedef struct {
  const c8* c_type;
  const c8* toml_read;
  const c8* json_get;
  const c8* json_write;
} gen_prim_row_t;

static const gen_prim_row_t GEN_PRIMS[] = {
  [PRIM_BOOL] = { .c_type = "bool", .toml_read = "read_bool", .json_get = "yyjson_get_bool", .json_write = "spn_codegen_json_bool" },
  [PRIM_U64]  = { .c_type = "u64",  .toml_read = "read_u64",  .json_get = "yyjson_get_uint", .json_write = "spn_codegen_json_u64" },
};

static const c8* GEN_FORMATS[] = {
  [GEN_FORMAT_TOML] = "toml",
  [GEN_FORMAT_JSON] = "json",
};

static sp_str_t struct_type(gen_t* g, sp_str_t name) {
  return sp_fmt(g->mem, "spn_cg_{}_t", sp_fmt_str(name)).value;
}

static sp_str_t enum_type(gen_t* g, sp_str_t name) {
  return sp_fmt(g->mem, "spn_{}_t", sp_fmt_str(name)).value;
}

static sp_str_t decl_type(gen_t* g, gen_field_t* f) {
  switch (f->kind) {
    case FIELD_STR: {
      return sp_str_lit("sp_str_t");
    }
    case FIELD_PRIM: {
      sp_str_t c_type = sp_cstr_as_str(GEN_PRIMS[f->as.prim].c_type);
      if (f->required) {
        return c_type;
      }
      return sp_fmt(g->mem, "sp_opt({})", sp_fmt_str(c_type)).value;
    }
    case FIELD_ENUM: {
      sp_str_t c_type = enum_type(g, f->as.enumeration);
      if (f->required) {
        return c_type;
      }
      return sp_fmt(g->mem, "sp_opt({})", sp_fmt_str(c_type)).value;
    }
    case FIELD_OBJECT: {
      return struct_type(g, f->as.object->name);
    }
    case FIELD_EXTERN: {
      return enum_type(g, f->as.ext);
    }
    case FIELD_STR_ARRAY: {
      return sp_str_lit("sp_da(sp_str_t)");
    }
    case FIELD_ENUM_ARRAY: {
      return sp_fmt(g->mem, "sp_da({})", sp_fmt_str(enum_type(g, f->as.enumeration))).value;
    }
    case FIELD_OBJECT_ARRAY: {
      return sp_fmt(g->mem, "sp_da({})", sp_fmt_str(struct_type(g, f->as.array.object->name))).value;
    }
    case FIELD_KEYED: {
      return sp_fmt(g->mem, "spn_cg_{}_om_t", sp_fmt_str(f->as.keyed.object->name)).value;
    }
    case FIELD_MAP: {
      return sp_fmt(g->mem, "sp_da({})", sp_fmt_str(struct_type(g, f->as.map->name))).value;
    }
  }
  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

static bool value_is_str(gen_field_t* f) {
  switch (f->kind) {
    case FIELD_STR:
    case FIELD_STR_ARRAY: {
      return true;
    }
    case FIELD_MAP: {
      return !f->as.map->object;
    }
    case FIELD_PRIM:
    case FIELD_ENUM:
    case FIELD_OBJECT:
    case FIELD_EXTERN:
    case FIELD_ENUM_ARRAY:
    case FIELD_OBJECT_ARRAY:
    case FIELD_KEYED: {
      return false;
    }
  }
  SP_UNREACHABLE_RETURN(false);
}

static sp_str_t present(gen_t* g, gen_field_t* f, sp_str_t recv) {
  if (f->kind == FIELD_KEYED) {
    return sp_fmt(g->mem, "sp_om_size({}{}) > 0", sp_fmt_str(recv), sp_fmt_str(f->name)).value;
  }
  if (f->required && !value_is_str(f)) {
    return sp_str_lit("true");
  }
  switch (f->kind) {
    case FIELD_STR_ARRAY:
    case FIELD_ENUM_ARRAY:
    case FIELD_OBJECT_ARRAY:
    case FIELD_MAP: {
      return sp_fmt(g->mem, "sp_da_size({}{}) > 0", sp_fmt_str(recv), sp_fmt_str(f->name)).value;
    }
    case FIELD_STR: {
      return sp_fmt(g->mem, "!sp_str_empty({}{})", sp_fmt_str(recv), sp_fmt_str(f->name)).value;
    }
    case FIELD_PRIM:
    case FIELD_ENUM: {
      return sp_fmt(g->mem, "!sp_opt_is_null({}{})", sp_fmt_str(recv), sp_fmt_str(f->name)).value;
    }
    case FIELD_EXTERN: {
      return sp_fmt(g->mem, "spn_codegen_{}_present(&{}{})", sp_fmt_str(f->as.ext), sp_fmt_str(recv), sp_fmt_str(f->name)).value;
    }
    case FIELD_OBJECT: {
      gen_type_t* object = f->as.object;
      sp_str_t inner = sp_fmt(g->mem, "{}{}.", sp_fmt_str(recv), sp_fmt_str(f->name)).value;
      sp_io_dyn_mem_writer_t terms;
      sp_io_dyn_mem_writer_init(g->mem, &terms);
      sp_da_for(object->fields, it) {
        sp_fmt_io(&terms.base, it ? " || {}" : "{}", sp_fmt_str(present(g, &object->fields[it], inner)));
      }
      return sp_fmt(g->mem, "({})", sp_fmt_str(sp_io_dyn_mem_writer_as_str(&terms))).value;
    }
    case FIELD_KEYED: {
      break;
    }
  }
  SP_UNREACHABLE_RETURN(sp_str_lit("true"));
}

static sp_str_t table_present(gen_t* g, gen_field_t* f) {
  gen_type_t* object = f->as.array.object;
  sp_str_t recv = sp_fmt(g->mem, "in->{}[it].", sp_fmt_str(f->name)).value;
  sp_io_dyn_mem_writer_t terms;
  sp_io_dyn_mem_writer_init(g->mem, &terms);
  bool any = false;
  sp_da_for(object->fields, it) {
    gen_field_t* inner = &object->fields[it];
    if (sp_str_equal(inner->key, f->as.array.shorthand)) {
      continue;
    }
    sp_fmt_io(&terms.base, any ? " || {}" : "{}", sp_fmt_str(present(g, inner, recv)));
    any = true;
  }
  return any ? sp_io_dyn_mem_writer_as_str(&terms) : sp_str_lit("false");
}

static sp_str_t write_value(gen_t* g, gen_field_t* f) {
  switch (f->kind) {
    case FIELD_STR: {
      return sp_fmt(g->mem, "in->{}", sp_fmt_str(f->name)).value;
    }
    case FIELD_PRIM: {
      if (f->required) {
        return sp_fmt(g->mem, "in->{}", sp_fmt_str(f->name)).value;
      }
      return sp_fmt(g->mem, "in->{}.value", sp_fmt_str(f->name)).value;
    }
    case FIELD_ENUM: {
      if (f->required) {
        return sp_fmt(g->mem, "spn_{}_to_str(in->{})", sp_fmt_str(f->as.enumeration), sp_fmt_str(f->name)).value;
      }
      return sp_fmt(g->mem, "spn_{}_to_str(in->{}.value)", sp_fmt_str(f->as.enumeration), sp_fmt_str(f->name)).value;
    }
    case FIELD_OBJECT:
    case FIELD_EXTERN:
    case FIELD_STR_ARRAY:
    case FIELD_ENUM_ARRAY:
    case FIELD_OBJECT_ARRAY:
    case FIELD_KEYED:
    case FIELD_MAP: {
      break;
    }
  }
  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

static void bind_field(gen_t* g, sp_template_scope_t* scope, gen_field_t* f) {
  sp_template_set(scope, sp_str_lit("key"), f->key);
  sp_template_set(scope, sp_str_lit("name"), f->name);
  if (f->required) {
    sp_template_set(scope, sp_str_lit("req"), sp_str_lit("1"));
  }

  bool guarded = !f->required || f->kind == FIELD_KEYED;
  if (guarded) {
    sp_template_set(scope, sp_str_lit("guarded"), sp_str_lit("1"));
    sp_template_set(scope, sp_str_lit("present"), present(g, f, sp_str_lit("in->")));
  }

  switch (f->kind) {
    case FIELD_STR: {
      sp_template_set(scope, sp_str_lit("k_str"), sp_str_lit("1"));
      sp_template_set(scope, sp_str_lit("jwrite"), sp_str_lit("spn_codegen_json_str"));
      sp_template_set(scope, sp_str_lit("warg"), write_value(g, f));
      break;
    }
    case FIELD_PRIM: {
      const gen_prim_row_t* prim = &GEN_PRIMS[f->as.prim];
      sp_template_set(scope, sp_str_lit("k_prim"), sp_str_lit("1"));
      sp_template_set(scope, sp_str_lit("getter"), sp_cstr_as_str(prim->toml_read));
      sp_template_set(scope, sp_str_lit("jget"), sp_cstr_as_str(prim->json_get));
      sp_template_set(scope, sp_str_lit("jwrite"), sp_cstr_as_str(prim->json_write));
      sp_template_set(scope, sp_str_lit("warg"), write_value(g, f));
      break;
    }
    case FIELD_ENUM: {
      sp_template_set(scope, sp_str_lit("k_enum"), sp_str_lit("1"));
      sp_template_set(scope, sp_str_lit("from"), sp_fmt(g->mem, "spn_{}_from_str", sp_fmt_str(f->as.enumeration)).value);
      sp_template_set(scope, sp_str_lit("jwrite"), sp_str_lit("spn_codegen_json_str"));
      sp_template_set(scope, sp_str_lit("warg"), write_value(g, f));
      break;
    }
    case FIELD_OBJECT: {
      sp_template_set(scope, sp_str_lit("k_object"), sp_str_lit("1"));
      sp_template_set(scope, sp_str_lit("object"), f->as.object->name);
      sp_template_set(scope, sp_str_lit("required"), f->required ? sp_str_lit("true") : sp_str_lit("false"));
      break;
    }
    case FIELD_EXTERN: {
      sp_template_set(scope, sp_str_lit("k_extern"), sp_str_lit("1"));
      sp_template_set(scope, sp_str_lit("extern"), f->as.ext);
      break;
    }
    case FIELD_STR_ARRAY: {
      sp_template_set(scope, sp_str_lit("k_str_array"), sp_str_lit("1"));
      break;
    }
    case FIELD_ENUM_ARRAY: {
      sp_template_set(scope, sp_str_lit("k_enum_array"), sp_str_lit("1"));
      sp_template_set(scope, sp_str_lit("type"), enum_type(g, f->as.enumeration));
      sp_template_set(scope, sp_str_lit("from"), sp_fmt(g->mem, "spn_{}_from_str", sp_fmt_str(f->as.enumeration)).value);
      sp_template_set(scope, sp_str_lit("to"), sp_fmt(g->mem, "spn_{}_to_str", sp_fmt_str(f->as.enumeration)).value);
      break;
    }
    case FIELD_OBJECT_ARRAY: {
      sp_template_set(scope, sp_str_lit("k_object_array"), sp_str_lit("1"));
      sp_template_set(scope, sp_str_lit("object"), f->as.array.object->name);
      if (!sp_str_empty(f->as.array.shorthand)) {
        sp_template_set(scope, sp_str_lit("shorthand"), f->as.array.shorthand);
        sp_template_set(scope, sp_str_lit("table_present"), table_present(g, f));
      }
      break;
    }
    case FIELD_KEYED: {
      sp_template_set(scope, sp_str_lit("k_keyed"), sp_str_lit("1"));
      sp_template_set(scope, sp_str_lit("object"), f->as.keyed.object->name);
      sp_template_set(scope, sp_str_lit("key_field"), f->as.keyed.field);
      sp_template_set(scope, sp_str_lit("type"), struct_type(g, f->as.keyed.object->name));
      break;
    }
    case FIELD_MAP: {
      sp_template_set(scope, sp_str_lit("k_map"), sp_str_lit("1"));
      sp_template_set(scope, sp_str_lit("entry"), f->as.map->name);
      sp_template_set(scope, sp_str_lit("entry_type"), struct_type(g, f->as.map->name));
      if (f->as.map->object) {
        sp_template_set(scope, sp_str_lit("map_object"), sp_str_lit("1"));
        sp_template_set(scope, sp_str_lit("object"), f->as.map->object->name);
      }
      break;
    }
  }
}

static void bind_struct_field(sp_template_scope_t* scope, sp_str_t type, sp_str_t name) {
  sp_template_scope_t* child = sp_template_push(scope, sp_str_lit("fields"));
  sp_template_set(child, sp_str_lit("type"), type);
  sp_template_set(child, sp_str_lit("name"), name);
}

static void bind_types_block(gen_t* g, sp_template_scope_t* scope) {
  sp_template_set(scope, sp_str_lit("types"), sp_str_lit("types"));

  sp_template_list(scope, sp_str_lit("tags"));
  sp_da_for(g->types, it) {
    gen_type_t* type = g->types[it];
    if (type->shared) {
      continue;
    }
    sp_template_scope_t* tag = sp_template_push(scope, sp_str_lit("tags"));
    sp_template_set(tag, sp_str_lit("tag"), sp_fmt(g->mem, "spn_cg_{}", sp_fmt_str(type->name)).value);
  }
  sp_da_for(g->entries, it) {
    sp_template_scope_t* tag = sp_template_push(scope, sp_str_lit("tags"));
    sp_template_set(tag, sp_str_lit("tag"), sp_fmt(g->mem, "spn_cg_{}", sp_fmt_str(g->entries[it]->name)).value);
  }

  sp_template_list(scope, sp_str_lit("oms"));
  sp_da_for(g->containers.keyed, it) {
    sp_template_scope_t* om = sp_template_push(scope, sp_str_lit("oms"));
    sp_template_set(om, sp_str_lit("object"), g->containers.keyed[it].object->name);
    sp_template_set(om, sp_str_lit("type"), struct_type(g, g->containers.keyed[it].object->name));
  }

  sp_template_list(scope, sp_str_lit("structs"));
  sp_da_for(g->types, it) {
    gen_type_t* type = g->types[it];
    if (type->shared) {
      continue;
    }
    sp_template_scope_t* child = sp_template_push(scope, sp_str_lit("structs"));
    sp_template_set(child, sp_str_lit("name"), type->name);
    sp_template_list(child, sp_str_lit("fields"));
    sp_da_for(type->fields, field) {
      bind_struct_field(child, decl_type(g, &type->fields[field]), type->fields[field].name);
    }
  }
  sp_da_for(g->entries, it) {
    gen_entry_t* entry = g->entries[it];
    sp_template_scope_t* child = sp_template_push(scope, sp_str_lit("structs"));
    sp_template_set(child, sp_str_lit("name"), entry->name);
    sp_template_list(child, sp_str_lit("fields"));
    bind_struct_field(child, sp_str_lit("sp_str_t"), sp_str_lit("key"));
    bind_struct_field(child, entry->object ? struct_type(g, entry->object->name) : sp_str_lit("sp_str_t"), sp_str_lit("value"));
  }
}

static void bind_includes(gen_t* g, sp_template_scope_t* scope) {
  sp_template_list(scope, sp_str_lit("includes"));
  sp_da_for(g->includes, it) {
    sp_template_set(sp_template_push(scope, sp_str_lit("includes")), sp_str_lit("name"), g->includes[it]);
  }
}

gen_render_t gen_render_common(gen_t* g) {
  sp_template_scope_t* scope = sp_template_scope_create(g->mem);
  bind_includes(g, scope);
  bind_types_block(g, scope);
  return (gen_render_t) { .template = sp_str_lit("common.h"), .scope = scope };
}

static sp_template_scope_t* root_scope(gen_t* g) {
  sp_template_scope_t* scope = sp_template_scope_create(g->mem);
  sp_template_set(scope, sp_str_lit("root_name"), g->root->name);
  sp_template_set(scope, sp_str_lit("guard"), sp_str_to_upper(g->mem, g->root->name));
  return scope;
}

gen_render_t gen_render_decls(gen_t* g) {
  sp_template_scope_t* scope = root_scope(g);
  bind_includes(g, scope);
  bind_types_block(g, scope);
  return (gen_render_t) {
    .template = sp_fmt(g->mem, "{}/decls.h", sp_fmt_cstr(GEN_FORMATS[g->format])).value,
    .scope = scope,
  };
}

gen_render_t gen_render_impl(gen_t* g) {
  sp_template_scope_t* scope = root_scope(g);
  sp_template_set(scope, sp_str_lit("write"), sp_str_lit("write"));
  sp_template_set(scope, sp_str_lit("root_write"), sp_str_lit("root_write"));

  sp_template_list(scope, sp_str_lit("fwd"));
  sp_da_for(g->types, it) {
    sp_template_scope_t* child = sp_template_push(scope, sp_str_lit("fwd"));
    sp_template_set(child, sp_str_lit("name"), g->types[it]->name);
    sp_template_set(child, sp_str_lit("type"), struct_type(g, g->types[it]->name));
  }

  sp_template_list(scope, sp_str_lit("arrays"));
  sp_da_for(g->containers.array, it) {
    sp_template_scope_t* child = sp_template_push(scope, sp_str_lit("arrays"));
    sp_template_set(child, sp_str_lit("object"), g->containers.array[it]->name);
    sp_template_set(child, sp_str_lit("type"), struct_type(g, g->containers.array[it]->name));
  }

  sp_template_list(scope, sp_str_lit("shorthands"));
  sp_da_for(g->containers.shorthand, it) {
    gen_shorthand_t* shorthand = &g->containers.shorthand[it];
    sp_template_scope_t* child = sp_template_push(scope, sp_str_lit("shorthands"));
    sp_template_set(child, sp_str_lit("object"), shorthand->object->name);
    sp_template_set(child, sp_str_lit("type"), struct_type(g, shorthand->object->name));
    sp_template_set(child, sp_str_lit("shorthand"), shorthand->field);
  }

  sp_template_list(scope, sp_str_lit("keyeds"));
  sp_da_for(g->containers.keyed, it) {
    gen_keyed_t* keyed = &g->containers.keyed[it];
    sp_template_scope_t* child = sp_template_push(scope, sp_str_lit("keyeds"));
    sp_template_set(child, sp_str_lit("object"), keyed->object->name);
    sp_template_set(child, sp_str_lit("type"), struct_type(g, keyed->object->name));
    sp_template_set(child, sp_str_lit("key_field"), keyed->key_field);
  }

  sp_template_list(scope, sp_str_lit("objects"));
  sp_da_for(g->containers.object, it) {
    sp_template_scope_t* child = sp_template_push(scope, sp_str_lit("objects"));
    sp_template_set(child, sp_str_lit("object"), g->containers.object[it]->name);
    sp_template_set(child, sp_str_lit("type"), struct_type(g, g->containers.object[it]->name));
  }

  sp_template_list(scope, sp_str_lit("entries"));
  sp_da_for(g->entries, it) {
    gen_entry_t* entry = g->entries[it];
    sp_template_scope_t* child = sp_template_push(scope, sp_str_lit("entries"));
    sp_template_set(child, sp_str_lit("entry"), entry->name);
    sp_template_set(child, sp_str_lit("entry_type"), struct_type(g, entry->name));
    if (entry->object) {
      sp_template_set(child, sp_str_lit("object"), entry->object->name);
      if (!sp_str_empty(entry->shorthand)) {
        sp_template_set(child, sp_str_lit("shorthand"), entry->shorthand);
      }
    }
  }

  sp_template_list(scope, sp_str_lit("types"));
  sp_da_for(g->types, it) {
    gen_type_t* type = g->types[it];
    sp_template_scope_t* child = sp_template_push(scope, sp_str_lit("types"));
    sp_template_set(child, sp_str_lit("name"), type->name);
    sp_template_set(child, sp_str_lit("type"), struct_type(g, type->name));
    sp_template_list(child, sp_str_lit("fields"));
    sp_da_for(type->fields, field) {
      bind_field(g, sp_template_push(child, sp_str_lit("fields")), &type->fields[field]);
    }
  }

  return (gen_render_t) {
    .template = sp_fmt(g->mem, "{}/impl.c", sp_fmt_cstr(GEN_FORMATS[g->format])).value,
    .scope = scope,
  };
}
