#include "codegen/codegen.h"
#include "external/tom.h"
#include "manifest.gen.h"
#include "sp/compat.h"

void spn_codegen_ctx_init(spn_codegen_ctx_t* ctx, sp_mem_t mem, sp_intern_t* intern) {
  ctx->mem = mem;
  ctx->intern = intern;
  ctx->depth = 0;
  ctx->issues = sp_da_new(mem, spn_codegen_issue_t);
}

void spn_codegen_push_key(spn_codegen_ctx_t* ctx, const c8* key) {
  if (ctx->depth >= SPN_CODEGEN_PATH_MAX) {
    return;
  }
  ctx->path[ctx->depth] = (spn_codegen_path_seg_t) { .kind = SPN_CODEGEN_PATH_KEY, .key = key };
  ctx->depth += 1;
}

void spn_codegen_push_index(spn_codegen_ctx_t* ctx, u32 index) {
  if (ctx->depth >= SPN_CODEGEN_PATH_MAX) {
    return;
  }
  ctx->path[ctx->depth] = (spn_codegen_path_seg_t) { .kind = SPN_CODEGEN_PATH_INDEX, .index = index };
  ctx->depth += 1;
}

void spn_codegen_pop(spn_codegen_ctx_t* ctx) {
  if (ctx->depth) {
    ctx->depth -= 1;
  }
}

static sp_str_t spn_codegen_path(spn_codegen_ctx_t* ctx) {
  sp_io_dyn_mem_writer_t writer;
  sp_io_dyn_mem_writer_init(ctx->mem, &writer);
  sp_for(it, ctx->depth) {
    spn_codegen_path_seg_t* seg = &ctx->path[it];
    if (seg->kind == SPN_CODEGEN_PATH_KEY) {
      sp_fmt_io(&writer.base, it ? ".{}" : "{}", sp_fmt_cstr(seg->key));
    } else {
      sp_fmt_io(&writer.base, "[{}]", sp_fmt_uint(seg->index));
    }
  }
  return sp_io_dyn_mem_writer_as_str(&writer);
}

static void spn_codegen_record(spn_codegen_ctx_t* ctx, spn_codegen_err_t code, sp_str_t detail) {
  spn_codegen_issue_t issue = { .code = code, .path = spn_codegen_path(ctx), .detail = detail };
  sp_da_push(ctx->issues, issue);
}

bool spn_codegen_issue(spn_codegen_ctx_t* ctx, spn_codegen_err_t code, const c8* key) {
  spn_codegen_push_key(ctx, key);
  spn_codegen_record(ctx, code, sp_cstr_as_str(key));
  spn_codegen_pop(ctx);
  return true;
}

bool spn_codegen_issue_at(spn_codegen_ctx_t* ctx, spn_codegen_err_t code, sp_str_t detail) {
  spn_codegen_record(ctx, code, detail);
  return true;
}

sp_str_t spn_codegen_intern(spn_codegen_ctx_t* ctx, sp_str_t value) {
  return sp_intern_get_or_insert_str(ctx->intern, value);
}

sp_str_t spn_codegen_path_join(spn_codegen_ctx_t* ctx, sp_str_t raw) {
  return spn_codegen_intern(ctx, sp_fs_join_path(ctx->mem, ctx->dir, raw));
}

sp_da(sp_str_t) spn_codegen_read_path_array(spn_codegen_ctx_t* ctx, toml_table_t* table, const c8* key) {
  toml_array_t* array = toml_table_array(table, key);
  if (!array) {
    return SP_NULLPTR;
  }

  sp_da(sp_str_t) values = sp_da_new(ctx->mem, sp_str_t);
  spn_codegen_push_key(ctx, key);
  sp_for(it, (u32)toml_array_len(array)) {
    spn_codegen_push_index(ctx, it);
    toml_value_t element = toml_array_string(array, (s32)it);
    if (element.ok) {
      sp_da_push(values, spn_codegen_path_join(ctx, sp_str(element.u.s, (u32)element.u.sl)));
    } else {
      spn_codegen_record(ctx, SPN_CODEGEN_ERR_EXPECTED_STR, sp_str_lit(""));
    }
    spn_codegen_pop(ctx);
  }
  spn_codegen_pop(ctx);
  return values;
}

