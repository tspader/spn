#define SP_IMPLEMENTATION
#include "sp.h"

#define SP_CLI_IMPLEMENTATION
#include "sp_cli.h"

#include "utest.h"

UTEST(sp_cli, match_opt_by_long_name) {
  sp_cli_command_usage_t cmd = {
    .opts = {
      { .brief = "f", .name = "force", .kind = SP_CLI_OPT_KIND_BOOLEAN },
      { .brief = "v", .name = "verbose", .kind = SP_CLI_OPT_KIND_BOOLEAN },
      { .brief = "o", .name = "output", .kind = SP_CLI_OPT_KIND_STRING },
    }
  };

  sp_cli_opt_usage_t* opt = sp_cli_match_opt(&cmd, sp_str_lit("force"));
  ASSERT_NE(opt, NULL);
  ASSERT_STREQ(opt->name, "force");
  ASSERT_STREQ(opt->brief, "f");
}

UTEST(sp_cli, match_opt_not_found) {
  sp_cli_command_usage_t cmd = {
    .opts = {
      { .brief = "f", .name = "force", .kind = SP_CLI_OPT_KIND_BOOLEAN },
    }
  };

  sp_cli_opt_usage_t* opt = sp_cli_match_opt(&cmd, sp_str_lit("unknown"));
  ASSERT_EQ(opt, NULL);
}

UTEST(sp_cli, match_opt_empty_command) {
  sp_cli_command_usage_t cmd = { .opts = {} };

  sp_cli_opt_usage_t* opt = sp_cli_match_opt(&cmd, sp_str_lit("force"));
  ASSERT_EQ(opt, NULL);
}

UTEST(sp_cli, match_brief_opt) {
  sp_cli_command_usage_t cmd = {
    .opts = {
      { .brief = "f", .name = "force", .kind = SP_CLI_OPT_KIND_BOOLEAN },
      { .brief = "v", .name = "verbose", .kind = SP_CLI_OPT_KIND_BOOLEAN },
    }
  };

  sp_cli_opt_usage_t* opt = sp_cli_match_brief_opt(&cmd, 'f');
  ASSERT_NE(opt, NULL);
  ASSERT_STREQ(opt->name, "force");
}

UTEST(sp_cli, match_brief_opt_not_found) {
  sp_cli_command_usage_t cmd = {
    .opts = {
      { .brief = "f", .name = "force", .kind = SP_CLI_OPT_KIND_BOOLEAN },
    }
  };

  sp_cli_opt_usage_t* opt = sp_cli_match_brief_opt(&cmd, 'x');
  ASSERT_EQ(opt, NULL);
}

UTEST(sp_cli, parse_opt_value_with_equals) {
  c8* argv[] = { "ignored" };
  sp_cli_parser_t p = { .argv = argv, .argc = 1, .it = 0 };

  sp_str_t val = sp_cli_parse_opt_value(&p, sp_str_lit("=bar"));

  ASSERT_TRUE(sp_str_equal_cstr(val, "bar"));
  ASSERT_EQ(p.it, 0);
}

UTEST(sp_cli, parse_opt_value_next_arg) {
  c8* argv[] = { "myvalue" };
  sp_cli_parser_t p = { .argv = argv, .argc = 1, .it = 0 };

  sp_str_t val = sp_cli_parse_opt_value(&p, (sp_str_t){0});

  ASSERT_TRUE(sp_str_equal_cstr(val, "myvalue"));
  ASSERT_EQ(p.it, 1);
}

UTEST(sp_cli, parse_opt_value_next_arg_is_option) {
  c8* argv[] = { "-f" };
  sp_cli_parser_t p = { .argv = argv, .argc = 1, .it = 0 };

  sp_str_t val = sp_cli_parse_opt_value(&p, (sp_str_t){0});

  ASSERT_EQ(val.len, 0);
  ASSERT_EQ(p.it, 0);
}

UTEST(sp_cli, parse_opt_value_no_next_arg) {
  sp_cli_parser_t p = { .argc = 0, .it = 0 };

  sp_str_t val = sp_cli_parse_opt_value(&p, (sp_str_t){0});

  ASSERT_EQ(val.len, 0);
}

UTEST(sp_cli, parse_long_opt_boolean) {
  bool force = false;
  c8* argv[] = { "--force" };
  sp_cli_parser_t p = {
    .argv = argv,
    .argc = 1,
    .it = 0,
    .cli = {
      .opts = {
        { .name = "force", .kind = SP_CLI_OPT_KIND_BOOLEAN, .ptr = &force }
      }
    }
  };

  bool ok = sp_cli_parse_long_opt(&p, sp_str_from_cstr(argv[0]));

  ASSERT_TRUE(ok);
  ASSERT_TRUE(force);
  ASSERT_EQ(p.it, 1);
}

