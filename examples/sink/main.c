#define SP_IMPLEMENTATION
#include "sp.h"

#define ARGPARSE_IMPLEMENTATION
#include "argparse.h"

#include "jerry.h"

s32 main(s32 num_args, const c8** args) {
  sp_init((sp_config_t) {
    .allocator = sp_allocator_default()
  });

  bool enable_jerry = false;

  struct argparse argparse = SP_ZERO_INITIALIZE();
  argparse_init(
    &argparse,
    (struct argparse_option []) {
      OPT_HELP(),
      OPT_BOOLEAN('j', "jerry", &enable_jerry, SP_NULLPTR, SP_NULLPTR),
      OPT_END()
    },
    (const c8* const []) {
      "kitchen-sink [options]",
      SP_NULLPTR
    },
    SP_NULL
  );


  num_args = argparse_parse(&argparse, num_args, args);

  if (enable_jerry) {
    SP_LOG(jerry);
    SP_LOG("And we bid you {:color green}!", SP_FMT_CSTR("goodnight"));
  }
  else {
    argparse_usage(&argparse);
  }

}

