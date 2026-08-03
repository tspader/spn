#include "harness.h"

sp_test(compile_commands, written_on_build) {
  return run_command_test(t, (command_test_t) {
    .project = "test/integration/fixtures/compile_commands/simple",
    .args = { "build" },
    .expect.files = {
      { .file = sp_str_lit("compile_commands.json"), .json = true },
    },
  });
}

sp_test(compile_commands, module_flags) {
  return run_command_test(t, (command_test_t) {
    .project = "test/integration/fixtures/script/build_dep_closure",
    .args = { "build" },
    .expect = {
      .files = {
        { .file = sp_str_lit("compile_commands.json"), .json = true },
      },
      .cc = {
        { .args = { "wasm32-wasi" } },
        { .args = { "store/core/alpha" } },
        { .args = { "store/core/beta" } },
      },
    },
  });
}

sp_test(compile_commands, written_when_compile_fails) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/compile_commands/simple",
    .actions = {
      { .kind = ACTION_CREATE_FILE, .create = { .file = sp_str_lit("main.c"), .content = sp_str_lit("int main( {") } },
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .rc = 1 } },
      { .kind = ACTION_VERIFY_EXISTS, .exists = sp_str_lit("compile_commands.json") },
    },
  });
}
