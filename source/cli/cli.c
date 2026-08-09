#include "cli/cli.h"

#include "tui/tui.h"

spn_cli_t args;
spn_cli_host_t host;

static sp_cli_cmd_t cmd_init = {
  .name = "init",
  .summary = "Scaffold a new project",
  .opts = {
    {
      .name = "bare",
      .summary = "Only write a manifest",
      .kind = SP_CLI_OPT_BOOLEAN,
      .ptr = &args.init.bare,
    },
  },
  .args = {
    {
      .name = "path",
      .arity = SP_CLI_ARG_OPTIONAL,
      .kind = SP_CLI_OPT_STR,
      .summary = "Directory to scaffold into",
      .ptr = &args.init.path,
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
      .kind = SP_CLI_OPT_BOOLEAN,
      .ptr = &args.add.test,
    },
    {
      .name = "build",
      .summary = "Add to build dependencies",
      .kind = SP_CLI_OPT_BOOLEAN,
      .ptr = &args.add.build,
    },
  },
  .args = {
    {
      .name = "package",
      .kind = SP_CLI_OPT_STR,
      .summary = "Package to add (name or name@version)",
      .ptr = &args.add.package,
    },
  },
  .handler = spn_cli_add,
};

static sp_cli_cmd_t cmd_clean = {
  .name = "clean",
  .summary = "Remove the project build directory",
  .opts = {
    {
      .brief = 'p',
      .name = "profile",
      .kind = SP_CLI_OPT_STR,
      .summary = "Only remove this profile's outputs",
      .placeholder = "PROFILE",
      .ptr = &args.profile.name,
    },
  },
  .handler = spn_cli_clean,
};

static sp_cli_cmd_t cmd_build = {
  .name = "build",
  .summary = "Build the project, including dependencies, from source",
  .opts = {
    {
      .brief = 'f',
      .name = "force",
      .summary = "Force build, even if it exists in store",
      .kind = SP_CLI_OPT_BOOLEAN,
      .ptr = &args.build.force,
    },
    {
      .brief = 'p',
      .name = "profile",
      .kind = SP_CLI_OPT_STR,
      .summary = "Profile to use for building",
      .placeholder = "PROFILE",
      .ptr = &args.profile.name,
    },
    {
      .name = "bin",
      .summary = "Build only binary targets",
      .kind = SP_CLI_OPT_BOOLEAN,
      .ptr = &args.build.only.bin,
    },
    {
      .name = "lib",
      .summary = "Build only library targets",
      .kind = SP_CLI_OPT_BOOLEAN,
      .ptr = &args.build.only.lib,
    },
    {
      .name = "test",
      .summary = "Build only test targets",
      .kind = SP_CLI_OPT_BOOLEAN,
      .ptr = &args.build.only.test,
    },
    {
      .name = "script",
      .summary = "Build only script targets",
      .kind = SP_CLI_OPT_BOOLEAN,
      .ptr = &args.build.only.script,
    },
    {
      .name = "toolchain",
      .kind = SP_CLI_OPT_STR,
      .summary = "Override toolchain",
      .placeholder = "NAME",
      .ptr = &args.profile.toolchain,
    },
    {
      .brief = 'm',
      .name = "mode",
      .kind = SP_CLI_OPT_STR,
      .summary = "Override build mode (debug, release)",
      .placeholder = "MODE",
      .ptr = &args.profile.mode,
    },
    {
      .name = "opt",
      .kind = SP_CLI_OPT_STR,
      .summary = "Override optimization level (0, 1, 2, 3, s, z)",
      .placeholder = "LEVEL",
      .ptr = &args.profile.opt,
    },
    {
      .name = "sanitize",
      .kind = SP_CLI_OPT_STR,
      .summary = "Enable sanitizers (address, thread, undefined, memory, leak)",
      .placeholder = "LIST",
      .ptr = &args.profile.sanitize,
    },
    {
      .name = "target",
      .kind = SP_CLI_OPT_STR,
      .summary = "Target triple (e.g. aarch64-linux-gnu)",
      .placeholder = "TRIPLE",
      .ptr = &args.profile.target,
    },
    {
      .name = "os",
      .kind = SP_CLI_OPT_STR,
      .summary = "Override target OS (linux, macos, windows)",
      .placeholder = "OS",
      .ptr = &args.profile.os,
    },
    {
      .name = "arch",
      .kind = SP_CLI_OPT_STR,
      .summary = "Override target architecture (x86_64, aarch64)",
      .placeholder = "ARCH",
      .ptr = &args.profile.arch,
    },
    {
      .name = "abi",
      .kind = SP_CLI_OPT_STR,
      .summary = "Override target ABI (gnu, musl, mingw)",
      .placeholder = "ABI",
      .ptr = &args.profile.abi,
    },
  },
  .args = {
    {
      .name = "name",
      .arity = SP_CLI_ARG_REST,
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
      .brief = 'p',
      .name = "profile",
      .kind = SP_CLI_OPT_STR,
      .summary = "Profile to use when resolving build dependencies",
      .placeholder = "PROFILE",
      .ptr = &args.profile.name,
    },
    {
      .name = "toolchain",
      .kind = SP_CLI_OPT_STR,
      .summary = "Override toolchain",
      .placeholder = "NAME",
      .ptr = &args.profile.toolchain,
    },
    {
      .brief = 'm',
      .name = "mode",
      .kind = SP_CLI_OPT_STR,
      .summary = "Override build mode (debug, release)",
      .placeholder = "MODE",
      .ptr = &args.profile.mode,
    },
    {
      .name = "opt",
      .kind = SP_CLI_OPT_STR,
      .summary = "Override optimization level (0, 1, 2, 3, s, z)",
      .placeholder = "LEVEL",
      .ptr = &args.profile.opt,
    },
    {
      .name = "sanitize",
      .kind = SP_CLI_OPT_STR,
      .summary = "Enable sanitizers (address, thread, undefined, memory, leak)",
      .placeholder = "LIST",
      .ptr = &args.profile.sanitize,
    },
  },
  .args = {
    {
      .name = "entry",
      .kind = SP_CLI_OPT_STR,
      .summary = "Script target name or relative .c file",
      .ptr = &args.run.entry,
    },
  },
  .handler = spn_cli_run,
};

