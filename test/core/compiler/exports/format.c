#include "../compiler.h"
#include "toolchain/linker.h"

typedef struct {
  spn_cc_exports_format_t format;
  const c8* extension;
} format_expect_t;

typedef struct {
  const c8* name;
  spn_cc_output_kind_t kind;
  spn_format_t format;
  format_expect_t expect;
} format_test_t;

static const format_test_t tests [] = {
  {
    .name = "elf_shared",
    .kind = SPN_CC_OUTPUT_SHARED_LIB,
    .format = SPN_FORMAT_ELF,
    .expect = { .format = SPN_CC_EXPORTS_VERSION_SCRIPT, .extension = "map" },
  },
  {
    .name = "wasm_shared",
    .kind = SPN_CC_OUTPUT_SHARED_LIB,
    .format = SPN_FORMAT_WASM,
    .expect = { .format = SPN_CC_EXPORTS_VERSION_SCRIPT, .extension = "map" },
  },
  {
    .name = "macho_shared",
    .kind = SPN_CC_OUTPUT_SHARED_LIB,
    .format = SPN_FORMAT_MACHO,
    .expect = { .format = SPN_CC_EXPORTS_SYMBOL_LIST, .extension = "exp" },
  },
  {
    .name = "coff_shared",
    .kind = SPN_CC_OUTPUT_SHARED_LIB,
    .format = SPN_FORMAT_COFF,
    .expect = { .format = SPN_CC_EXPORTS_DEF, .extension = "def" },
  },
  {
    .name = "wasm_reactor",
    .kind = SPN_CC_OUTPUT_REACTOR,
    .format = SPN_FORMAT_WASM,
    .expect = { .format = SPN_CC_EXPORTS_WASM, .extension = "sym" },
  },
};

sp_test_each(exports_format, select, format_test_t, tests) {
  spn_cc_exports_format_t format = spn_cc_exports_format(it->kind, it->format);
  sp_expect_eq(t, format, it->expect.format);
  sp_expect_str_eq_c(t, sp_cstr_as_str(spn_cc_exports_extension(format)), it->expect.extension);
  return SP_OK;
}
