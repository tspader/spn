#include "cli/cli.h"

#include "app/types.h"
#include "ctx/types.h"
#include "sp/cli.h"
#include "toolchain/types.h"
#include "tui/table.h"
#include "log/log.h"
#include "pkg/pkg.h"

static sp_str_t spn_cli_opt_kind_to_str(spn_cli_opt_kind_t kind) {
  switch (kind) {
    SPN_CLI_OPT_KIND(SP_X_NAMED_ENUM_CASE_TO_STRING_LOWER)
  }
  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

static sp_str_t cstr(const c8* cstr) {
  return sp_str_from_cstr(cstr ? cstr : "");
}

spn_cli_command_info_t spn_cli_command_info_from_usage(spn_cli_usage_t cmd) {
  spn_cli_command_info_t info = {
    .name = cstr(cmd.name),
    .usage = cstr(cmd.usage),
    .summary = cstr(cmd.summary),
  };

  sp_carr_for(cmd.opts, it) {
    if (!cmd.opts[it].name) break;

    spn_cli_opt_info_t opt = {
      .brief = cstr(cmd.opts[it].brief),
      .name = cstr(cmd.opts[it].name),
      .kind = cmd.opts[it].kind,
      .summary = cstr(cmd.opts[it].summary),
      .placeholder = cstr(cmd.opts[it].placeholder ? cmd.opts[it].placeholder : ""),
    };
    sp_da_push(info.opts, opt);
  }

  sp_carr_for(cmd.args, it) {
    if (!cmd.args[it].name) break;

    spn_cli_arg_info_t arg = {
      .name = cstr(cmd.args[it].name),
      .kind = cmd.args[it].kind,
      .summary = cstr(cmd.args[it].summary),
    };
    sp_da_push(info.args, arg);
    sp_da_push(info.brief, arg.name);
  }

  return info;
}

sp_str_t spn_cli_command_usage(spn_cli_usage_t cmd) {
  spn_cli_command_info_t info = spn_cli_command_info_from_usage(cmd);

  sp_str_builder_t builder = SP_ZERO_INITIALIZE();

  SP_ASSERT(!sp_str_empty(info.summary));
  sp_str_builder_append_fmt(&builder, "{}", SP_FMT_CSTR(cmd.summary));
  sp_str_builder_new_line(&builder);

  if (!sp_dyn_array_empty(info.opts)) {
    sp_str_builder_new_line(&builder);

    sp_str_builder_append_fmt(&builder, "{:fg brightgreen}", SP_FMT_CSTR("options"));
    sp_str_builder_new_line(&builder);

    sp_tui_begin_table(&spn.tui.table);
    sp_tui_table_setup_column(&spn.tui.table, sp_str_lit("Short"));
    sp_tui_table_setup_column(&spn.tui.table, sp_str_lit("Long"));
    sp_tui_table_setup_column(&spn.tui.table, sp_str_lit("Type"));
    sp_tui_table_setup_column(&spn.tui.table, sp_str_lit("Description"));
    sp_tui_table_header_row(&spn.tui.table);

    sp_dyn_array_for(info.opts, it) {
      spn_cli_opt_info_t opt = info.opts[it];

      sp_str_t short_display;
      if (!sp_str_empty(opt.brief)) {
        sp_str_t short_text = sp_format("-{}", SP_FMT_STR(opt.brief));
        short_display = sp_format("{:fg brightyellow}", SP_FMT_STR(short_text));
      } else {
        short_display = sp_str_lit("");
      }

      sp_str_t long_display;
      if (!sp_str_empty(opt.placeholder)) {
        sp_str_t long_text = sp_format("--{}", SP_FMT_STR(opt.name));
        long_display = sp_format("{:fg brightyellow}={:fg white}", SP_FMT_STR(long_text), SP_FMT_STR(opt.placeholder));
      } else {
        sp_str_t long_text = sp_format("--{}", SP_FMT_STR(opt.name));
        long_display = sp_format("{:fg brightyellow}", SP_FMT_STR(long_text));
      }

      sp_str_t kind_str = sp_format("{:fg brightblack}", SP_FMT_STR(spn_cli_opt_kind_to_str(opt.kind)));

      sp_tui_table_next_row(&spn.tui.table);
      sp_tui_table_str(&spn.tui.table, short_display);
      sp_tui_table_str(&spn.tui.table, long_display);
      sp_tui_table_str(&spn.tui.table, kind_str);
      sp_tui_table_str(&spn.tui.table, opt.summary);
    }

    sp_tui_table_set_indent(&spn.tui.table, 1);
    sp_tui_table_end(&spn.tui.table);

    sp_str_builder_append(&builder, sp_tui_table_render(&spn.tui.table));
  }

  if (!sp_dyn_array_empty(info.args)) {
    sp_str_builder_new_line(&builder);

    sp_str_builder_append_fmt(&builder, "{:fg brightgreen}", SP_FMT_CSTR("arguments"));
    sp_str_builder_new_line(&builder);

    sp_tui_begin_table(&spn.tui.table);
    sp_tui_table_setup_column(&spn.tui.table, sp_str_lit("Name"));
    sp_tui_table_setup_column(&spn.tui.table, sp_str_lit("Type"));
    sp_tui_table_setup_column(&spn.tui.table, sp_str_lit("Description"));
    sp_tui_table_header_row(&spn.tui.table);

    sp_dyn_array_for(info.args, it) {
      spn_cli_arg_info_t arg = info.args[it];

      sp_tui_table_next_row(&spn.tui.table);
      sp_tui_table_fmt(&spn.tui.table, "{:fg brightyellow}", SP_FMT_STR(arg.name));
      sp_tui_table_str(&spn.tui.table, sp_str_lit("str"));
      sp_tui_table_str(&spn.tui.table, arg.summary);
    }

    sp_tui_table_set_indent(&spn.tui.table, 1);
    sp_tui_table_end(&spn.tui.table);
    sp_str_t table = sp_tui_table_render(&spn.tui.table);

    sp_str_builder_append_fmt(&builder, "{}", SP_FMT_STR(table));
  }

  return sp_str_builder_to_str(&builder);
}

sp_str_t spn_cli_usage(spn_cli_usage_t* cmd) {
  spn_cli_command_info_t cmd_info = spn_cli_command_info_from_usage(*cmd);
  spn_cli_usage_info_t info = SP_ZERO_INITIALIZE();

  if (cmd->commands) {
    for (spn_cli_usage_t* sub = cmd->commands; sub->name; sub++) {
      spn_cli_command_info_t sub_info = spn_cli_command_info_from_usage(*sub);
      sp_dyn_array_push(info.commands, sub_info);
    }
  }

  sp_str_builder_t builder = SP_ZERO_INITIALIZE();

  if (cmd->summary) {
    sp_str_builder_append_fmt(&builder, "{}", SP_FMT_CSTR(cmd->summary));
    sp_str_builder_new_line(&builder);
    sp_str_builder_new_line(&builder);
  }

  if (cmd->usage) {
    sp_str_builder_append_fmt(&builder, "{:fg brightgreen}", SP_FMT_CSTR("usage"));
    sp_str_builder_indent(&builder);
    sp_str_builder_new_line(&builder);
    sp_str_builder_append_fmt(&builder, "{:fg brightcyan}", SP_FMT_CSTR(cmd->usage));
    sp_str_builder_dedent(&builder);
    sp_str_builder_new_line(&builder);
    sp_str_builder_new_line(&builder);
  }

  if (!sp_dyn_array_empty(cmd_info.opts)) {
    sp_str_builder_append_fmt(&builder, "{:fg brightgreen}", SP_FMT_CSTR("options"));
    sp_str_builder_new_line(&builder);

    sp_tui_begin_table(&spn.tui.table);
    sp_tui_table_setup_column(&spn.tui.table, sp_str_lit("Short"));
    sp_tui_table_setup_column(&spn.tui.table, sp_str_lit("Long"));
    sp_tui_table_setup_column(&spn.tui.table, sp_str_lit("Type"));
    sp_tui_table_setup_column(&spn.tui.table, sp_str_lit("Description"));
    sp_tui_table_header_row(&spn.tui.table);

    sp_dyn_array_for(cmd_info.opts, it) {
      spn_cli_opt_info_t opt = cmd_info.opts[it];

      sp_str_t short_display;
      if (!sp_str_empty(opt.brief)) {
        sp_str_t short_text = sp_format("-{}", SP_FMT_STR(opt.brief));
        short_display = sp_format("{:fg brightyellow}", SP_FMT_STR(short_text));
      } else {
        short_display = sp_str_lit("");
      }

      sp_str_t long_display;
      if (!sp_str_empty(opt.placeholder)) {
        sp_str_t long_text = sp_format("--{}", SP_FMT_STR(opt.name));
        long_display = sp_format("{:fg brightyellow}={:fg white}", SP_FMT_STR(long_text), SP_FMT_STR(opt.placeholder));
      } else {
        sp_str_t long_text = sp_format("--{}", SP_FMT_STR(opt.name));
        long_display = sp_format("{:fg brightyellow}", SP_FMT_STR(long_text));
      }

      sp_str_t kind_str = sp_format("{:fg brightblack}", SP_FMT_STR(spn_cli_opt_kind_to_str(opt.kind)));

      sp_tui_table_next_row(&spn.tui.table);
      sp_tui_table_str(&spn.tui.table, short_display);
      sp_tui_table_str(&spn.tui.table, long_display);
      sp_tui_table_str(&spn.tui.table, kind_str);
      sp_tui_table_str(&spn.tui.table, opt.summary);
    }

    sp_tui_table_set_indent(&spn.tui.table, 1);
    sp_tui_table_end(&spn.tui.table);
    sp_str_builder_append(&builder, sp_tui_table_render(&spn.tui.table));
    sp_str_builder_new_line(&builder);
  }

  if (!sp_dyn_array_empty(info.commands)) {
    sp_str_builder_append_fmt(&builder, "{:fg brightgreen}", SP_FMT_CSTR("commands"));
    sp_str_builder_new_line(&builder);

    sp_tui_begin_table(&spn.tui.table);
    sp_tui_table_setup_column(&spn.tui.table, sp_str_lit("Command"));
    sp_tui_table_setup_column(&spn.tui.table, sp_str_lit("Arguments"));
    sp_tui_table_setup_column(&spn.tui.table, sp_str_lit("Description"));
    sp_tui_table_header_row(&spn.tui.table);

    sp_dyn_array_for(info.commands, it) {
      spn_cli_command_info_t command = info.commands[it];
      sp_str_t args = sp_str_lit("");
      if (!sp_da_empty(command.brief)) {
        args = sp_str_join_n(command.brief, sp_dyn_array_size(command.brief), sp_str_lit(", "));
      }

      sp_tui_table_next_row(&spn.tui.table);
      sp_tui_table_fmt(&spn.tui.table, "{:fg brightcyan}", SP_FMT_STR(command.name));
      sp_tui_table_fmt(&spn.tui.table, "{:fg brightyellow}", SP_FMT_STR(args));
      sp_tui_table_str(&spn.tui.table, command.summary);
    }

    sp_tui_table_set_indent(&spn.tui.table, 1);
    sp_tui_table_end(&spn.tui.table);
    sp_str_builder_append(&builder, sp_tui_table_render(&spn.tui.table));
  }

  return sp_str_builder_to_str(&builder);
}

sp_app_result_t spn_cli_help(spn_cli_parser_t* p) {
  if (!p->resolved) {
    sp_log(spn_cli_command_usage(*p->cmd));
    return SP_APP_QUIT;
  }
  else if (!p->resolved->commands) {
    sp_log(spn_cli_command_usage(*p->resolved));
    return SP_APP_QUIT;
  }
  else {
    sp_log(spn_cli_command_usage(*p->resolved->commands));
    return SP_APP_QUIT;
  }
  return SP_APP_QUIT;
}


sp_app_result_t spn_cli_root(spn_cli_t* cli) {
  sp_str_t help = spn_cli_usage(&cli->usage);
  sp_log(help);
  return SP_APP_QUIT;
}

static spn_cli_usage_t tools[] = {
  {
    .name = "install",
    .opts = {
      {
        .brief = "f",
        .name = "force",
        .kind = SPN_CLI_OPT_KIND_BOOLEAN,
        .summary = "Force reinstall even if already installed",
        .ptr = &spn.cli.tool.install.force
      },
      {
        .brief = "v",
        .name = "version",
        .kind = SPN_CLI_OPT_KIND_STRING,
        .summary = "Version to install",
        .placeholder = "VERSION",
        .ptr = &spn.cli.tool.install.version
      }
    },
    .args = {
      {
        .name = "package",
        .kind = SPN_CLI_ARG_KIND_REQUIRED,
        .summary = "The package to install",
        .ptr = &spn.cli.tool.install.package
      }
    },
    .summary = "Install a package's binary targets to the PATH",
    .handler = spn_cli_tool_install
  },
  {
    .name = "uninstall",
    .opts = {
      {
        .brief = "f",
        .name = "force",
        .kind = SPN_CLI_OPT_KIND_BOOLEAN,
        .summary = "Force removal even if not installed by spn",
        .ptr = &spn.cli.tool.install.force
      }
    },
    .args = {
      {
        .name = "package",
        .kind = SPN_CLI_ARG_KIND_REQUIRED,
        .summary = "The package to uninstall",
        .ptr = &spn.cli.tool.install.package
      }
    },
    .summary = "Uninstall a package's binary targets from the PATH",
    .handler = spn_cli_tool_uninstall
  },
  {
    .name = "run",
    .args = {
      {
        .name = "package",
        .kind = SPN_CLI_ARG_KIND_REQUIRED,
        .summary = "The package to run",
        .ptr = &spn.cli.tool.run.package
      },
      {
        .name = "command",
        .kind = SPN_CLI_ARG_KIND_OPTIONAL,
        .summary = "The command to run",
        .ptr = &spn.cli.tool.run.command
      }
    },
    .summary = "Run a binary from a package",
    .handler = spn_cli_tool_run
  },
  {
    0
  },
};

static spn_cli_usage_t commands[] = {
  {
    .name = "init",
    .handler = spn_cli_init,
    .summary = "Initialize a project in the current directory",
    .opts = {
      {
        .brief = "b",
        .name = "bare",
        .kind = SPN_CLI_OPT_KIND_BOOLEAN,
        .summary = "Create minimal project without sp dependency or main.c",
        .ptr = &spn.cli.init.bare
      }
    },
  },
  {
    .name = "add",
    .args = {
      {
        .name = "package",
        .kind = SPN_CLI_ARG_KIND_REQUIRED,
        .summary = "The package to add",
        .ptr = &spn.cli.add.package
      }
    },
    .opts = {
      {
        .brief = "t",
        .name = "test",
        .kind = SPN_CLI_OPT_KIND_BOOLEAN,
        .summary = "Add as a test dependency",
        .ptr = &spn.cli.add.test
      },
      {
        .brief = "b",
        .name = "build",
        .kind = SPN_CLI_OPT_KIND_BOOLEAN,
        .summary = "Add as a build dependency",
        .ptr = &spn.cli.add.build
      }
    },
    .summary = "Add the latest version of a package to the project",
    .handler = spn_cli_add
  },

    {
      .name = "build",
      .opts = {
        {
          .brief = "f",
          .name = "force",
          .kind = SPN_CLI_OPT_KIND_BOOLEAN,
          .summary = "Force build, even if it exists in store",
          .ptr = &spn.cli.build.force
        },
        {
          .brief = "p",
          .name = "profile",
          .kind = SPN_CLI_OPT_KIND_STRING,
          .summary = "Profile to use for building",
          .placeholder = "PROFILE",
          .ptr = &spn.cli.build.profile
        },
        {
          .name = "bin",
          .kind = SPN_CLI_OPT_KIND_BOOLEAN,
          .summary = "Build only binary targets",
          .ptr = &spn.cli.build.only.bin
        },
        {
          .name = "lib",
          .kind = SPN_CLI_OPT_KIND_BOOLEAN,
          .summary = "Build only library targets",
          .ptr = &spn.cli.build.only.lib
        },
        {
          .name = "test",
          .kind = SPN_CLI_OPT_KIND_BOOLEAN,
          .summary = "Build only test targets",
          .ptr = &spn.cli.build.only.test
        },
        {
          .name = "script",
          .kind = SPN_CLI_OPT_KIND_BOOLEAN,
          .summary = "Build only script targets",
          .ptr = &spn.cli.build.only.script
        },
        {
          .name = "toolchain",
          .kind = SPN_CLI_OPT_KIND_STRING,
          .summary = "Override toolchain",
          .placeholder = "NAME",
          .ptr = &spn.cli.build.toolchain
        },
        {
          .brief = "m",
          .name = "mode",
          .kind = SPN_CLI_OPT_KIND_STRING,
          .summary = "Override build mode (debug, release)",
          .placeholder = "MODE",
          .ptr = &spn.cli.build.mode
        },
        {
          .name = "target",
          .kind = SPN_CLI_OPT_KIND_STRING,
          .summary = "Target triple (e.g. aarch64-linux-gnu)",
          .placeholder = "TRIPLE",
          .ptr = &spn.cli.build.target
        },
        {
          .name = "os",
          .kind = SPN_CLI_OPT_KIND_STRING,
          .summary = "Override target OS (linux, macos, windows)",
          .placeholder = "OS",
          .ptr = &spn.cli.build.os
        },
        {
          .name = "arch",
          .kind = SPN_CLI_OPT_KIND_STRING,
          .summary = "Override target architecture (x86_64, aarch64)",
          .placeholder = "ARCH",
          .ptr = &spn.cli.build.arch
        },
        {
          .name = "abi",
          .kind = SPN_CLI_OPT_KIND_STRING,
          .summary = "Override target ABI (gnu, musl, mingw)",
          .placeholder = "ABI",
          .ptr = &spn.cli.build.abi
        }
      },
      .args = {
        {
          .name = "name",
          .kind = SPN_CLI_ARG_KIND_OPTIONAL,
          .summary = "Target name to build",
          .ptr = &spn.cli.build.name
        }
      },
      .summary = "Build the project, including dependencies, from source",
      .handler = spn_cli_build
    },

    {
      .name = "run",
      .opts = {
        {
          .brief = "p",
          .name = "profile",
          .kind = SPN_CLI_OPT_KIND_STRING,
          .summary = "Profile to use when resolving build dependencies",
          .placeholder = "PROFILE",
          .ptr = &spn.cli.run.profile
        },
        {
          .name = "toolchain",
          .kind = SPN_CLI_OPT_KIND_STRING,
          .summary = "Override toolchain",
          .placeholder = "NAME",
          .ptr = &spn.cli.run.toolchain
        },
        {
          .brief = "m",
          .name = "mode",
          .kind = SPN_CLI_OPT_KIND_STRING,
          .summary = "Override build mode (debug, release)",
          .placeholder = "MODE",
          .ptr = &spn.cli.run.mode
        }
      },
      .args = {
        {
          .name = "entry",
          .kind = SPN_CLI_ARG_KIND_REQUIRED,
          .summary = "Script target name or relative .c file",
          .ptr = &spn.cli.run.entry
        }
      },
      .summary = "Run a manifest script target or a relative C source file",
      .handler = spn_cli_run
    },

    {
      .name = "test",
      .opts = {
        {
          .brief = "p",
          .name = "profile",
          .kind = SPN_CLI_OPT_KIND_STRING,
          .summary = "Profile to use for building",
          .placeholder = "PROFILE",
          .ptr = &spn.cli.test.profile
        },
        {
          .name = "toolchain",
          .kind = SPN_CLI_OPT_KIND_STRING,
          .summary = "Override toolchain",
          .placeholder = "NAME",
          .ptr = &spn.cli.test.toolchain
        },
        {
          .brief = "m",
          .name = "mode",
          .kind = SPN_CLI_OPT_KIND_STRING,
          .summary = "Override build mode (debug, release)",
          .placeholder = "MODE",
          .ptr = &spn.cli.test.mode
        }
      },
      .args = {
        {
          .name = "name",
          .kind = SPN_CLI_ARG_KIND_OPTIONAL,
          .summary = "Test target name to run",
          .ptr = &spn.cli.test.name
        }
      },
      .summary = "Build and run tests",
      .handler = spn_cli_test
    },

    {
      .name = "generate",
      .opts = {
        {
          .brief = "g",
          .name = "generator",
          .kind = SPN_CLI_OPT_KIND_STRING,
          .summary = "Generator type (raw, shell, make)",
          .placeholder = "TYPE",
          .ptr = &spn.cli.generate.generator
        },
        {
          .brief = "c",
          .name = "compiler",
          .kind = SPN_CLI_OPT_KIND_STRING,
          .summary = "Compiler to format flags for (gcc, clang, tcc)",
          .placeholder = "COMPILER",
          .ptr = &spn.cli.generate.compiler
        },
        {
          .brief = "p",
          .name = "path",
          .kind = SPN_CLI_OPT_KIND_STRING,
          .summary = "Output directory for generated file",
          .placeholder = "PATH",
          .ptr = &spn.cli.generate.path
        }
      },
      .summary = "Generate build system files with dependency flags",
      .handler = spn_cli_generate
    },
    {
      .name = "link",
      .handler = spn_cli_copy,
      .args = {
        {
          .name = "kind",
          .kind = SPN_CLI_ARG_KIND_REQUIRED,
          .summary = "The link kind",
          .ptr = &spn.cli.copy.directory
        }
      },
      .summary = "Link or copy the binary outputs of your dependencies",
    },
    {
      .name = "update",
      .args = {
        {
          .name = "package",
          .kind = SPN_CLI_ARG_KIND_REQUIRED,
          .summary = "The package to update",
          .ptr = &spn.cli.update.package
        }
      },
      .summary = "Update an existing package to the latest version in the project",
      .handler = spn_cli_update
    },
    {
      .name = "list",
      .summary = "List all known packages in all registries",
      .handler = spn_cli_list
    },
    {
      .name = "which",
      .opts = {
        {
          .brief = "d",
          .name = "dir",
          .kind = SPN_CLI_OPT_KIND_STRING,
          .summary = "Which directory to show (store, include, lib, source, work, vendor)",
          .placeholder = "DIR",
          .ptr = &spn.cli.which.dir
        }
      },
      .args = {
        {
          .name = "package",
          .kind = SPN_CLI_ARG_KIND_OPTIONAL,
          .summary = "The package to show path for",
          .ptr = &spn.cli.which.package
        }
      },
      .summary = "Print the absolute path of a cache dir for a package",
      .handler = spn_cli_which
    },
    {
      .name = "ls",
      .opts = {
        {
          .brief = "d",
          .name = "dir",
          .kind = SPN_CLI_OPT_KIND_STRING,
          .summary = "Which directory to list (store, include, lib, source, work, vendor)",
          .placeholder = "DIR",
          .ptr = &spn.cli.ls.dir
        }
      },
      .args = {
        {
          .name = "package",
          .kind = SPN_CLI_ARG_KIND_OPTIONAL,
          .summary = "The package to list",
          .ptr = &spn.cli.ls.package
        }
      },
      .summary = "Run ls against a cache dir for a package (e.g. to see build output)",
      .handler = spn_cli_ls
    },
    {
      .name = "manifest",
      .args = {
        {
          .name = "package",
          .kind = SPN_CLI_ARG_KIND_REQUIRED,
          .summary = "The package name",
          .ptr = &spn.cli.manifest.package
        }
      },
      .summary = "Print the full manifest source for a package",
      .handler = spn_cli_manifest
    },
    {
      .name = "graph",
      .opts = {
        {
          .brief = "o",
          .name = "output",
          .kind = SPN_CLI_OPT_KIND_STRING,
          .summary = "Output file path (stdout if not specified)",
          .placeholder = "FILE",
          .ptr = &spn.cli.graph.output
        },
        {
          .brief = "d",
          .name = "dirty",
          .kind = SPN_CLI_OPT_KIND_BOOLEAN,
          .summary = "Color nodes by dirtiness instead of type",
          .ptr = &spn.cli.graph.dirty
        }
      },
      .summary = "Output the build graph as mermaid",
      .handler = spn_cli_graph
    },
    {
      .name = "tool",
      .summary = "Run, install, and manage binaries defined by spn packages",
      .handler = spn_cli_tool,
      .commands = tools
    },
    {
      .name = "publish",
      .opts = {
        {
          .brief = "i",
          .name = "index",
          .kind = SPN_CLI_OPT_KIND_STRING,
          .summary = "Index to publish to",
          .placeholder = "NAME",
          .ptr = &spn.cli.publish.index
        },
        {
          .name = "source-url",
          .kind = SPN_CLI_OPT_KIND_STRING,
          .summary = "Source repository URL (autodetected if omitted)",
          .placeholder = "URL",
          .ptr = &spn.cli.publish.source_url
        },
        {
          .name = "source-rev",
          .kind = SPN_CLI_OPT_KIND_STRING,
          .summary = "Source commit (autodetected if omitted)",
          .placeholder = "REV",
          .ptr = &spn.cli.publish.source_rev
        }
      },
      .summary = "Publish a release to an index",
      .handler = spn_cli_publish
    },
    {
      .name = "clean",
      .summary = "Remove build directory and lock file",
      .opts = {
        {
          .brief = "p",
          .name = "profile",
          .kind = SPN_CLI_OPT_KIND_STRING,
          .summary = "Only clean the specified profile",
          .placeholder = "PROFILE",
          .ptr = &spn.cli.clean.profile
        }
      },
      .handler = spn_cli_clean
    },
    {0},
  };

spn_cli_usage_t spn_cli() {
  return (spn_cli_usage_t) {
    .name = "spn",
    .handler = spn_cli_root,
    .summary = "A package manager and build tool for C",
    .opts = {
      {
        .brief = "h",
        .name = "help",
        .kind = SPN_CLI_OPT_KIND_BOOLEAN,
        .summary = "Print help message",
        .ptr = &spn.cli.help
      },
      {
        .brief = "C",
        .name = "project-dir",
        .kind = SPN_CLI_OPT_KIND_STRING,
        .summary = "Specify the directory containing project file",
        .placeholder = "DIR",
        .ptr = &spn.cli.project_dir
      },
      {
        .brief = "f",
        .name = "file",
        .kind = SPN_CLI_OPT_KIND_STRING,
        .summary = "Specify the project file path",
        .placeholder = "FILE",
        .ptr = &spn.cli.project_file
      },
      {
        .brief = "o",
        .name = "output",
        .kind = SPN_CLI_OPT_KIND_STRING,
        .summary = "Output mode: interactive, noninteractive, quiet, none",
        .placeholder = "MODE",
        .ptr = &spn.cli.output
      },
      {
        .brief = "v",
        .name = "verbose",
        .kind = SPN_CLI_OPT_KIND_BOOLEAN,
        .summary = "Show verbose output",
        .ptr = &spn.cli.verbose
      },
      {
        .brief = "q",
        .name = "quiet",
        .kind = SPN_CLI_OPT_KIND_BOOLEAN,
        .summary = "Only show errors",
        .ptr = &spn.cli.quiet
      }
    },
    .commands = commands
  };
}
