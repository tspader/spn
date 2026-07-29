#include "toml.h"

sp_err_t toml_read_source(sp_test_t* t, const c8* manifest, const c8* toml, sp_str_t* source) {
  if (!manifest) {
    *source = sp_str_view(toml);
    return SP_OK;
  }

  sp_mem_t mem = sp_test_arena(t);
  sp_str_t path = test_repo_path(mem, sp_test_format(t, "test/core/toml/manifests/{}.toml", sp_fmt_cstr(manifest)));
  sp_must_ok(t, sp_io_read_file(mem, path, source));
  return SP_OK;
}

u32 toml_collect_path(const c8* (*segments)[TOML_TEST_MAX_PATH], sp_str_t* path) {
  u32 num_segments = 0;
  sp_carr_detect_len(*segments, num_segments, (*segments)[num_segments]);
  sp_for(it, num_segments) {
    path[it] = sp_str_view((*segments)[it]);
  }
  return num_segments;
}
