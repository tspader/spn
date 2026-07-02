#include "codegen.h"

static sp_str_t abi_struct_type(gen_t* g, sp_str_t name) {
  return sp_fmt(g->mem, "spn_{}_t", sp_fmt_str(name)).value;
}

static sp_str_t abi_wire_type(gen_t* g, sp_str_t name) {
  return sp_fmt(g->mem, "spn_wire_{}_t", sp_fmt_str(name)).value;
}

static sp_str_t abi_handle_type(gen_t* g, sp_str_t name) {
  return sp_fmt(g->mem, "spn_{}_t*", sp_fmt_str(name)).value;
}

static bool abi_check(gen_t* g, type_t* type) {
  sp_da_for(type->fields, it) {
    field_t* field = &type->fields[it];
    bool ok = false;
    if (field->card == CARD_SCALAR) {
      ok = field->kind == FIELD_STR || field->kind == FIELD_HANDLE || field->kind == FIELD_STRUCT;
    }
    if (field->card == CARD_ARRAY) {
      ok = field->kind == FIELD_STR && field->cap > 0;
    }
    if (!ok) {
      g->err = sp_fmt(g->mem, "{.cyan}.{.cyan}: abi supports strings, handles, nested structs, and capped string arrays",
        sp_fmt_str(type->name), sp_fmt_str(field->key)).value;
      return false;
    }
  }
  return true;
}

static u32 abi_type_size(gen_t* g, type_t* type);
static u32 abi_type_align(gen_t* g, type_t* type);

static u32 abi_field_size(gen_t* g, field_t* field) {
  if (field->card == CARD_ARRAY) {
    return 4 * field->cap;
  }
  if (field->kind == FIELD_STRUCT) {
    return abi_type_size(g, gen_type(g, field->type_name));
  }
  return 4;
}

static u32 abi_field_align(gen_t* g, field_t* field) {
  if (field->kind == FIELD_STRUCT && field->card == CARD_SCALAR) {
    return abi_type_align(g, gen_type(g, field->type_name));
  }
  return 4;
}

static u32 abi_align_up(u32 value, u32 align) {
  return (value + align - 1) & ~(align - 1);
}

static u32 abi_type_align(gen_t* g, type_t* type) {
  u32 align = 1;
  sp_da_for(type->fields, it) {
    u32 field_align = abi_field_align(g, &type->fields[it]);
    if (field_align > align) align = field_align;
  }
  return align;
}

static u32 abi_type_size(gen_t* g, type_t* type) {
  u32 size = 0;
  sp_da_for(type->fields, it) {
    field_t* field = &type->fields[it];
    size = abi_align_up(size, abi_field_align(g, field));
    size += abi_field_size(g, field);
  }
  return abi_align_up(size, abi_type_align(g, type));
}

static sp_str_t abi_field_decl(gen_t* g, field_t* field, bool wire) {
  sp_str_t type = sp_zero;
  switch (field->kind) {
    case FIELD_STR:    type = wire ? sp_str_lit("u32") : sp_str_lit("const c8*"); break;
    case FIELD_HANDLE: type = wire ? sp_str_lit("u32") : abi_handle_type(g, field->type_name); break;
    case FIELD_STRUCT: type = wire ? abi_wire_type(g, field->type_name) : abi_struct_type(g, field->type_name); break;
    case FIELD_BOOL:
    case FIELD_ENUM: break;
  }

  if (field->card == CARD_ARRAY) {
    return sp_fmt(g->mem, "{} {} [{}]", sp_fmt_str(type), sp_fmt_str(field->key), sp_fmt_uint(field->cap)).value;
  }
  return sp_fmt(g->mem, "{} {}", sp_fmt_str(type), sp_fmt_str(field->key)).value;
}

static void abi_bind_field(gen_t* g, sp_template_scope_t* scope, field_t* field) {
  sp_template_set(scope, sp_str_lit("key"), field->key);
  sp_template_set(scope, sp_str_lit("decl"), abi_field_decl(g, field, false));
  sp_template_set(scope, sp_str_lit("wire_decl"), abi_field_decl(g, field, true));

  if (field->card == CARD_ARRAY) {
    sp_template_set(scope, sp_str_lit("cap"), sp_fmt(g->mem, "{}", sp_fmt_uint(field->cap)).value);
    sp_template_set(scope, sp_str_lit("read"), sp_str_lit("read/str_array"));
    return;
  }
  switch (field->kind) {
    case FIELD_STR: {
      sp_template_set(scope, sp_str_lit("read"), sp_str_lit("read/str"));
      break;
    }
    case FIELD_HANDLE: {
      sp_str_t kind = sp_fmt(g->mem, "SPN_ABI_KIND_{}", sp_fmt_str(sp_str_to_upper(g->mem, field->type_name))).value;
      sp_template_set(scope, sp_str_lit("kind"), kind);
      sp_template_set(scope, sp_str_lit("read"), sp_str_lit("read/handle"));
      break;
    }
    case FIELD_STRUCT: {
      sp_template_set(scope, sp_str_lit("object"), field->type_name);
      sp_template_set(scope, sp_str_lit("read"), sp_str_lit("read/object"));
      break;
    }
    case FIELD_BOOL:
    case FIELD_ENUM: break;
  }
}

static sp_template_scope_t* abi_scope(gen_t* g) {
  sp_template_scope_t* root = sp_template_scope_create(g->mem);

  sp_om_for(g->types, it) {
    type_t* type = sp_str_om_at(g->types, it);
    sp_template_scope_t* scope = sp_template_push(root, sp_str_lit("types"));
    sp_template_set(scope, sp_str_lit("name"), type->name);
    sp_template_set(scope, sp_str_lit("type"), abi_struct_type(g, type->name));
    sp_template_set(scope, sp_str_lit("wire_type"), abi_wire_type(g, type->name));
    sp_template_set(scope, sp_str_lit("size"), sp_fmt(g->mem, "{}", sp_fmt_uint(abi_type_size(g, type))).value);
    sp_da_for(type->fields, field) {
      abi_bind_field(g, sp_template_push(scope, sp_str_lit("fields")), &type->fields[field]);
    }
  }

  sp_da_for(g->roots, it) {
    sp_template_scope_t* scope = sp_template_push(root, sp_str_lit("functions"));
    sp_template_set(scope, sp_str_lit("name"), g->roots[it]);
    sp_template_set(scope, sp_str_lit("type"), abi_struct_type(g, g->roots[it]));
  }

  return root;
}

bool render_abi_decls(gen_t* g, sp_io_writer_t* out, sp_template_registry_t* reg) {
  sp_om_for(g->types, it) {
    if (!abi_check(g, sp_str_om_at(g->types, it))) {
      return false;
    }
  }
  return gen_render(g, out, reg, "decls", abi_scope(g));
}

bool render_abi_impl(gen_t* g, sp_io_writer_t* out, sp_template_registry_t* reg) {
  return gen_render(g, out, reg, "impl", abi_scope(g));
}
