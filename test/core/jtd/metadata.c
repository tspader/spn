#include "jtd_test.h"

typedef struct {
  const c8* as;
  const c8* from;
  const c8* context;
} metadata_expect_t;

static void compare_metadata(s32* utest_result, const jtd_result_t* root, const void* expect_v) {
  const metadata_expect_t* expect = (const metadata_expect_t*)expect_v;
  jtd_expect_str(utest_result, jtd_metadata(root->root, "as"), expect->as);
  jtd_expect_str(utest_result, jtd_metadata(root->root, "from"), expect->from);
  jtd_expect_str(utest_result, jtd_metadata(root->root, "context"), expect->context);
  EXPECT_TRUE(jtd_metadata_has(root->root, "as"));
  EXPECT_FALSE(jtd_metadata_has(root->root, "missing"));
}

UTEST(metadata, ok) {
  run_jtd_case(utest_result, (jtd_case_t){
    .json    = "metadata.ok.json",
    .expect  = &(metadata_expect_t){ .as = "spn_semver_t", .from = "spn_semver_from_str", .context = "true" },
    .compare = compare_metadata,
  });
}
