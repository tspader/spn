#include "toml/edit.h"

typedef struct {
  spn_toml_edit_t* edit;
  u32 it;
  bool err;
} spn_toml_edit_parser_t;

static c8 spn_toml_edit_peek_at(spn_toml_edit_parser_t* parser, u32 offset) {
  u32 it = parser->it + offset;
  if (it >= parser->edit->source.len) {
    return 0;
  }
  return parser->edit->source.data[it];
}

static c8 spn_toml_edit_peek(spn_toml_edit_parser_t* parser) {
  return spn_toml_edit_peek_at(parser, 0);
}

static bool spn_toml_edit_done(spn_toml_edit_parser_t* parser) {
  return parser->it >= parser->edit->source.len;
}

static void spn_toml_edit_eat(spn_toml_edit_parser_t* parser) {
  parser->it++;
}

static bool spn_toml_edit_accept(spn_toml_edit_parser_t* parser, c8 c) {
  if (spn_toml_edit_peek(parser) != c) {
    return false;
  }
  spn_toml_edit_eat(parser);
  return true;
}

static bool spn_toml_edit_is_whitespace(c8 c) {
  return c == ' ' || c == '\t';
}

static bool spn_toml_edit_is_blank(c8 c) {
  return spn_toml_edit_is_whitespace(c) || c == '\r' || c == '\n';
}

static bool spn_toml_edit_is_bare(c8 c) {
  if (c >= 'a' && c <= 'z') {
    return true;
  }
  if (c >= 'A' && c <= 'Z') {
    return true;
  }
  if (c >= '0' && c <= '9') {
    return true;
  }
  return c == '_' || c == '-';
}

static bool spn_toml_edit_is_hex(c8 c) {
  if (c >= '0' && c <= '9') {
    return true;
  }
  if (c >= 'a' && c <= 'f') {
    return true;
  }
  return c >= 'A' && c <= 'F';
}

static u32 spn_toml_edit_hex(c8 c) {
  if (c >= '0' && c <= '9') {
    return (u32)(c - '0');
  }
  if (c >= 'a' && c <= 'f') {
    return (u32)(c - 'a' + 10);
  }
  return (u32)(c - 'A' + 10);
}

static void spn_toml_edit_skip_whitespace(spn_toml_edit_parser_t* parser) {
  while (spn_toml_edit_is_whitespace(spn_toml_edit_peek(parser))) {
    spn_toml_edit_eat(parser);
  }
}

static void spn_toml_edit_skip_blank(spn_toml_edit_parser_t* parser) {
  while (!spn_toml_edit_done(parser) && spn_toml_edit_is_blank(spn_toml_edit_peek(parser))) {
    spn_toml_edit_eat(parser);
  }
}

static void spn_toml_edit_skip_comment(spn_toml_edit_parser_t* parser) {
  if (spn_toml_edit_peek(parser) != '#') {
    return;
  }
  while (!spn_toml_edit_done(parser) && spn_toml_edit_peek(parser) != '\n') {
    spn_toml_edit_eat(parser);
  }
}

static bool spn_toml_edit_skip_line_end(spn_toml_edit_parser_t* parser) {
  spn_toml_edit_skip_whitespace(parser);
  spn_toml_edit_skip_comment(parser);
  if (spn_toml_edit_done(parser)) {
    return true;
  }
  spn_toml_edit_accept(parser, '\r');
  return spn_toml_edit_accept(parser, '\n');
}

static bool spn_toml_edit_codepoint_valid(u32 codepoint) {
  if (codepoint > 0x10FFFF) {
    return false;
  }
  return codepoint < 0xD800 || codepoint > 0xDFFF;
}

