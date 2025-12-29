#define SP_IMPLEMENTATION
#include "sp.h"

#define SPN_CLI_IMPLEMENTATION
#include "cli.h"

#define SP_TEST_IMPLEMENTATION
#include "test.h"
#include "utest.h"

UTEST_MAIN()

#define uf utest_fixture
#define ur (*utest_result)

#define CLI_TEST_MAX_ARGS 8

///////////////
// PARSE CMD //
///////////////
typedef struct {
  const c8* args[CLI_TEST_MAX_ARGS];
  u32 num_args;
  spn_cli_command_usage_t cmd;
  s32 expect_err;
} cli_parse_test_t;

void expect_parse(s32* utest_result, cli_parse_test_t t) {
  spn_cli_parser_t parser = {
    .args = t.args,
    .num_args = t.num_args,
    .cmd = &t.cmd,
  };

  s32 err = spn_cli_parse_opts(&parser, &t.cmd);

  if (t.expect_err) {
    EXPECT_NE(0, err);
  } else {
    EXPECT_EQ(0, err);
  }
}

struct spn_parse_cmd {
  sp_test_file_manager_t fm;
};

UTEST_F_SETUP(spn_parse_cmd) {
  sp_test_file_manager_init(&uf->fm);
}

UTEST_F_TEARDOWN(spn_parse_cmd) {
  sp_test_file_manager_cleanup(&uf->fm);
}

UTEST_F(spn_parse_cmd, empty_args) {
  expect_parse(&ur, (cli_parse_test_t) {
    .cmd = { .name = "test", .summary = "test" },
  });
}

UTEST_F(spn_parse_cmd, required_arg_missing) {
  sp_str_t bound = SP_ZERO_INITIALIZE();
  expect_parse(&ur, (cli_parse_test_t) {
    .cmd = {
      .name = "test", .summary = "test",
      .args = {{ .name = "path", .kind = SPN_CLI_ARG_KIND_REQUIRED, .ptr = &bound }},
    },
    .expect_err = 1,
  });
}

UTEST_F(spn_parse_cmd, required_arg_present) {
  sp_str_t bound = SP_ZERO_INITIALIZE();
  expect_parse(&ur, (cli_parse_test_t) {
    .args = { "foo" },
    .num_args = 1,
    .cmd = {
      .name = "test", .summary = "test",
      .args = {{ .name = "path", .kind = SPN_CLI_ARG_KIND_REQUIRED, .ptr = &bound }},
    },
  });
  EXPECT_TRUE(sp_str_equal(bound, sp_str_lit("foo")));
}

UTEST_F(spn_parse_cmd, optional_arg_missing) {
  sp_str_t bound = SP_ZERO_INITIALIZE();
  expect_parse(&ur, (cli_parse_test_t) {
    .cmd = {
      .name = "test", .summary = "test",
      .args = {{ .name = "path", .kind = SPN_CLI_ARG_KIND_OPTIONAL, .ptr = &bound }},
    },
  });
  EXPECT_TRUE(sp_str_empty(bound));
}

UTEST_F(spn_parse_cmd, optional_arg_present) {
  sp_str_t bound = SP_ZERO_INITIALIZE();
  expect_parse(&ur, (cli_parse_test_t) {
    .args = { "bar" },
    .num_args = 1,
    .cmd = {
      .name = "test", .summary = "test",
      .args = {{ .name = "path", .kind = SPN_CLI_ARG_KIND_OPTIONAL, .ptr = &bound }},
    },
  });
  EXPECT_TRUE(sp_str_equal(bound, sp_str_lit("bar")));
}

UTEST_F(spn_parse_cmd, bool_opt_short) {
  bool verbose = false;
  expect_parse(&ur, (cli_parse_test_t) {
    .args = { "-v" },
    .num_args = 1,
    .cmd = {
      .name = "test", .summary = "test",
      .opts = {{ .brief = "v", .name = "verbose", .kind = SPN_CLI_OPT_KIND_BOOLEAN, .ptr = &verbose }},
    },
  });
  EXPECT_TRUE(verbose);
}

