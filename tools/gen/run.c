#include "gen.h"
#include "sp/atomic_file.h"

#define try(expr) do { if (!(expr)) return false; } while (0)

typedef struct {
  sp_mem_t mem;
  yyjson_alc alc;
  codegen_paths_t paths;
  codegen_log_fn_t log;
  void* user;
  sp_template_registry_t* reg;
  struct {
    jtd_result_t jtd;
    yyjson_doc* doc;
  } common;
  sp_ht(sp_str_t, u8) names;
  sp_str_t err;
} codegen_t;

static bool fail(codegen_t* c, sp_str_t message) {
  c->err = message;
  return false;
}

static void emit(codegen_t* c, sp_str_t message) {
  if (c->log) {
    c->log(c->user, message);
  }
}

static bool render_one(codegen_t* c, sp_str_t path, gen_render_t render) {
  sp_str_t source = sp_zero;
  if (!sp_template_get(c->reg, render.template, &source)) {
    return fail(c, sp_fmt(c->mem, "failed to find template {.cyan}", sp_fmt_str(render.template)).value);
  }

  sp_fs_atomic_t file = sp_zero;
  if (sp_fs_atomic_open(&file, path)) {
    return fail(c, sp_fmt(c->mem, "failed to open {}", sp_fmt_str(path)).value);
  }

  u8 buffer [64 * 1024];
  sp_io_writer_t* io = sp_fs_atomic_writer(&file);
  sp_io_writer_set_buffer(io, buffer, sizeof(buffer));

  sp_template_err_t err = sp_template_render(io, source, render.scope, c->reg);
  if (err) {
    sp_fs_atomic_abort(&file);
    return fail(c, sp_fmt(c->mem, "failed to render template {.cyan} with code {.red}", sp_fmt_str(render.template), sp_fmt_int(err)).value);
  }

  if (sp_fs_atomic_commit(&file, SP_FS_ATOMIC_REPLACE)) {
    return fail(c, sp_fmt(c->mem, "failed to write {}", sp_fmt_str(path)).value);
  }

  return true;
}

static bool write_file(codegen_t* c, sp_str_t path, sp_str_t contents) {
  if (sp_fs_write_atomic(path, contents)) {
    return fail(c, sp_fmt(c->mem, "failed to write {}", sp_fmt_str(path)).value);
  }
  return true;
}

static sp_str_t out_path(codegen_t* c, sp_str_t dir, sp_str_t name, const c8* suffix) {
  sp_str_t file = sp_fmt(c->mem, "{}{}", sp_fmt_str(name), sp_fmt_cstr(suffix)).value;
  return sp_fs_join_path(c->mem, dir, file);
}

static bool read_json(codegen_t* c, sp_str_t path, sp_str_t* json, yyjson_doc** doc) {
  if (sp_io_read_file(c->mem, path, json)) {
    return fail(c, sp_fmt(c->mem, "failed to read {}", sp_fmt_str(path)).value);
  }
  *doc = yyjson_read_opts((c8*)json->data, json->len, 0, &c->alc, SP_NULLPTR);
  if (!*doc) {
    return fail(c, sp_fmt(c->mem, "{} is not valid json", sp_fmt_str(path)).value);
  }
  return true;
}

static bool parse_jtd(codegen_t* c, sp_str_t what, sp_str_t json, jtd_result_t* jtd) {
  *jtd = jtd_parse(c->mem, json);
  if (!jtd->ok) {
    return fail(c, sp_fmt(c->mem, "{}: {}", sp_fmt_str(what), sp_fmt_str(jtd_diagnostic_message(c->mem, &jtd->diag))).value);
  }
  return true;
}