static bool spn_toml_edit_scan_unicode(spn_toml_edit_parser_t* parser, c8 kind, sp_io_writer_t* io) {
  u32 num_digits = (kind == 'u') ? 4 : 8;
  u32 codepoint = 0;
  sp_for(it, num_digits) {
    c8 digit = spn_toml_edit_peek(parser);
    if (!spn_toml_edit_is_hex(digit)) {
      return false;
    }
    codepoint = codepoint * 16 + spn_toml_edit_hex(digit);
    spn_toml_edit_eat(parser);
  }

  if (!spn_toml_edit_codepoint_valid(codepoint)) {
    return false;
  }
  if (io) {
    c8 encoded [4] = sp_zero;
    u8 len = sp_utf8_encode(codepoint, encoded);
    sp_io_write(io, encoded, len, SP_NULLPTR);
  }
  return true;
}

static bool spn_toml_edit_unescape(spn_toml_edit_parser_t* parser, sp_io_writer_t* io) {
  c8 c = spn_toml_edit_peek(parser);
  spn_toml_edit_eat(parser);

  switch (c) {
    case 'b': sp_io_write_c8(io, '\b'); return true;
    case 't': sp_io_write_c8(io, '\t'); return true;
    case 'n': sp_io_write_c8(io, '\n'); return true;
    case 'f': sp_io_write_c8(io, '\f'); return true;
    case 'r': sp_io_write_c8(io, '\r'); return true;
    case '"': sp_io_write_c8(io, '"'); return true;
    case '\\': sp_io_write_c8(io, '\\'); return true;
    case 'u':
    case 'U': return spn_toml_edit_scan_unicode(parser, c, io);
  }

  return false;
}

static sp_str_t spn_toml_edit_parse_basic(spn_toml_edit_parser_t* parser) {
  spn_toml_edit_eat(parser);

  sp_io_dyn_mem_writer_t decoded = sp_zero;
  sp_io_dyn_mem_writer_init(parser->edit->mem, &decoded);

  while (true) {
    if (spn_toml_edit_done(parser) || spn_toml_edit_peek(parser) == '\n') {
      parser->err = true;
      return sp_zero_s(sp_str_t);
    }

    c8 c = spn_toml_edit_peek(parser);
    if (c == '"') {
      spn_toml_edit_eat(parser);
      return sp_io_dyn_mem_writer_take_str(&decoded);
    }
    if (c == '\\') {
      spn_toml_edit_eat(parser);
      if (!spn_toml_edit_unescape(parser, &decoded.base)) {
        parser->err = true;
        return sp_zero_s(sp_str_t);
      }
      continue;
    }

    sp_io_write_c8(&decoded.base, c);
    spn_toml_edit_eat(parser);
  }
}

static sp_str_t spn_toml_edit_parse_literal(spn_toml_edit_parser_t* parser) {
  spn_toml_edit_eat(parser);

  u32 start = parser->it;
  while (true) {
    if (spn_toml_edit_done(parser) || spn_toml_edit_peek(parser) == '\n') {
      parser->err = true;
      return sp_zero_s(sp_str_t);
    }
    if (spn_toml_edit_peek(parser) == '\'') {
      sp_str_t inner = sp_str_sub(parser->edit->source, start, parser->it - start);
      spn_toml_edit_eat(parser);
      return inner;
    }
    spn_toml_edit_eat(parser);
  }
}

static sp_da(sp_str_t) spn_toml_edit_parse_key(spn_toml_edit_parser_t* parser) {
  sp_da(sp_str_t) segments = sp_da_new(parser->edit->mem, sp_str_t);

  while (true) {
    spn_toml_edit_skip_whitespace(parser);

    c8 c = spn_toml_edit_peek(parser);
    sp_str_t segment = sp_zero;
    if (c == '"') {
      segment = spn_toml_edit_parse_basic(parser);
    }
    else if (c == '\'') {
      segment = spn_toml_edit_parse_literal(parser);
    }
    else {
      u32 start = parser->it;
      while (spn_toml_edit_is_bare(spn_toml_edit_peek(parser))) {
        spn_toml_edit_eat(parser);
      }
      if (parser->it == start) {
        parser->err = true;
      }
      segment = sp_str_sub(parser->edit->source, start, parser->it - start);
    }

    if (parser->err) {
      return segments;
    }
    sp_da_push(segments, segment);

    spn_toml_edit_skip_whitespace(parser);
    if (!spn_toml_edit_accept(parser, '.')) {
      break;
    }
  }

  return segments;
}

