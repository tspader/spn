#include "spn.c"

#include "utest.h"

#ifndef sp_carr_len
#define sp_carr_len(a) (sizeof(a)/sizeof((a)[0]))
#endif

UTEST(spn_cli, parse_build_command_with_force) {
  spn_cli_t cli = SP_ZERO_INITIALIZE();

  const c8* argv[] = {
    "spn",
    "build",
    "--force",
    "--file",
    "foo/spn.toml"
  };

  sp_cli_usage_t usage = spn_build_cli_usage(&cli);

  sp_cli_command_usage_t* match = SP_NULLPTR;
  bool ok = sp_cli_parse(&usage, sp_carr_len(argv), (c8**)argv, &match);

  EXPECT_TRUE(ok);
  EXPECT_NE(match, SP_NULLPTR);
  EXPECT_STREQ(match->name, "build");
  EXPECT_EQ(cli.cmd, SPN_CLI_BUILD);
  EXPECT_TRUE(cli.build.force);
  EXPECT_TRUE(sp_str_equal_cstr(cli.project_file, "foo/spn.toml"));
}

UTEST(spn_cli, parse_tool_command_without_subcommand_should_be_parser_error) {
  spn_cli_t cli = SP_ZERO_INITIALIZE();

  const c8* argv[] = {
    "spn",
    "tool"
  };

  sp_cli_usage_t usage = spn_build_cli_usage(&cli);

  sp_cli_command_usage_t* match = SP_NULLPTR;
  bool ok = sp_cli_parse(&usage, sp_carr_len(argv), (c8**)argv, &match);

  EXPECT_FALSE(ok);
  EXPECT_NE(match, SP_NULLPTR);
  EXPECT_STREQ(match->name, "tool");
}

UTEST(spn_cli, parse_help_flag_should_exit_success) {
  spn_cli_t cli = SP_ZERO_INITIALIZE();

  const c8* argv[] = {
    "spn",
    "--help"
  };

  sp_cli_usage_t usage = spn_build_cli_usage(&cli);

  sp_cli_command_usage_t* match = SP_NULLPTR;
  bool ok = sp_cli_parse(&usage, sp_carr_len(argv), (c8**)argv, &match);

  EXPECT_TRUE(ok);
  EXPECT_TRUE(cli.help);
}

UTEST(spn_cli, parse_version_flag_should_exit_success) {
  spn_cli_t cli = SP_ZERO_INITIALIZE();

  const c8* argv[] = {
    "spn",
    "--version"
  };

  sp_cli_usage_t usage = spn_build_cli_usage(&cli);

  sp_cli_command_usage_t* match = SP_NULLPTR;
  bool ok = sp_cli_parse(&usage, sp_carr_len(argv), (c8**)argv, &match);

  EXPECT_TRUE(ok);
  EXPECT_TRUE(cli.version);
}

UTEST(spn_cli, parse_global_project_dir) {
  spn_cli_t cli = SP_ZERO_INITIALIZE();
  const c8* argv[] = { "spn", "-C", "/tmp/proj", "list" };
  sp_cli_usage_t usage = spn_build_cli_usage(&cli);
  sp_cli_command_usage_t* match = SP_NULLPTR;
  bool ok = sp_cli_parse(&usage, sp_carr_len(argv), (c8**)argv, &match);
  EXPECT_TRUE(ok);
  EXPECT_TRUE(sp_str_equal_cstr(cli.project_directory, "/tmp/proj"));
  EXPECT_EQ(cli.cmd, SPN_CLI_LIST);
}

UTEST(spn_cli, parse_global_output) {
  spn_cli_t cli = SP_ZERO_INITIALIZE();
  const c8* argv[] = { "spn", "--output", "json", "list" };
  sp_cli_usage_t usage = spn_build_cli_usage(&cli);
  sp_cli_command_usage_t* match = SP_NULLPTR;
  bool ok = sp_cli_parse(&usage, sp_carr_len(argv), (c8**)argv, &match);
  EXPECT_TRUE(ok);
  EXPECT_TRUE(sp_str_equal_cstr(cli.output, "json"));
}