static bool emit_common(codegen_t* c) {
  sp_str_t path = sp_fs_join_path(c->mem, c->paths.schema, sp_str_lit("common.jtd.json"));
  sp_str_t json = sp_zero;
  try(read_json(c, path, &json, &c->common.doc));
  try(parse_jtd(c, path, json, &c->common.jtd));

  gen_t* gen = gen_new(c->mem);
  sp_da_for(c->common.jtd.definitions, it) {
    jtd_definition_t* def = &c->common.jtd.definitions[it];
    if (def->schema->form != JTD_FORM_PROPERTIES) {
      continue;
    }
    if (!gen_lower(gen, def->name, def->schema)) {
      return fail(c, gen->err);
    }
  }

  sp_str_t path_out = out_path(c, c->paths.out, sp_str_lit("common"), ".gen.h");
  try(render_one(c, path_out, gen_render_common(gen)));
  emit(c, sp_fmt(c->mem, "wrote {} common types to {}", sp_fmt_uint(sp_da_size(gen->types)), sp_fmt_str(path_out)).value);
  return true;
}

static bool merge_schema(codegen_t* c, sp_str_t path, yyjson_doc* doc, sp_str_t* merged) {
  yyjson_mut_doc* mut = yyjson_doc_mut_copy(doc, &c->alc);
  yyjson_mut_val* root = yyjson_mut_doc_get_root(mut);
  yyjson_mut_val* defs = yyjson_mut_obj_get(root, "definitions");
  if (!defs) {
    defs = yyjson_mut_obj(mut);
    yyjson_mut_obj_add_val(mut, root, "definitions", defs);
  }

  yyjson_val* common = yyjson_obj_get(yyjson_doc_get_root(c->common.doc), "definitions");
  size_t idx, max;
  yyjson_val *key, *value;
  yyjson_obj_foreach(common, idx, max, key, value) {
    if (yyjson_mut_obj_get(defs, yyjson_get_str(key))) {
      return fail(c, sp_fmt(c->mem, "{}: definition {} shadows common", sp_fmt_str(path), sp_fmt_cstr(yyjson_get_str(key))).value);
    }
    yyjson_mut_obj_add(defs, yyjson_val_mut_copy(mut, key), yyjson_val_mut_copy(mut, value));
  }

  size_t len = 0;
  c8* json = yyjson_mut_write_opts(mut, YYJSON_WRITE_PRETTY_TWO_SPACES | YYJSON_WRITE_NEWLINE_AT_END, &c->alc, &len, SP_NULLPTR);
  if (!json) {
    return fail(c, sp_fmt(c->mem, "{}: failed to serialize merged schema", sp_fmt_str(path)).value);
  }
  *merged = sp_str(json, (u32)len);
  return true;
}

static bool mark_shared(codegen_t* c, sp_str_t path, gen_t* gen, jtd_result_t* jtd) {
  sp_da_for(c->common.jtd.definitions, it) {
    sp_str_t name = c->common.jtd.definitions[it].name;
    gen_type_t* type = gen_find(gen, name);
    if (!type) {
      continue;
    }
    if (type->schema != jtd_definition(jtd, name)) {
      return fail(c, sp_fmt(c->mem, "{}: type {} shadows a common definition", sp_fmt_str(path), sp_fmt_str(name)).value);
    }
    type->shared = true;
  }

  sp_da_for(gen->containers.keyed, it) {
    if (gen->containers.keyed[it].object->shared) {
      return fail(c, sp_fmt(c->mem, "{}: keyed arrays of common type {} are not supported", sp_fmt_str(path), sp_fmt_str(gen->containers.keyed[it].object->name)).value);
    }
  }

  sp_da_for(gen->types, it) {
    gen_type_t* type = gen->types[it];
    if (type->shared) {
      continue;
    }
    if (sp_ht_getp(c->names, type->name)) {
      return fail(c, sp_fmt(c->mem, "{}: type {} is already defined by another schema", sp_fmt_str(path), sp_fmt_str(type->name)).value);
    }
    sp_ht_insert(c->names, type->name, true);
  }

  return true;
}

