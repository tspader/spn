#include "cli/cli.h"

#include "cli/types.h"
#include "ctx/types.h"

static spn_cli_raw_t spn_cli_raw;

static sp_cli_cmd_t cmd_init = {
  .name = "init",
  .summary = "Scaffold a new project",
  .opts = {
    {
      .name = "bare",
      .summary = "Only write a manifest",
      .ptr = &spn.cli.init.bare,
    },
  },
  .args = {
    {
      .name = "path",
      .kind = SP_CLI_ARG_OPTIONAL,
      .summary = "Directory to scaffold into",
      .ptr = &spn_cli_raw.init.path,
    },
  },
  .handler = spn_cli_init,
};

static sp_cli_cmd_t cmd_add = {
  .name = "add",
  .summary = "Add a dependency to the manifest",
  .opts = {
    {
      .name = "test",
      .summary = "Add to test dependencies",
      .ptr = &spn.cli.add.test,
    },
    {
      .name = "build",
      .summary = "Add to build dependencies",
      .ptr = &spn.cli.add.build,
    },
  },
  .args = {
    {
      .name = "package",
      .summary = "Package to add (name or name@version)",
      .ptr = &spn_cli_raw.add.package,
    },
  },
  .handler = spn_cli_add,
};

static sp_cli_cmd_t cmd_update = {
  .name = "update",
  .summary = "Update dependencies to the latest compatible versions",
  .handler = spn_cli_update,
};

static sp_cli_cmd_t cmd_clean = {
  .name = "clean",
  .summary = "Remove the project build directory",
  .opts = {
    {
      .brief = "p",
      .name = "profile",
      .kind = SP_CLI_OPT_STRING,
      .summary = "Only remove this profile's outputs",
      .placeholder = "PROFILE",
      .ptr = &spn_cli_raw.profile.name,
    },
  },
  .handler = spn_cli_clean,
};

static sp_cli_cmd_t cmd_build = {
  .name = "build",
  .summary = "Build the project, including dependencies, from source",
  .opts = {
    {
      .brief = "f",
      .name = "force",
      .summary = "Force build, even if it exists in store",
      .ptr = &spn.cli.build.force,
    },
    {
      .brief = "p",
      .name = "profile",
      .kind = SP_CLI_OPT_STRING,
      .summary = "Profile to use for building",
      .placeholder = "PROFILE",
      .ptr = &spn_cli_raw.profile.name,
    },
    {
      .name = "bin",
      .summary = "Build only binary targets",
      .ptr = &spn.cli.build.only.bin,
    },
    {
      .name = "lib",
      .summary = "Build only library targets",
      .ptr = &spn.cli.build.only.lib,
    },
    {
      .name = "test",
      .summary = "Build only test targets",
      .ptr = &spn.cli.build.only.test,
    },
    {
      .name = "script",
      .summary = "Build only script targets",
      .ptr = &spn.cli.build.only.script,
    },
    {
      .name = "toolchain",
      .kind = SP_CLI_OPT_STRING,
      .summary = "Override toolchain",
      .placeholder = "NAME",
      .ptr = &spn_cli_raw.profile.toolchain,
    },
    {
      .brief = "m",
      .name = "mode",
      .kind = SP_CLI_OPT_STRING,
      .summary = "Override build mode (debug, release)",
      .placeholder = "MODE",
      .ptr = &spn_cli_raw.profile.mode,
    },
    {
      .name = "target",
      .kind = SP_CLI_OPT_STRING,
      .summary = "Target triple (e.g. aarch64-linux-gnu)",
      .placeholder = "TRIPLE",
      .ptr = &spn_cli_raw.profile.target,
    },
    {
      .name = "os",
      .kind = SP_CLI_OPT_STRING,
      .summary = "Override target OS (linux, macos, windows)",
      .placeholder = "OS",
      .ptr = &spn_cli_raw.profile.os,
    },
    {
      .name = "arch",
      .kind = SP_CLI_OPT_STRING,
      .summary = "Override target architecture (x86_64, aarch64)",
      .placeholder = "ARCH",
      .ptr = &spn_cli_raw.profile.arch,
    },
    {
      .name = "abi",
      .kind = SP_CLI_OPT_STRING,
      .summary = "Override target ABI (gnu, musl, mingw)",
      .placeholder = "ABI",
      .ptr = &spn_cli_raw.profile.abi,
    },
  },
  .args = {
    {
      .name = "name",
      .kind = SP_CLI_ARG_REST,
      .summary = "Name of entry to build",
    },
  },
  .handler = spn_cli_build,
};