UTEST(spn_cli, parse_init_bare) {
  spn_cli_t cli = SP_ZERO_INITIALIZE();
  const c8* argv[] = { "spn", "init", "--bare" };
  sp_cli_usage_t usage = spn_build_cli_usage(&cli);
  sp_cli_command_usage_t* match = SP_NULLPTR;
  bool ok = sp_cli_parse(&usage, sp_carr_len(argv), (c8**)argv, &match);
  EXPECT_TRUE(ok);
  EXPECT_EQ(cli.cmd, SPN_CLI_INIT);
  EXPECT_TRUE(cli.init.bare);
}

UTEST(spn_cli, parse_list) {
  spn_cli_t cli = SP_ZERO_INITIALIZE();
  const c8* argv[] = { "spn", "list" };
  sp_cli_usage_t usage = spn_build_cli_usage(&cli);
  sp_cli_command_usage_t* match = SP_NULLPTR;
  bool ok = sp_cli_parse(&usage, sp_carr_len(argv), (c8**)argv, &match);
  EXPECT_TRUE(ok);
  EXPECT_EQ(cli.cmd, SPN_CLI_LIST);
}

UTEST(spn_cli, parse_add_package) {
  spn_cli_t cli = SP_ZERO_INITIALIZE();
  const c8* argv[] = { "spn", "add", "pkg" };
  sp_cli_usage_t usage = spn_build_cli_usage(&cli);
  sp_cli_command_usage_t* match = SP_NULLPTR;
  bool ok = sp_cli_parse(&usage, sp_carr_len(argv), (c8**)argv, &match);
  EXPECT_TRUE(ok);
  EXPECT_EQ(cli.cmd, SPN_CLI_ADD);
  EXPECT_TRUE(sp_str_equal_cstr(cli.add.package, "pkg"));
}

UTEST(spn_cli, parse_copy_directory) {
  spn_cli_t cli = SP_ZERO_INITIALIZE();
  const c8* argv[] = { "spn", "copy", "dist" };
  sp_cli_usage_t usage = spn_build_cli_usage(&cli);
  sp_cli_command_usage_t* match = SP_NULLPTR;
  bool ok = sp_cli_parse(&usage, sp_carr_len(argv), (c8**)argv, &match);
  EXPECT_TRUE(ok);
  EXPECT_EQ(cli.cmd, SPN_CLI_COPY);
  EXPECT_TRUE(sp_str_equal_cstr(cli.copy.directory, "dist"));
}

UTEST(spn_cli, parse_update_package) {
  spn_cli_t cli = SP_ZERO_INITIALIZE();
  const c8* argv[] = { "spn", "update", "pkg" };
  sp_cli_usage_t usage = spn_build_cli_usage(&cli);
  sp_cli_command_usage_t* match = SP_NULLPTR;
  bool ok = sp_cli_parse(&usage, sp_carr_len(argv), (c8**)argv, &match);
  EXPECT_TRUE(ok);
  EXPECT_EQ(cli.cmd, SPN_CLI_UPDATE);
  EXPECT_TRUE(sp_str_equal_cstr(cli.update.package, "pkg"));
}

UTEST(spn_cli, parse_manifest_package) {
  spn_cli_t cli = SP_ZERO_INITIALIZE();
  const c8* argv[] = { "spn", "manifest", "pkg" };
  sp_cli_usage_t usage = spn_build_cli_usage(&cli);
  sp_cli_command_usage_t* match = SP_NULLPTR;
  bool ok = sp_cli_parse(&usage, sp_carr_len(argv), (c8**)argv, &match);
  EXPECT_TRUE(ok);
  EXPECT_EQ(cli.cmd, SPN_CLI_MANIFEST);
  EXPECT_TRUE(sp_str_equal_cstr(cli.manifest.package, "pkg"));
}