static bool spn_toml_edit_escape_valid(c8 c, bool multiline) {
  switch (c) {
    case 'b':
    case 't':
    case 'n':
    case 'f':
    case 'r':
    case '"':
    case '\\':
    case 'u':
    case 'U': {
      return true;
    }
  }
  return multiline && spn_toml_edit_is_blank(c);
}

static bool spn_toml_edit_scan_escape(spn_toml_edit_parser_t* parser, bool multiline) {
  spn_toml_edit_eat(parser);

  c8 c = spn_toml_edit_peek(parser);
  if (!spn_toml_edit_escape_valid(c, multiline)) {
    return false;
  }
  spn_toml_edit_eat(parser);

  if (c == 'u' || c == 'U') {
    return spn_toml_edit_scan_unicode(parser, c, SP_NULLPTR);
  }
  return true;
}

static bool spn_toml_edit_scan_string(spn_toml_edit_parser_t* parser) {
  c8 quote = spn_toml_edit_peek(parser);
  bool multiline = spn_toml_edit_peek_at(parser, 1) == quote && spn_toml_edit_peek_at(parser, 2) == quote;
  parser->it += multiline ? 3 : 1;

  while (true) {
    if (spn_toml_edit_done(parser)) {
      return false;
    }

    c8 c = spn_toml_edit_peek(parser);
    if (c == '\n' && !multiline) {
      return false;
    }
    if (c == '\\' && quote == '"') {
      if (!spn_toml_edit_scan_escape(parser, multiline)) {
        return false;
      }
      continue;
    }
    if (c != quote) {
      spn_toml_edit_eat(parser);
      continue;
    }

    if (!multiline) {
      spn_toml_edit_eat(parser);
      return true;
    }

    u32 run = 0;
    while (spn_toml_edit_peek_at(parser, run) == quote) {
      run++;
    }
    parser->it += run;
    if (run >= 3) {
      return run <= 5;
    }
  }
}

static bool spn_toml_edit_scan_value(spn_toml_edit_parser_t* parser, spn_toml_edit_entry_t* entry, bool nested);

static bool spn_toml_edit_scan_pair(spn_toml_edit_parser_t* parser, spn_toml_edit_entry_t* entry, bool nested) {
  entry->entries = sp_da_new(parser->edit->mem, spn_toml_edit_entry_t);

  entry->key.start = parser->it;
  entry->path = spn_toml_edit_parse_key(parser);
  entry->key.end = parser->it;
  if (parser->err) {
    return false;
  }

  spn_toml_edit_skip_whitespace(parser);
  if (!spn_toml_edit_accept(parser, '=')) {
    return false;
  }
  spn_toml_edit_skip_whitespace(parser);

  entry->value.start = parser->it;
  return spn_toml_edit_scan_value(parser, entry, nested);
}

static bool spn_toml_edit_scan_inline_table(spn_toml_edit_parser_t* parser, spn_toml_edit_entry_t* entry) {
  spn_toml_edit_eat(parser);
  spn_toml_edit_skip_blank(parser);

  if (spn_toml_edit_accept(parser, '}')) {
    return true;
  }

  while (true) {
    spn_toml_edit_skip_blank(parser);

    spn_toml_edit_entry_t sub = sp_zero;
    if (!spn_toml_edit_scan_pair(parser, &sub, true)) {
      return false;
    }
    sp_da_push(entry->entries, sub);

    spn_toml_edit_skip_blank(parser);
    if (spn_toml_edit_accept(parser, ',')) {
      continue;
    }
    return spn_toml_edit_accept(parser, '}');
  }
}

