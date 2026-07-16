#include "common.h"
#include "compiler/toc.h"

#define toc_syms_max 4
#define toc_buf_max 4096

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
  toc_format_t format;
  const c8* symbols [toc_syms_max];
  toc_expect_t expect;
} toc_test_t;

typedef struct {
  u8 data [toc_buf_max];
  u32 len;
} ar_buf_t;

static void ar_bytes(ar_buf_t* b, const void* ptr, u32 len) {
  memcpy(b->data + b->len, ptr, len);
  b->len += len;
}

static void ar_cstr(ar_buf_t* b, const c8* s) {
  ar_bytes(b, s, (u32)strlen(s));
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
  if (b->len & 1) {
    ar_cstr(b, "\n");
  }
}

static void ar_field(c8* hdr, u32 offset, u32 width, const c8* value) {
  u32 len = (u32)strlen(value);
  memcpy(hdr + offset, value, len < width ? len : width);
}

static void ar_header(ar_buf_t* b, const c8* name, u32 size) {
  c8 hdr [60];
  memset(hdr, ' ', 60);
  ar_field(hdr, 0, 16, name);
  ar_field(hdr, 16, 12, "0");
  ar_field(hdr, 28, 6, "0");
  ar_field(hdr, 34, 6, "0");
  ar_field(hdr, 40, 8, "644");
  c8 size_str [11];
  snprintf(size_str, sizeof(size_str), "%u", size);
  ar_field(hdr, 48, 10, size_str);
  hdr[58] = 0x60;
  hdr[59] = '\n';
  ar_bytes(b, hdr, 60);
}

static u32 count_symbols(const c8* const* symbols) {
  u32 n = 0;
  while (n < toc_syms_max && symbols[n]) {
    n++;
  }
  return n;
}

static u32 names_size(const c8* const* symbols, u32 n) {
  u32 size = 0;
  sp_for(it, n) {
    size += (u32)strlen(symbols[it]) + 1;
  }
  return size;
}

static void ar_names(ar_buf_t* b, const c8* const* symbols, u32 n) {
  sp_for(it, n) {
    ar_bytes(b, symbols[it], (u32)strlen(symbols[it]) + 1);
  }
}

static void ar_object(ar_buf_t* b) {
  ar_header(b, "A.o/", 4);
  ar_cstr(b, "OBJ\n");
}

static void build_gnu(ar_buf_t* b, const c8* const* symbols, bool wide, bool thin) {
  u32 n = count_symbols(symbols);
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

static void build_bsd(ar_buf_t* b, const c8* const* symbols, bool sorted) {
  u32 n = count_symbols(symbols);
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
    strx += (u32)strlen(symbols[it]) + 1;
  }
  ar_u32le(b, strtab_size);
  ar_names(b, symbols, n);
  ar_pad_even(b);
  ar_object(b);
}

static void build_coff(ar_buf_t* b, const c8* const* symbols) {
  u32 n = count_symbols(symbols);
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

static void build_archive(ar_buf_t* b, toc_format_t format, const c8* const* symbols) {
  switch (format) {
    case TOC_AR_GNU: {
      build_gnu(b, symbols, false, false);
      break;
    }
    case TOC_AR_GNU64: {
      build_gnu(b, symbols, true, false);
      break;
    }
    case TOC_AR_THIN: {
      build_gnu(b, symbols, false, true);
      break;
    }
    case TOC_AR_BSD: {
      build_bsd(b, symbols, false);
      break;
    }
    case TOC_AR_BSD_SORTED: {
      build_bsd(b, symbols, true);
      break;
    }
    case TOC_AR_COFF: {
      build_coff(b, symbols);
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

static void run_toc_test(s32* utest_result, toc_test_t t) {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();

  ar_buf_t buf = sp_zero;
  build_archive(&buf, t.format, t.symbols);

  spn_toc_t toc = sp_zero;
  spn_err_union_t err = spn_toc_read(scratch.mem, sp_str((const c8*)buf.data, buf.len), &toc);
  EXPECT_EQ(err.kind, t.expect.err);

  if (!t.expect.err) {
    u32 count = count_symbols(t.expect.symbols);
    ASSERT_EQ(sp_da_size(toc.symbols), count);
    sp_for(it, count) {
      utest_kv("symbol", sp_str_view(t.expect.symbols[it]));
      EXPECT_TRUE(sp_str_equal_cstr(toc.symbols[it], t.expect.symbols[it]));
    }
  }

  sp_mem_end_scratch(scratch);
}

UTEST(toc, gnu) {
  UTEST_SKIP("");
  run_toc_test(utest_result, (toc_test_t) {
    .format = TOC_AR_GNU,
    .symbols = { "A", "B" },
    .expect = {
      .symbols = { "A", "B" },
    },
  });
}

UTEST(toc, gnu_sym64) {
  UTEST_SKIP("");
  run_toc_test(utest_result, (toc_test_t) {
    .format = TOC_AR_GNU64,
    .symbols = { "A", "B", "C" },
    .expect = {
      .symbols = { "A", "B", "C" },
    },
  });
}

UTEST(toc, gnu_no_symbols) {
  UTEST_SKIP("");
  run_toc_test(utest_result, (toc_test_t) {
    .format = TOC_AR_GNU,
  });
}

UTEST(toc, thin) {
  UTEST_SKIP("");
  run_toc_test(utest_result, (toc_test_t) {
    .format = TOC_AR_THIN,
    .symbols = { "A" },
    .expect = {
      .symbols = { "A" },
    },
  });
}

UTEST(toc, bsd) {
  UTEST_SKIP("");
  run_toc_test(utest_result, (toc_test_t) {
    .format = TOC_AR_BSD,
    .symbols = { "A", "B" },
    .expect = {
      .symbols = { "A", "B" },
    },
  });
}

UTEST(toc, bsd_sorted_extended_name) {
  UTEST_SKIP("");
  run_toc_test(utest_result, (toc_test_t) {
    .format = TOC_AR_BSD_SORTED,
    .symbols = { "A", "B" },
    .expect = {
      .symbols = { "A", "B" },
    },
  });
}

UTEST(toc, coff_second_linker_member_ignored) {
  UTEST_SKIP("");
  run_toc_test(utest_result, (toc_test_t) {
    .format = TOC_AR_COFF,
    .symbols = { "A", "B" },
    .expect = {
      .symbols = { "A", "B" },
    },
  });
}

UTEST(toc, empty_archive) {
  UTEST_SKIP("");
  run_toc_test(utest_result, (toc_test_t) {
    .format = TOC_AR_EMPTY,
  });
}

UTEST(toc, missing_symtab) {
  UTEST_SKIP("");
  run_toc_test(utest_result, (toc_test_t) {
    .format = TOC_AR_NO_SYMTAB,
    .expect = {
      .err = SPN_ERR_TOC_MISSING,
    },
  });
}

UTEST(toc, bad_magic) {
  UTEST_SKIP("");
  run_toc_test(utest_result, (toc_test_t) {
    .format = TOC_AR_BAD_MAGIC,
    .expect = {
      .err = SPN_ERR_TOC_MAGIC,
    },
  });
}

UTEST(toc, truncated_symtab) {
  UTEST_SKIP("");
  run_toc_test(utest_result, (toc_test_t) {
    .format = TOC_AR_TRUNCATED,
    .expect = {
      .err = SPN_ERR_TOC_TRUNCATED,
    },
  });
}