UTEST_F(spn_parse_cmd, bool_opt_long) {
  bool verbose = false;
  expect_parse(&ur, (cli_parse_test_t) {
    .args = { "--verbose" },
    .num_args = 1,
    .cmd = {
      .name = "test", .summary = "test",
      .opts = {{ .brief = "v", .name = "verbose", .kind = SPN_CLI_OPT_KIND_BOOLEAN, .ptr = &verbose }},
    },
  });
  EXPECT_TRUE(verbose);
}

UTEST_F(spn_parse_cmd, string_opt_short) {
  sp_str_t output = SP_ZERO_INITIALIZE();
  expect_parse(&ur, (cli_parse_test_t) {
    .args = { "-o", "foo" },
    .num_args = 2,
    .cmd = {
      .name = "test", .summary = "test",
      .opts = {{ .brief = "o", .name = "output", .kind = SPN_CLI_OPT_KIND_STRING, .ptr = &output }},
    },
  });
  EXPECT_TRUE(sp_str_equal(output, sp_str_lit("foo")));
}

UTEST_F(spn_parse_cmd, string_opt_long_eq) {
  sp_str_t output = SP_ZERO_INITIALIZE();
  expect_parse(&ur, (cli_parse_test_t) {
    .args = { "--output=bar" },
    .num_args = 1,
    .cmd = {
      .name = "test", .summary = "test",
      .opts = {{ .brief = "o", .name = "output", .kind = SPN_CLI_OPT_KIND_STRING, .ptr = &output }},
    },
  });
  EXPECT_TRUE(sp_str_equal(output, sp_str_lit("bar")));
}

UTEST_F(spn_parse_cmd, string_opt_long_space) {
  sp_str_t output = SP_ZERO_INITIALIZE();
  expect_parse(&ur, (cli_parse_test_t) {
    .args = { "--output", "baz" },
    .num_args = 2,
    .cmd = {
      .name = "test", .summary = "test",
      .opts = {{ .brief = "o", .name = "output", .kind = SPN_CLI_OPT_KIND_STRING, .ptr = &output }},
    },
  });
  EXPECT_TRUE(sp_str_equal(output, sp_str_lit("baz")));
}

UTEST_F(spn_parse_cmd, int_opt) {
  s64 jobs = 0;
  expect_parse(&ur, (cli_parse_test_t) {
    .args = { "--jobs", "4" },
    .num_args = 2,
    .cmd = {
      .name = "test", .summary = "test",
      .opts = {{ .brief = "j", .name = "jobs", .kind = SPN_CLI_OPT_KIND_INTEGER, .ptr = &jobs }},
    },
  });
  EXPECT_EQ(4, jobs);
}

UTEST_F(spn_parse_cmd, mixed_opts_args) {
  bool verbose = false;
  sp_str_t path = SP_ZERO_INITIALIZE();
  sp_str_t output = SP_ZERO_INITIALIZE();

  expect_parse(&ur, (cli_parse_test_t) {
    .args = { "--verbose", "input.c", "--output", "out.o" },
    .num_args = 4,
    .cmd = {
      .name = "test", .summary = "test",
      .opts = {
        { .brief = "v", .name = "verbose", .kind = SPN_CLI_OPT_KIND_BOOLEAN, .ptr = &verbose },
        { .brief = "o", .name = "output", .kind = SPN_CLI_OPT_KIND_STRING, .ptr = &output },
      },
      .args = {{ .name = "path", .kind = SPN_CLI_ARG_KIND_REQUIRED, .ptr = &path }},
    },
  });
  EXPECT_TRUE(verbose);
  EXPECT_TRUE(sp_str_equal(path, sp_str_lit("input.c")));
  EXPECT_TRUE(sp_str_equal(output, sp_str_lit("out.o")));
}

