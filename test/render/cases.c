#include "env.h"
#include "render.h"

typedef struct {
  const c8* name;
  const c8* project;
  const c8* setup [2][SPN_TEST_COMMAND_MAX_ARGS];
  const c8* args [SPN_TEST_COMMAND_MAX_ARGS];
  const c8* env [SPN_TEST_COMMAND_MAX_ENV];
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
    .args = { "build", "--json" },
  },
  {
    .name = "build_warm",
    .project = "test/render/fixtures/ok",
    .setup = { { "build" } },
    .args = { "build" },
  },
  {
    .name = "build_quiet",
    .project = "test/render/fixtures/ok",
    .args = { "build", "-q" },
  },
  {
    .name = "build_verbose",
    .project = "test/render/fixtures/ok",
    .args = { "build", "-v" },
  },
  {
    .name = "build_no_manifest",
    .args = { "build" },
  },
  {
    .name = "manifest_invalid",
    .project = "test/render/fixtures/bad_manifest",
    .args = { "build" },
  },
  {
    .name = "usage_bad_choice",
    .args = { "build", "--mode", "bogus" },
  },
  {
    .name = "test_ok",
    .project = "test/render/fixtures/test_ok",
    .args = { "test" },
  },
  {
    .name = "test_fail",
    .project = "test/render/fixtures/test_error",
    .args = { "test" },
  },
  {
    .name = "index_list",
    .args = { "index", "list" },
  },
  {
    .name = "init",
    .args = { "init", "B" },
  },
  {
    .name = "version",
    .args = { "-V" },
  },
  {
    .name = "build_cold_color",
    .project = "test/render/fixtures/ok",
    .args = { "build" },
    .env = { "CLICOLOR_FORCE=1" },
  },
  {
    .name = "build_fail_color",
    .project = "test/render/fixtures/compile_error",
    .args = { "build" },
    .env = { "CLICOLOR_FORCE=1" },
  },
  {
    .name = "build_no_color",
    .project = "test/render/fixtures/ok",
    .args = { "build", "--no-color" },
    .env = { "CLICOLOR_FORCE=1" },
  },
};

static sp_str_t scrub(sp_mem_t mem, fixture_t* fixture, sp_str_t text) {
  text = str_replace_all(mem, text, fixture->root, sp_str_lit("$FIXTURE"));
  return str_replace_all(mem, text, sp_str_replace_c8(mem, fixture->root, '\\', '/'), sp_str_lit("$FIXTURE"));
}

sp_test_each(render, cells, cell_t, cells) {
  fixture_t fixture = sp_zero;
  sp_try(fixture_init(t, &fixture));
  sp_try(prepare_test(t, &fixture, it->project, SP_NULLPTR));

  sp_carr_for(it->setup, step) {
    if (!it->setup[step][0]) {
      break;
    }
    run_spn(t, &fixture, it->setup[step], SP_NULLPTR);
  }

  sp_ps_output_t output = run_spn(t, &fixture, it->args, it->env);

  sp_mem_t mem = fixture.mem;
  sp_str_t dir = sp_fs_join_path(mem, render_out_path(mem, "current"), sp_str_view(it->name));
  write_file(sp_fs_join_path(mem, dir, sp_str_lit("stderr")), scrub(mem, &fixture, output.err));
  write_file(sp_fs_join_path(mem, dir, sp_str_lit("stdout")), scrub(mem, &fixture, output.out));
  write_file(sp_fs_join_path(mem, dir, sp_str_lit("exit")), sp_fmt(mem, "{}\n", sp_fmt_int(output.status.exit_code)).value);
  return SP_OK;
}
