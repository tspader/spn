#include "../compiler.h"
#include "toolchain/linker.h"

typedef struct {
  spn_cc_exports_format_t format;
  const c8* extension;
} format_expect_t;

typedef struct {
  const c8* name;
  spn_cc_output_kind_t kind;
  spn_ld_flavor_t flavor;
  format_expect_t expect;
} format_test_t;

static const format_test_t tests [] = {
  {
    .name = "elf_shared",
    .kind = SPN_CC_OUTPUT_SHARED_LIB,
    .flavor = SPN_LD_FLAVOR_ELF,
    .expect = { .format = SPN_CC_EXPORTS_VERSION_SCRIPT, .extension = "map" },
  },
  {
    .name = "wasm_shared",
    .kind = SPN_CC_OUTPUT_SHARED_LIB,
    .flavor = SPN_LD_FLAVOR_WASM,
    .expect = { .format = SPN_CC_EXPORTS_VERSION_SCRIPT, .extension = "map" },
  },
  {
    .name = "macho_shared",
    .kind = SPN_CC_OUTPUT_SHARED_LIB,
    .flavor = SPN_LD_FLAVOR_MACHO,
    .expect = { .format = SPN_CC_EXPORTS_SYMBOL_LIST, .extension = "exp" },
  },
  {
    .name = "mingw_shared",
    .kind = SPN_CC_OUTPUT_SHARED_LIB,
    .flavor = SPN_LD_FLAVOR_MINGW,
    .expect = { .format = SPN_CC_EXPORTS_DEF, .extension = "def" },
  },
  {
    .name = "msvc_shared",
    .kind = SPN_CC_OUTPUT_SHARED_LIB,
    .flavor = SPN_LD_FLAVOR_MSVC,
    .expect = { .format = SPN_CC_EXPORTS_DEF, .extension = "def" },
  },
  {
    .name = "wasm_reactor",
    .kind = SPN_CC_OUTPUT_REACTOR,
    .flavor = SPN_LD_FLAVOR_WASM,
    .expect = { .format = SPN_CC_EXPORTS_WASM, .extension = "sym" },
  },
};

sp_test_each(exports_format, select, format_test_t, tests) {
  spn_cc_exports_format_t format = spn_cc_exports_format(it->kind, it->flavor);
  sp_expect_eq(t, format, it->expect.format);
  sp_expect_str_eq_c(t, sp_cstr_as_str(spn_cc_exports_extension(format)), it->expect.extension);
  return SP_OK;
}