static sp_cli_cmd_t cmd_run = {
  .name = "run",
  .summary = "Run a manifest script target or a relative C source file",
  .opts = {
    {
      .brief = "p",
      .name = "profile",
      .kind = SP_CLI_OPT_STRING,
      .summary = "Profile to use when resolving build dependencies",
      .placeholder = "PROFILE",
      .ptr = &spn_cli_raw.profile.name,
    },
    {
      .name = "toolchain",
      .kind = SP_CLI_OPT_STRING,
      .summary = "Override toolchain",
      .placeholder = "NAME",
      .ptr = &spn_cli_raw.profile.toolchain,
    },
    {
      .brief = "m",
      .name = "mode",
      .kind = SP_CLI_OPT_STRING,
      .summary = "Override build mode (debug, release)",
      .placeholder = "MODE",
      .ptr = &spn_cli_raw.profile.mode,
    },
  },
  .args = {
    {
      .name = "entry",
      .summary = "Script target name or relative .c file",
      .ptr = &spn_cli_raw.run.entry,
    },
  },
  .handler = spn_cli_run,
};

static sp_cli_cmd_t cmd_test = {
  .name = "test",
  .summary = "Build and run tests",
  .opts = {
    {
      .brief = "p",
      .name = "profile",
      .kind = SP_CLI_OPT_STRING,
      .summary = "Profile to use for building",
      .placeholder = "PROFILE",
      .ptr = &spn_cli_raw.profile.name,
    },
    {
      .name = "toolchain",
      .kind = SP_CLI_OPT_STRING,
      .summary = "Override toolchain",
      .placeholder = "NAME",
      .ptr = &spn_cli_raw.profile.toolchain,
    },
    {
      .brief = "m",
      .name = "mode",
      .kind = SP_CLI_OPT_STRING,
      .summary = "Override build mode (debug, release)",
      .placeholder = "MODE",
      .ptr = &spn_cli_raw.profile.mode,
    },
  },
  .args = {
    {
      .name = "name",
      .kind = SP_CLI_ARG_OPTIONAL,
      .summary = "Test target name to run",
      .ptr = &spn_cli_raw.test.name,
    },
  },
  .handler = spn_cli_test,
};

static sp_cli_cmd_t cmd_generate = {
  .name = "generate",
  .summary = "Generate build system files with dependency flags",
  .opts = {
    {
      .brief = "g",
      .name = "generator",
      .kind = SP_CLI_OPT_STRING,
      .summary = "Generator type (raw, shell, make)",
      .placeholder = "TYPE",
      .ptr = &spn_cli_raw.generate.generator,
    },
    {
      .brief = "c",
      .name = "compiler",
      .kind = SP_CLI_OPT_STRING,
      .summary = "Compiler to format flags for (gcc, clang, tcc)",
      .placeholder = "COMPILER",
      .ptr = &spn_cli_raw.generate.compiler,
    },
    {
      .brief = "p",
      .name = "path",
      .kind = SP_CLI_OPT_STRING,
      .summary = "Output directory for generated file",
      .placeholder = "PATH",
      .ptr = &spn_cli_raw.generate.path,
    },
  },
  .handler = spn_cli_generate,
};

static sp_cli_cmd_t cmd_which = {
  .name = "which",
  .summary = "Print the absolute path of a cache dir for a package",
  .opts = {
    {
      .brief = "d",
      .name = "dir",
      .kind = SP_CLI_OPT_STRING,
      .summary = "Which directory to show (store, include, lib, source, work, vendor)",
      .placeholder = "DIR",
      .ptr = &spn_cli_raw.which.dir,
    },
  },
  .args = {
    {
      .name = "package",
      .kind = SP_CLI_ARG_OPTIONAL,
      .summary = "The package to show path for",
      .ptr = &spn_cli_raw.which.package,
    },
  },
  .handler = spn_cli_which,
};

