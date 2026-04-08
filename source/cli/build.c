#include "ctx/types.h"

#include "cli/cli.h"
#include "enum/enum.h"
#include "event/event.h"
#include "triple/triple.h"

sp_app_result_t spn_cli_build(spn_cli_t* cli) {
  spn_cli_build_t* command = &cli->build;

  app.config.force = command->force,
  app.config.filter = (spn_target_filter_t) {
    .name = command->name,
    .only = {
      .bin = command->only.bin,
      .lib = command->only.lib,
      .test = command->only.test,
      .script = command->only.script,
    },
    .disabled = {
      .test = sp_str_empty(command->name) && !command->only.test,
      .script = sp_str_empty(command->name) && !command->only.script,
    }
  };

  // Resolve target triple: --target provides a full triple, --os/--arch/--abi override individual parts.
  // Partial overrides merge with the host triple for missing components.
  spn_triple_t target_triple = {0};
  if (!sp_str_empty(command->target)) {
    target_triple = spn_triple_from_str(command->target);
  }
  // @spader @target We shouldn't need a ternary
  spn_triple_t cli_triple = {
    .arch = sp_str_empty(command->arch) ? 0 : spn_arch_from_str(command->arch),
    .os   = sp_str_empty(command->os)   ? 0 : spn_os_from_str(command->os),
    .abi  = sp_str_empty(command->abi)  ? 0 : spn_abi_from_str(command->abi),
  };
  target_triple = spn_triple_merge(target_triple, cli_triple);

  app.config.overrides = (spn_profile_info_t) {
    .name = command->profile,
    .toolchain = command->toolchain,
    .mode = sp_str_empty(command->mode) ? 0 : spn_dep_build_mode_from_str(command->mode),
    .os = target_triple.os,
    .arch = target_triple.arch,
    .abi = target_triple.abi,
  };

  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_RESOLVE);
  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_SYNC);
  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_CONFIGURE);
  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_PREPARE_BUILD_GRAPH);
  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_RUN_BUILD_GRAPH);

  return SP_APP_CONTINUE;
}