UTEST(sp_cli, parse_long_opt_with_equals) {
  sp_str_t output = sp_str_lit("");
  c8* argv[] = { "--output=file.txt" };
  sp_cli_parser_t p = {
    .argv = argv,
    .argc = 1,
    .it = 0,
    .cli = {
      .opts = {
        { .name = "output", .kind = SP_CLI_OPT_KIND_STRING, .ptr = &output }
      }
    }
  };

  bool ok = sp_cli_parse_long_opt(&p, sp_str_from_cstr(argv[0]));

  ASSERT_TRUE(ok);
  ASSERT_TRUE(sp_str_equal_cstr(output, "file.txt"));
  ASSERT_EQ(p.it, 1);
}

UTEST(sp_cli, parse_long_opt_next_arg) {
  sp_str_t output = sp_str_lit("");
  c8* argv[] = { "--output", "file.txt" };
  sp_cli_parser_t p = {
    .argv = argv,
    .argc = 2,
    .it = 0,
    .cli = {
      .opts = {
        { .name = "output", .kind = SP_CLI_OPT_KIND_STRING, .ptr = &output }
      }
    }
  };

  bool ok = sp_cli_parse_long_opt(&p, sp_str_from_cstr(argv[0]));

  ASSERT_TRUE(ok);
  ASSERT_TRUE(sp_str_equal_cstr(output, "file.txt"));
  ASSERT_EQ(p.it, 2);
}

UTEST(sp_cli, parse_long_opt_unknown) {
  c8* argv[] = { "--unknown" };
  sp_cli_parser_t p = {
    .argv = argv,
    .argc = 1,
    .it = 0,
    .cli = { .opts = {} }
  };

  bool ok = sp_cli_parse_long_opt(&p, sp_str_from_cstr(argv[0]));

  ASSERT_FALSE(ok);
}

UTEST(sp_cli, parse_short_opt_single) {
  bool force = false;
  c8* argv[] = { "-f" };
  sp_cli_parser_t p = {
    .argv = argv,
    .argc = 1,
    .it = 0,
    .cli = {
      .opts = {
        { .brief = "f", .kind = SP_CLI_OPT_KIND_BOOLEAN, .ptr = &force }
      }
    }
  };

  bool ok = sp_cli_parse_short_opts(&p, sp_str_from_cstr(argv[0]));

  ASSERT_TRUE(ok);
  ASSERT_TRUE(force);
  ASSERT_EQ(p.it, 1);
}

UTEST(sp_cli, parse_short_opt_chained) {
  bool force = false;
  bool verbose = false;
  c8* argv[] = { "-fv" };
  sp_cli_parser_t p = {
    .argv = argv,
    .argc = 1,
    .it = 0,
    .cli = {
      .opts = {
        { .brief = "f", .kind = SP_CLI_OPT_KIND_BOOLEAN, .ptr = &force },
        { .brief = "v", .kind = SP_CLI_OPT_KIND_BOOLEAN, .ptr = &verbose }
      }
    }
  };

  bool ok = sp_cli_parse_short_opts(&p, sp_str_from_cstr(argv[0]));

  ASSERT_TRUE(ok);
  ASSERT_TRUE(force);
  ASSERT_TRUE(verbose);
}

UTEST(sp_cli, parse_short_opt_valued) {
  sp_str_t output = sp_str_lit("");
  c8* argv[] = { "-o", "file.txt" };
  sp_cli_parser_t p = {
    .argv = argv,
    .argc = 2,
    .it = 0,
    .cli = {
      .opts = {
        { .brief = "o", .kind = SP_CLI_OPT_KIND_STRING, .ptr = &output }
      }
    }
  };

  bool ok = sp_cli_parse_short_opts(&p, sp_str_from_cstr(argv[0]));

  ASSERT_TRUE(ok);
  ASSERT_TRUE(sp_str_equal_cstr(output, "file.txt"));
  ASSERT_EQ(p.it, 2);
}

UTEST(sp_cli, parse_short_opt_unknown) {
  c8* argv[] = { "-x" };
  sp_cli_parser_t p = {
    .argv = argv,
    .argc = 1,
    .it = 0,
    .cli = { .opts = {} }
  };

  bool ok = sp_cli_parse_short_opts(&p, sp_str_from_cstr(argv[0]));

  ASSERT_FALSE(ok);
}

UTEST(sp_cli, bind_positionals_required) {
  sp_str_t package = sp_str_lit("");
  sp_cli_parser_t p = {
    .positionals = { sp_str_lit("mypackage") },
    .num_positionals = 1,
    .cli = {
      .args = {
        { .name = "package", .kind = SP_CLI_ARG_KIND_REQUIRED, .ptr = &package }
      }
    }
  };

  bool ok = sp_cli_bind_positionals(&p);

  ASSERT_TRUE(ok);
  ASSERT_TRUE(sp_str_equal_cstr(package, "mypackage"));
}

