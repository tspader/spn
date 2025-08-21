#define SP_IMPLEMENTATION
#define SP_OS_BACKEND_SDL
#include "sp/sp.h"

#include "SDL3/SDL.h"


#include "argparse/argparse.h"

#include "toml/toml.h"

/*
 * pull in argparse and make:
 *   spn add sdl
 *   spn init
 *
 * [deps.sdl]
 * url = "https://github.com/libsdl-org/SDL"
 * branch = "main"
 * commit = "abc123" // or this
 *
 * default to main, check it out in ~/.cache/spn/repos/SDL
 * recipes
*/

/////////
// CLI //
/////////
typedef struct {
  const c8* package;
} spn_cli_add_t;

typedef struct {
  u32 num_args;
  const c8** args;
  spn_cli_add_t add;
} spn_cli_t;

void spn_cli_build(spn_cli_t* cli, u32 num_args, const c8** args);
void spn_cli_add(spn_cli_t* cli);


typedef struct {
  sp_str_t install;
  sp_str_t cache;
} spn_paths_t;


void spn_cli_build(spn_cli_t* cli, u32 num_args, const c8** args) {
  struct argparse_option options [] = {
    OPT_HELP(),
    OPT_END(),
  };

  const c8* const usages [] = {
    "subcommands [options] [cmd]",
    SP_NULLPTR
  };

  struct argparse argparse;
  argparse_init(&argparse, options, usages, ARGPARSE_STOP_AT_NON_OPTION);

  cli->args = args;
  cli->num_args = num_args;
  cli->num_args = argparse_parse(&argparse, cli->num_args, cli->args);

  if (!cli->num_args) {
    argparse_usage(&argparse);
    exit(0);
  }

  if (sp_cstr_equal("add", args[0])) {
    spn_cli_add(cli);
  }
}

void spn_cli_add(spn_cli_t* cli) {
  spn_cli_add_t* add = &cli->add;
  struct argparse_option options [] = {
    OPT_HELP(),
    OPT_STRING('p', "package", &add->package, SP_NULLPTR, SP_NULLPTR),
    OPT_END()
  };

  const c8* const usages [] = {
    "spn add [options] [url]",
    SP_NULLPTR
  };

  struct argparse argparse;
  argparse_init(&argparse, options, usages, SP_NULL);
  cli->num_args = argparse_parse(&argparse, cli->num_args, cli->args);

  if (!cli->num_args) {
    argparse_usage(&argparse);
    exit(0);
  }
}

s32 main(s32 num_args, const char** args) {
  spn_cli_t cli = SP_ZERO_INITIALIZE();
  spn_cli_build(&cli, num_args, args);

  return 0;
}