static sp_cli_cmd_t cmd_graph = {
  .name = "graph",
  .summary = "Output the build graph as mermaid",
  .opts = {
    {
      .brief = "o",
      .name = "output",
      .kind = SP_CLI_OPT_STRING,
      .summary = "Output file path (stdout if not specified)",
      .placeholder = "FILE",
      .ptr = &spn_cli_raw.graph.output,
    },
    {
      .brief = "d",
      .name = "dirty",
      .summary = "Color nodes by dirtiness instead of type",
      .ptr = &spn.cli.graph.dirty,
    },
  },
  .handler = spn_cli_graph,
};

static sp_cli_cmd_t cmd_publish = {
  .name = "publish",
  .summary = "Publish a release to an index",
  .opts = {
    {
      .brief = "i",
      .name = "index",
      .kind = SP_CLI_OPT_STRING,
      .summary = "Index to publish to",
      .placeholder = "NAME",
      .ptr = &spn_cli_raw.publish.index,
    },
    {
      .name = "source-url",
      .kind = SP_CLI_OPT_STRING,
      .summary = "Source repository URL (autodetected if omitted)",
      .placeholder = "URL",
      .ptr = &spn_cli_raw.publish.source_url,
    },
    {
      .name = "source-rev",
      .kind = SP_CLI_OPT_STRING,
      .summary = "Source commit (autodetected if omitted)",
      .placeholder = "REV",
      .ptr = &spn_cli_raw.publish.source_rev,
    },
    {
      .name = "dry",
      .summary = "Print the release entry without publishing it",
      .ptr = &spn.cli.publish.dry,
    },
    {
      .name = "allow-dirty",
      .summary = "Publish even if the working tree has uncommitted changes",
      .ptr = &spn.cli.publish.allow_dirty,
    },
  },
  .handler = spn_cli_publish,
};

static sp_cli_cmd_t cmd_index_list = {
  .name = "list",
  .summary = "List configured indexes",
  .handler = spn_cli_index_list,
};

static sp_cli_cmd_t cmd_index_path = {
  .name = "path",
  .summary = "Print the local checkout path of an index",
  .args = {
    {
      .name = "name",
      .kind = SP_CLI_ARG_OPTIONAL,
      .summary = "Index name (default: core)",
      .ptr = &spn_cli_raw.index.name,
    },
  },
  .handler = spn_cli_index_path,
};

static sp_cli_cmd_t cmd_index_sync = {
  .name = "sync",
  .summary = "Refresh indexes, ignoring the staleness window",
  .args = {
    {
      .name = "name",
      .kind = SP_CLI_ARG_OPTIONAL,
      .summary = "Only sync this index",
      .ptr = &spn_cli_raw.index.name,
    },
  },
  .handler = spn_cli_index_sync,
};

static sp_cli_cmd_t cmd_index = {
  .name = "index",
  .summary = "Inspect and manage package indexes",
  .commands = {
    &cmd_index_list,
    &cmd_index_path,
    &cmd_index_sync,
  },
  .handler = spn_cli_index,
};