UTEST(spn_cli, parse_build_args) {
  spn_cli_t cli = SP_ZERO_INITIALIZE();
  const c8* argv[] = { "spn", "build", "tgt", "prof" };
  sp_cli_usage_t usage = spn_build_cli_usage(&cli);
  sp_cli_command_usage_t* match = SP_NULLPTR;
  bool ok = sp_cli_parse(&usage, sp_carr_len(argv), (c8**)argv, &match);
  EXPECT_TRUE(ok);
  EXPECT_EQ(cli.cmd, SPN_CLI_BUILD);
  EXPECT_TRUE(sp_str_equal_cstr(cli.build.target, "tgt"));
  EXPECT_TRUE(sp_str_equal_cstr(cli.build.profile, "prof"));
}

UTEST(spn_cli, parse_build_opts) {
  spn_cli_t cli = SP_ZERO_INITIALIZE();
  const c8* argv[] = { "spn", "build", "--target", "tgt", "--profile", "prof" };
  sp_cli_usage_t usage = spn_build_cli_usage(&cli);
  sp_cli_command_usage_t* match = SP_NULLPTR;
  bool ok = sp_cli_parse(&usage, sp_carr_len(argv), (c8**)argv, &match);
  EXPECT_TRUE(ok);
  EXPECT_EQ(cli.cmd, SPN_CLI_BUILD);
  EXPECT_TRUE(sp_str_equal_cstr(cli.build.target, "tgt"));
  EXPECT_TRUE(sp_str_equal_cstr(cli.build.profile, "prof"));
}

UTEST(spn_cli, parse_print_opts) {
  spn_cli_t cli = SP_ZERO_INITIALIZE();
  const c8* argv[] = { "spn", "print", "--generator", "make", "--compiler", "gcc", "--path", "out" };
  sp_cli_usage_t usage = spn_build_cli_usage(&cli);
  sp_cli_command_usage_t* match = SP_NULLPTR;
  bool ok = sp_cli_parse(&usage, sp_carr_len(argv), (c8**)argv, &match);
  EXPECT_TRUE(ok);
  EXPECT_EQ(cli.cmd, SPN_CLI_PRINT);
  EXPECT_TRUE(sp_str_equal_cstr(cli.print.generator, "make"));
  EXPECT_TRUE(sp_str_equal_cstr(cli.print.compiler, "gcc"));
  EXPECT_TRUE(sp_str_equal_cstr(cli.print.path, "out"));
}

UTEST(spn_cli, parse_which_package) {
  spn_cli_t cli = SP_ZERO_INITIALIZE();
  const c8* argv[] = { "spn", "which", "pkg" };
  sp_cli_usage_t usage = spn_build_cli_usage(&cli);
  sp_cli_command_usage_t* match = SP_NULLPTR;
  bool ok = sp_cli_parse(&usage, sp_carr_len(argv), (c8**)argv, &match);
  EXPECT_TRUE(ok);
  EXPECT_EQ(cli.cmd, SPN_CLI_WHICH);
  EXPECT_TRUE(sp_str_equal_cstr(cli.which.package, "pkg"));
}

UTEST(spn_cli, parse_which_dir) {
  spn_cli_t cli = SP_ZERO_INITIALIZE();
  const c8* argv[] = { "spn", "which", "--dir", "lib" };
  sp_cli_usage_t usage = spn_build_cli_usage(&cli);
  sp_cli_command_usage_t* match = SP_NULLPTR;
  bool ok = sp_cli_parse(&usage, sp_carr_len(argv), (c8**)argv, &match);
  EXPECT_TRUE(ok);
  EXPECT_EQ(cli.cmd, SPN_CLI_WHICH);
  EXPECT_TRUE(sp_str_equal_cstr(cli.which.dir, "lib"));
}

UTEST(spn_cli, parse_ls_package) {
  spn_cli_t cli = SP_ZERO_INITIALIZE();
  const c8* argv[] = { "spn", "ls", "pkg" };
  sp_cli_usage_t usage = spn_build_cli_usage(&cli);
  sp_cli_command_usage_t* match = SP_NULLPTR;
  bool ok = sp_cli_parse(&usage, sp_carr_len(argv), (c8**)argv, &match);
  EXPECT_TRUE(ok);
  EXPECT_EQ(cli.cmd, SPN_CLI_LS);
  EXPECT_TRUE(sp_str_equal_cstr(cli.ls.package, "pkg"));
}