UTEST_F(spn_parse_cmd, unknown_opt_error) {
  expect_parse(&ur, (cli_parse_test_t) {
    .args = { "--bogus" },
    .num_args = 1,
    .cmd = { .name = "test", .summary = "test" },
    .expect_err = 1,
  });
}

UTEST_F(spn_parse_cmd, stop_at_non_option) {
  sp_str_t bound = SP_ZERO_INITIALIZE();

  spn_cli_command_usage_t cmd = {
    .name = "test", .summary = "test",
    .args = {{ .name = "cmd", .kind = SPN_CLI_ARG_KIND_OPTIONAL, .ptr = &bound }},
  };

  spn_cli_parser_t parser = {
    .args = (const c8*[]){ "subcmd", "--after" },
    .num_args = 2,
    .stop_at_non_option = true,
  };

  s32 err = spn_cli_parse_opts(&parser, &cmd);
  EXPECT_EQ(0, err);
  EXPECT_EQ(0, parser.num_positionals);
  EXPECT_EQ(0, parser.it);
}

//////////////
// DISPATCH //
//////////////

static spn_cli_result_t stub_handler_done(spn_cli_t* cli) {
  (void)cli;
  return SPN_CLI_DONE;
}

static spn_cli_result_t stub_handler_continue(spn_cli_t* cli) {
  (void)cli;
  return SPN_CLI_DONE;
}

static spn_cli_result_t stub_handler_err(spn_cli_t* cli) {
  (void)cli;
  return SPN_CLI_ERR;
}

typedef struct {
  const c8* args[CLI_TEST_MAX_ARGS];
  u32 num_args;
  spn_cli_command_usage_t cmd;
  spn_cli_result_t expect_result;
} cli_dispatch_test_t;

void expect_dispatch(s32* utest_result, cli_dispatch_test_t t) {
  spn_cli_result_t result = spn_cli_run(&t.cmd, SP_NULLPTR, t.args, t.num_args);
  EXPECT_EQ(t.expect_result, result);
}

struct spn_dispatch {
  sp_test_file_manager_t fm;
};

UTEST_F_SETUP(spn_dispatch) {
  sp_test_file_manager_init(&uf->fm);
}

UTEST_F_TEARDOWN(spn_dispatch) {
  sp_test_file_manager_cleanup(&uf->fm);
}

UTEST_F(spn_dispatch, known_command) {
  expect_dispatch(&ur, (cli_dispatch_test_t) {
    .args = { "build" },
    .num_args = 1,
    .cmd = {
      .commands = (spn_cli_command_usage_t[]) {
        { .name = "build", .summary = "build project", .handler = stub_handler_done },
        { 0 },
      },
    },
    .expect_result = SPN_CLI_DONE,
  });
}

UTEST_F(spn_dispatch, unknown_command) {
  expect_dispatch(&ur, (cli_dispatch_test_t) {
    .args = { "bogus" },
    .num_args = 1,
    .cmd = {
      .commands = (spn_cli_command_usage_t[]) {
        { .name = "build", .summary = "build project", .handler = stub_handler_done },
        { 0 },
      },
    },
    .expect_result = SPN_CLI_ERR,
  });
}

UTEST_F(spn_dispatch, handler_return_value) {
  expect_dispatch(&ur, (cli_dispatch_test_t) {
    .args = { "test" },
    .num_args = 1,
    .cmd = {
      .commands = (spn_cli_command_usage_t[]) {
        { .name = "test", .summary = "run tests", .handler = stub_handler_continue },
        { 0 },
      },
    },
    .expect_result = SPN_CLI_DONE,
  });
}

static spn_cli_command_usage_t tool_subcommands[] = {
  { .name = "install", .summary = "install tool", .handler = stub_handler_continue },
  { .name = "uninstall", .summary = "uninstall tool", .handler = stub_handler_done },
  { 0 },
};

