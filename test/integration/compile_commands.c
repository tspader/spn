#include "common.h"

SPN_TEST_SUITE(compile_commands)

UTEST_F(compile_commands, written_on_build) {
  tmpfs_init_named(&uf->fixture.fs, "compile_commands_written_on_build");

  run_command_test(utest_result, &uf->fixture, (command_test_t) {
    .project = "test/integration/fixtures/compile_commands/simple",
    .args = { "build" },
    .expect.files = {
      { .file = sp_str_lit("compile_commands.json"), .json = true },
    },
  });
}

UTEST_F(compile_commands, module_flags) {
  tmpfs_init_named(&uf->fixture.fs, "compile_commands_module_flags");

  run_command_test(utest_result, &uf->fixture, (command_test_t) {
    .project = "test/integration/fixtures/script/build_dep_closure",
    .args = { "build" },
    .expect.files = {
      { .file = sp_str_lit("compile_commands.json"), .json = true, .contains = {
        "wasm32-wasi",
        "store/core/alpha",
        "store/core/beta",
      } },
    },
  });
}

UTEST_F(compile_commands, written_when_compile_fails) {
  tmpfs_init_named(&uf->fixture.fs, "compile_commands_written_when_compile_fails");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/compile_commands/simple",
    .actions = {
      { .kind = ACTION_CREATE_FILE, .create = { .file = sp_str_lit("main.c"), .content = sp_str_lit("int main( {") } },
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .rc = 1 } },
      { .kind = ACTION_VERIFY_EXISTS, .exists = sp_str_lit("compile_commands.json") },
    },
  });
}
