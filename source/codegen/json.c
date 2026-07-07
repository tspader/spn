#include "codegen/codegen.h"
#include "sp.h"

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
