#include "spn_test.h"

#include "elf/elf.h"
#include "elf_emit.h"

typedef struct {
  const c8* value;
  bool malformed;
} interp_expect_t;

typedef struct {
  const c8* name;
  elf_spec_t elf;
  interp_expect_t expect;
} interp_t;

static const interp_t interp_tests [] = {
  { .name = "gnu_loader",        .elf = { .interp = "/lib64/ld-linux-x86-64.so.2" }, .expect = { "/lib64/ld-linux-x86-64.so.2" } },
  { .name = "musl_loader",       .elf = { .interp = "/lib/ld-musl-x86_64.so.1" }, .expect = { "/lib/ld-musl-x86_64.so.1" } },
  { .name = "interp_after_load", .elf = { .interp = "/lib/ld-musl-x86_64.so.1", .load_first = true }, .expect = { "/lib/ld-musl-x86_64.so.1" } },
  { .name = "static_binary" },
  { .name = "bad_magic",         .elf = { .interp = "/lib64/ld-linux-x86-64.so.2", .bad_magic = true }, .expect = { .malformed = true } },
  { .name = "elf32_rejected",    .elf = { .interp = "/lib64/ld-linux-x86-64.so.2", .elf32 = true }, .expect = { .malformed = true } },
  { .name = "truncated_phdrs",   .elf = { .interp = "/lib64/ld-linux-x86-64.so.2", .truncated = true }, .expect = { .malformed = true } },
};

sp_test_each(elf, interp, interp_t, interp_tests) {
  sp_str_t elf = elf_emit(sp_test_arena(t), &it->elf);
  sp_io_reader_t backing = sp_zero;
  sp_io_seeking_reader_t reader = elf_reader(&backing, elf);
  sp_str_t interp = sp_zero;
  spn_err_t err = spn_elf_interp(sp_test_arena(t), &reader, &interp);
  sp_expect_eq(t, (u32)(it->expect.malformed ? SPN_ERROR : SPN_OK), (u32)err);
  if (!err) {
    sp_expect_str_eq_c(t, interp, it->expect.value ? it->expect.value : "");
  }
  return SP_OK;
}

typedef struct {
  u64 value;
  bool malformed;
} entry_expect_t;

typedef struct {
  const c8* name;
  elf_spec_t elf;
  entry_expect_t expect;
} entry_t;

static const entry_t entry_tests [] = {
  { .name = "reads_entry",    .elf = { .entry = 0x400000 }, .expect = { .value = 0x400000 } },
  { .name = "zero_entry" },
  { .name = "bad_magic",      .elf = { .entry = 0x400000, .bad_magic = true }, .expect = { .malformed = true } },
  { .name = "elf32_rejected", .elf = { .entry = 0x400000, .elf32 = true }, .expect = { .malformed = true } },
};

sp_test_each(elf, entry, entry_t, entry_tests) {
  sp_str_t elf = elf_emit(sp_test_arena(t), &it->elf);
  sp_io_reader_t backing = sp_zero;
  sp_io_seeking_reader_t reader = elf_reader(&backing, elf);
  u64 entry = 0;
  spn_err_t err = spn_elf_entry(&reader, &entry);
  sp_expect_eq(t, (u32)(it->expect.malformed ? SPN_ERROR : SPN_OK), (u32)err);
  if (!err) {
    sp_expect_eq(t, it->expect.value, entry);
  }
  return SP_OK;
}

typedef struct {
  bool value;
  bool malformed;
} defines_expect_t;

typedef struct {
  const c8* name;
  elf_spec_t elf;
  const c8* prefix;
  defines_expect_t expect;
} defines_t;

static const defines_t defines_tests [] = {
  { .name = "defined_symbol",    .elf = { .symbols = { "A", "BC" } },                .prefix = "B", .expect = { .value = true } },
  { .name = "prefix_not_substr", .elf = { .symbols = { "A", "BC" } },                .prefix = "C" },
  { .name = "undefined_ignored", .elf = { .symbols = { "A" }, .undefined = { "B" } }, .prefix = "B" },
  { .name = "no_symbol_table",                                                       .prefix = "B", .expect = { .malformed = true } },
  { .name = "bad_magic",         .elf = { .symbols = { "B" }, .bad_magic = true },    .prefix = "B", .expect = { .malformed = true } },
  { .name = "name_past_strtab",  .elf = { .symbols = { "B" }, .bad_name = true },     .prefix = "B", .expect = { .malformed = true } },
};

sp_test_each(elf, defines_prefix, defines_t, defines_tests) {
  sp_str_t elf = elf_emit(sp_test_arena(t), &it->elf);
  sp_io_reader_t backing = sp_zero;
  sp_io_seeking_reader_t reader = elf_reader(&backing, elf);
  bool defined = false;
  spn_err_t err = spn_elf_defines_prefix(&reader, sp_cstr_as_str(it->prefix), &defined);
  sp_expect_eq(t, (u32)(it->expect.malformed ? SPN_ERROR : SPN_OK), (u32)err);
  if (!err) {
    sp_expect_eq(t, it->expect.value, defined);
  }
  return SP_OK;
}