UTEST(sp_cli, bind_positionals_missing_required) {
  sp_str_t package = sp_str_lit("");
  sp_cli_parser_t p = {
    .num_positionals = 0,
    .cli = {
      .args = {
        { .name = "package", .kind = SP_CLI_ARG_KIND_REQUIRED, .ptr = &package }
      }
    }
  };

  bool ok = sp_cli_bind_positionals(&p);

  ASSERT_FALSE(ok);
}

UTEST(sp_cli, bind_positionals_optional_missing) {
  sp_str_t target = sp_str_lit("");
  sp_cli_parser_t p = {
    .num_positionals = 0,
    .cli = {
      .args = {
        { .name = "target", .kind = SP_CLI_ARG_KIND_OPTIONAL, .ptr = &target }
      }
    }
  };

  bool ok = sp_cli_bind_positionals(&p);

  ASSERT_TRUE(ok);
}

UTEST(sp_cli, parse_command_long_opts) {
  bool force = false;
  sp_str_t output = sp_str_lit("");
  c8* argv[] = { "--force", "--output", "file.txt" };
  sp_cli_parser_t p = {
    .argv = argv,
    .argc = 3,
    .cli = {
      .opts = {
        { .name = "force", .kind = SP_CLI_OPT_KIND_BOOLEAN, .ptr = &force },
        { .name = "output", .kind = SP_CLI_OPT_KIND_STRING, .ptr = &output }
      }
    }
  };

  bool ok = sp_cli_parse_command(&p);

  ASSERT_TRUE(ok);
  ASSERT_TRUE(force);
  ASSERT_TRUE(sp_str_equal_cstr(output, "file.txt"));
}

UTEST(sp_cli, parse_command_positionals) {
  sp_str_t package = sp_str_lit("");
  c8* argv[] = { "mypackage" };
  sp_cli_parser_t p = {
    .argv = argv,
    .argc = 1,
    .cli = {
      .args = {
        { .name = "package", .kind = SP_CLI_ARG_KIND_REQUIRED, .ptr = &package }
      }
    }
  };

  bool ok = sp_cli_parse_command(&p);

  ASSERT_TRUE(ok);
  ASSERT_TRUE(sp_str_equal_cstr(package, "mypackage"));
}

UTEST(sp_cli, parse_command_mixed) {
  bool force = false;
  sp_str_t package = sp_str_lit("");
  c8* argv[] = { "--force", "mypackage" };
  sp_cli_parser_t p = {
    .argv = argv,
    .argc = 2,
    .cli = {
      .opts = {
        { .name = "force", .kind = SP_CLI_OPT_KIND_BOOLEAN, .ptr = &force }
      },
      .args = {
        { .name = "package", .kind = SP_CLI_ARG_KIND_REQUIRED, .ptr = &package }
      }
    }
  };

  bool ok = sp_cli_parse_command(&p);

  ASSERT_TRUE(ok);
  ASSERT_TRUE(force);
  ASSERT_TRUE(sp_str_equal_cstr(package, "mypackage"));
}

UTEST(sp_cli, parse_command_double_dash) {
  sp_str_t arg1 = sp_str_lit("");
  sp_str_t arg2 = sp_str_lit("");
  c8* argv[] = { "--", "--not-a-flag", "-f" };
  sp_cli_parser_t p = {
    .argv = argv,
    .argc = 3,
    .cli = {
      .args = {
        { .name = "arg1", .kind = SP_CLI_ARG_KIND_REQUIRED, .ptr = &arg1 },
        { .name = "arg2", .kind = SP_CLI_ARG_KIND_REQUIRED, .ptr = &arg2 }
      }
    }
  };

  bool ok = sp_cli_parse_command(&p);

  ASSERT_TRUE(ok);
  ASSERT_TRUE(sp_str_equal_cstr(arg1, "--not-a-flag"));
  ASSERT_TRUE(sp_str_equal_cstr(arg2, "-f"));
}

UTEST(sp_cli, parse_command_stop_at_non_option) {
  bool force = false;
  c8* argv[] = { "--force", "subcommand", "--other" };
  sp_cli_parser_t p = {
    .argv = argv,
    .argc = 3,
    .stop_at_non_option = true,
    .cli = {
      .opts = {
        { .name = "force", .kind = SP_CLI_OPT_KIND_BOOLEAN, .ptr = &force }
      }
    }
  };

  bool ok = sp_cli_parse_command(&p);

  ASSERT_TRUE(ok);
  ASSERT_TRUE(force);
  ASSERT_EQ(p.it, 1);
}

UTEST(sp_cli, parse_command_unknown_option) {
  c8* argv[] = { "--unknown" };
  sp_cli_parser_t p = {
    .argv = argv,
    .argc = 1,
    .cli = { .opts = {} }
  };

  bool ok = sp_cli_parse_command(&p);

  ASSERT_FALSE(ok);
}

UTEST_MAIN();