static bool render_union(codegen_t* c, sp_fs_entry_t* entry, sp_str_t json, gen_format_t format, sp_str_t prefix) {
  jtd_result_t jtd = sp_zero;
  try(parse_jtd(c, entry->path, json, &jtd));

  sp_str_t name = sp_str_strip_right(entry->name, sp_str_lit(".jtd.json"));
  gen_t* gen = gen_new(c->mem);
  gen->format = format;
  gen->name = name;
  gen->prefix = prefix;
  if (!gen_lower_union(gen, jtd.root)) {
    return fail(c, gen->err);
  }
  if (sp_da_size(gen->includes)) {
    return fail(c, sp_fmt(c->mem, "{}: public schemas cannot include {.cyan}", sp_fmt_str(name), sp_fmt_str(gen->includes[0])).value);
  }

  if (format == GEN_FORMAT_ERRORS) {
    try(render_one(c, out_path(c, c->paths.include, sp_str_lit("err"), ".h"), gen_render_union_codes(gen)));
  }
  try(render_one(c, out_path(c, c->paths.include, name, ".h"), gen_render_union_decls(gen)));
  try(render_one(c, out_path(c, c->paths.out, name, ".gen.c"), gen_render_union_impl(gen)));
  emit(c, sp_fmt(c->mem, "wrote {} ({} kinds)", sp_fmt_str(name), sp_fmt_uint(sp_da_size(gen->kinds))).value);
  return true;
}

static bool lower_record(codegen_t* c, sp_fs_entry_t* entry, yyjson_doc* doc, gen_format_t format, gen_t** lowered, sp_str_t* merged) {
  try(merge_schema(c, entry->path, doc, merged));

  jtd_result_t jtd = sp_zero;
  try(parse_jtd(c, entry->path, *merged, &jtd));

  sp_str_t name = sp_str_strip_right(entry->name, sp_str_lit(".jtd.json"));
  gen_t* gen = gen_new(c->mem);
  gen->format = format;
  gen->name = name;
  if (!gen_lower(gen, name, jtd.root)) {
    return fail(c, gen->err);
  }
  gen->root = gen_find(gen, name);
  try(mark_shared(c, entry->path, gen, &jtd));
  *lowered = gen;
  return true;
}

static bool commit_record(codegen_t* c, gen_t* gen, sp_str_t merged, gen_render_t decls, gen_render_t impl) {
  try(render_one(c, out_path(c, c->paths.out, gen->name, ".gen.h"), decls));
  try(render_one(c, out_path(c, c->paths.out, gen->name, ".gen.c"), impl));
  try(write_file(c, out_path(c, c->paths.out, gen->name, ".jtd.json"), merged));
  emit(c, sp_fmt(c->mem, "wrote {} ({})", sp_fmt_str(gen->name), sp_fmt_cstr(gen_format_name(gen->format))).value);
  return true;
}

static bool render_toml(codegen_t* c, sp_fs_entry_t* entry, yyjson_doc* doc) {
  sp_str_t merged = sp_zero;
  gen_t* gen = SP_NULLPTR;
  try(lower_record(c, entry, doc, GEN_FORMAT_TOML, &gen, &merged));
  return commit_record(c, gen, merged, gen_render_decls(gen), gen_render_impl(gen));
}

static bool render_json(codegen_t* c, sp_fs_entry_t* entry, yyjson_doc* doc) {
  sp_str_t merged = sp_zero;
  gen_t* gen = SP_NULLPTR;
  try(lower_record(c, entry, doc, GEN_FORMAT_JSON, &gen, &merged));
  if (sp_da_size(gen->containers.shorthand)) {
    return fail(c, sp_fmt(c->mem, "{.cyan}: shorthand arrays are not supported for json schemas", sp_fmt_str(gen->name)).value);
  }
  return commit_record(c, gen, merged, gen_render_decls(gen), gen_render_impl(gen));
}

static bool render_data(codegen_t* c, sp_fs_entry_t* entry, yyjson_doc* doc) {
  sp_str_t merged = sp_zero;
  gen_t* gen = SP_NULLPTR;
  try(lower_record(c, entry, doc, GEN_FORMAT_DATA, &gen, &merged));
  if (!gen_data_check(gen)) {
    return fail(c, gen->err);
  }
  return commit_record(c, gen, merged, gen_render_data_decls(gen), gen_render_data_impl(gen));
}