static bool spn_toml_edit_scan_array(spn_toml_edit_parser_t* parser) {
  u32 depth = 0;
  while (true) {
    if (spn_toml_edit_done(parser)) {
      return false;
    }

    switch (spn_toml_edit_peek(parser)) {
      case '"':
      case '\'': {
        if (!spn_toml_edit_scan_string(parser)) {
          return false;
        }
        continue;
      }
      case '#': {
        spn_toml_edit_skip_comment(parser);
        continue;
      }
      case '[': {
        depth++;
        spn_toml_edit_eat(parser);
        continue;
      }
      case ']': {
        depth--;
        spn_toml_edit_eat(parser);
        if (!depth) {
          return true;
        }
        continue;
      }
      default: {
        spn_toml_edit_eat(parser);
        break;
      }
    }
  }
}

static bool spn_toml_edit_scan_value(spn_toml_edit_parser_t* parser, spn_toml_edit_entry_t* entry, bool nested) {
  c8 c = spn_toml_edit_peek(parser);

  if (c == '"' || c == '\'') {
    entry->kind = SPN_TOML_EDIT_VALUE_STRING;
    if (!spn_toml_edit_scan_string(parser)) {
      return false;
    }
    entry->value.end = parser->it;
    return true;
  }

  if (c == '[') {
    entry->kind = SPN_TOML_EDIT_VALUE_ARRAY;
    if (!spn_toml_edit_scan_array(parser)) {
      return false;
    }
    entry->value.end = parser->it;
    return true;
  }

  if (c == '{') {
    entry->kind = SPN_TOML_EDIT_VALUE_TABLE;
    if (!spn_toml_edit_scan_inline_table(parser, entry)) {
      return false;
    }
    entry->value.end = parser->it;
    return true;
  }

  entry->kind = SPN_TOML_EDIT_VALUE_SCALAR;
  while (!spn_toml_edit_done(parser)) {
    c = spn_toml_edit_peek(parser);
    if (c == '\n' || c == '\r' || c == '#') {
      break;
    }
    if (nested && (c == ',' || c == ']' || c == '}')) {
      break;
    }
    spn_toml_edit_eat(parser);
  }

  u32 end = parser->it;
  while (end > entry->value.start && spn_toml_edit_is_whitespace(parser->edit->source.data[end - 1])) {
    end--;
  }
  entry->value.end = end;

  return end > entry->value.start;
}

static void spn_toml_edit_parse_entry(spn_toml_edit_parser_t* parser, spn_toml_edit_section_t* section) {
  spn_toml_edit_entry_t entry = sp_zero;
  if (!spn_toml_edit_scan_pair(parser, &entry, false) || !spn_toml_edit_skip_line_end(parser)) {
    parser->err = true;
    return;
  }
  entry.line_end = parser->it;

  sp_da_push(section->entries, entry);
}

static void spn_toml_edit_parse_header(spn_toml_edit_parser_t* parser) {
  spn_toml_edit_section_t section = sp_zero;
  section.entries = sp_da_new(parser->edit->mem, spn_toml_edit_entry_t);

  spn_toml_edit_eat(parser);
  section.array = spn_toml_edit_accept(parser, '[');

  section.path = spn_toml_edit_parse_key(parser);
  if (parser->err) {
    return;
  }

  spn_toml_edit_skip_whitespace(parser);
  if (!spn_toml_edit_accept(parser, ']')) {
    parser->err = true;
    return;
  }
  if (section.array && !spn_toml_edit_accept(parser, ']')) {
    parser->err = true;
    return;
  }

  if (!spn_toml_edit_skip_line_end(parser)) {
    parser->err = true;
    return;
  }
  section.content = parser->it;

  sp_da_push(parser->edit->sections, section);
}

