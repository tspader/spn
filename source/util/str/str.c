#include "str/str.h"

bool sp_str_iequal(sp_str_t a, sp_str_t b) {
  if (a.len != b.len) {
    return false;
  }
  sp_str_for(a, it) {
    if (sp_c8_to_lower(a.data[it]) != sp_c8_to_lower(b.data[it])) {
      return false;
    }
  }
  return true;
}

bool sp_str_line_it_valid(const sp_str_line_it_t* it) {
  return !it->done;
}

void sp_str_line_it_next(sp_str_line_it_t* it) {
  if (it->cursor >= it->str.len) {
    it->done = true;
    it->line = sp_zero_s(sp_str_t);
    return;
  }

  u32 start = it->cursor;
  while (it->cursor < it->str.len && it->str.data[it->cursor] != '\n') {
    it->cursor++;
  }

  it->index = start;
  it->line = sp_str_sub(it->str, start, it->cursor - start);

  if (it->cursor < it->str.len) {
    it->cursor++;
  }
}

sp_str_line_it_t sp_str_line_it_begin(sp_str_t str) {
  sp_str_line_it_t it = {
    .str = str,
    .line = sp_zero_s(sp_str_t),
    .index = 0,
    .cursor = 0,
    .done = sp_str_empty(str),
  };

  if (!it.done) {
    sp_str_line_it_next(&it);
  }

  return it;
}

bool sp_str_word_it_valid(const sp_str_word_it_t* it) {
  return !sp_str_empty(it->entry);
}

void sp_str_word_it_next(sp_str_word_it_t* it) {
  it->entry = sp_zero_s(sp_str_t);
  while (!sp_str_empty(it->remaining) && sp_str_empty(it->entry)) {
    s32 sep = sp_str_find_c8(it->remaining, it->sep);
    if (sep == SP_STR_NO_MATCH) {
      it->entry = it->remaining;
      it->remaining = sp_zero_s(sp_str_t);
    }
    else {
      it->entry = sp_str_prefix(it->remaining, sep);
      it->remaining = sp_str_suffix(it->remaining, it->remaining.len - sep - 1);
    }
  }
}

sp_str_word_it_t sp_str_word_it_begin(sp_str_t str, c8 sep) {
  sp_str_word_it_t it = { .remaining = str, .sep = sep };
  sp_str_word_it_next(&it);
  return it;
}

sp_hash_t sp_hash_str(sp_str_t str) {
  return sp_hash_bytes(str.data, str.len, 0);
}

sp_str_t sp_str_repeat(sp_mem_t mem, c8 c, u32 len) {
  if (!len) {
    return sp_zero_s(sp_str_t);
  }

  c8* buffer = (c8*)sp_alloc(mem, len);
  sp_mem_fill(buffer, len, &c, sizeof(c8));
  return sp_str(buffer, len);
}
