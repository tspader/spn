#ifndef SPN_TEST_ARG_H
#define SPN_TEST_ARG_H

#include "sp.h"
#include "sp/sp_test.h"
#include "paths/paths.h"

typedef struct {
  const c8* name;
  const c8* path;
  spn_path_root_t root;
} test_arg_t;

static sp_err_t test_check_arg(sp_test_t* t, spn_arg_t arg, test_arg_t expect) {
  if (!expect.name && !expect.path) {
    return SP_OK;
  }
  if (expect.path) {
    sp_expect(t, sp_str_empty(arg.prefix));
    sp_expect_eq(t, (u32)expect.root, (u32)arg.path.root);
    sp_expect_str_eq_c(t, arg.path.sub, expect.path);
    return SP_OK;
  }
  sp_expect_str_eq_c(t, arg.prefix, expect.name);
  sp_expect(t, spn_path_empty(arg.path));
  return SP_OK;
}

#endif
