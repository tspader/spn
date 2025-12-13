#define SP_IMPLEMENTATION
#include "sp.h"
#include "libtcc.h"
#include <math.h>

s32 main(s32 num_args, const c8** args) {
  SP_UNUSED(num_args);
  SP_UNUSED(args);

  // Test tcc
  TCCState* tcc = tcc_new();
  tcc_set_output_type(tcc, TCC_OUTPUT_MEMORY);
  tcc_compile_string(tcc, "int foo(int bar) { return bar * 2 + 1; }");
  tcc_relocate(tcc);
  int (*foo)(int) = tcc_get_symbol(tcc, "foo");
  SP_LOG("foo(34) from tcc = {}", SP_FMT_S32(foo(34)));
  tcc_delete(tcc);

  // Test math
  f64 x = sqrt(4761);
  SP_LOG("sqrt(4761) from libm = {}", SP_FMT_F64(x));

  SP_EXIT_SUCCESS();
}
