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
    .cli = t.cmd,
  };

  s32 err = spn_cli_parse_command(&parser);

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

  spn_cli_parser_t parser = {
    .args = (const c8*[]){ "subcmd", "--after" },
    .num_args = 2,
    .cli = {
      .name = "test", .summary = "test",
      .args = {{ .name = "cmd", .kind = SPN_CLI_ARG_KIND_OPTIONAL, .ptr = &bound }},
    },
    .stop_at_non_option = true,
  };

  s32 err = spn_cli_parse_command(&parser);
  EXPECT_EQ(0, err);
  // Parser stopped at "subcmd", did not consume "--after"
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
  return SPN_CLI_CONTINUE;
}

static spn_cli_result_t stub_handler_err(spn_cli_t* cli) {
  (void)cli;
  return SPN_CLI_ERR;
}

typedef struct {
  const c8* args[CLI_TEST_MAX_ARGS];
  u32 num_args;
  spn_cli_usage_t cli;
  spn_cli_result_t expect_result;
} cli_dispatch_test_t;

void expect_dispatch(s32* utest_result, cli_dispatch_test_t t) {
  spn_cli_result_t result = spn_cli_dispatch(&t.cli, SP_NULLPTR, t.args, t.num_args);
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
    .cli = {
      .commands = {
        { .name = "build", .summary = "build project", .handler = stub_handler_done },
      },
    },
    .expect_result = SPN_CLI_DONE,
  });
}

UTEST_F(spn_dispatch, unknown_command) {
  expect_dispatch(&ur, (cli_dispatch_test_t) {
    .args = { "bogus" },
    .num_args = 1,
    .cli = {
      .commands = {
        { .name = "build", .summary = "build project", .handler = stub_handler_done },
      },
    },
    .expect_result = SPN_CLI_ERR,
  });
}

UTEST_F(spn_dispatch, handler_return_value) {
  expect_dispatch(&ur, (cli_dispatch_test_t) {
    .args = { "test" },
    .num_args = 1,
    .cli = {
      .commands = {
        { .name = "test", .summary = "run tests", .handler = stub_handler_continue },
      },
    },
    .expect_result = SPN_CLI_CONTINUE,
  });
}

static spn_cli_subcommand_usage_t tool_subcommands = {
  .commands = {
    { .name = "install", .summary = "install tool", .handler = stub_handler_continue },
    { .name = "uninstall", .summary = "uninstall tool", .handler = stub_handler_done },
  },
};

UTEST_F(spn_dispatch, subcommand_found) {
  expect_dispatch(&ur, (cli_dispatch_test_t) {
    .args = { "tool", "install" },
    .num_args = 2,
    .cli = {
      .commands = {
        { .name = "tool", .summary = "manage tools", .subcommands = &tool_subcommands },
      },
    },
    .expect_result = SPN_CLI_CONTINUE,
  });
}

UTEST_F(spn_dispatch, subcommand_missing) {
  expect_dispatch(&ur, (cli_dispatch_test_t) {
    .args = { "tool" },
    .num_args = 1,
    .cli = {
      .commands = {
        { .name = "tool", .summary = "manage tools", .subcommands = &tool_subcommands },
      },
    },
    .expect_result = SPN_CLI_ERR,
  });
}
