#define SP_IMPLEMENTATION
#include "sp.h"

#include "libtcc.h"

typedef struct {
  s32 value;
} spn_build_config_t;

SP_TYPEDEF_FN(spn_build_config_t, spn_build_fn_t, void);

void on_tcc_error(void* opaque, const c8* message) {
  SP_UNUSED(opaque);
  SP_LOG("{}", SP_FMT_CSTR(message));
}

int main(void) {
  s32 result = 0;

  sp_str_t exe_path = sp_fs_get_exe_path();
  sp_str_t build_dir = sp_fs_parent_path(exe_path);
  sp_str_t project_root = sp_fs_parent_path(build_dir);
  sp_str_t script_path = sp_fs_join_path(project_root, SP_LIT("script.c"));

  SP_LOG("exe: {}", SP_FMT_STR(exe_path));
  SP_LOG("script: {}", SP_FMT_STR(script_path));

  TCCState* tcc = tcc_new();
  tcc_set_error_func(tcc, SP_NULLPTR, on_tcc_error);
  result = tcc_set_output_type(tcc, TCC_OUTPUT_MEMORY);
  result = tcc_add_file(tcc, sp_str_to_cstr(script_path));
  if (result < 0) {
    SP_LOG("failed to add file");
    return 1;
  }
  result = tcc_relocate(tcc);
  if (result < 0) {
    SP_LOG("failed to relocate");
    return 1;
  }
  spn_build_fn_t build = (spn_build_fn_t)tcc_get_symbol(tcc, "spn_build_fn");
  if (!build) {
    SP_LOG("failed to get symbol");
    return 1;
  }

  spn_build_config_t config = build();
  SP_LOG("config.value = {}", SP_FMT_S32(config.value));

  tcc_delete(tcc);
  return 0;
}
