#include "gen.h"

static void bind_kinds(gen_t* g, sp_template_scope_t* scope) {
  sp_template_list(scope, sp_str_lit("kinds"));
  sp_da_for(g->kinds, it) {
    gen_kind_t* kind = &g->kinds[it];
    sp_template_scope_t* child = sp_template_push(scope, sp_str_lit("kinds"));
    sp_template_set(child, sp_str_lit("tag"), kind->tag);
    sp_template_set(child, sp_str_lit("code"), sp_fmt(g->mem, "{}_{}",
      sp_fmt_cstr(g->format == GEN_FORMAT_ERRORS ? "SPN_ERR" : "SPN_EVENT"),
      sp_fmt_str(sp_str_to_upper(g->mem, kind->tag))).value);

    if (kind->payload) {
      sp_template_set(child, sp_str_lit("k_payload"), sp_str_lit("1"));
      sp_template_set(child, sp_str_lit("arm"), kind->payload->name);
      sp_template_set(child, sp_str_lit("awrite"), gen_undecorated(g, kind->payload->name));
    }

    if (g->format == GEN_FORMAT_EVENTS) {
      sp_template_set(child, sp_str_lit("verb"), kind->verb);
      sp_template_set(child, sp_str_lit("verbosity"), kind->verbosity);
      sp_template_set(child, sp_str_lit("severity"), kind->severity);
    }
  }

  sp_template_list(scope, sp_str_lit("arms"));
  sp_da_for(g->arms, it) {
    sp_template_scope_t* child = sp_template_push(scope, sp_str_lit("arms"));
    sp_template_set(child, sp_str_lit("name"), g->arms[it]->name);
    sp_template_set(child, sp_str_lit("type"), gen_struct_type(g, g->arms[it]->name));
  }
}

static sp_template_scope_t* union_scope(gen_t* g) {
  sp_template_scope_t* scope = sp_template_scope_create(g->mem);
  sp_template_set(scope, sp_str_lit("root_name"), g->name);
  sp_template_set(scope, sp_str_lit("guard"), sp_str_to_upper(g->mem, g->name));
  return scope;
}

gen_render_t gen_render_union_decls(gen_t* g) {
  sp_template_scope_t* scope = union_scope(g);
  gen_bind_includes(g, scope);
  gen_bind_types(g, scope);
  bind_kinds(g, scope);
  return (gen_render_t) {
    .template = sp_fmt(g->mem, "{}/decls.h", sp_fmt_cstr(gen_format_name(g->format))).value,
    .scope = scope,
  };
}

gen_render_t gen_render_union_impl(gen_t* g) {
  sp_template_scope_t* scope = union_scope(g);
  gen_bind_type_impls(g, scope);
  bind_kinds(g, scope);
  return (gen_render_t) {
    .template = sp_fmt(g->mem, "{}/impl.c", sp_fmt_cstr(gen_format_name(g->format))).value,
    .scope = scope,
  };
}
