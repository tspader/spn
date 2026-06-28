#define SP_IMPLEMENTATION
#include "sp.h"

s32 main(s32 num_args, const c8** args) {
  SP_LOG("hello, {.cyan}", SP_FMT_CSTR("world"));
  SP_EXIT_SUCCESS();
}
