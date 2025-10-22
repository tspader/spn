#define SP_IMPLEMENTATION
#include "sp.h"

#include "spn.h"

#include "libtcc.h"

SP_TYPEDEF_FN(spn_build_config_t, spn_build_fn_t, void);

void on_tcc_error(void* opaque, const char* message) {
  printf("%s\n", message);
}

int main(void) {
  int result = 0;

  TCCState* tcc = tcc_new();
  tcc_set_error_func(tcc, NULL, on_tcc_error);
  result = tcc_set_output_type(tcc, TCC_OUTPUT_MEMORY);
  result = tcc_add_include_path(tcc, "/home/spader/source/sp");
  result = tcc_add_include_path(tcc, "/home/spader/source/spn/source/");
  result = tcc_add_file(tcc, "/home/spader/source/spn/examples/spn_run/main.c");
  result = tcc_relocate(tcc);
  spn_build_fn_t build = (spn_build_fn_t)tcc_get_symbol(tcc, "spn_build_fn");

  spn_build_config_t config = build();
  return 0;
}
