#include "compiler.h"

#define toc_syms_max 4

typedef enum {
  TOC_AR_GNU,
  TOC_AR_GNU64,
  TOC_AR_BSD,
  TOC_AR_BSD_SORTED,
  TOC_AR_THIN,
  TOC_AR_COFF,
  TOC_AR_NO_SYMTAB,
  TOC_AR_EMPTY,
  TOC_AR_BAD_MAGIC,
  TOC_AR_TRUNCATED,
} toc_format_t;

typedef struct {
  spn_err_t err;
  const c8* symbols [toc_syms_max];
} toc_expect_t;

typedef struct {
  const c8* name;
  toc_format_t format;
  const c8* symbols [toc_syms_max];
  toc_expect_t expect;
} toc_test_t;

typedef sp_io_dyn_mem_writer_t ar_buf_t;

static void ar_bytes(ar_buf_t* b, const void* ptr, u32 len) {
  sp_io_write(&b->base, ptr, len, SP_NULLPTR);
}

static void ar_cstr(ar_buf_t* b, const c8* s) {
  sp_io_write_cstr(&b->base, s, SP_NULLPTR);
}

static void ar_u32be(ar_buf_t* b, u32 v) {
  u8 p [4] = { (u8)(v >> 24), (u8)(v >> 16), (u8)(v >> 8), (u8)v };
  ar_bytes(b, p, 4);
}

static void ar_u64be(ar_buf_t* b, u64 v) {
  ar_u32be(b, (u32)(v >> 32));
  ar_u32be(b, (u32)v);
}

static void ar_u32le(ar_buf_t* b, u32 v) {
  u8 p [4] = { (u8)v, (u8)(v >> 8), (u8)(v >> 16), (u8)(v >> 24) };
  ar_bytes(b, p, 4);
}

static void ar_pad_even(ar_buf_t* b) {
  u64 size = 0;
  sp_io_dyn_mem_writer_size(b, &size);
  if (size & 1) {
    ar_cstr(b, "\n");
  }
}

static void ar_field(c8* hdr, u32 offset, u32 width, sp_str_t value) {
  memcpy(hdr + offset, value.data, sp_min(value.len, width));
}

static void ar_header(ar_buf_t* b, const c8* name, u32 size) {
  c8 hdr [60];
  memset(hdr, ' ', 60);
  ar_field(hdr, 0, 16, sp_str_view(name));
  ar_field(hdr, 16, 12, sp_str_lit("0"));
  ar_field(hdr, 28, 6, sp_str_lit("0"));
  ar_field(hdr, 34, 6, sp_str_lit("0"));
  ar_field(hdr, 40, 8, sp_str_lit("644"));
  ar_field(hdr, 48, 10, sp_fmt(b->allocator, "{}", sp_fmt_uint(size)).value);
  hdr[58] = 0x60;
  hdr[59] = '\n';
  ar_bytes(b, hdr, 60);
}

static u32 names_size(const c8* const* symbols, u32 n) {
  u32 size = 0;
  sp_for(it, n) {
    size += sp_cstr_len(symbols[it]) + 1;
  }
  return size;
}

static void ar_names(ar_buf_t* b, const c8* const* symbols, u32 n) {
  sp_for(it, n) {
    ar_bytes(b, symbols[it], sp_cstr_len(symbols[it]) + 1);
  }
}

static void ar_object(ar_buf_t* b) {
  ar_header(b, "A.o/", 4);
  ar_cstr(b, "OBJ\n");
}

static void build_gnu(ar_buf_t* b, const c8* const* symbols, u32 n, bool wide, bool thin) {
  u32 word = wide ? 8 : 4;
  u32 symtab_size = word + word * n + names_size(symbols, n);
  u32 object_offset = 8 + 60 + symtab_size + (symtab_size & 1);

  ar_cstr(b, thin ? "!<thin>\n" : "!<arch>\n");
  ar_header(b, wide ? "/SYM64/" : "/", symtab_size);
  if (wide) {
    ar_u64be(b, n);
    sp_for(it, n) { ar_u64be(b, object_offset); }
  } else {
    ar_u32be(b, n);
    sp_for(it, n) { ar_u32be(b, object_offset); }
  }
  ar_names(b, symbols, n);
  ar_pad_even(b);
  if (thin) {
    ar_header(b, "A.o/", 4);
  } else {
    ar_object(b);
  }
}

static void build_bsd(ar_buf_t* b, const c8* const* symbols, u32 n, bool sorted) {
  u32 name_size = sorted ? 20 : 0;
  u32 strtab_size = names_size(symbols, n);
  u32 symdef_size = name_size + 4 + 8 * n + 4 + strtab_size;
  u32 object_offset = 8 + 60 + symdef_size + (symdef_size & 1);

  ar_cstr(b, "!<arch>\n");
  ar_header(b, sorted ? "#1/20" : "__.SYMDEF", symdef_size);
  if (sorted) {
    ar_bytes(b, "__.SYMDEF SORTED\0\0\0\0", 20);
  }
  ar_u32le(b, 8 * n);
  u32 strx = 0;
  sp_for(it, n) {
    ar_u32le(b, strx);
    ar_u32le(b, object_offset);
    strx += sp_cstr_len(symbols[it]) + 1;
  }
  ar_u32le(b, strtab_size);
  ar_names(b, symbols, n);
  ar_pad_even(b);
  ar_object(b);
}