static sp_cli_cmd_t cmd_test = {
  .name = "test",
  .summary = "Build and run tests",
  .opts = {
    {
      .brief = 'p',
      .name = "profile",
      .kind = SP_CLI_OPT_STR,
      .summary = "Profile to use for building",
      .placeholder = "PROFILE",
      .ptr = &args.profile.name,
    },
    {
      .name = "toolchain",
      .kind = SP_CLI_OPT_STR,
      .summary = "Override toolchain",
      .placeholder = "NAME",
      .ptr = &args.profile.toolchain,
    },
    {
      .brief = 'm',
      .name = "mode",
      .kind = SP_CLI_OPT_STR,
      .summary = "Override build mode (debug, release)",
      .placeholder = "MODE",
      .ptr = &args.profile.mode,
    },
    {
      .name = "opt",
      .kind = SP_CLI_OPT_STR,
      .summary = "Override optimization level (0, 1, 2, 3, s, z)",
      .placeholder = "LEVEL",
      .ptr = &args.profile.opt,
    },
    {
      .name = "sanitize",
      .kind = SP_CLI_OPT_STR,
      .summary = "Enable sanitizers (address, thread, undefined, memory, leak)",
      .placeholder = "LIST",
      .ptr = &args.profile.sanitize,
    },
  },
  .args = {
    {
      .name = "name",
      .arity = SP_CLI_ARG_REST,
      .summary = "Test targets to build and run (default: all)",
    },
  },
  .handler = spn_cli_test,
};

