#include "host/host.h"

static struct {
  sp_str_t package;
  bool test;
  bool build;
} args;

static sp_cli_result_t add(sp_cli_t* cli) {
  try(spn_cli_open(false));

  if (args.test && args.build) {
    return spn_cli_usage("pass at most one of {.cyan} and {.cyan}", sp_fmt_cstr("--test"), sp_fmt_cstr("--build"));
  }

  sp_str_pair_t spec = sp_str_cleave_c8(args.package, '@');
  if (sp_str_empty(spec.first)) {
    return spn_cli_usage("expected a package name");
  }

  try(spn_cli_refresh_indexes());

  return spn_cli_op(spn_add_dependency(host.ctx, (spn_add_request_t) {
    .name = spec.first,
    .version = spec.second,
    .kind = args.test ? SPN_DEP_KIND_TEST : args.build ? SPN_DEP_KIND_BUILD : SPN_DEP_KIND_PACKAGE,
  }));
}

sp_cli_cmd_t spn_cmd_add = {
  .name = "add",
  .summary = "Add a dependency to the manifest",
  .opts = {
    {
      .name = "test",
      .summary = "Add to test dependencies",
      .kind = SP_CLI_OPT_BOOLEAN,
      .ptr = &args.test,
    },
    {
      .name = "build",
      .summary = "Add to build dependencies",
      .kind = SP_CLI_OPT_BOOLEAN,
      .ptr = &args.build,
    },
  },
  .args = {
    {
      .name = "package",
      .kind = SP_CLI_OPT_STR,
      .summary = "Package to add (name or name@version)",
      .ptr = &args.package,
    },
  },
  .handler = add,
};