static void build_coff(ar_buf_t* b, const c8* const* symbols, u32 n) {
  u32 first_size = 4 + 4 * n + names_size(symbols, n);
  u32 first_end = 8 + 60 + first_size + (first_size & 1);
  u32 object_offset = first_end + 60 + 4;

  ar_cstr(b, "!<arch>\n");
  ar_header(b, "/", first_size);
  ar_u32be(b, n);
  sp_for(it, n) { ar_u32be(b, object_offset); }
  ar_names(b, symbols, n);
  ar_pad_even(b);
  ar_header(b, "/", 4);
  ar_u32le(b, 0);
  ar_object(b);
}

static void build_archive(ar_buf_t* b, toc_format_t format, const c8* const* symbols, u32 n) {
  switch (format) {
    case TOC_AR_GNU: {
      build_gnu(b, symbols, n, false, false);
      break;
    }
    case TOC_AR_GNU64: {
      build_gnu(b, symbols, n, true, false);
      break;
    }
    case TOC_AR_THIN: {
      build_gnu(b, symbols, n, false, true);
      break;
    }
    case TOC_AR_BSD: {
      build_bsd(b, symbols, n, false);
      break;
    }
    case TOC_AR_BSD_SORTED: {
      build_bsd(b, symbols, n, true);
      break;
    }
    case TOC_AR_COFF: {
      build_coff(b, symbols, n);
      break;
    }
    case TOC_AR_NO_SYMTAB: {
      ar_cstr(b, "!<arch>\n");
      ar_object(b);
      break;
    }
    case TOC_AR_EMPTY: {
      ar_cstr(b, "!<arch>\n");
      break;
    }
    case TOC_AR_BAD_MAGIC: {
      ar_cstr(b, "!<arch~\nnot an archive");
      break;
    }
    case TOC_AR_TRUNCATED: {
      ar_cstr(b, "!<arch>\n");
      ar_header(b, "/", 100);
      ar_u32be(b, 1);
      break;
    }
  }
}

static const toc_test_t tests [] = {
  {
    .name = "gnu",
    .format = TOC_AR_GNU,
    .symbols = { "A", "B" },
    .expect = {
      .symbols = { "A", "B" },
    },
  },
  {
    .name = "gnu_sym64",
    .format = TOC_AR_GNU64,
    .symbols = { "A", "B", "C" },
    .expect = {
      .symbols = { "A", "B", "C" },
    },
  },
  {
    .name = "gnu_no_symbols",
    .format = TOC_AR_GNU,
  },
  {
    .name = "thin",
    .format = TOC_AR_THIN,
    .symbols = { "A" },
    .expect = {
      .symbols = { "A" },
    },
  },
  {
    .name = "bsd",
    .format = TOC_AR_BSD,
    .symbols = { "A", "B" },
    .expect = {
      .symbols = { "A", "B" },
    },
  },
  {
    .name = "bsd_sorted_extended_name",
    .format = TOC_AR_BSD_SORTED,
    .symbols = { "A", "B" },
    .expect = {
      .symbols = { "A", "B" },
    },
  },
  {
    .name = "coff_second_linker_member_ignored",
    .format = TOC_AR_COFF,
    .symbols = { "A", "B" },
    .expect = {
      .symbols = { "A", "B" },
    },
  },
  {
    .name = "empty_archive",
    .format = TOC_AR_EMPTY,
  },
  {
    .name = "missing_symtab",
    .format = TOC_AR_NO_SYMTAB,
    .expect = {
      .err = SPN_ERR_TOC_MISSING,
    },
  },
  {
    .name = "bad_magic",
    .format = TOC_AR_BAD_MAGIC,
    .expect = {
      .err = SPN_ERR_TOC_MAGIC,
    },
  },
  {
    .name = "truncated_symtab",
    .format = TOC_AR_TRUNCATED,
    .expect = {
      .err = SPN_ERR_TOC_TRUNCATED,
    },
  },
};

sp_test_each(toc, parse, toc_test_t, tests) {
  sp_mem_t mem = sp_test_arena(t);

  ar_buf_t buf = sp_zero;
  sp_io_dyn_mem_writer_init(mem, &buf);
  u32 n = 0;
  sp_carr_detect_len(it->symbols, n, it->symbols[n]);
  build_archive(&buf, it->format, it->symbols, n);

  sp_str_t bytes = sp_io_dyn_mem_writer_as_str(&buf);
  sp_io_reader_t reader = sp_zero;
  sp_io_reader_from_mem(&reader, bytes.data, bytes.len);

  spn_toc_parser_t toc;
  spn_err_t err = spn_toc_init(&toc, &reader);

  sp_da(sp_str_t) symbols = sp_da_new(mem, sp_str_t);
  sp_str_t symbol = sp_zero;
  while (spn_toc_next(&toc, &symbol)) {
    sp_da_push(symbols, sp_str_copy(mem, symbol));
  }
  if (!err) {
    err = toc.err;
  }
  sp_expect_eq(t, err, it->expect.err);

  if (!it->expect.err) {
    sp_must_strs_eq(t, symbols, sp_da_size(symbols), it->expect.symbols);
  }

  return SP_OK;
}
