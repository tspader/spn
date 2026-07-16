#include "../common.h"
#include "compiler/exports.h"

#define exports_syms_max 4
#define exports_tocs_max 3

typedef struct {
  const c8* symbols [exports_syms_max];
} collect_expect_t;

typedef struct {
  const c8* tocs [exports_tocs_max][exports_syms_max];
  collect_expect_t expect;
} collect_test_t;

static void run_collect_test(s32* utest_result, collect_test_t t) {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();

  sp_da(spn_toc_t) tocs = sp_da_new(scratch.mem, spn_toc_t);
  sp_carr_for(t.tocs, it) {
    if (!t.tocs[it][0]) break;
    spn_toc_t toc = sp_zero;
    sp_da_init(scratch.mem, toc.symbols);
    sp_carr_for(t.tocs[it], jt) {
      if (!t.tocs[it][jt]) break;
      sp_da_push(toc.symbols, sp_str_view(t.tocs[it][jt]));
    }
    sp_da_push(tocs, toc);
  }

  sp_da(sp_str_t) symbols = spn_exports_collect(scratch.mem, tocs);

  u32 count = 0;
  sp_carr_for(t.expect.symbols, it) {
    if (!t.expect.symbols[it]) break;
    count++;
  }
  ASSERT_EQ(sp_da_size(symbols), count);
  sp_for(it, count) {
    utest_kv("symbol", sp_str_view(t.expect.symbols[it]));
    EXPECT_TRUE(sp_str_equal_cstr(symbols[it], t.expect.symbols[it]));
  }

  sp_mem_end_scratch(scratch);
}

UTEST(exports_collect, dedup_first_seen) {
  UTEST_SKIP("");
  run_collect_test(utest_result, (collect_test_t) {
    .tocs = {
      { "A", "B" },
      { "B", "C" },
    },
    .expect = {
      .symbols = { "A", "B", "C" },
    },
  });
}

UTEST(exports_collect, order_across_tocs) {
  UTEST_SKIP("");
  run_collect_test(utest_result, (collect_test_t) {
    .tocs = {
      { "B" },
      { "A" },
    },
    .expect = {
      .symbols = { "B", "A" },
    },
  });
}

UTEST(exports_collect, no_tocs) {
  UTEST_SKIP("");
  run_collect_test(utest_result, (collect_test_t) { 0 });
}