UTEST_F(spn_dispatch, subcommand_found) {
  expect_dispatch(&ur, (cli_dispatch_test_t) {
    .args = { "tool", "install" },
    .num_args = 2,
    .cmd = {
      .commands = (spn_cli_command_usage_t[]) {
        { .name = "tool", .summary = "manage tools", .commands = tool_subcommands },
        { 0 },
      },
    },
    .expect_result = SPN_CLI_DONE,
  });
}

UTEST_F(spn_dispatch, subcommand_missing) {
  expect_dispatch(&ur, (cli_dispatch_test_t) {
    .args = { "tool" },
    .num_args = 1,
    .cmd = {
      .commands = (spn_cli_command_usage_t[]) {
        { .name = "tool", .summary = "manage tools", .commands = tool_subcommands },
        { 0 },
      },
    },
    .expect_result = SPN_CLI_ERR,
  });
}

//////////////////////
// DISPATCH_CMD     //
//////////////////////

// Test: root command has opts, parses them before dispatching to subcommand
UTEST_F(spn_dispatch, cmd_with_opts) {
  bool verbose = false;
  spn_cli_command_usage_t root = {
    .name = "root",
    .opts = {
      { .brief = "v", .name = "verbose", .kind = SPN_CLI_OPT_KIND_BOOLEAN, .ptr = &verbose },
    },
    .commands = (spn_cli_command_usage_t[]) {
      { .name = "build", .handler = stub_handler_done },
      { 0 },
    },
  };

  const c8* args[] = { "--verbose", "build" };
  spn_cli_result_t result = spn_cli_run(&root, SP_NULLPTR, args, 2);

  EXPECT_EQ(SPN_CLI_DONE, result);
  EXPECT_TRUE(verbose);
}

// Test: three levels deep (root -> tool -> install)
UTEST_F(spn_dispatch, nested_commands) {
  spn_cli_command_usage_t root = {
    .name = "root",
    .commands = (spn_cli_command_usage_t[]) {
      {
        .name = "tool",
        .commands = (spn_cli_command_usage_t[]) {
          { .name = "install", .handler = stub_handler_continue },
          { 0 },
        },
      },
      { 0 },
    },
  };

  const c8* args[] = { "tool", "install" };
  spn_cli_result_t result = spn_cli_run(&root, SP_NULLPTR, args, 2);

  EXPECT_EQ(SPN_CLI_DONE, result);
}

// Test: opts parsed at root level, then at subcommand level
UTEST_F(spn_dispatch, opts_at_each_level) {
  bool root_opt = false;
  bool sub_opt = false;

  spn_cli_command_usage_t root = {
    .name = "root",
    .opts = {
      { .brief = "r", .name = "root-flag", .kind = SPN_CLI_OPT_KIND_BOOLEAN, .ptr = &root_opt },
    },
    .commands = (spn_cli_command_usage_t[]) {
      {
        .name = "sub",
        .opts = {
          { .brief = "s", .name = "sub-flag", .kind = SPN_CLI_OPT_KIND_BOOLEAN, .ptr = &sub_opt },
        },
        .handler = stub_handler_done,
      },
      { 0 },
    },
  };

  const c8* args[] = { "--root-flag", "sub", "--sub-flag" };
  spn_cli_result_t result = spn_cli_run(&root, SP_NULLPTR, args, 3);

  EXPECT_EQ(SPN_CLI_DONE, result);
  EXPECT_TRUE(root_opt);
  EXPECT_TRUE(sub_opt);
}

// Test: command with opts and handler (no subcommands)
UTEST_F(spn_dispatch, root_handler) {
  bool flag = false;
  sp_str_t target = SP_ZERO_INITIALIZE();

  spn_cli_command_usage_t root = {
    .name = "build",
    .opts = {
      { .brief = "f", .name = "flag", .kind = SPN_CLI_OPT_KIND_BOOLEAN, .ptr = &flag },
      { .brief = "t", .name = "target", .kind = SPN_CLI_OPT_KIND_STRING, .ptr = &target },
    },
    .handler = stub_handler_done,
  };

  const c8* args[] = { "--flag", "--target", "foo" };
  spn_cli_result_t result = spn_cli_run(&root, SP_NULLPTR, args, 3);

  EXPECT_EQ(SPN_CLI_DONE, result);
  EXPECT_TRUE(flag);
  EXPECT_TRUE(sp_str_equal(target, sp_str_lit("foo")));
}