static bool spn_codegen_read_raw_value(toml_table_t* table, const c8* key, sp_str_t* value) {
  toml_value_t found = toml_table_string(table, key);
  if (!found.ok) {
    return false;
  }
  *value = sp_str(found.u.s, (u32)found.u.sl);
  return true;
}

sp_str_t spn_codegen_str_required(spn_codegen_ctx_t* ctx, toml_table_t* table, const c8* key) {
  sp_str_t value = sp_zero;
  if (!spn_codegen_read_raw_value(table, key, &value)) {
    spn_codegen_issue(ctx, SPN_CODEGEN_ERR_MISSING_KEY, key);
    return value;
  }
  return sp_intern_get_or_insert_str(ctx->intern, value);
}

bool spn_codegen_str_optional(spn_codegen_ctx_t* ctx, toml_table_t* table, const c8* key, sp_str_t* value) {
  if (!spn_codegen_read_raw_value(table, key, value)) {
    return false;
  }
  *value = sp_intern_get_or_insert_str(ctx->intern, *value);
  return true;
}

sp_str_t spn_codegen_raw_required(spn_codegen_ctx_t* ctx, toml_table_t* table, const c8* key) {
  sp_str_t value = sp_zero;
  if (!spn_codegen_read_raw_value(table, key, &value)) {
    spn_codegen_issue(ctx, SPN_CODEGEN_ERR_MISSING_KEY, key);
  }
  return value;
}

bool spn_codegen_raw_optional(spn_codegen_ctx_t* ctx, toml_table_t* table, const c8* key, sp_str_t* value) {
  (void)ctx;
  return spn_codegen_read_raw_value(table, key, value);
}

bool spn_codegen_read_bool(spn_codegen_ctx_t* ctx, toml_table_t* table, const c8* key, bool* value) {
  toml_value_t found = toml_table_bool(table, key);
  if (!found.ok) {
    return false;
  }
  *value = found.u.b;
  return true;
}

sp_da(sp_str_t) spn_codegen_read_str_array(spn_codegen_ctx_t* ctx, toml_table_t* table, const c8* key) {
  toml_array_t* array = toml_table_array(table, key);
  if (!array) {
    return SP_NULLPTR;
  }

  sp_da(sp_str_t) values = sp_da_new(ctx->mem, sp_str_t);
  spn_codegen_push_key(ctx, key);
  sp_for(it, (u32)toml_array_len(array)) {
    spn_codegen_push_index(ctx, it);
    toml_value_t element = toml_array_string(array, (s32)it);
    if (element.ok) {
      sp_da_push(values, sp_intern_get_or_insert_str(ctx->intern, sp_str(element.u.s, (u32)element.u.sl)));
    } else {
      spn_codegen_record(ctx, SPN_CODEGEN_ERR_EXPECTED_STR, sp_str_lit(""));
    }
    spn_codegen_pop(ctx);
  }
  spn_codegen_pop(ctx);
  return values;
}

void spn_codegen_launcher_from_str(spn_codegen_ctx_t* ctx, sp_str_t raw, sp_str_t* program, sp_da(sp_str_t)* args) {
  if (sp_str_empty(raw)) {
    return;
  }
  if (sp_str_contains(raw, sp_str_lit(" "))) {
    sp_da(sp_str_t) parts = sp_str_split_c8(ctx->mem, raw, ' ');
    *program = sp_intern_get_or_insert_str(ctx->intern, parts[0]);
    *args = sp_da_new(ctx->mem, sp_str_t);
    for (u32 i = 1; i < sp_da_size(parts); i++) {
      sp_da_push(*args, sp_intern_get_or_insert_str(ctx->intern, parts[i]));
    }
  }
  else {
    *program = sp_intern_get_or_insert_str(ctx->intern, raw);
  }
}

