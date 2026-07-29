#include "dag_test.h"

typedef struct {
  dag_test_roots_t roots;
  const c8* str;
} masked_input_t;

typedef struct {
  bool equal;
} masked_expect_t;

typedef struct {
  const c8* name;
  masked_input_t a;
  masked_input_t b;
  masked_expect_t expect;
} masked_test_t;

static const masked_test_t masked_tests [] = {
  {
    .name = "identical_inputs_match",
    .a = { .roots = { .project = "/A" }, .str = "-I/A/H" },
    .b = { .roots = { .project = "/A" }, .str = "-I/A/H" },
    .expect = { .equal = true }
  },
  {
    .name = "relocated_root_matches",
    .a = { .roots = { .project = "/A" }, .str = "-I/A/H" },
    .b = { .roots = { .project = "/B" }, .str = "-I/B/H" },
    .expect = { .equal = true }
  },
  {
    .name = "mid_path_root_relocates",
    .a = { .roots = { .project = "/A" }, .str = "/X/A/H" },
    .b = { .roots = { .project = "/B" }, .str = "/X/B/H" },
    .expect = { .equal = true }
  },
  {
    .name = "every_occurrence_relocates",
    .a = { .roots = { .project = "/A" }, .str = "-I/A/H -L/A/L" },
    .b = { .roots = { .project = "/B" }, .str = "-I/B/H -L/B/L" },
    .expect = { .equal = true }
  },
  {
    .name = "root_at_end_relocates",
    .a = { .roots = { .project = "/A" }, .str = "-I/A" },
    .b = { .roots = { .project = "/B" }, .str = "-I/B" },
    .expect = { .equal = true }
  },
  {
    .name = "unmatched_str_ignores_roots",
    .a = { .roots = { .project = "/A" }, .str = "H" },
    .b = { .str = "H" },
    .expect = { .equal = true }
  },
  {
    .name = "longest_root_wins",
    .a = { .roots = { .project = "/A", .store = "/A/S" }, .str = "/A/S/H" },
    .b = { .roots = { .project = "/B", .store = "/C" }, .str = "/C/H" },
    .expect = { .equal = true }
  },
  {
    .name = "literal_change_differs",
    .a = { .roots = { .project = "/A" }, .str = "-I/A/H" },
    .b = { .roots = { .project = "/A" }, .str = "-I/A/G" }
  },
  {
    .name = "root_kind_change_differs",
    .a = { .roots = { .project = "/A", .store = "/B" }, .str = "-I/A/H" },
    .b = { .roots = { .project = "/A", .store = "/B" }, .str = "-I/B/H" }
  },
  {
    .name = "boundary_violation_not_masked",
    .a = { .roots = { .project = "/A" }, .str = "/AB" },
    .b = { .roots = { .project = "/B" }, .str = "/BB" }
  },
  {
    .name = "masked_root_differs_from_literal",
    .a = { .roots = { .project = "/A" }, .str = "/A" },
    .b = { .str = "/A" }
  },
};

static spn_dag_digest_t masked_digest(const masked_input_t* in) {
  spn_dag_roots_t storage = sp_zero;
  const spn_dag_roots_t* roots = dag_test_roots_build(in->roots, &storage);
  spn_sha256_ctx_t ctx = sp_zero;
  spn_sha256_init(&ctx);
  spn_dag_hash_masked(&ctx, roots, sp_str_view(in->str));
  return spn_dag_hash_final(&ctx);
}

sp_test_each(dag_masked, digest, masked_test_t, masked_tests) {
  sp_expect_eq(t, it->expect.equal, spn_dag_digest_equal(masked_digest(&it->a), masked_digest(&it->b)));
  return SP_OK;
}

typedef struct {
  const c8* strs [DAG_TEST_MAX_INPUTS];
} masked_strs_input_t;

typedef struct {
  const c8* name;
  masked_strs_input_t a;
  masked_strs_input_t b;
  masked_expect_t expect;
} masked_strs_test_t;

static const masked_strs_test_t masked_strs_tests [] = {
  {
    .name = "element_boundary_differs",
    .a = { .strs = { "A", "B" } },
    .b = { .strs = { "AB" } }
  },
};

static spn_dag_digest_t masked_strs_digest(sp_mem_t mem, const masked_strs_input_t* in) {
  spn_dag_roots_t roots = sp_zero;
  sp_da(sp_str_t) strs = sp_da_new(mem, sp_str_t);
  sp_carr_for(in->strs, it) {
    if (!in->strs[it]) {
      break;
    }
    sp_da_push(strs, sp_str_view(in->strs[it]));
  }
  spn_sha256_ctx_t ctx = sp_zero;
  spn_sha256_init(&ctx);
  spn_dag_hash_masked_strs(&ctx, &roots, strs);
  return spn_dag_hash_final(&ctx);
}

sp_test_each(dag_masked, strs, masked_strs_test_t, masked_strs_tests) {
  sp_mem_t mem = sp_test_arena(t);
  spn_dag_digest_t a = masked_strs_digest(mem, &it->a);
  spn_dag_digest_t b = masked_strs_digest(mem, &it->b);
  sp_expect_eq(t, it->expect.equal, spn_dag_digest_equal(a, b));
  return SP_OK;
}