spn_err_t spn_toml_edit_init(spn_toml_edit_t* edit, sp_mem_t mem, sp_str_t source) {
  edit->mem = mem;
  edit->source = sp_str_copy(mem, source);
  edit->eol = sp_str_contains(edit->source, sp_str_lit("\r\n")) ? sp_str_lit("\r\n") : sp_str_lit("\n");
  edit->sections = sp_da_new(mem, spn_toml_edit_section_t);
  edit->splices = sp_da_new(mem, spn_toml_edit_splice_t);

  spn_toml_edit_section_t root = sp_zero;
  root.path = sp_da_new(mem, sp_str_t);
  root.entries = sp_da_new(mem, spn_toml_edit_entry_t);
  sp_da_push(edit->sections, root);

  spn_toml_edit_parser_t parser = { .edit = edit };

  while (true) {
    spn_toml_edit_skip_blank(&parser);
    if (spn_toml_edit_done(&parser)) {
      break;
    }

    c8 c = spn_toml_edit_peek(&parser);
    if (c == '#') {
      spn_toml_edit_skip_comment(&parser);
      continue;
    }
    if (c == '[') {
      spn_toml_edit_parse_header(&parser);
    }
    else {
      spn_toml_edit_parse_entry(&parser, sp_da_back(edit->sections));
    }

    if (parser.err) {
      return SPN_ERR_TOML_PARSE;
    }
  }

  return SPN_OK;
}

static bool spn_toml_edit_path_prefix(sp_da(sp_str_t) prefix, const sp_str_t* path, u32 num_segments) {
  if (sp_da_size(prefix) > num_segments) {
    return false;
  }
  sp_da_for(prefix, it) {
    if (!sp_str_equal(prefix[it], path[it])) {
      return false;
    }
  }
  return true;
}

static bool spn_toml_edit_path_starts_with(sp_da(sp_str_t) segments, const sp_str_t* path, u32 num_segments) {
  if (sp_da_size(segments) < num_segments) {
    return false;
  }
  sp_for(it, num_segments) {
    if (!sp_str_equal(segments[it], path[it])) {
      return false;
    }
  }
  return true;
}

static spn_toml_edit_entry_t* spn_toml_edit_find_in(sp_da(spn_toml_edit_entry_t) entries, const sp_str_t* path, u32 num_segments) {
  sp_da_for(entries, it) {
    spn_toml_edit_entry_t* entry = &entries[it];
    if (!spn_toml_edit_path_prefix(entry->path, path, num_segments)) {
      continue;
    }

    u32 len = sp_da_size(entry->path);
    if (len == num_segments) {
      return entry;
    }
    if (entry->kind == SPN_TOML_EDIT_VALUE_TABLE) {
      spn_toml_edit_entry_t* sub = spn_toml_edit_find_in(entry->entries, path + len, num_segments - len);
      if (sub) {
        return sub;
      }
    }
  }
  return SP_NULLPTR;
}

spn_toml_edit_entry_t* spn_toml_edit_find(spn_toml_edit_t* edit, const sp_str_t* path, u32 num_segments) {
  spn_toml_edit_entry_t* found = SP_NULLPTR;
  sp_da_for(edit->sections, it) {
    spn_toml_edit_section_t* section = &edit->sections[it];
    if (!spn_toml_edit_path_prefix(section->path, path, num_segments)) {
      continue;
    }

    u32 len = sp_da_size(section->path);
    if (len == num_segments) {
      continue;
    }

    spn_toml_edit_entry_t* entry = spn_toml_edit_find_in(section->entries, path + len, num_segments - len);
    if (entry) {
      found = entry;
    }
  }
  return found;
}

