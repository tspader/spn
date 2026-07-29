#include "jtd_test.h"

sp_err_t run_jtd_case(sp_test_t* t, const jtd_case_t* c) {
  sp_mem_t mem = sp_test_arena(t);

  sp_str_t path = sp_fs_join_path(mem, test_repo_path(mem, sp_str_lit(JTD_TEST_JSON_DIR)), sp_cstr_as_str(c->json));
  sp_str_t json = sp_zero; sp_io_read_file(mem, path, &json);

  sp_test_kv(t, "fixture", path);
  sp_must(t, !sp_str_empty(json));

  jtd_result_t result = jtd_parse(mem, json);

  sp_err_t err = SP_OK;
  if (c->error == JTD_OK) {
    sp_expect(t, result.ok);
    if (result.ok && c->compare) {
      err = c->compare(t, &result, c->expect);
    }
  }
  else {
    sp_expect(t, !result.ok);
    sp_expect_eq(t, (s32)c->error, (s32)result.diag.code);
    if (c->error_path) {
      sp_expect_str_eq_c(t, result.diag.path, c->error_path);
    }
  }

  jtd_free(&result);
  return err;
}
