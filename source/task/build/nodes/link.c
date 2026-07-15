#include "ctx/types.h"
#include "sp/sp_graph.h"
#include "spn.h"
#include "unit/types.h"
#include "session/types.h"

#include "external/cc.h"
#include "compiler/driver.h"
#include "event/event.h"
#include "session/invocation.h"
#include "task/build/build.h"
#include "unit/compiler.h"
#include "unit/package.h"

static spn_cc_link_t spn_build_link_desc(sp_mem_t mem, spn_target_unit_t* target) {
  spn_cc_link_t link = {
    .lang = target->link.lang,
    .kind = target->kind,
    .system_libs = target->link.system_libs,
    .hidden_libs = target->link.hidden_libs,
    .lib_dirs = target->link.lib_dirs,
    .frameworks = target->link.frameworks,
  };
  sp_da_init(mem, link.objects);
  sp_da_init(mem, link.args);
  sp_da_init(mem, link.libs);
  sp_da_init(mem, link.rpath);

  switch (target->pkg->build->profile.os) {
    case SPN_OS_LINUX: {
      sp_da_push(link.rpath, sp_str_lit("$ORIGIN"));
      break;
    }
    case SPN_OS_MACOS: {
      sp_da_push(link.rpath, sp_str_lit("@loader_path"));
      link.min_os = target->link.min_os;
      break;
    }
    case SPN_OS_WINDOWS: {
      link.subsystem = target->info->windows.subsystem;
      break;
    }
    case SPN_OS_WASI:
    case SPN_OS_NONE: {
      break;
    }
  }

  return link;
}

spn_err_union_t spn_build_render_target(sp_mem_t mem, spn_target_unit_t* target, sp_str_t output, sp_da(sp_str_t) objects, spn_invocation_t* invocation) {
  spn_profile_info_t* profile = &target->pkg->build->profile;
  spn_cc_toolchain_t toolchain = spn_toolchain_unit_compiler(target->pkg->build->toolchain);
  sp_ps_config_t ps = sp_zero_s(sp_ps_config_t);

  switch (target->kind) {
    case SPN_CC_OUTPUT_STATIC_LIB: {
      spn_cc_archive_t archive = {
        .output = output,
        .objects = objects,
      };
      sp_da_init(mem, archive.args);
      try_union(spn_cc_render_archive(mem, &toolchain, profile, &archive, &ps));
      break;
    }
    case SPN_CC_OUTPUT_EXE:
    case SPN_CC_OUTPUT_SHARED_LIB:
    case SPN_CC_OUTPUT_REACTOR: {
      spn_cc_link_t link = spn_build_link_desc(mem, target);
      link.output = output;
      link.objects = objects;
      try_union(spn_cc_render_link(mem, &toolchain, profile, &link, &ps));
      break;
    }
    case SPN_CC_OUTPUT_OBJECT: {
      sp_unreachable_case();
    }
  }

  *invocation = (spn_invocation_t) {
    .program = ps.command,
    .args = ps.dyn_args,
    .cwd = target->pkg->paths.work,
  };
  return spn_result(SPN_OK);
}

spn_err_union_t spn_build_validate_target(spn_target_unit_t* target) {
  spn_profile_info_t* profile = &target->pkg->build->profile;
  spn_cc_toolchain_t toolchain = spn_toolchain_unit_compiler(target->pkg->build->toolchain);

  if (!sp_da_empty(target->objects)) {
    try_union(spn_cc_validate_compile(&toolchain, profile));
  }

  switch (target->kind) {
    case SPN_CC_OUTPUT_STATIC_LIB: {
      return spn_cc_validate_archive(&toolchain, profile);
    }
    case SPN_CC_OUTPUT_EXE:
    case SPN_CC_OUTPUT_SHARED_LIB:
    case SPN_CC_OUTPUT_REACTOR: {
      return spn_cc_validate_link(&toolchain, profile, target->kind, !sp_da_empty(target->link.frameworks));
    }
    case SPN_CC_OUTPUT_OBJECT: {
      return spn_result(SPN_OK);
    }
  }
  sp_unreachable_return(spn_result(SPN_ERROR));
}

sp_da(sp_str_t) spn_build_target_objects(sp_mem_t mem, spn_target_unit_t* target) {
  sp_da(sp_str_t) objects = sp_da_new(mem, sp_str_t);
  sp_da_for(target->objects, it) {
    sp_da_push(objects, target->objects[it]->paths.object);
  }
  if (!sp_da_empty(target->info->embed)) {
    sp_da_push(objects, get_embed_object_path(mem, target));
  }
  return objects;
}

spn_err_t emit_link_passed(spn_target_unit_t* unit, sp_str_t output, sp_str_t out, u64 elapsed) {
  spn_event_buffer_push(spn.events, (spn_build_event_t) {
    .kind = SPN_EVENT_LINK_PASSED,
    .pkg = unit->pkg->info,
    .io = &unit->logs,
    .target.name = unit->info->name,
    .target.link_passed = {
      .output_path = output,
      .invocation = &unit->invocation,
      .out = out,
      .time = elapsed,
    }
  });
  return SPN_OK;
}

spn_err_t emit_link_failed(spn_target_unit_t* unit, s32 rc, sp_str_t out, sp_str_t err) {
  spn_event_buffer_push(spn.events, (spn_build_event_t) {
    .kind = SPN_EVENT_LINK_FAILED,
    .pkg = unit->pkg->info,
    .io = &unit->logs,
    .target.name = unit->info->name,
    .target.link_failed = {
      .exit_code = rc,
      .out = out,
      .err = err,
      .invocation = &unit->invocation,
    }
  });
  return SPN_ERROR;
}


s32 spn_link_target_run(spn_target_unit_t* target, sp_str_t output, sp_da(sp_str_t) objects) {
  spn_pkg_unit_announce_compile(target->pkg);

  spn_event_buffer_push(spn.events, (spn_build_event_t) {
    .kind = SPN_EVENT_LINK_START,
    .pkg = target->pkg->info,
    .io = &target->logs,
    .target.name = target->info->name,
    .target.link_start = {
      .target = target
    }
  });

  spn_err_union_t render = spn_build_render_target(spn.mem, target, output, objects, &target->invocation);
  if (render.kind) {
    spn_event_buffer_push(spn.events, (spn_build_event_t) {
      .kind = SPN_EVENT_ERR,
      .err = render,
    });
    return 1;
  }

  spn_invocation_result_t run = spn_invocation_run(&target->invocation);

  if (run.result.status.exit_code) {
    return emit_link_failed(target, run.result.status.exit_code, run.result.out, run.result.err);
  }

  return emit_link_passed(target, get_target_output_path(spn.mem, target), run.result.out, run.elapsed);
}

s32 link_target(spn_bg_cmd_t* cmd, void* user_data) {
  spn_target_unit_t* target = (spn_target_unit_t*)user_data;

  if (sp_da_empty(target->objects)) return 0;

  sp_str_t output = get_target_output_path(spn.mem, target);
  return spn_link_target_run(target, output, spn_build_target_objects(spn.mem, target));
}
