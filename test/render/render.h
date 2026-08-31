#ifndef SPN_TEST_RENDER_H
#define SPN_TEST_RENDER_H

#include "sp.h"
#include "fixture.h"

static inline sp_str_t render_out_root(sp_mem_t mem) {
  return test_repo_path(mem, sp_str_lit("test/render/.out"));
}

static inline sp_str_t render_out_path(sp_mem_t mem, const c8* sub) {
  return sp_fs_join_path(mem, render_out_root(mem), sp_str_view(sub));
}

#endif
