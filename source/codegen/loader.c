#include "codegen/codegen.h"
#include "external/tom.h"
#include "intern/intern.h"
#include "sp.h"
#include "sp/macro.h"
#include "spn.h"

void spn_toml_loader_init(spn_toml_loader_t* ctx, sp_mem_t mem, sp_intern_t* intern) {
  ctx->mem = mem;
  ctx->intern = intern;
  ctx->depth = 0;
  ctx->issues = sp_da_new(mem, spn_codegen_issue_t);
}

void spn_toml_loader_push_key(spn_toml_loader_t* ctx, const c8* key) {
  if (ctx->depth >= SPN_CODEGEN_PATH_MAX) {
    return;
  }
  ctx->path[ctx->depth] = (spn_codegen_path_seg_t) { .kind = SPN_CODEGEN_PATH_KEY, .key = key };
  ctx->depth += 1;
}

void spn_toml_loader_push_index(spn_toml_loader_t* ctx, u32 index) {
  if (ctx->depth >= SPN_CODEGEN_PATH_MAX) {
    return;
  }
  ctx->path[ctx->depth] = (spn_codegen_path_seg_t) { .kind = SPN_CODEGEN_PATH_INDEX, .index = index };
  ctx->depth += 1;
}

void spn_toml_loader_pop(spn_toml_loader_t* ctx) {
  if (ctx->depth) {
    ctx->depth -= 1;
  }
}

