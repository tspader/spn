#include "gen.h"

static const c8* kind_name(gen_field_kind_t kind) {
  switch (kind) {
    case FIELD_STR: {
      return "SPN_REFLECT_STR";
    }
    case FIELD_OBJECT: {
      return "SPN_REFLECT_OBJECT";
    }
    case FIELD_STR_ARRAY: {
      return "SPN_REFLECT_STR_ARRAY";
    }
    case FIELD_OBJECT_ARRAY: {
      return "SPN_REFLECT_OBJECT_ARRAY";
    }
    case FIELD_PRIM:
    case FIELD_ENUM:
    case FIELD_EXTERN:
    case FIELD_ENUM_ARRAY:
    case FIELD_KEYED:
    case FIELD_MAP: {
      return SP_NULLPTR;
    }
  }
  SP_UNREACHABLE_RETURN(SP_NULLPTR);
}

bool gen_data_check(gen_t* g) {
  if (sp_da_size(g->containers.shorthand)) {
    g->err = sp_fmt(g->mem, "{.cyan}: shorthand arrays are not supported for data schemas", sp_fmt_str(g->name)).value;
    return false;
  }
  sp_da_for(g->types, it) {
    gen_type_t* type = g->types[it];
    if (type->shared) {
      g->err = sp_fmt(g->mem, "{}: data schemas cannot use common type {.cyan}", sp_fmt_str(g->name), sp_fmt_str(type->name)).value;
      return false;
    }
    sp_da_for(type->fields, field) {
      if (!kind_name(type->fields[field].kind)) {
        g->err = sp_fmt(g->mem, "{}.{}: field kind is not supported by data schemas", sp_fmt_str(type->name), sp_fmt_str(type->fields[field].key)).value;
        return false;
      }
    }
  }
  return true;
}

gen_render_t gen_render_data_decls(gen_t* g) {
  sp_template_scope_t* scope = gen_root_scope(g);
  gen_bind_includes(g, scope);
  gen_bind_types(g, scope);

  sp_template_list(scope, sp_str_lit("reflects"));
  sp_da_for(g->types, it) {
    sp_template_scope_t* child = sp_template_push(scope, sp_str_lit("reflects"));
    sp_template_set(child, sp_str_lit("name"), g->types[it]->name);
  }

  return (gen_render_t) { .template = sp_str_lit("data/decls.h"), .scope = scope };
}

gen_render_t gen_render_data_impl(gen_t* g) {
  sp_template_scope_t* scope = gen_root_scope(g);

  sp_template_list(scope, sp_str_lit("types"));
  sp_da_for(g->types, it) {
    gen_type_t* type = g->types[it];
    sp_template_scope_t* child = sp_template_push(scope, sp_str_lit("types"));
    sp_template_set(child, sp_str_lit("name"), type->name);
    sp_template_set(child, sp_str_lit("type"), gen_struct_type(g, type->name));
    sp_template_list(child, sp_str_lit("fields"));
    sp_da_for(type->fields, field) {
      gen_field_t* f = &type->fields[field];
      sp_template_scope_t* entry = sp_template_push(child, sp_str_lit("fields"));
      sp_template_set(entry, sp_str_lit("key"), f->key);
      sp_template_set(entry, sp_str_lit("name"), f->name);
      sp_template_set(entry, sp_str_lit("kind"), sp_cstr_as_str(kind_name(f->kind)));
      sp_template_set(entry, sp_str_lit("owner"), gen_struct_type(g, type->name));
      if (f->kind == FIELD_OBJECT) {
        sp_template_set(entry, sp_str_lit("object"), f->as.object->name);
      }
      if (f->kind == FIELD_OBJECT_ARRAY) {
        sp_template_set(entry, sp_str_lit("object"), f->as.array.object->name);
      }
    }
  }

  return (gen_render_t) { .template = sp_str_lit("data/impl.c"), .scope = scope };
}
