#include "common.h"

typedef struct {
  dag_test_roots_t roots;
  const c8* str;
} masked_input_t;

typedef struct {
  bool equal;
} masked_expect_t;

typedef struct {
  masked_input_t a;
  masked_input_t b;
  masked_expect_t expect;
} masked_test_t;

UTEST_EMPTY_FIXTURE(masked)

static spn_dag_digest_t masked_digest(masked_input_t in) {
  spn_dag_roots_t storage = sp_zero;
  const spn_dag_roots_t* roots = dag_test_roots_build(in.roots, &storage);
  spn_sha256_ctx_t ctx = sp_zero;
  spn_sha256_init(&ctx);
  spn_dag_hash_masked(&ctx, roots, sp_str_view(in.str));
  return spn_dag_hash_final(&ctx);
}

static void run_test(s32* utest_result, masked_test_t t) {
  EXPECT_EQ(t.expect.equal, spn_dag_digest_equal(masked_digest(t.a), masked_digest(t.b)));
}

UTEST_F(masked, identical_inputs_match) {
  run_test(&ur, (masked_test_t) {
    .a = { .roots = { .project = "/A" }, .str = "-I/A/H" },
    .b = { .roots = { .project = "/A" }, .str = "-I/A/H" },
    .expect = { .equal = true }
  });
}

UTEST_F(masked, relocated_root_matches) {
  run_test(&ur, (masked_test_t) {
    .a = { .roots = { .project = "/A" }, .str = "-I/A/H" },
    .b = { .roots = { .project = "/B" }, .str = "-I/B/H" },
    .expect = { .equal = true }
  });
}

UTEST_F(masked, mid_path_root_relocates) {
  run_test(&ur, (masked_test_t) {
    .a = { .roots = { .project = "/A" }, .str = "/X/A/H" },
    .b = { .roots = { .project = "/B" }, .str = "/X/B/H" },
    .expect = { .equal = true }
  });
}

UTEST_F(masked, every_occurrence_relocates) {
  run_test(&ur, (masked_test_t) {
    .a = { .roots = { .project = "/A" }, .str = "-I/A/H -L/A/L" },
    .b = { .roots = { .project = "/B" }, .str = "-I/B/H -L/B/L" },
    .expect = { .equal = true }
  });
}

UTEST_F(masked, root_at_end_relocates) {
  run_test(&ur, (masked_test_t) {
    .a = { .roots = { .project = "/A" }, .str = "-I/A" },
    .b = { .roots = { .project = "/B" }, .str = "-I/B" },
    .expect = { .equal = true }
  });
}

UTEST_F(masked, unmatched_str_ignores_roots) {
  run_test(&ur, (masked_test_t) {
    .a = { .roots = { .project = "/A" }, .str = "H" },
    .b = { .str = "H" },
    .expect = { .equal = true }
  });
}

UTEST_F(masked, longest_root_wins) {
  run_test(&ur, (masked_test_t) {
    .a = { .roots = { .project = "/A", .store = "/A/S" }, .str = "/A/S/H" },
    .b = { .roots = { .project = "/B", .store = "/C" }, .str = "/C/H" },
    .expect = { .equal = true }
  });
}

UTEST_F(masked, literal_change_differs) {
  run_test(&ur, (masked_test_t) {
    .a = { .roots = { .project = "/A" }, .str = "-I/A/H" },
    .b = { .roots = { .project = "/A" }, .str = "-I/A/G" }
  });
}

UTEST_F(masked, root_kind_change_differs) {
  run_test(&ur, (masked_test_t) {
    .a = { .roots = { .project = "/A", .store = "/B" }, .str = "-I/A/H" },
    .b = { .roots = { .project = "/A", .store = "/B" }, .str = "-I/B/H" }
  });
}

UTEST_F(masked, boundary_violation_not_masked) {
  run_test(&ur, (masked_test_t) {
    .a = { .roots = { .project = "/A" }, .str = "/AB" },
    .b = { .roots = { .project = "/B" }, .str = "/BB" }
  });
}

UTEST_F(masked, masked_root_differs_from_literal) {
  run_test(&ur, (masked_test_t) {
    .a = { .roots = { .project = "/A" }, .str = "/A" },
    .b = { .str = "/A" }
  });
}

typedef struct {
  const c8* strs [DAG_TEST_MAX_INPUTS];
} masked_strs_input_t;

typedef struct {
  masked_strs_input_t a;
  masked_strs_input_t b;
  masked_expect_t expect;
} masked_strs_test_t;

static spn_dag_digest_t masked_strs_digest(sp_mem_t mem, masked_strs_input_t in) {
  spn_dag_roots_t roots = sp_zero;
  sp_da(sp_str_t) strs = sp_da_new(mem, sp_str_t);
  sp_carr_for(in.strs, it) {
    if (!in.strs[it]) {
      break;
    }
    sp_da_push(strs, sp_str_view(in.strs[it]));
  }
  spn_sha256_ctx_t ctx = sp_zero;
  spn_sha256_init(&ctx);
  spn_dag_hash_masked_strs(&ctx, &roots, strs);
  return spn_dag_hash_final(&ctx);
}

static void run_strs_test(s32* utest_result, masked_strs_test_t t) {
  sp_mem_arena_marker_t s = sp_mem_begin_scratch();
  spn_dag_digest_t a = masked_strs_digest(s.mem, t.a);
  spn_dag_digest_t b = masked_strs_digest(s.mem, t.b);
  EXPECT_EQ(t.expect.equal, spn_dag_digest_equal(a, b));
  sp_mem_end_scratch(s);
}

UTEST_F(masked, element_boundary_differs) {
  run_strs_test(&ur, (masked_strs_test_t) {
    .a = { .strs = { "A", "B" } },
    .b = { .strs = { "AB" } }
  });
}
