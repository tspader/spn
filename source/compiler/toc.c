#include "compiler/toc.h"

#include "sp/macro.h"

static u32 toc_u32be(const u8* p) {
  return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | (u32)p[3];
}

static u64 toc_u64be(const u8* p) {
  return ((u64)toc_u32be(p) << 32) | (u64)toc_u32be(p + 4);
}

static u32 toc_u32le(const u8* p) {
  return ((u32)p[3] << 24) | ((u32)p[2] << 16) | ((u32)p[1] << 8) | (u32)p[0];
}

static sp_str_t toc_field(const u8* p, u32 width) {
  while (width && (p[width - 1] == ' ' || p[width - 1] == 0)) {
    width--;
  }
  return sp_str((const c8*)p, width);
}

static bool toc_parse_u64(sp_str_t field, u64* value) {
  if (sp_str_empty(field)) {
    return false;
  }
  u64 result = 0;
  sp_for(it, field.len) {
    c8 c = field.data[it];
    if (c < '0' || c > '9') {
      return false;
    }
    result = result * 10 + (u64)(c - '0');
  }
  *value = result;
  return true;
}

static spn_err_t toc_set(spn_toc_parser_t* toc, spn_err_t err) {
  toc->err = err;
  return err;
}

static spn_err_t toc_init_gnu(spn_toc_parser_t* toc, u64 body_len, u32 word) {
  u8 field [8];
  u64 bytes = 0;
  if (sp_io_read_all(&toc->body.base, field, word, &bytes) || bytes < word) {
    return SPN_ERR_TOC_TRUNCATED;
  }
  u64 count = word == 8 ? toc_u64be(field) : toc_u32be(field);
  if (count > body_len / word) {
    return SPN_ERR_TOC_TRUNCATED;
  }
  u64 discarded = 0;
  if (sp_io_discard(&toc->body.base, count * word, &discarded) || discarded < count * word) {
    return SPN_ERR_TOC_TRUNCATED;
  }
  toc->format = SPN_TOC_FORMAT_GNU;
  toc->count = count;
  return SPN_OK;
}

static spn_err_t toc_init_bsd(spn_toc_parser_t* toc, u64 body_len) {
  u8 field [4];
  u64 bytes = 0;
  if (sp_io_read_all(&toc->body.base, field, 4, &bytes) || bytes < 4) {
    return SPN_ERR_TOC_TRUNCATED;
  }
  u64 ranlib_bytes = toc_u32le(field);
  if (body_len < 8 || ranlib_bytes > body_len - 8) {
    return SPN_ERR_TOC_TRUNCATED;
  }
  u64 discarded = 0;
  if (sp_io_discard(&toc->body.base, ranlib_bytes, &discarded) || discarded < ranlib_bytes) {
    return SPN_ERR_TOC_TRUNCATED;
  }
  if (sp_io_read_all(&toc->body.base, field, 4, &bytes) || bytes < 4) {
    return SPN_ERR_TOC_TRUNCATED;
  }
  u64 strtab_size = toc_u32le(field);
  if (strtab_size > body_len - 8 - ranlib_bytes) {
    return SPN_ERR_TOC_TRUNCATED;
  }
  toc->format = SPN_TOC_FORMAT_BSD;
  toc->remaining = strtab_size;
  return SPN_OK;
}

spn_err_t spn_toc_init(spn_toc_parser_t* toc, sp_io_reader_t* io) {
  *toc = sp_zero_s(spn_toc_parser_t);

  u8 magic [8];
  u64 bytes = 0;
  if (sp_io_read_all(io, magic, 8, &bytes) || bytes < 8) {
    return toc_set(toc, SPN_ERR_TOC_MAGIC);
  }
  if (memcmp(magic, "!<arch>\n", 8) && memcmp(magic, "!<thin>\n", 8)) {
    return toc_set(toc, SPN_ERR_TOC_MAGIC);
  }

  u8 header [60];
  sp_err_t io_err = sp_io_read_all(io, header, 60, &bytes);
  if (io_err == SP_ERR_IO_EOF && !bytes) {
    return SPN_OK;
  }
  if (io_err || bytes < 60) {
    return toc_set(toc, SPN_ERR_TOC_TRUNCATED);
  }
  if (header[58] != 0x60 || header[59] != '\n') {
    return toc_set(toc, SPN_ERR_TOC_TRUNCATED);
  }

  sp_str_t name = toc_field(header, 16);
  u64 size = 0;
  if (!toc_parse_u64(toc_field(header + 48, 10), &size)) {
    return toc_set(toc, SPN_ERR_TOC_TRUNCATED);
  }

  sp_io_limit_reader_init(&toc->body, io, size);
  sp_io_reader_set_buffer(&toc->body.base, toc->buf, sizeof(toc->buf));

  u64 body_len = size;
  u8 long_name [64];
  if (sp_str_starts_with(name, sp_str_lit("#1/"))) {
    u64 name_len = 0;
    if (!toc_parse_u64(sp_str_sub(name, 3, name.len - 3), &name_len) || name_len > size) {
      return toc_set(toc, SPN_ERR_TOC_TRUNCATED);
    }
    u64 read_len = sp_min(name_len, sizeof(long_name));
    if (sp_io_read_all(&toc->body.base, long_name, read_len, &bytes) || bytes < read_len) {
      return toc_set(toc, SPN_ERR_TOC_TRUNCATED);
    }
    u64 discarded = 0;
    if (sp_io_discard(&toc->body.base, name_len - read_len, &discarded) || discarded < name_len - read_len) {
      return toc_set(toc, SPN_ERR_TOC_TRUNCATED);
    }
    name = toc_field(long_name, (u32)read_len);
    body_len = size - name_len;
  }

  if (sp_str_equal_cstr(name, "/")) {
    return toc_set(toc, toc_init_gnu(toc, body_len, 4));
  }
  if (sp_str_equal_cstr(name, "/SYM64/")) {
    return toc_set(toc, toc_init_gnu(toc, body_len, 8));
  }
  if (sp_str_starts_with(name, sp_str_lit("__.SYMDEF"))) {
    return toc_set(toc, toc_init_bsd(toc, body_len));
  }
  return toc_set(toc, SPN_ERR_TOC_MISSING);
}

bool spn_toc_next(spn_toc_parser_t* toc, sp_str_t* symbol) {
  if (toc->err) {
    return false;
  }

  switch (toc->format) {
    case SPN_TOC_FORMAT_NONE: {
      return false;
    }
    case SPN_TOC_FORMAT_GNU: {
      if (!toc->count) {
        return false;
      }
      sp_str_t view = sp_zero;
      if (sp_io_peek_until(&toc->body.base, sp_str_lit("\0"), &view)) {
        toc->err = SPN_ERR_TOC_TRUNCATED;
        return false;
      }
      *symbol = sp_str(view.data, view.len - 1);
      sp_io_consume(&toc->body.base, view.len);
      toc->count--;
      return true;
    }
    case SPN_TOC_FORMAT_BSD: {
      while (toc->remaining) {
        sp_str_t view = sp_zero;
        if (sp_io_peek_until(&toc->body.base, sp_str_lit("\0"), &view) || view.len > toc->remaining) {
          toc->err = SPN_ERR_TOC_TRUNCATED;
          return false;
        }
        sp_io_consume(&toc->body.base, view.len);
        toc->remaining -= view.len;
        if (view.len == 1) {
          continue;
        }
        *symbol = sp_str(view.data, view.len - 1);
        return true;
      }
      return false;
    }
  }
  sp_unreachable_return(false);
}
