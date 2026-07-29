#include "toml.h"

typedef struct {
  const c8* name;
  const c8* manifest;
  const c8* toml;
} test_t;

static const test_t tests [] = {
  {
    .name = "empty",
    .toml = "",
  },
  {
    .name = "manifest",
    .manifest = "manifest",
  },
  {
    .name = "strings",
    .manifest = "strings",
  },
  {
    .name = "crlf",
    .toml = "[deps.package]\r\nfoo = \"1.0.0\"\r\n",
  },
  {
    .name = "dotted_and_ws",
    .manifest = "dotted_ws",
  },
};

sp_test_each(toml_edit, roundtrip, test_t, tests) {
  sp_mem_t mem = sp_test_arena(t);
  sp_str_t source = sp_zero;
  if (toml_read_source(t, it->manifest, it->toml, &source)) return SP_ERR;

  spn_toml_edit_t edit = sp_zero;
  sp_must_eq(t, SPN_OK, spn_toml_edit_init(&edit, mem, source));
  sp_expect_str_eq(t, spn_toml_edit_render(&edit, mem), source);

  return SP_OK;
}
