#define SP_IMPLEMENTATION
#include "sp.h"
#include "sp/sp_cli.h"
#include "sp/atomic_file.h"
#include "codegen.h"

typedef struct {
  const c8* schema;
  const c8* out;
  const c8* templates;
} args_t;

static void log_line(void* user, sp_str_t message) {
  (void)user;
  sp_log("{}", sp_fmt_str(message));
}

static sp_cli_result_t run_cli(sp_cli_t* cli) {
  args_t* args = sp_cast(args_t*, cli->user_data);

  sp_mem_t mem = sp_mem_heap_as_allocator(sp_mem_heap_new());
  sp_str_t err = codegen_run(mem, (codegen_paths_t) {
    .schema = sp_cstr_as_str(args->schema),
    .out = sp_cstr_as_str(args->out),
    .templates = sp_cstr_as_str(args->templates),
  }, log_line, SP_NULLPTR);

  if (!sp_str_empty(err)) {
    return sp_cli_set_error(cli, err);
  }
  return SP_CLI_OK;
}

s32 main(s32 num_args, const c8** args) {
  args_t parsed = sp_zero;

  sp_cli_cmd_t root = {
    .name = "jtd_gen",
    .summary = "Generate C structs and load/write code from JTD schemas",
    .args = {
      {
        .name = "schema",
        .summary = "Path to the schema directory",
        .ptr = &parsed.schema,
      },
      {
        .name = "out",
        .summary = "Path to the generated output directory",
        .ptr = &parsed.out,
      },
      {
        .name = "templates",
        .summary = "Path to the template root directory",
        .ptr = &parsed.templates,
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