sp_str_t spn_toml_edit_entry_str(spn_toml_edit_t* edit, spn_toml_edit_entry_t* entry) {
  switch (entry->kind) {
    case SPN_TOML_EDIT_VALUE_SCALAR: {
      return sp_str_sub(edit->source, entry->value.start, entry->value.end - entry->value.start);
    }
    case SPN_TOML_EDIT_VALUE_STRING: {
      break;
    }
    case SPN_TOML_EDIT_VALUE_ARRAY:
    case SPN_TOML_EDIT_VALUE_TABLE: {
      return sp_zero_s(sp_str_t);
    }
  }

  spn_toml_edit_parser_t parser = { .edit = edit, .it = entry->value.start };
  c8 quote = spn_toml_edit_peek(&parser);
  bool multiline = spn_toml_edit_peek_at(&parser, 1) == quote && spn_toml_edit_peek_at(&parser, 2) == quote;

  if (!multiline) {
    if (quote == '\'') {
      return spn_toml_edit_parse_literal(&parser);
    }
    return spn_toml_edit_parse_basic(&parser);
  }

  parser.it += 3;
  if (spn_toml_edit_peek(&parser) == '\r' && spn_toml_edit_peek_at(&parser, 1) == '\n') {
    parser.it += 2;
  }
  else if (spn_toml_edit_peek(&parser) == '\n') {
    parser.it += 1;
  }

  u32 end = entry->value.end - 3;
  if (quote == '\'') {
    return sp_str_sub(edit->source, parser.it, end - parser.it);
  }

  sp_io_dyn_mem_writer_t decoded = sp_zero;
  sp_io_dyn_mem_writer_init(edit->mem, &decoded);
  while (parser.it < end) {
    c8 c = spn_toml_edit_peek(&parser);
    if (c != '\\') {
      sp_io_write_c8(&decoded.base, c);
      spn_toml_edit_eat(&parser);
      continue;
    }

    spn_toml_edit_eat(&parser);
    if (spn_toml_edit_is_blank(spn_toml_edit_peek(&parser))) {
      while (parser.it < end && spn_toml_edit_is_blank(spn_toml_edit_peek(&parser))) {
        spn_toml_edit_eat(&parser);
      }
      continue;
    }
    spn_toml_edit_unescape(&parser, &decoded.base);
  }
  return sp_io_dyn_mem_writer_take_str(&decoded);
}

static bool spn_toml_edit_key_is_bare(sp_str_t key) {
  if (sp_str_empty(key)) {
    return false;
  }
  sp_str_for(key, it) {
    if (!spn_toml_edit_is_bare(key.data[it])) {
      return false;
    }
  }
  return true;
}

static void spn_toml_edit_write_key(sp_io_writer_t* io, sp_str_t key) {
  if (spn_toml_edit_key_is_bare(key)) {
    sp_io_write_str(io, key, SP_NULLPTR);
    return;
  }

  sp_io_write_c8(io, '"');
  sp_str_for(key, it) {
    c8 c = key.data[it];
    if (c == '"' || c == '\\') {
      sp_io_write_c8(io, '\\');
    }
    sp_io_write_c8(io, c);
  }
  sp_io_write_c8(io, '"');
}

static void spn_toml_edit_write_key_path(sp_io_writer_t* io, const sp_str_t* path, u32 num_segments) {
  sp_for(it, num_segments) {
    if (it) {
      sp_io_write_c8(io, '.');
    }
    spn_toml_edit_write_key(io, path[it]);
  }
}

static void spn_toml_edit_write_str_value(sp_io_writer_t* io, sp_str_t value) {
  sp_io_write_c8(io, '"');
  sp_str_for(value, it) {
    c8 c = value.data[it];
    switch (c) {
      case '"': sp_io_write_cstr(io, "\\\"", SP_NULLPTR); break;
      case '\\': sp_io_write_cstr(io, "\\\\", SP_NULLPTR); break;
      case '\b': sp_io_write_cstr(io, "\\b", SP_NULLPTR); break;
      case '\t': sp_io_write_cstr(io, "\\t", SP_NULLPTR); break;
      case '\n': sp_io_write_cstr(io, "\\n", SP_NULLPTR); break;
      case '\f': sp_io_write_cstr(io, "\\f", SP_NULLPTR); break;
      case '\r': sp_io_write_cstr(io, "\\r", SP_NULLPTR); break;
      default: sp_io_write_c8(io, c); break;
    }
  }
  sp_io_write_c8(io, '"');
}

static bool spn_toml_edit_splice(spn_toml_edit_t* edit, u32 at, u32 remove, sp_str_t insert) {
  u32 end = at + remove;
  sp_da_for(edit->splices, it) {
    spn_toml_edit_splice_t* other = &edit->splices[it];
    u32 other_end = other->at + other->remove;

    bool conflict = false;
    if (remove && other->remove) {
      conflict = at < other_end && other->at < end;
    }
    else if (remove) {
      conflict = other->at > at && other->at < end;
    }
    else if (other->remove) {
      conflict = at > other->at && at < other_end;
    }
    if (conflict) {
      return false;
    }
  }

  u32 seq = (u32)sp_da_size(edit->splices);
  sp_da_push(edit->splices, ((spn_toml_edit_splice_t) {
    .at = at,
    .remove = remove,
    .seq = seq,
    .insert = insert,
  }));
  return true;
}