UTEST_F(spn_parse_cmd, multiple_positionals) {
  sp_str_t arg1 = SP_ZERO_INITIALIZE();
  sp_str_t arg2 = SP_ZERO_INITIALIZE();

  expect_parse(&ur, (cli_parse_test_t) {
    .args = { "first", "second" },
    .num_args = 2,
    .cmd = {
      .name = "test", .summary = "test",
      .args = {
        { .name = "arg1", .kind = SPN_CLI_ARG_KIND_REQUIRED, .ptr = &arg1 },
        { .name = "arg2", .kind = SPN_CLI_ARG_KIND_REQUIRED, .ptr = &arg2 },
      },
    },
  });
  EXPECT_TRUE(sp_str_equal(arg1, sp_str_lit("first")));
  EXPECT_TRUE(sp_str_equal(arg2, sp_str_lit("second")));
}

UTEST_F(spn_parse_cmd, opt_after_positional) {
  bool verbose = false;
  sp_str_t file = SP_ZERO_INITIALIZE();

  expect_parse(&ur, (cli_parse_test_t) {
    .args = { "input.c", "--verbose" },
    .num_args = 2,
    .cmd = {
      .name = "test", .summary = "test",
      .opts = {{ .brief = "v", .name = "verbose", .kind = SPN_CLI_OPT_KIND_BOOLEAN, .ptr = &verbose }},
      .args = {{ .name = "file", .kind = SPN_CLI_ARG_KIND_REQUIRED, .ptr = &file }},
    },
  });
  EXPECT_TRUE(verbose);
  EXPECT_TRUE(sp_str_equal(file, sp_str_lit("input.c")));
}

static spn_cli_t* captured_cli = SP_NULLPTR;
static spn_cli_result_t capture_handler(spn_cli_t* cli) {
  captured_cli = cli;
  return SPN_CLI_DONE;
}

UTEST_F(spn_dispatch, handler_receives_user_data) {
  captured_cli = SP_NULLPTR;
  int dummy_cli_data = 42;  // just need any pointer

  spn_cli_command_usage_t root = {
    .name = "test",
    .handler = capture_handler,
  };

  const c8* args[] = {};
  spn_cli_run(&root, (spn_cli_t*)&dummy_cli_data, args, 0);

  EXPECT_EQ((spn_cli_t*)&dummy_cli_data, captured_cli);
}

UTEST_F(spn_dispatch, no_handler_no_subcommands) {
  spn_cli_command_usage_t root = {
    .name = "test",
    // no handler, no commands
  };

  const c8* args[] = {};
  spn_cli_result_t result = spn_cli_run(&root, SP_NULLPTR, args, 0);

  EXPECT_EQ(SPN_CLI_ERR, result);
}

UTEST_F(spn_parse_cmd, bool_opt_short_no_consume) {
  bool force = false;
  sp_str_t pkg = SP_ZERO_INITIALIZE();

  expect_parse(&ur, (cli_parse_test_t) {
    .args = { "-f", "pkg" },
    .num_args = 2,
    .cmd = {
      .name = "test", .summary = "test",
      .opts = {{ .brief = "f", .name = "force", .kind = SPN_CLI_OPT_KIND_BOOLEAN, .ptr = &force }},
      .args = {{ .name = "package", .kind = SPN_CLI_ARG_KIND_REQUIRED, .ptr = &pkg }},
    },
  });
  EXPECT_TRUE(force);
  EXPECT_TRUE(sp_str_equal(pkg, sp_str_lit("pkg")));
}