static sp_cli_cmd_t cmd_publish = {
  .name = "publish",
  .summary = "Publish a release to an index",
  .opts = {
    {
      .brief = 'i',
      .name = "index",
      .kind = SP_CLI_OPT_STR,
      .summary = "Index to publish to",
      .placeholder = "NAME",
      .ptr = &args.publish.index,
    },
    {
      .name = "source-url",
      .kind = SP_CLI_OPT_STR,
      .summary = "Source repository URL (autodetected if omitted)",
      .placeholder = "URL",
      .ptr = &args.publish.source_url,
    },
    {
      .name = "source-rev",
      .kind = SP_CLI_OPT_STR,
      .summary = "Source commit (autodetected if omitted)",
      .placeholder = "REV",
      .ptr = &args.publish.source_rev,
    },
    {
      .name = "dry",
      .summary = "Print the release entry without publishing it",
      .kind = SP_CLI_OPT_BOOLEAN,
      .ptr = &args.publish.dry,
    },
    {
      .name = "allow-dirty",
      .summary = "Publish even if the working tree has uncommitted changes",
      .ptr = &args.publish.allow_dirty,
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
      .arity = SP_CLI_ARG_OPTIONAL,
      .kind = SP_CLI_OPT_STR,
      .summary = "Index name (default: core)",
      .ptr = &args.index.name,
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
      .arity = SP_CLI_ARG_OPTIONAL,
      .kind = SP_CLI_OPT_STR,
      .summary = "Only sync this index",
      .ptr = &args.index.name,
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
      .brief = 'C',
      .name = "project-dir",
      .kind = SP_CLI_OPT_STR,
      .summary = "Specify the directory containing project file",
      .placeholder = "DIR",
      .ptr = &args.project_dir,
    },
    {
      .brief = 'f',
      .name = "file",
      .kind = SP_CLI_OPT_STR,
      .summary = "Specify the project file path",
      .placeholder = "FILE",
      .ptr = &args.project_file,
    },
    {
      .brief = 'o',
      .name = "output",
      .kind = SP_CLI_OPT_STR,
      .summary = "Output mode: interactive, noninteractive, quiet, none, json",
      .placeholder = "MODE",
      .ptr = &args.output,
    },
    {
      .brief = 'v',
      .name = "verbose",
      .summary = "Show verbose output",
      .kind = SP_CLI_OPT_BOOLEAN,
      .ptr = &args.verbose,
    },
    {
      .brief = 'q',
      .name = "quiet",
      .summary = "Only show errors",
      .kind = SP_CLI_OPT_BOOLEAN,
      .ptr = &args.quiet,
    },
  },
  .env = {
    {
      .name = "SPN_INDEX_REFRESH_SECONDS",
      .kind = SP_CLI_OPT_U32,
      .summary = "Number of seconds which must elapse before the index gets refreshed",
      .ptr = &args.refresh
    },
  },
  .commands = {
    &cmd_init,
    &cmd_add,
    &cmd_clean,
    &cmd_build,
    &cmd_run,
    &cmd_test,
    &cmd_publish,
    &cmd_index,
  },
};

sp_cli_cmd_t* spn_cli() {
  return &cmd_root;
}

spn_str_arr_t spn_cli_rest_names(sp_cli_t* cli) {
  u32 count = 0;
  for (const c8** it = cli->rest; *it; it++) {
    count++;
  }

  spn_str_arr_t names = { .items = sp_alloc_n(host.mem, sp_str_t, count), .count = count };
  sp_for(it, count) {
    names.items[it] = sp_cstr_as_str(cli->rest[it]);
  }
  return names;
}

sp_cli_result_t spn_cli_open(bool project_optional) {
  spn_err_t err = spn_ctx_open(host.ctx, (spn_open_request_t) {
    .dir = args.project_dir,
    .index_refresh_seconds = args.refresh,
    .project_optional = project_optional,
  });
  return err ? SP_CLI_ERR : SP_CLI_OK;
}

sp_cli_result_t spn_cli_session(sp_cli_t* cli, spn_session_config_t config) {
  try(spn_cli_parse_profile(cli, &config.profile));
  return spn_ctx_open_session(host.ctx, &config, &host.session) ? SP_CLI_ERR : SP_CLI_OK;
}

sp_cli_result_t spn_cli_refresh_indexes() {
  return spn_cli_op(spn_sync_indexes(host.ctx, (spn_sync_request_t) sp_zero));
}

spn_err_t spn_cli_wait(spn_op_t* op) {
  sp_atomic_ptr_store(&host.active, op, SP_ATOMIC_SEQ_CST);
  while (!spn_op_done(op)) {
    if (sp_atomic_s32_load(&host.interrupted, SP_ATOMIC_SEQ_CST)) {
      spn_op_cancel(op);
    }
    spn_tui_poll(&tui, op);
    if (!spn_op_done(op)) {
      sp_semaphore_wait_for(&host.wake, 10);
    }
  }
  sp_atomic_ptr_store(&host.active, SP_NULLPTR, SP_ATOMIC_RELEASE);

  spn_tui_op_done(&tui, op);
  return spn_op_result(op).err;
}

sp_cli_result_t spn_cli_op(spn_op_t* op) {
  spn_err_t err = spn_cli_wait(op);
  spn_op_free(op);
  return err ? SP_CLI_ERR : SP_CLI_OK;
}

sp_cli_result_t spn_cli_error(sp_cli_t* cli, const c8* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  sp_str_t message = sp_fmt_mem_v(host.mem, sp_cstr_as_str(fmt), args).value;
  va_end(args);
  return sp_cli_set_error(cli, message);
}

