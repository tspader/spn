#include "../compiler.h"

#define render_syms_max 4

typedef enum {
  EXPORTS_RENDER_VERSION_SCRIPT,
  EXPORTS_RENDER_SYMBOL_LIST,
  EXPORTS_RENDER_DEF,
} exports_render_t;

typedef struct {
  const c8* rendered;
} render_exports_expect_t;

typedef struct {
  const c8* name;
  exports_render_t render;
  const c8* library;
  const c8* symbols [render_syms_max];
  render_exports_expect_t expect;
} render_exports_test_t;

static const render_exports_test_t tests [] = {
  {
    .name = "version_script",
    .render = EXPORTS_RENDER_VERSION_SCRIPT,
    .symbols = { "A", "B" },
    .expect = {
      .rendered = "{\nglobal:\n  A;\n  B;\nlocal:\n  *;\n};\n",
    },
  },
  {
    .name = "version_script_no_symbols",
    .render = EXPORTS_RENDER_VERSION_SCRIPT,
    .expect = {
      .rendered = "{\nlocal:\n  *;\n};\n",
    },
  },
  {
    .name = "symbol_list",
    .render = EXPORTS_RENDER_SYMBOL_LIST,
    .symbols = { "_A", "_B" },
    .expect = {
      .rendered = "_A\n_B\n",
    },
  },
  {
    .name = "def",
    .render = EXPORTS_RENDER_DEF,
    .library = "S",
    .symbols = { "A", "B" },
    .expect = {
      .rendered = "LIBRARY S\nEXPORTS\n  A\n  B\n",
    },
  },
};

sp_test_each(exports_render, render, render_exports_test_t, tests) {
  sp_mem_t mem = sp_test_arena(t);

  sp_da(sp_str_t) symbols = sp_da_new(mem, sp_str_t);
  sp_carr_for(it->symbols, s) {
    if (!it->symbols[s]) break;
    sp_da_push(symbols, sp_str_view(it->symbols[s]));
  }

  sp_io_dyn_mem_writer_t buf;
  sp_io_dyn_mem_writer_init(mem, &buf);
  switch (it->render) {
    case EXPORTS_RENDER_VERSION_SCRIPT: {
      spn_exports_render_version_script(&buf.base, symbols);
      break;
    }
    case EXPORTS_RENDER_SYMBOL_LIST: {
      spn_exports_render_symbol_list(&buf.base, symbols);
      break;
    }
    case EXPORTS_RENDER_DEF: {
      spn_exports_render_def(&buf.base, sp_str_view(it->library), symbols);
      break;
    }
  }
  sp_str_t result = sp_io_dyn_mem_writer_as_str(&buf);

  sp_test_kv(t, "rendered", result);
  sp_expect_str_eq_c(t, result, it->expect.rendered);

  return SP_OK;
}
