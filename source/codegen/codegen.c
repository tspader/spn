#include "codegen/codegen.h"

void spn_codegen_ctx_init(spn_codegen_ctx_t* ctx, sp_mem_t mem) {
  ctx->mem = mem;
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

void spn_codegen_issue(spn_codegen_ctx_t* ctx, spn_codegen_err_t code, const c8* key) {
  spn_codegen_push_key(ctx, key);
  spn_codegen_record(ctx, code, sp_cstr_as_str(key));
  spn_codegen_pop(ctx);
}

bool spn_codegen_read_str(spn_codegen_ctx_t* ctx, toml_table_t* table, const c8* key, sp_str_t* value) {
  toml_value_t found = toml_table_string(table, key);
  if (!found.ok) {
    return false;
  }
  *value = sp_str_copy(ctx->mem, sp_str(found.u.s, (u32)found.u.sl));
  return true;
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
      sp_da_push(values, sp_str_copy(ctx->mem, sp_str(element.u.s, (u32)element.u.sl)));
    } else {
      spn_codegen_record(ctx, SPN_CODEGEN_ERR_EXPECTED_STR, sp_str_lit(""));
    }
    spn_codegen_pop(ctx);
  }
  spn_codegen_pop(ctx);
  return values;
}

static void spn_codegen_push(sp_da(c8)* out, sp_str_t raw) {
  sp_for(it, raw.len) {
    sp_da_push(*out, raw.data[it]);
  }
}

void spn_codegen_json_str(sp_da(c8)* out, sp_str_t value) {
  static const c8 hex[] = "0123456789abcdef";
  sp_da_push(*out, '"');
  sp_for(it, value.len) {
    c8 c = value.data[it];
    if (c == '"' || c == '\\') {
      sp_da_push(*out, '\\');
      sp_da_push(*out, c);
    } else if ((u8)c < 0x20) {
      spn_codegen_push(out, sp_str_lit("\\u00"));
      sp_da_push(*out, hex[((u8)c >> 4) & 0xf]);
      sp_da_push(*out, hex[(u8)c & 0xf]);
    } else {
      sp_da_push(*out, c);
    }
  }
  sp_da_push(*out, '"');
}

void spn_codegen_json_bool(sp_da(c8)* out, bool value) {
  spn_codegen_push(out, value ? sp_str_lit("true") : sp_str_lit("false"));
}

void spn_codegen_json_str_array(sp_da(c8)* out, sp_da(sp_str_t) values) {
  sp_da_push(*out, '[');
  sp_da_for(values, it) {
    if (it) {
      sp_da_push(*out, ',');
    }
    spn_codegen_json_str(out, values[it]);
  }
  sp_da_push(*out, ']');
}
