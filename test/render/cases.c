#include "env.h"
#include "render.h"

typedef struct {
  const c8* name;
  const c8* project;
  const c8* args [SPN_TEST_COMMAND_MAX_ARGS];
  const c8* output;
} cell_t;

static cell_t cells [] = {
  {
    .name = "help",
    .args = { "--help" },
  },
  {
    .name = "usage_unknown_flag",
    .args = { "build", "--bogus" },
  },
  {
    .name = "index_bare",
    .args = { "index" },
  },
  {
    .name = "build_cold",
    .project = "test/render/fixtures/ok",
    .args = { "build" },
  },
  {
    .name = "build_fail",
    .project = "test/render/fixtures/compile_error",
    .args = { "build" },
  },
  {
    .name = "build_json",
    .project = "test/render/fixtures/ok",
    .args = { "build" },
    .output = "json",
  },
};

sp_test_each(render, cells, cell_t, cells) {
  fixture_t fixture = sp_zero;
  sp_try(fixture_init(t, &fixture));
  sp_try(prepare_test(t, &fixture, it->project, SP_NULLPTR));

  sp_ps_output_t output = run_spn_command(t, &fixture, it->output, it->args, SP_NULLPTR);

  sp_mem_t mem = fixture.mem;
  sp_str_t dir = sp_fs_join_path(mem, render_out_path(mem, "current"), sp_str_view(it->name));
  write_file(sp_fs_join_path(mem, dir, sp_str_lit("stderr")), output.err);
  write_file(sp_fs_join_path(mem, dir, sp_str_lit("stdout")), output.out);
  write_file(sp_fs_join_path(mem, dir, sp_str_lit("exit")), sp_fmt(mem, "{}\n", sp_fmt_int(output.status.exit_code)).value);
  return SP_OK;
}