static sp_cli_cmd_t cmd_root = {
  .name = "spn",
  .summary = "A package manager and build tool for C",
  .opts = {
    {
      .brief = "C",
      .name = "project-dir",
      .kind = SP_CLI_OPT_STRING,
      .summary = "Specify the directory containing project file",
      .placeholder = "DIR",
      .ptr = &spn_cli_raw.project_dir,
    },
    {
      .brief = "f",
      .name = "file",
      .kind = SP_CLI_OPT_STRING,
      .summary = "Specify the project file path",
      .placeholder = "FILE",
      .ptr = &spn_cli_raw.project_file,
    },
    {
      .brief = "o",
      .name = "output",
      .kind = SP_CLI_OPT_STRING,
      .summary = "Output mode: interactive, noninteractive, quiet, none",
      .placeholder = "MODE",
      .ptr = &spn_cli_raw.output,
    },
    {
      .brief = "v",
      .name = "verbose",
      .summary = "Show verbose output",
      .ptr = &spn.cli.verbose,
    },
    {
      .brief = "q",
      .name = "quiet",
      .summary = "Only show errors",
      .ptr = &spn.cli.quiet,
    },
  },
  .env = {
    {
      .name = "SPN_INDEX_REFRESH_SECONDS",
      .kind = SP_CLI_OPT_INTEGER,
      .summary = "Number of seconds which must elapse before the index gets refreshed",
      .ptr = &spn.cli.refresh
    },
  },
  .commands = {
    &cmd_init,
    &cmd_add,
    &cmd_update,
    &cmd_clean,
    &cmd_build,
    &cmd_run,
    &cmd_test,
    &cmd_publish,
    &cmd_index,
  },
};

sp_cli_cmd_t* spn_cli(void) {
  return &cmd_root;
}

sp_cli_result_t spn_cli_errf(sp_cli_t* cli, const c8* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  sp_str_t message = sp_fmt_mem_v(spn.heap, sp_str_view(fmt), args).value;
  va_end(args);
  return sp_cli_set_error(cli, message);
}

bool spn_cli_requires_manifest(sp_cli_cmd_t* cmd) {
  if (cmd == &cmd_init || cmd == &cmd_run) {
    return false;
  }
  sp_carr_for(cmd_index.commands, it) {
    if (cmd == cmd_index.commands[it]) {
      return false;
    }
  }
  return cmd != &cmd_index;
}

void spn_cli_commit(void) {
  spn.cli.project_dir = sp_cstr_as_str(spn_cli_raw.project_dir);
  spn.cli.project_file = sp_cstr_as_str(spn_cli_raw.project_file);
  spn.cli.output = sp_cstr_as_str(spn_cli_raw.output);

  spn.cli.profile.name = sp_cstr_as_str(spn_cli_raw.profile.name);
  spn.cli.profile.toolchain = sp_cstr_as_str(spn_cli_raw.profile.toolchain);
  spn.cli.profile.mode = sp_cstr_as_str(spn_cli_raw.profile.mode);
  spn.cli.profile.target = sp_cstr_as_str(spn_cli_raw.profile.target);
  spn.cli.profile.os = sp_cstr_as_str(spn_cli_raw.profile.os);
  spn.cli.profile.arch = sp_cstr_as_str(spn_cli_raw.profile.arch);
  spn.cli.profile.abi = sp_cstr_as_str(spn_cli_raw.profile.abi);

  spn.cli.init.path = sp_cstr_as_str(spn_cli_raw.init.path);
  spn.cli.add.package = sp_cstr_as_str(spn_cli_raw.add.package);

  spn.cli.run.entry = sp_cstr_as_str(spn_cli_raw.run.entry);
  spn.cli.test.name = sp_cstr_as_str(spn_cli_raw.test.name);

  spn.cli.generate.generator = sp_cstr_as_str(spn_cli_raw.generate.generator);
  spn.cli.generate.compiler = sp_cstr_as_str(spn_cli_raw.generate.compiler);
  spn.cli.generate.path = sp_cstr_as_str(spn_cli_raw.generate.path);

  spn.cli.which.dir = sp_cstr_as_str(spn_cli_raw.which.dir);
  spn.cli.which.package = sp_cstr_as_str(spn_cli_raw.which.package);

  spn.cli.graph.output = sp_cstr_as_str(spn_cli_raw.graph.output);

  spn.cli.publish.index = sp_cstr_as_str(spn_cli_raw.publish.index);
  spn.cli.publish.source_url = sp_cstr_as_str(spn_cli_raw.publish.source_url);
  spn.cli.publish.source_rev = sp_cstr_as_str(spn_cli_raw.publish.source_rev);

  spn.cli.index.name = sp_cstr_as_str(spn_cli_raw.index.name);
}
