#include "closure.h"

#include "utest.h"

typedef struct {
  closure_graph_test_t graph;
  struct {
    const c8* name;
    bool links_code;
  } expect [CLOSURE_TEST_MAX_NAMES];
} links_code_test_t;

static void run_test(s32* utest_result, links_code_test_t t) {
  closure_graph_t g = build_graph(&t.graph);
  sp_da(spn_closure_entry_t) closure = spn_target_link_closure(g.mem, g.root);

  u32 expected = 0;
  sp_carr_for(t.expect, it) {
    if (!t.expect[it].name) {
      break;
    }
    expected++;
  }

  ASSERT_EQ(expected, sp_da_size(closure));
  sp_for(it, expected) {
    EXPECT_TRUE(sp_str_equal(closure[it].pkg->info->name, sp_str_view(t.expect[it].name)));
    EXPECT_EQ(t.expect[it].links_code, closure[it].links_code);
  }
}

// links_code gates per-package link baggage (frameworks): header-only and
// static packages contribute, shared-only and no_link-only packages do not
UTEST(links_code, marks_static_linkers) {
  run_test(utest_result, (links_code_test_t) {
    .graph = {
      .root = "test",
      .pkgs = {
        { .name = "test", .deps = { { "stat" }, { "shl" }, { "hdr" }, { "noli" } } },
        { .name = "stat", .kind = SPN_LIB_KIND_STATIC },
        { .name = "shl", .kind = SPN_LIB_KIND_SHARED },
        { .name = "hdr" },
        { .name = "noli", .libs = { { "noli_a", SPN_LIB_KIND_STATIC, .no_link = true } } },
      },
    },
    .expect = {
      { "test", .links_code = true },
      { "noli", .links_code = false },
      { "hdr", .links_code = true },
      { "shl", .links_code = false },
      { "stat", .links_code = true },
    },
  });
}