static void spn_codegen_json_writer_newline(spn_codegen_json_writer_t* w) {
  sp_io_write_c8(w->inner, '\n');
  sp_for(it, w->depth) {
    sp_io_write_str(w->inner, sp_str_lit("  "), SP_NULLPTR);
  }
}

static void spn_codegen_json_writer_pending(spn_codegen_json_writer_t* w) {
  if (w->pending) {
    w->pending = false;
    spn_codegen_json_writer_newline(w);
  }
}

static sp_err_t spn_codegen_json_writer_write(sp_io_writer_t* base, const void* ptr, u64 size, u64* bytes_written) {
  spn_codegen_json_writer_t* w = (spn_codegen_json_writer_t*)base;
  const c8* data = (const c8*)ptr;

  sp_for(it, size) {
    c8 c = data[it];

    if (w->in_string) {
      sp_io_write_c8(w->inner, c);
      if (w->escape) {
        w->escape = false;
      } else if (c == '\\') {
        w->escape = true;
      } else if (c == '"') {
        w->in_string = false;
      }
      continue;
    }

    switch (c) {
      case '"': {
        spn_codegen_json_writer_pending(w);
        sp_io_write_c8(w->inner, c);
        w->in_string = true;
        break;
      }
      case '{':
      case '[': {
        spn_codegen_json_writer_pending(w);
        sp_io_write_c8(w->inner, c);
        w->depth += 1;
        w->pending = true;
        break;
      }
      case '}':
      case ']': {
        w->depth -= 1;
        if (w->pending) {
          w->pending = false;
        } else {
          spn_codegen_json_writer_newline(w);
        }
        sp_io_write_c8(w->inner, c);
        break;
      }
      case ',': {
        sp_io_write_c8(w->inner, c);
        w->pending = true;
        break;
      }
      case ':': {
        sp_io_write_str(w->inner, sp_str_lit(": "), SP_NULLPTR);
        break;
      }
      default: {
        spn_codegen_json_writer_pending(w);
        sp_io_write_c8(w->inner, c);
        break;
      }
    }
  }

  if (bytes_written) *bytes_written = size;
  return SP_OK;
}

void spn_codegen_json_writer_init(spn_codegen_json_writer_t* writer, sp_io_writer_t* inner) {
  *writer = (spn_codegen_json_writer_t) {
    .base = { .write = spn_codegen_json_writer_write },
    .inner = inner,
  };
}

void spn_codegen_json_key(sp_io_writer_t* out, bool* first, sp_str_t key) {
  if (!*first) {
    sp_io_write_c8(out, ',');
  }
  *first = false;
  spn_codegen_json_str(out, key);
  sp_io_write_c8(out, ':');
}

void spn_codegen_json_str(sp_io_writer_t* out, sp_str_t value) {
  static const c8 hex[] = "0123456789abcdef";
  sp_io_write_c8(out, '"');
  sp_for(it, value.len) {
    c8 c = value.data[it];
    if (c == '"' || c == '\\') {
      sp_io_write_c8(out, '\\');
      sp_io_write_c8(out, c);
    } else if ((u8)c < 0x20) {
      sp_io_write_str(out, sp_str_lit("\\u00"), SP_NULLPTR);
      sp_io_write_c8(out, hex[((u8)c >> 4) & 0xf]);
      sp_io_write_c8(out, hex[(u8)c & 0xf]);
    } else {
      sp_io_write_c8(out, c);
    }
  }
  sp_io_write_c8(out, '"');
}

void spn_codegen_json_bool(sp_io_writer_t* out, bool value) {
  sp_io_write_str(out, value ? sp_str_lit("true") : sp_str_lit("false"), SP_NULLPTR);
}

const c8* spn_codegen_err_name(spn_codegen_err_t code) {
  switch (code) {
    case SPN_CODEGEN_OK:                 return "ok";
    case SPN_CODEGEN_ERR_EXPECTED_BOOL:  return "expected_bool";
    case SPN_CODEGEN_ERR_EXPECTED_STR:   return "expected_str";
    case SPN_CODEGEN_ERR_EXPECTED_OBJECT:return "expected_object";
    case SPN_CODEGEN_ERR_MISSING_KEY:    return "missing_key";
    case SPN_CODEGEN_ERR_DUPLICATE_KEY:  return "duplicate_key";
    case SPN_CODEGEN_ERR_PARSE:          return "parse";
    case SPN_CODEGEN_ERR_FILE_MISSING:   return "file_missing";
    case SPN_CODEGEN_ERR_INVALID:        return "invalid";
  }
  return "unknown";
}

