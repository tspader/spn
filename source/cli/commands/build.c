#include "host/host.h"

static struct {
  bool force;
  struct {
    bool test;
    bool bin;
    bool lib;
    bool script;
    bool example;
  } only;
} args;

static void set_rule(spn_target_rule_t* rule, bool selected, spn_str_arr_t names) {
  if (!selected) {
    rule->kind = SPN_TARGET_RULE_NONE;
    return;
  }

  rule->kind = names.count ? SPN_TARGET_RULE_NAMED : SPN_TARGET_RULE_ALL;
  rule->names = names;
}

static sp_cli_result_t build(sp_cli_t* cli) {
  try(spn_cli_open(false));

  spn_session_config_t config = { .force = args.force };
  spn_str_arr_t names = spn_cli_rest_names(cli);

  bool specific = args.only.bin || args.only.lib || args.only.test || args.only.script || args.only.example;
  if (specific || names.count) {
    bool all_kinds = !specific;
    set_rule(&config.selection.bin, all_kinds || args.only.bin, names);
    set_rule(&config.selection.lib, all_kinds || args.only.lib, names);
    set_rule(&config.selection.test, all_kinds || args.only.test, names);
    set_rule(&config.selection.script, all_kinds || args.only.script, names);
    set_rule(&config.selection.example, all_kinds || args.only.example, names);
  }

  try(spn_cli_refresh_indexes());
  try(spn_cli_session(config));
  return spn_cli_op(spn_build(host.session));
}

sp_cli_cmd_t spn_cmd_build = {
  .name = "build",
  .summary = "Build the project, including dependencies, from source",
  .opts = {
    {
      .brief = 'f',
      .name = "force",
      .summary = "Force build, even if it exists in store",
      .kind = SP_CLI_OPT_BOOLEAN,
      .ptr = &args.force,
    },
    {
      .brief = 'p',
      .name = "profile",
      .kind = SP_CLI_OPT_STR,
      .summary = "Profile to use for building",
      .placeholder = "PROFILE",
      .ptr = &host.args.profile.name,
    },
    {
      .name = "bin",
      .summary = "Build only binary targets",
      .kind = SP_CLI_OPT_BOOLEAN,
      .ptr = &args.only.bin,
    },
    {
      .name = "lib",
      .summary = "Build only library targets",
      .kind = SP_CLI_OPT_BOOLEAN,
      .ptr = &args.only.lib,
    },
    {
      .name = "test",
      .summary = "Build only test targets",
      .kind = SP_CLI_OPT_BOOLEAN,
      .ptr = &args.only.test,
    },
    {
      .name = "script",
      .summary = "Build only script targets",
      .kind = SP_CLI_OPT_BOOLEAN,
      .ptr = &args.only.script,
    },
    {
      .name = "example",
      .summary = "Build only example targets",
      .kind = SP_CLI_OPT_BOOLEAN,
      .ptr = &args.only.example,
    },
    {
      .name = "toolchain",
      .kind = SP_CLI_OPT_STR,
      .summary = "Override toolchain",
      .placeholder = "NAME",
      .ptr = &host.args.profile.toolchain,
    },
    {
      .brief = 'm',
      .name = "mode",
      .kind = SP_CLI_OPT_CHOICE,
      .summary = "Override build mode",
      .placeholder = "MODE",
      .ptr = &host.args.profile.mode,
      .choices = {
        { .name = "debug", .value = SPN_MODE_DEBUG },
        { .name = "release", .value = SPN_MODE_RELEASE },
      },
    },
    {
      .name = "opt",
      .kind = SP_CLI_OPT_CHOICE,
      .summary = "Override optimization level",
      .placeholder = "LEVEL",
      .ptr = &host.args.profile.opt,
      .choices = {
        { .name = "0", .value = SPN_OPT_LEVEL_0 },
        { .name = "1", .value = SPN_OPT_LEVEL_1 },
        { .name = "2", .value = SPN_OPT_LEVEL_2 },
        { .name = "3", .value = SPN_OPT_LEVEL_3 },
        { .name = "s", .value = SPN_OPT_LEVEL_S },
        { .name = "z", .value = SPN_OPT_LEVEL_Z },
      },
    },
    {
      .name = "sanitize",
      .kind = SP_CLI_OPT_STR,
      .summary = "Enable sanitizers (address, thread, undefined, memory, leak)",
      .placeholder = "LIST",
      .ptr = &host.args.profile.sanitize,
    },
    {
      .name = "target",
      .kind = SP_CLI_OPT_STR,
      .summary = "Target triple (e.g. aarch64-linux-gnu)",
      .placeholder = "TRIPLE",
      .ptr = &host.args.profile.target,
    },
    {
      .name = "os",
      .kind = SP_CLI_OPT_CHOICE,
      .summary = "Override target OS",
      .placeholder = "OS",
      .ptr = &host.args.profile.os,
      .choices = {
        { .name = "linux", .value = SPN_OS_LINUX },
        { .name = "macos", .value = SPN_OS_MACOS },
        { .name = "windows", .value = SPN_OS_WINDOWS },
        { .name = "wasi", .value = SPN_OS_WASI },
      },
    },
    {
      .name = "arch",
      .kind = SP_CLI_OPT_CHOICE,
      .summary = "Override target architecture",
      .placeholder = "ARCH",
      .ptr = &host.args.profile.arch,
      .choices = {
        { .name = "x86_64", .value = SPN_ARCH_X64 },
        { .name = "aarch64", .value = SPN_ARCH_ARM64 },
        { .name = "wasm32", .value = SPN_ARCH_WASM32 },
      },
    },
    {
      .name = "abi",
      .kind = SP_CLI_OPT_CHOICE,
      .summary = "Override target ABI",
      .placeholder = "ABI",
      .ptr = &host.args.profile.abi,
      .choices = {
        { .name = "gnu", .value = SPN_ABI_GNU },
        { .name = "musl", .value = SPN_ABI_MUSL },
        { .name = "msvc", .value = SPN_ABI_MSVC },
        { .name = "apple", .value = SPN_ABI_APPLE },
      },
    },
  },
  .args = {
    {
      .name = "name",
      .arity = SP_CLI_ARG_REST,
      .summary = "Name of entry to build",
    },
  },
  .handler = build,
};