static bool render_kind(codegen_t* c, sp_fs_entry_t* entry) {
  sp_str_t json = sp_zero;
  yyjson_doc* doc = SP_NULLPTR;
  try(read_json(c, entry->path, &json, &doc));

  yyjson_val* metadata = yyjson_obj_get(yyjson_doc_get_root(doc), "metadata");
  const c8* format = yyjson_get_str(yyjson_obj_get(metadata, "format"));
  if (!format) {
    return fail(c, sp_fmt(c->mem, "{}: schema has no root metadata.format", sp_fmt_str(entry->path)).value);
  }

  if (sp_cstr_equal(format, "toml")) {
    return render_toml(c, entry, doc);
  }
  if (sp_cstr_equal(format, "json")) {
    return render_json(c, entry, doc);
  }
  if (sp_cstr_equal(format, "data")) {
    return render_data(c, entry, doc);
  }
  if (sp_cstr_equal(format, "errors")) {
    return render_union(c, entry, json, GEN_FORMAT_ERRORS, sp_str_lit("spn_err"));
  }
  if (sp_cstr_equal(format, "events")) {
    return render_union(c, entry, json, GEN_FORMAT_EVENTS, sp_str_lit("spn_event"));
  }
  return fail(c, sp_fmt(c->mem, "{}: unknown format {}", sp_fmt_str(entry->path), sp_fmt_cstr(format)).value);
}

static bool render_abi(codegen_t* c) {
  sp_str_t path = sp_fs_join_path(c->mem, c->paths.schema, sp_str_lit("abi.json"));
  abi_t* abi = abi_parse(c->mem, path);
  if (!sp_str_empty(abi->err)) {
    return fail(c, abi->err);
  }

  try(render_one(c, out_path(c, c->paths.out, sp_str_lit("abi"), ".gen.h"), abi_render_decls(abi)));
  try(render_one(c, out_path(c, c->paths.out, sp_str_lit("abi"), ".gen.c"), abi_render_impl(abi)));
  emit(c, sp_fmt(c->mem, "wrote abi ({} exports)", sp_fmt_uint(sp_da_size(abi->exports))).value);
  return true;
}

static s32 sort_entries(const void* a, const void* b) {
  const sp_fs_entry_t* ea = (const sp_fs_entry_t*)a;
  const sp_fs_entry_t* eb = (const sp_fs_entry_t*)b;
  return sp_str_compare_alphabetical(ea->name, eb->name);
}

static bool run(codegen_t* c) {
  c->reg = sp_template_registry_create(c->mem);
  if (!sp_fs_is_dir(c->paths.templates) || sp_template_load_dir(c->reg, c->paths.templates)) {
    return fail(c, sp_fmt(c->mem, "failed to load templates from {}", sp_fmt_str(c->paths.templates)).value);
  }

  try(emit_common(c));

  sp_da(sp_fs_entry_t) entries = sp_zero;
  sp_fs_collect(c->mem, c->paths.schema, &entries);
  sp_da_sort(entries, sort_entries);
  sp_da_for(entries, it) {
    sp_fs_entry_t* entry = &entries[it];
    if (!sp_str_ends_with(entry->name, sp_str_lit(".jtd.json"))) {
      continue;
    }
    if (sp_str_equal_cstr(entry->name, "common.jtd.json")) {
      continue;
    }
    try(render_kind(c, entry));
  }

  return render_abi(c);
}

sp_str_t codegen_run(sp_mem_t mem, codegen_paths_t paths, codegen_log_fn_t log, void* user) {
  codegen_t ctx = {
    .mem = mem,
    .paths = paths,
    .log = log,
    .user = user,
  };
  ctx.alc = gen_yyjson_alc(&ctx.mem);
  sp_str_ht_init(ctx.mem, ctx.names);

  if (!run(&ctx)) {
    return ctx.err;
  }
  return sp_str_lit("");
}