static sp_str_t spn_toml_edit_quoted(spn_toml_edit_t* edit, sp_str_t value) {
  sp_io_dyn_mem_writer_t quoted = sp_zero;
  sp_io_dyn_mem_writer_init(edit->mem, &quoted);
  spn_toml_edit_write_str_value(&quoted.base, value);
  return sp_io_dyn_mem_writer_take_str(&quoted);
}

typedef struct {
  spn_toml_edit_section_t* section;
  u32 at;
  u32 num_matched;
} spn_toml_edit_site_t;

static spn_toml_edit_site_t spn_toml_edit_find_site(spn_toml_edit_t* edit, const sp_str_t* path, u32 num_segments) {
  spn_toml_edit_site_t site = sp_zero;
  bool found = false;

  sp_da_for(edit->sections, it) {
    spn_toml_edit_section_t* section = &edit->sections[it];
    if (section->array) {
      continue;
    }
    if (!spn_toml_edit_path_prefix(section->path, path, num_segments - 1)) {
      continue;
    }

    u32 len = sp_da_size(section->path);
    bool exact = len == num_segments - 1;

    u32 at = section->content;
    bool dotted = false;
    sp_da_for(section->entries, entry_it) {
      spn_toml_edit_entry_t* entry = &section->entries[entry_it];
      if (exact) {
        at = entry->line_end;
      }
      else if (
        spn_toml_edit_path_starts_with(entry->path, path + len, num_segments - 1 - len) &&
        sp_da_size(entry->path) > num_segments - 1 - len
      ) {
        at = entry->line_end;
        dotted = true;
      }
    }

    if (!exact && !dotted) {
      continue;
    }
    if (found && len < site.num_matched) {
      continue;
    }

    found = true;
    site.section = section;
    site.at = at;
    site.num_matched = len;
  }

  return site;
}

