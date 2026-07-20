#include "codegen.h"
#include "sp/atomic_file.h"

#define try(expr) do { if (!(expr)) return false; } while (0)

typedef struct {
  sp_mem_t mem;
  yyjson_alc alc;
  codegen_paths_t paths;
  codegen_log_fn_t log;
  void* user;
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

static bool load_registry(codegen_t* c, const c8** dirs, u32 num_dirs, sp_template_registry_t** registry) {
  sp_template_registry_t* reg = sp_template_registry_create(c->mem);
  sp_for(it, num_dirs) {
    sp_str_t dir = sp_fs_join_path(c->mem, c->paths.templates, sp_cstr_as_str(dirs[it]));
    if (!sp_fs_is_dir(dir) || sp_template_load_dir(reg, dir)) {
      return fail(c, sp_fmt(c->mem, "failed to load templates from {}", sp_fmt_str(dir)).value);
    }
  }
  *registry = reg;
  return true;
}

typedef bool (*render_fn_t)(void* model, sp_io_writer_t* io, sp_template_registry_t* reg);

static bool common_fn(void* model, sp_io_writer_t* io, sp_template_registry_t* reg) { return render_common(model, io, reg); }
static bool decls_fn(void* model, sp_io_writer_t* io, sp_template_registry_t* reg) { return render_decls(model, io, reg); }
static bool impl_fn(void* model, sp_io_writer_t* io, sp_template_registry_t* reg) { return render_impl(model, io, reg); }
static bool abi_decls_fn(void* model, sp_io_writer_t* io, sp_template_registry_t* reg) { return abi_render_decls(model, io, reg); }
static bool abi_impl_fn(void* model, sp_io_writer_t* io, sp_template_registry_t* reg) { return abi_render_impl(model, io, reg); }

static bool render_one(codegen_t* c, sp_str_t path, void* model, sp_str_t* model_err, render_fn_t fn, sp_template_registry_t* reg) {
  sp_fs_atomic_t file = sp_zero;
  if (sp_fs_atomic_open(&file, path)) {
    return fail(c, sp_fmt(c->mem, "failed to open {}", sp_fmt_str(path)).value);
  }

  u8 buffer [64 * 1024];
  sp_io_writer_t* io = sp_fs_atomic_writer(&file);
  sp_io_writer_set_buffer(io, buffer, sizeof(buffer));

  if (!fn(model, io, reg)) {
    sp_fs_atomic_abort(&file);
    return fail(c, *model_err);
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

static sp_str_t out_path(codegen_t* c, sp_str_t name, const c8* suffix) {
  sp_str_t file = sp_fmt(c->mem, "{}{}", sp_fmt_str(name), sp_fmt_cstr(suffix)).value;
  return sp_fs_join_path(c->mem, c->paths.out, file);
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
    if (!gen_extract(gen, def->name, def->schema)) {
      return fail(c, gen->err);
    }
  }

  const c8* dirs [] = { "types" };
  sp_template_registry_t* reg = SP_NULLPTR;
  try(load_registry(c, dirs, sp_carr_len(dirs), &reg));

  sp_str_t out = out_path(c, sp_str_lit("common"), ".gen.h");
  try(render_one(c, out, gen, &gen->err, common_fn, reg));
  emit(c, sp_fmt(c->mem, "wrote {} common types to {}", sp_fmt_uint(sp_str_om_size(gen->types)), sp_fmt_str(out)).value);
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
    type_t* type = gen_type(gen, name);
    if (!type) {
      continue;
    }
    if (type->schema != jtd_definition(jtd, name)) {
      return fail(c, sp_fmt(c->mem, "{}: type {} shadows a common definition", sp_fmt_str(path), sp_fmt_str(name)).value);
    }
    type->shared = true;
  }

  sp_da_for(gen->containers.map, it) {
    if (gen_type(gen, gen->containers.map[it].object)->shared) {
      return fail(c, sp_fmt(c->mem, "{}: keyed arrays of common type {} are not supported", sp_fmt_str(path), sp_fmt_str(gen->containers.map[it].object)).value);
    }
  }

  sp_om_for(gen->types, it) {
    type_t* type = sp_str_om_at(gen->types, it);
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

static bool render_kind(codegen_t* c, sp_fs_entry_t* entry) {
  sp_str_t json = sp_zero;
  yyjson_doc* doc = SP_NULLPTR;
  try(read_json(c, entry->path, &json, &doc));

  yyjson_val* metadata = yyjson_obj_get(yyjson_doc_get_root(doc), "metadata");
  const c8* format = yyjson_get_str(yyjson_obj_get(metadata, "format"));
  if (!format) {
    return fail(c, sp_fmt(c->mem, "{}: schema has no root metadata.format", sp_fmt_str(entry->path)).value);
  }
  if (!sp_cstr_equal(format, "toml") && !sp_cstr_equal(format, "json")) {
    return fail(c, sp_fmt(c->mem, "{}: unknown format {}", sp_fmt_str(entry->path), sp_fmt_cstr(format)).value);
  }

  sp_str_t merged = sp_zero;
  try(merge_schema(c, entry->path, doc, &merged));

  jtd_result_t jtd = sp_zero;
  try(parse_jtd(c, entry->path, merged, &jtd));

  sp_str_t name = sp_str_strip_right(entry->name, sp_str_lit(".jtd.json"));
  gen_t* gen = gen_new(c->mem);
  if (!gen_extract(gen, name, jtd.root)) {
    return fail(c, gen->err);
  }
  gen->root = gen_type(gen, name);
  try(mark_shared(c, entry->path, gen, &jtd));

  const c8* dirs [] = { format, "shared", "types" };
  sp_template_registry_t* reg = SP_NULLPTR;
  try(load_registry(c, dirs, sp_carr_len(dirs), &reg));

  try(render_one(c, out_path(c, name, ".gen.h"), gen, &gen->err, decls_fn, reg));
  try(render_one(c, out_path(c, name, ".gen.c"), gen, &gen->err, impl_fn, reg));
  try(write_file(c, out_path(c, name, ".jtd.json"), merged));
  emit(c, sp_fmt(c->mem, "wrote {} ({})", sp_fmt_str(name), sp_fmt_cstr(format)).value);
  return true;
}

static bool render_abi(codegen_t* c) {
  sp_str_t path = sp_fs_join_path(c->mem, c->paths.schema, sp_str_lit("abi.json"));
  abi_t* abi = abi_parse(c->mem, path);
  if (!sp_str_empty(abi->err)) {
    return fail(c, abi->err);
  }

  const c8* dirs [] = { "abi" };
  sp_template_registry_t* reg = SP_NULLPTR;
  try(load_registry(c, dirs, sp_carr_len(dirs), &reg));

  try(render_one(c, out_path(c, sp_str_lit("abi"), ".gen.h"), abi, &abi->err, abi_decls_fn, reg));
  try(render_one(c, out_path(c, sp_str_lit("abi"), ".gen.c"), abi, &abi->err, abi_impl_fn, reg));
  emit(c, sp_fmt(c->mem, "wrote abi ({} exports)", sp_fmt_uint(sp_da_size(abi->exports))).value);
  return true;
}

static s32 sort_entries(const void* a, const void* b) {
  const sp_fs_entry_t* ea = (const sp_fs_entry_t*)a;
  const sp_fs_entry_t* eb = (const sp_fs_entry_t*)b;
  return sp_str_compare_alphabetical(ea->name, eb->name);
}

static bool run(codegen_t* c) {
  try(emit_common(c));

  sp_da(sp_fs_entry_t) entries = sp_fs_collect(c->mem, c->paths.schema);
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
  ctx.alc = cg_yyjson_alc(&ctx.mem);
  sp_str_ht_init(ctx.mem, ctx.names);

  if (!run(&ctx)) {
    return ctx.err;
  }
  return sp_str_lit("");
}