void spn_codegen_json_issues(sp_io_writer_t* out, sp_da(spn_codegen_issue_t) issues) {
  sp_io_write_c8(out, '{');
  bool first = true;
  spn_codegen_json_key(out, &first, sp_str_lit("issues"));
  sp_io_write_c8(out, '[');
  sp_da_for(issues, it) {
    if (it) {
      sp_io_write_c8(out, ',');
    }
    spn_codegen_issue_t* issue = &issues[it];
    sp_io_write_c8(out, '{');
    bool ifirst = true;
    spn_codegen_json_key(out, &ifirst, sp_str_lit("code"));
    spn_codegen_json_str(out, sp_cstr_as_str(spn_codegen_err_name(issue->code)));
    spn_codegen_json_key(out, &ifirst, sp_str_lit("path"));
    spn_codegen_json_str(out, issue->path);
    spn_codegen_json_key(out, &ifirst, sp_str_lit("detail"));
    spn_codegen_json_str(out, issue->detail);
    sp_io_write_c8(out, '}');
  }
  sp_io_write_c8(out, ']');
  sp_io_write_c8(out, '}');
}

sp_str_t spn_codegen_issues_to_str(sp_mem_t mem, sp_da(spn_codegen_issue_t) issues) {
  sp_io_dyn_mem_writer_t sink;
  sp_io_dyn_mem_writer_init(mem, &sink);
  spn_codegen_json_writer_t pretty;
  spn_codegen_json_writer_init(&pretty, &sink.base);
  spn_codegen_json_issues(&pretty.base, issues);
  return sp_io_dyn_mem_writer_as_str(&sink);
}

void spn_codegen_json_str_array(sp_io_writer_t* out, sp_da(sp_str_t) values) {
  sp_io_write_c8(out, '[');
  sp_da_for(values, it) {
    if (it) {
      sp_io_write_c8(out, ',');
    }
    spn_codegen_json_str(out, values[it]);
  }
  sp_io_write_c8(out, ']');
}

void spn_codegen_validate_toolchain(spn_codegen_ctx_t* ctx, spn_cg_toolchain_t* toolchain) {
  if (!sp_str_empty(toolchain->package)) {
    return;
  }
  if (sp_str_empty(toolchain->name)) {
    spn_codegen_issue(ctx, SPN_CODEGEN_ERR_MISSING_KEY, "name");
  }
  if (sp_str_empty(toolchain->compiler.program)) {
    spn_codegen_issue(ctx, SPN_CODEGEN_ERR_MISSING_KEY, "compiler");
  }
  if (sp_str_empty(toolchain->linker.program)) {
    spn_codegen_issue(ctx, SPN_CODEGEN_ERR_MISSING_KEY, "linker");
  }
  if (sp_str_empty(toolchain->archiver.program)) {
    spn_codegen_issue(ctx, SPN_CODEGEN_ERR_MISSING_KEY, "archiver");
  }
  if (sp_opt_is_null(toolchain->driver) || toolchain->driver.value == SPN_CC_DRIVER_NONE) {
    spn_codegen_issue(ctx, SPN_CODEGEN_ERR_MISSING_KEY, "driver");
  }
  if (sp_da_empty(toolchain->host)) {
    spn_codegen_issue(ctx, SPN_CODEGEN_ERR_MISSING_KEY, "host");
  }
  if (sp_da_empty(toolchain->target)) {
    spn_codegen_issue(ctx, SPN_CODEGEN_ERR_MISSING_KEY, "target");
  }
}