UTEST(spn_cli, parse_tool_install) {
  spn_cli_t cli = SP_ZERO_INITIALIZE();
  const c8* argv[] = { "spn", "tool", "install", "pkg", "--version", "1.0", "--force" };
  sp_cli_usage_t usage = spn_build_cli_usage(&cli);
  sp_cli_command_usage_t* match = SP_NULLPTR;
  bool ok = sp_cli_parse(&usage, sp_carr_len(argv), (c8**)argv, &match);
  EXPECT_TRUE(ok);
  EXPECT_EQ(cli.cmd, SPN_CLI_TOOL);
  EXPECT_EQ(cli.tool.subcommand, SPN_TOOL_INSTALL);
  EXPECT_TRUE(sp_str_equal_cstr(cli.tool.install.package, "pkg"));
  EXPECT_TRUE(sp_str_equal_cstr(cli.tool.install.version, "1.0"));
  EXPECT_TRUE(cli.tool.install.force);
}

UTEST(spn_cli, parse_tool_uninstall) {
  spn_cli_t cli = SP_ZERO_INITIALIZE();
  const c8* argv[] = { "spn", "tool", "uninstall", "pkg", "--force" };
  sp_cli_usage_t usage = spn_build_cli_usage(&cli);
  sp_cli_command_usage_t* match = SP_NULLPTR;
  bool ok = sp_cli_parse(&usage, sp_carr_len(argv), (c8**)argv, &match);
  EXPECT_TRUE(ok);
  EXPECT_EQ(cli.cmd, SPN_CLI_TOOL);
  EXPECT_EQ(cli.tool.subcommand, SPN_TOOL_UNINSTALL);
  EXPECT_TRUE(sp_str_equal_cstr(cli.tool.install.package, "pkg"));
  EXPECT_TRUE(cli.tool.install.force);
}

UTEST(spn_cli, parse_tool_run) {
  spn_cli_t cli = SP_ZERO_INITIALIZE();
  const c8* argv[] = { "spn", "tool", "run", "pkg", "cmd" };
  sp_cli_usage_t usage = spn_build_cli_usage(&cli);
  sp_cli_command_usage_t* match = SP_NULLPTR;
  bool ok = sp_cli_parse(&usage, sp_carr_len(argv), (c8**)argv, &match);
  EXPECT_TRUE(ok);
  EXPECT_EQ(cli.cmd, SPN_CLI_TOOL);
  EXPECT_EQ(cli.tool.subcommand, SPN_TOOL_RUN);
  EXPECT_TRUE(sp_str_equal_cstr(cli.tool.run.package, "pkg"));
  EXPECT_TRUE(sp_str_equal_cstr(cli.tool.run.command, "cmd"));
}

UTEST(spn_cli, parse_add_missing_arg_fails) {
  spn_cli_t cli = SP_ZERO_INITIALIZE();
  const c8* argv[] = { "spn", "add" };
  sp_cli_usage_t usage = spn_build_cli_usage(&cli);
  sp_cli_command_usage_t* match = SP_NULLPTR;
  bool ok = sp_cli_parse(&usage, sp_carr_len(argv), (c8**)argv, &match);
  EXPECT_FALSE(ok);
  EXPECT_EQ(cli.cmd, SPN_CLI_ADD);
}

UTEST(spn_cli, parse_tool_install_missing_arg_fails) {
  spn_cli_t cli = SP_ZERO_INITIALIZE();
  const c8* argv[] = { "spn", "tool", "install" };
  sp_cli_usage_t usage = spn_build_cli_usage(&cli);
  sp_cli_command_usage_t* match = SP_NULLPTR;
  bool ok = sp_cli_parse(&usage, sp_carr_len(argv), (c8**)argv, &match);
  EXPECT_FALSE(ok);
  EXPECT_EQ(cli.cmd, SPN_CLI_TOOL);
  EXPECT_EQ(cli.tool.subcommand, SPN_TOOL_INSTALL);
}

UTEST_MAIN();
