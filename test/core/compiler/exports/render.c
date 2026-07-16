#include "../common.h"
#include "compiler/exports.h"

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
  exports_render_t render;
  const c8* library;
  const c8* symbols [render_syms_max];
  render_exports_expect_t expect;
} render_exports_test_t;

static void run_render_exports_test(s32* utest_result, render_exports_test_t t) {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();

  sp_da(sp_str_t) symbols = sp_da_new(scratch.mem, sp_str_t);
  sp_carr_for(t.symbols, it) {
    if (!t.symbols[it]) break;
    sp_da_push(symbols, sp_str_view(t.symbols[it]));
  }

  sp_str_t result = sp_zero;
  switch (t.render) {
    case EXPORTS_RENDER_VERSION_SCRIPT: {
      result = spn_exports_render_version_script(scratch.mem, symbols);
      break;
    }
    case EXPORTS_RENDER_SYMBOL_LIST: {
      result = spn_exports_render_symbol_list(scratch.mem, symbols);
      break;
    }
    case EXPORTS_RENDER_DEF: {
      result = spn_exports_render_def(scratch.mem, sp_str_view(t.library), symbols);
      break;
    }
  }

  utest_kv("rendered", result);
  EXPECT_TRUE(sp_str_equal_cstr(result, t.expect.rendered));

  sp_mem_end_scratch(scratch);
}

UTEST(exports_render, version_script) {
  UTEST_SKIP("");
  run_render_exports_test(utest_result, (render_exports_test_t) {
    .render = EXPORTS_RENDER_VERSION_SCRIPT,
    .symbols = { "A", "B" },
    .expect = {
      .rendered = "{\nglobal:\n  A;\n  B;\nlocal:\n  *;\n};\n",
    },
  });
}

UTEST(exports_render, version_script_no_symbols) {
  UTEST_SKIP("");
  run_render_exports_test(utest_result, (render_exports_test_t) {
    .render = EXPORTS_RENDER_VERSION_SCRIPT,
    .expect = {
      .rendered = "{\nlocal:\n  *;\n};\n",
    },
  });
}

UTEST(exports_render, symbol_list) {
  UTEST_SKIP("");
  run_render_exports_test(utest_result, (render_exports_test_t) {
    .render = EXPORTS_RENDER_SYMBOL_LIST,
    .symbols = { "_A", "_B" },
    .expect = {
      .rendered = "_A\n_B\n",
    },
  });
}

UTEST(exports_render, def) {
  UTEST_SKIP("");
  run_render_exports_test(utest_result, (render_exports_test_t) {
    .render = EXPORTS_RENDER_DEF,
    .library = "S",
    .symbols = { "A", "B" },
    .expect = {
      .rendered = "LIBRARY S\nEXPORTS\n  A\n  B\n",
    },
  });
}
