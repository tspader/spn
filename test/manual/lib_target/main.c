#define SP_IMPLEMENTATION
#include "sp.h"

s32 main(s32 num_args, const c8** args) {
  SP_LOG("lib_target test: {:fg brightcyan}", SP_FMT_CSTR("ok"));
  SP_EXIT_SUCCESS();
}