static sp_str_t spn_codegen_path(spn_toml_loader_t* ctx) {
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

static void spn_toml_loader_record(spn_toml_loader_t* ctx, spn_err_t code, sp_str_t detail) {
  spn_codegen_issue_t issue = { .code = code, .path = spn_codegen_path(ctx), .detail = sp_str_copy(ctx->mem, detail) };
  sp_da_push(ctx->issues, issue);
}

bool spn_toml_loader_issue(spn_toml_loader_t* ctx, spn_err_t code, const c8* key) {
  spn_toml_loader_push_key(ctx, key);
  spn_toml_loader_record(ctx, code, sp_cstr_as_str(key));
  spn_toml_loader_pop(ctx);
  return true;
}

bool spn_toml_loader_issue_at(spn_toml_loader_t* ctx, spn_err_t code, sp_str_t detail) {
  spn_toml_loader_record(ctx, code, detail);
  return true;
}

sp_str_t spn_toml_loader_intern(spn_toml_loader_t* ctx, sp_str_t value) {
  return sp_intern_get_or_insert_str(ctx->intern, value);
}

sp_str_t spn_toml_loader_intern_value(spn_toml_loader_t* ctx, toml_value_t value) {
  sp_str_t interned = sp_intern_get_or_insert_str(ctx->intern, sp_str(value.u.s, (u32)value.u.sl));
  free(value.u.s);
  return interned;
}

bool spn_toml_loader_str_present(toml_table_t* table, const c8* key) {
  toml_value_t value = toml_table_string(table, key);
  if (value.ok) {
    free(value.u.s);
  }
  return value.ok;
}

static bool spn_toml_loader_read_raw_value(spn_toml_loader_t* ctx, toml_table_t* table, const c8* key, sp_str_t* value) {
  toml_value_t found = toml_table_string(table, key);
  if (!found.ok) {
    return false;
  }
  *value = spn_toml_loader_intern_value(ctx, found);
  return true;
}

bool spn_toml_loader_field_present(toml_table_t* table, const c8* key) {
  return toml_table_array(table, key) || toml_table_table(table, key) || toml_table_unparsed(table, key);
}

static spn_err_t spn_toml_loader_required_str_err(toml_table_t* table, const c8* key) {
  return spn_toml_loader_field_present(table, key) ? SPN_CODEGEN_ERR_EXPECTED_STR : SPN_CODEGEN_ERR_MISSING_KEY;
}

sp_str_t spn_toml_loader_str_required(spn_toml_loader_t* ctx, toml_table_t* table, const c8* key) {
  sp_str_t value = sp_zero;
  if (!spn_toml_loader_read_raw_value(ctx, table, key, &value)) {
    spn_toml_loader_issue(ctx, spn_toml_loader_required_str_err(table, key), key);
  }
  return value;
}

bool spn_toml_loader_str_optional(spn_toml_loader_t* ctx, toml_table_t* table, const c8* key, sp_str_t* value) {
  return spn_toml_loader_read_raw_value(ctx, table, key, value);
}

sp_str_t spn_toml_loader_raw_required(spn_toml_loader_t* ctx, toml_table_t* table, const c8* key) {
  sp_str_t value = sp_zero;
  if (!spn_toml_loader_read_raw_value(ctx, table, key, &value)) {
    spn_toml_loader_issue(ctx, spn_toml_loader_required_str_err(table, key), key);
  }
  return value;
}

bool spn_toml_loader_raw_optional(spn_toml_loader_t* ctx, toml_table_t* table, const c8* key, sp_str_t* value) {
  return spn_toml_loader_read_raw_value(ctx, table, key, value);
}

bool spn_toml_loader_read_bool(spn_toml_loader_t* ctx, toml_table_t* table, const c8* key, bool* value) {
  toml_value_t found = toml_table_bool(table, key);
  if (!found.ok) {
    return false;
  }
  *value = found.u.b;
  return true;
}

bool spn_toml_loader_read_u64(spn_toml_loader_t* ctx, toml_table_t* table, const c8* key, u64* value) {
  toml_value_t found = toml_table_int(table, key);
  if (found.ok) {
    if (found.u.i < 0) {
      spn_toml_loader_issue(ctx, SPN_CODEGEN_ERR_EXPECTED_INT, key);
      return false;
    }
    *value = (u64)found.u.i;
    return true;
  }

  if (spn_toml_loader_field_present(table, key)) {
    spn_toml_loader_issue(ctx, SPN_CODEGEN_ERR_EXPECTED_INT, key);
  }
  return false;
}

void spn_toml_loader_check_keys(spn_toml_loader_t* ctx, toml_table_t* table, const c8** keys, u32 count) {
  if (!ctx->strict) {
    return;
  }
  sp_for(it, (u32)toml_table_len(table)) {
    s32 len = 0;
    const c8* name = toml_table_key(table, (s32)it, &len);
    if (!name) {
      break;
    }
    bool known = false;
    sp_for(kt, count) {
      if (sp_str_equal(sp_str(name, (u32)len), sp_cstr_as_str(keys[kt]))) {
        known = true;
        break;
      }
    }
    if (!known) {
      spn_toml_loader_issue_at(ctx, SPN_CODEGEN_ERR_UNKNOWN_KEY, sp_str(name, (u32)len));
    }
  }
}

sp_da(sp_str_t) spn_toml_loader_read_str_array(spn_toml_loader_t* ctx, toml_table_t* table, const c8* key) {
  toml_array_t* array = toml_table_array(table, key);
  if (!array) {
    return SP_NULLPTR;
  }

  sp_da(sp_str_t) values = sp_da_new(ctx->mem, sp_str_t);
  spn_toml_loader_push_key(ctx, key);
  sp_for(it, (u32)toml_array_len(array)) {
    spn_toml_loader_push_index(ctx, it);
    toml_value_t element = toml_array_string(array, (s32)it);
    if (element.ok) {
      sp_da_push(values, spn_toml_loader_intern_value(ctx, element));
    } else {
      spn_toml_loader_record(ctx, SPN_CODEGEN_ERR_EXPECTED_STR, sp_str_lit(""));
    }
    spn_toml_loader_pop(ctx);
  }
  spn_toml_loader_pop(ctx);
  return values;
}

const c8* spn_codegen_err_name(spn_err_t code) {
  switch (code) {
    case SPN_OK:                         return "ok";
    case SPN_CODEGEN_ERR_EXPECTED_BOOL:  return "expected_bool";
    case SPN_CODEGEN_ERR_EXPECTED_STR:   return "expected_str";
    case SPN_CODEGEN_ERR_EXPECTED_INT:   return "expected_int";
    case SPN_CODEGEN_ERR_EXPECTED_OBJECT:return "expected_object";
    case SPN_CODEGEN_ERR_MISSING_KEY:    return "missing_key";
    case SPN_CODEGEN_ERR_DUPLICATE_KEY:  return "duplicate_key";
    case SPN_CODEGEN_ERR_UNKNOWN_KEY:    return "unknown_key";
    case SPN_CODEGEN_ERR_PARSE:          return "parse";
    case SPN_CODEGEN_ERR_FILE_MISSING:   return "file_missing";
    case SPN_CODEGEN_ERR_INVALID:        return "invalid";
    case SPN_CODEGEN_ERR_ROOT_ONLY:      return "root_only";
    default:                             return "unknown";
  }
}

void spn_codegen_issue_write(sp_io_writer_t* w, const spn_codegen_issue_t* issue) {
  switch (issue->code) {
    case SPN_CODEGEN_ERR_MISSING_KEY:
      sp_fmt_io(w, "missing required field {.cyan}", SP_FMT_STR(issue->path));
      break;
    case SPN_CODEGEN_ERR_EXPECTED_STR:
      sp_fmt_io(w, "{.cyan} must be a string", SP_FMT_STR(issue->path));
      break;
    case SPN_CODEGEN_ERR_EXPECTED_INT:
      sp_fmt_io(w, "{.cyan} must be a non-negative integer", SP_FMT_STR(issue->path));
      break;
    case SPN_CODEGEN_ERR_EXPECTED_BOOL:
      sp_fmt_io(w, "{.cyan} must be a boolean", SP_FMT_STR(issue->path));
      break;
    case SPN_CODEGEN_ERR_EXPECTED_OBJECT:
      sp_fmt_io(w, "{.cyan} must be a table", SP_FMT_STR(issue->path));
      break;
    case SPN_CODEGEN_ERR_DUPLICATE_KEY:
      sp_fmt_io(w, "duplicate {.yellow} at {.cyan}", SP_FMT_STR(issue->detail), SP_FMT_STR(issue->path));
      break;
    case SPN_CODEGEN_ERR_UNKNOWN_KEY:
      if (sp_str_empty(issue->path)) {
        sp_fmt_io(w, "unknown field {.red}", SP_FMT_STR(issue->detail));
      } else {
        sp_fmt_io(w, "unknown field {.red} in {.cyan}", SP_FMT_STR(issue->detail), SP_FMT_STR(issue->path));
      }
      break;
    case SPN_CODEGEN_ERR_INVALID:
      sp_fmt_io(w, "invalid value at {.cyan}", SP_FMT_STR(issue->path));
      break;
    case SPN_CODEGEN_ERR_PARSE:
      if (sp_str_empty(issue->detail)) {
        sp_io_write_str(w, sp_str_lit("not valid toml"), SP_NULLPTR);
      } else {
        sp_fmt_io(w, "not valid toml: {}", SP_FMT_STR(issue->detail));
      }
      break;
    case SPN_CODEGEN_ERR_FILE_MISSING:
      sp_io_write_str(w, sp_str_lit("file is missing"), SP_NULLPTR);
      break;
    case SPN_CODEGEN_ERR_ROOT_ONLY:
      sp_fmt_io(w, "{.cyan} is only allowed in the root manifest", SP_FMT_STR(issue->path));
      break;
    default:
      sp_fmt_io(w, "invalid field at {.cyan}", SP_FMT_STR(issue->path));
      break;
  }
}

sp_str_t spn_codegen_issues_message(sp_mem_t mem, sp_da(spn_codegen_issue_t) issues) {
  sp_io_dyn_mem_writer_t b = sp_zero;
  sp_io_dyn_mem_writer_init(mem, &b);
  sp_da_for(issues, it) {
    if (it) {
      sp_fmt_io(&b.base, "; ");
    }
    spn_codegen_issue_write(&b.base, &issues[it]);
  }
  return sp_io_dyn_mem_writer_as_str(&b);
}

toml_table_t* spn_codegen_parse(spn_toml_loader_t* ctx, sp_str_t path) {
  if (!sp_fs_is_file(path)) {
    spn_toml_loader_issue_at(ctx, SPN_CODEGEN_ERR_FILE_MISSING, sp_str_lit("missing file"));
    return SP_NULLPTR;
  }

  sp_str_t diag = sp_zero;
  toml_table_t* table = spn_toml_parse_diag(ctx->mem, path, &diag);
  if (!table) {
    spn_toml_loader_issue_at(ctx, SPN_CODEGEN_ERR_PARSE, diag);
  }
  return table;
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
