// spn add argparse
// verify sp still has a ^ in the project file

#define SP_IMPLEMENTATION
#include "sp.h"

s32 main(s32 num_args, const c8** args) {
  SP_LOG("hello, {:fg brightcyan}", SP_FMT_CSTR("world"));
  SP_EXIT_SUCCESS();
}