void spn_codegen_validate_lib(spn_codegen_ctx_t* ctx, const c8* key, spn_cg_target_om_t* libs) {
  spn_codegen_push_key(ctx, key);
  sp_om_for(*libs, it) {
    spn_cg_target_t* target = sp_om_at(*libs, it);
    bool object = false;
    bool linkable = false;
    sp_da_for(target->kinds, k) {
      sp_str_t kind = target->kinds[k];
      object |= sp_str_equal(kind, sp_str_lit("object"));
      linkable |= sp_str_equal(kind, sp_str_lit("source"))
               || sp_str_equal(kind, sp_str_lit("shared"))
               || sp_str_equal(kind, sp_str_lit("static"));
    }
    if (object && linkable) {
      spn_codegen_push_index(ctx, it);
      spn_codegen_issue(ctx, SPN_CODEGEN_ERR_INVALID, "kinds");
      spn_codegen_pop(ctx);
    }
  }
  spn_codegen_pop(ctx);
}

void spn_codegen_validate_no_link(spn_codegen_ctx_t* ctx, const c8* key, spn_cg_target_om_t* targets) {
  spn_codegen_push_key(ctx, key);
  sp_om_for(*targets, it) {
    spn_cg_target_t* target = sp_om_at(*targets, it);
    if (!sp_opt_is_null(target->link)) {
      spn_codegen_push_index(ctx, it);
      spn_codegen_issue(ctx, SPN_CODEGEN_ERR_INVALID, "link");
      spn_codegen_pop(ctx);
    }
  }
  spn_codegen_pop(ctx);
}

void spn_codegen_validate_manifest(spn_codegen_ctx_t* ctx, spn_cg_manifest_t* manifest) {
  sp_ht(sp_str_t, u8) seen;
  sp_str_ht_init(ctx->mem, seen);
  struct {
    const c8* key;
    spn_cg_target_om_t targets;
  } groups [] = {
    { "lib", manifest->lib },
    { "bin", manifest->bin },
    { "script", manifest->script },
    { "test", manifest->test },
  };
  sp_for(g, SP_CARR_LEN(groups)) {
    spn_codegen_push_key(ctx, groups[g].key);
    sp_om_for(groups[g].targets, it) {
      spn_cg_target_t* target = sp_om_at(groups[g].targets, it);
      if (sp_ht_getp(seen, target->name)) {
        spn_codegen_push_index(ctx, it);
        spn_codegen_issue_at(ctx, SPN_CODEGEN_ERR_DUPLICATE_KEY, target->name);
        spn_codegen_pop(ctx);
      } else {
        sp_ht_insert(seen, target->name, 1);
      }
    }
    spn_codegen_pop(ctx);
  }
}

void spn_codegen_compute_qualified(spn_codegen_ctx_t* ctx, spn_cg_manifest_t* out) {
  sp_str_t namespace = sp_str_empty(out->namespace) ? sp_str_lit("core") : out->namespace;
  out->qualified = spn_codegen_intern(ctx, sp_str_join(ctx->mem, namespace, out->name, sp_str_lit("/")));
}

void spn_codegen_compute_versions(spn_codegen_ctx_t* ctx, spn_cg_manifest_t* out) {
  out->versions = sp_da_new(ctx->mem, spn_cg_version_metadata_t);
  spn_cg_version_metadata_t entry = { .version = out->version, .commit = out->commit };
  sp_da_push(out->versions, entry);
}

void spn_codegen_compute_index_kind(spn_codegen_ctx_t* ctx, spn_cg_index_t* out) {
  (void)ctx;
  out->kind = SPN_INDEX_WORKSPACE;
}

bool spn_codegen_load(spn_codegen_ctx_t* ctx, sp_str_t path, spn_cg_manifest_t* out) {
  if (!sp_fs_exists(path)) {
    return spn_codegen_issue_at(ctx, SPN_CODEGEN_ERR_FILE_MISSING, sp_str_lit("missing file"));
  }

  toml_table_t* table = spn_toml_parse_ex(path, SP_NULLPTR);
  if (!table) {
    return spn_codegen_issue_at(ctx, SPN_CODEGEN_ERR_PARSE, sp_str_lit("invalid toml"));
  }
  spn_cg_root_read(ctx, table, out);
  return sp_da_size(ctx->issues) > 0;
}
