#define SP_IMPLEMENTATION
#include "sp.h"
#include "sp/sp_cli.h"
#include "render.h"

typedef struct {
  const c8* shasums;
  const c8* templates;
  const c8* out;
  const c8* version;
  const c8* tag;
  const c8* repo;
} args_t;

static sp_cli_result_t run_cli(sp_cli_t* cli) {
  args_t* args = sp_cast(args_t*, cli->user_data);

  sp_mem_t mem = sp_mem_heap_as_allocator(sp_mem_heap_new());
  sp_str_t shasums = sp_zero;
  if (sp_io_read_file(mem, sp_cstr_as_str(args->shasums), &shasums)) {
    return sp_cli_set_error(cli, sp_fmt(mem, "failed to read {}", sp_fmt_cstr(args->shasums)).value);
  }

  installer_result_t result = installer_render(mem, (installer_config_t) {
    .shasums = shasums,
    .templates = sp_cstr_as_str(args->templates),
    .out = sp_cstr_as_str(args->out),
    .version = sp_cstr_as_str(args->version),
    .tag = sp_cstr_as_str(args->tag),
    .repo = sp_cstr_as_str(args->repo),
  });

  if (result.err) {
    return sp_cli_set_error(cli, result.message);
  }
  return SP_CLI_OK;
}

s32 main(s32 num_args, const c8** args) {
  args_t parsed = sp_zero;

  sp_cli_cmd_t root = {
    .name = "installer",
    .summary = "Render the release installers from a SHASUMS256.txt",
    .args = {
      {
        .name = "shasums",
        .summary = "Path to SHASUMS256.txt",
        .ptr = &parsed.shasums,
      },
      {
        .name = "templates",
        .summary = "Path to the template root directory",
        .ptr = &parsed.templates,
      },
      {
        .name = "out",
        .summary = "Directory to write install.sh and install.ps1 to",
        .ptr = &parsed.out,
      },
      {
        .name = "version",
        .summary = "Version the installers report (e.g. 0.4.2)",
        .ptr = &parsed.version,
      },
      {
        .name = "tag",
        .summary = "Release tag the installers download from (e.g. v0.4.2)",
        .ptr = &parsed.tag,
      },
      {
        .name = "repo",
        .summary = "GitHub repository the installers download from (e.g. tspader/spn)",
        .ptr = &parsed.repo,
      },
    },
    .handler = run_cli,
  };

  return sp_cli_main((sp_cli_desc_t) {
    .root = &root,
    .num_args = num_args,
    .args = args,
    .user_data = &parsed,
  });
}
