#define SP_IMPLEMENTATION
#include "sp.h"

#include "pkg.h"
#include "target.h"

s32 main(s32 num_args, const c8** args) {
  SP_LOG("hello, {:fg brightcyan}! pkg() = {}, target() = {}", SP_FMT_CSTR("world"), SP_FMT_S32(package()), SP_FMT_S32(target()));
  SP_EXIT_SUCCESS();
}
