#include "jtd_test.h"

typedef struct {
  const c8* as;
  const c8* from;
  const c8* context;
} metadata_expect_t;

static sp_err_t compare_metadata(sp_test_t* t, const jtd_result_t* root, const void* expect_v) {
  const metadata_expect_t* expect = (const metadata_expect_t*)expect_v;
  sp_expect_str_eq_c(t, jtd_metadata(root->root, "as"), expect->as);
  sp_expect_str_eq_c(t, jtd_metadata(root->root, "from"), expect->from);
  sp_expect_str_eq_c(t, jtd_metadata(root->root, "context"), expect->context);
  sp_expect(t, jtd_metadata_has(root->root, "as"));
  sp_expect(t, !jtd_metadata_has(root->root, "missing"));
  return SP_OK;
}

static const jtd_case_t cases [] = {
  {
    .name    = "ok",
    .json    = "metadata.ok.json",
    .expect  = &(metadata_expect_t){ .as = "spn_semver_t", .from = "spn_semver_from_str", .context = "true" },
    .compare = compare_metadata,
  },
};

sp_test_each_fn(metadata, parse, jtd_case_t, cases, run_jtd_case);