spn_err_t spn_toml_edit_set_str(spn_toml_edit_t* edit, const sp_str_t* path, u32 num_segments, sp_str_t value) {
  if (!num_segments) {
    return SPN_ERROR;
  }

  sp_str_t quoted = spn_toml_edit_quoted(edit, value);

  spn_toml_edit_entry_t* entry = spn_toml_edit_find(edit, path, num_segments);
  if (entry) {
    if (!spn_toml_edit_splice(edit, entry->value.start, entry->value.end - entry->value.start, quoted)) {
      return SPN_ERROR;
    }
    return SPN_OK;
  }

  for (u32 len = num_segments - 1; len > 0; len--) {
    spn_toml_edit_entry_t* container = spn_toml_edit_find(edit, path, len);
    if (!container) {
      continue;
    }
    if (container->kind != SPN_TOML_EDIT_VALUE_TABLE) {
      return SPN_ERROR;
    }

    sp_io_dyn_mem_writer_t text = sp_zero;
    sp_io_dyn_mem_writer_init(edit->mem, &text);

    bool spliced = false;
    if (sp_da_empty(container->entries)) {
      sp_io_write_cstr(&text.base, "{ ", SP_NULLPTR);
      spn_toml_edit_write_key_path(&text.base, path + len, num_segments - len);
      sp_fmt_io(&text.base, " = {} }}", sp_fmt_str(quoted));
      spliced = spn_toml_edit_splice(edit, container->value.start, container->value.end - container->value.start, sp_io_dyn_mem_writer_take_str(&text));
    }
    else {
      spn_toml_edit_write_key_path(&text.base, path + len, num_segments - len);
      sp_fmt_io(&text.base, " = {}, ", sp_fmt_str(quoted));
      spliced = spn_toml_edit_splice(edit, container->entries[0].key.start, 0, sp_io_dyn_mem_writer_take_str(&text));
    }

    return spliced ? SPN_OK : SPN_ERROR;
  }

  spn_toml_edit_site_t site = spn_toml_edit_find_site(edit, path, num_segments);
  if (site.section) {
    sp_io_dyn_mem_writer_t text = sp_zero;
    sp_io_dyn_mem_writer_init(edit->mem, &text);

    if (site.at && edit->source.data[site.at - 1] != '\n') {
      sp_io_write_str(&text.base, edit->eol, SP_NULLPTR);
    }
    spn_toml_edit_write_key_path(&text.base, path + site.num_matched, num_segments - site.num_matched);
    sp_fmt_io(&text.base, " = {}{}", sp_fmt_str(quoted), sp_fmt_str(edit->eol));

    if (!spn_toml_edit_splice(edit, site.at, 0, sp_io_dyn_mem_writer_take_str(&text))) {
      return SPN_ERROR;
    }
    return SPN_OK;
  }

  sp_da_for(edit->sections, it) {
    spn_toml_edit_section_t* section = &edit->sections[it];
    if (!section->array) {
      continue;
    }
    if (sp_da_size(section->path) != num_segments - 1) {
      continue;
    }
    if (spn_toml_edit_path_prefix(section->path, path, num_segments - 1)) {
      return SPN_ERROR;
    }
  }

  sp_io_dyn_mem_writer_t text = sp_zero;
  sp_io_dyn_mem_writer_init(edit->mem, &text);

  if (!sp_str_empty(edit->source)) {
    if (!sp_str_ends_with(edit->source, sp_str_lit("\n"))) {
      sp_io_write_str(&text.base, edit->eol, SP_NULLPTR);
      sp_io_write_str(&text.base, edit->eol, SP_NULLPTR);
    }
    else {
      u32 end = edit->source.len - 1;
      if (end && edit->source.data[end - 1] == '\r') {
        end--;
      }
      if (end && edit->source.data[end - 1] != '\n') {
        sp_io_write_str(&text.base, edit->eol, SP_NULLPTR);
      }
    }
  }

  sp_io_write_c8(&text.base, '[');
  spn_toml_edit_write_key_path(&text.base, path, num_segments - 1);
  sp_fmt_io(&text.base, "]{}", sp_fmt_str(edit->eol));
  spn_toml_edit_write_key(&text.base, path[num_segments - 1]);
  sp_fmt_io(&text.base, " = {}{}", sp_fmt_str(quoted), sp_fmt_str(edit->eol));

  if (!spn_toml_edit_splice(edit, edit->source.len, 0, sp_io_dyn_mem_writer_take_str(&text))) {
    return SPN_ERROR;
  }
  return SPN_OK;
}

static s32 spn_toml_edit_splice_sort(const void* a, const void* b) {
  const spn_toml_edit_splice_t* lhs = (const spn_toml_edit_splice_t*)a;
  const spn_toml_edit_splice_t* rhs = (const spn_toml_edit_splice_t*)b;
  if (lhs->at != rhs->at) {
    return lhs->at < rhs->at ? -1 : 1;
  }
  if (lhs->seq != rhs->seq) {
    return lhs->seq < rhs->seq ? -1 : 1;
  }
  return 0;
}

sp_str_t spn_toml_edit_render(spn_toml_edit_t* edit, sp_mem_t mem) {
  sp_da_sort(edit->splices, spn_toml_edit_splice_sort);

  sp_io_dyn_mem_writer_t out = sp_zero;
  sp_io_dyn_mem_writer_init(mem, &out);

  u32 cursor = 0;
  sp_da_for(edit->splices, it) {
    spn_toml_edit_splice_t* splice = &edit->splices[it];
    SP_ASSERT(splice->at >= cursor);
    sp_io_write(&out.base, edit->source.data + cursor, splice->at - cursor, SP_NULLPTR);
    sp_io_write_str(&out.base, splice->insert, SP_NULLPTR);
    cursor = splice->at + splice->remove;
  }
  sp_io_write(&out.base, edit->source.data + cursor, edit->source.len - cursor, SP_NULLPTR);

  return sp_io_dyn_mem_writer_take_str(&out);
}
