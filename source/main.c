#define SPN_IMPLEMENTATION
#include "spn.h"

s32 main(s32 num_args, const c8** args) {
  app = SP_ZERO_STRUCT(spn_app_t);
  spn_app_init(&app, num_args, args);
  spn_app_run(&app);

  return 0;
}
