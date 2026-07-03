#include "sp.h"
#include "sp/macro.h"
#include "api/core/types.h"
#include "app/types.h"
#include "ctx/types.h"
#include "error/types.h"
#include "event/types.h"
#include "forward/types.h"
#include "graph/types.h"
#include "pkg/types.h"
#include "session/registry/types.h"
#include "spn.h"
#include "target/types.h"
#include "unit/types.h"

#include "event/event.h"
#include "external/cc.h"
#include "external/tcc/tcc.h"
#include "external/wasm/wasm.h"
#include "graph/graph.h"
#include "sp/sp_glob.h"
#include "session/session.h"
#include "task/task.h"
#include "unit/package.h"

#include <setjmp.h>

spn_err_t compile_package(spn_session_t* session, spn_pkg_unit_t* unit) {
  if (!sp_fs_exists(unit->paths.script)) {
    return SPN_OK;
  }

  sp_tm_timer_t timer = sp_tm_start_timer();

  spn_cc_t cc;
  spn_cc_init(&cc, spn.mem);
  spn_cc_add_runtime(&cc, spn.paths.runtime, spn.paths.include);
  spn_cc_set_profile(&cc, session->profile);
  spn_cc_target_t* target = spn_cc_add_target(&cc, SPN_CC_OUTPUT_JIT, unit->info->name);
  spn_cc_target_add_absolute_source(target, unit->paths.script);

  // Build deps are usable from the build script itself, so compile against them
  sp_ht_for_kv(unit->info->deps, it) {
    spn_requested_pkg_t* dep = it.val;
    if (dep->kind != SPN_DEP_KIND_BUILD) continue;

    spn_pkg_unit_t* dep_unit = spn_session_find_pkg_by_qualified(session, dep->qualified);
    if (!dep_unit) continue;

    spn_cc_target_add_absolute_include(target, dep_unit->paths.include);
    spn_cc_target_add_absolute_include(target, dep_unit->paths.source);
  }

  unit->tcc = sp_alloc_type(spn.mem, spn_tcc_t);
  spn_tcc_init(spn.mem, unit->tcc);
  s32 try_err = 0;
  spn_try_goto(spn_cc_target_to_tcc(&cc, target, unit->tcc), try_err, fail);
  spn_try_goto(tcc_relocate(unit->tcc->s), try_err, fail);

  unit->on_configure = tcc_get_symbol(unit->tcc->s, "configure");
  unit->on_package = tcc_get_symbol(unit->tcc->s, "package");

  unit->time.compile = sp_tm_read_timer(&timer);

  spn_event_buffer_push(spn.events, (spn_build_event_t) {
    .kind = SPN_EVENT_BUILD_SCRIPT_COMPILE,
    .pkg = unit->info,
    .io = &unit->logs.io,
    .script_compile = {
      .script_path = unit->paths.script,
      .time = unit->time.compile,
      .has_configure = unit->on_configure != SP_NULLPTR,
      .has_package = unit->on_package != SP_NULLPTR,
    }
  });

  return SPN_OK;

fail:
  spn_event_buffer_push(spn.events, (spn_build_event_t) {
    .kind = SPN_EVENT_BUILD_SCRIPT_COMPILE_FAILED,
    .pkg = unit->info,
    .io = &unit->logs.io,
    .compile_failed = {
      .script_path = unit->paths.script,
      .error = unit->tcc->error,
    }
  });
  return SPN_ERROR;
}

static bool configure_use_wasm(void) {
  return !sp_str_empty(sp_env_get(spn.env, sp_str_lit("SPN_USE_WASM")));
}

spn_err_t compile_wasm(spn_session_t* session, spn_pkg_unit_t* unit) {
  spn_target_info_t* script = &unit->info->configure;
  if (!sp_fs_is_file(unit->paths.configure)) return SPN_OK;

  sp_tm_timer_t timer = sp_tm_start_timer();
  spn_cc_t* cc = sp_alloc_type(spn.mem, spn_cc_t);
  spn_cc_init(cc, spn.mem);

  spn_profile_info_t profile = {
    .arch = SPN_ARCH_WASM32,
    .os = SPN_OS_WASI,
    .abi = SPN_ABI_NONE,
    .mode = SPN_BUILD_MODE_DEBUG,
    .linkage = SPN_LIB_KIND_SHARED,
    .standard = SPN_C99
  };

  spn_cc_set_profile(cc, profile);
  spn_cc_set_output_dir(cc, unit->paths.generated);
  spn_cc_set_toolchain(cc, session->units.script);
  spn_cc_add_include(cc, spn.paths.include);

  spn_cc_target_t* target = spn_cc_add_target(cc, SPN_CC_OUTPUT_WASM, sp_str_lit("configure.wasm"));
  sp_da_for(script->source, it) {
    spn_cc_target_add_absolute_source(target, script->source[it]);
  }
  sp_da_for(script->include, it) {
    spn_cc_target_add_absolute_include(target, script->include[it]);
  }
  sp_da_for(script->define, it) {
    spn_cc_target_add_define(target, script->define[it]);
  }
  sp_da_for(script->flags, it) {
    spn_cc_target_add_flag(target, script->flags[it]);
  }

  spn_cc_run_t run = spn_cc_target_run(target, unit->paths.work);

  unit->time.compile = sp_tm_read_timer(&timer);

  if (run.result.status.exit_code) {
    spn_event_buffer_push_ex(session->events, unit->info, &unit->logs.io, (spn_build_event_t) {
      .kind = SPN_EVENT_TARGET_BUILD_FAILED,
      .target.failed = {
        .source_file = unit->paths.configure,
        .object_file = unit->paths.object,
        .rc = run.result.status.exit_code,
        .out = run.result.out,
        .args = run.args,
        .time = run.elapsed,
      }
    });
    return SPN_ERROR;
  } else {
    spn_event_buffer_push_ex(session->events, unit->info, &unit->logs.io, (spn_build_event_t) {
      .kind = SPN_EVENT_TARGET_BUILD_PASSED,
      .target.passed = {
        .source_file = unit->paths.configure,
        .object_file = unit->paths.object,
        .args = run.args,
        .out = run.result.out,
        .time = run.elapsed,
      }
    });
  }

  return SPN_OK;
}

spn_err_t configure_package(spn_pkg_unit_t* unit) {
  if (!unit->on_configure) return SPN_OK;

  spn_event_buffer_push(spn.events, (spn_build_event_t) {
    .kind = SPN_EVENT_BUILD_SCRIPT_CONFIGURE,
    .pkg = unit->info,
    .io = &unit->logs.io
  });

  sp_tm_timer_t timer = sp_tm_start_timer();
  jmp_buf jump;
  int status = tcc_setjmp(unit->tcc->s, jump, unit->on_configure);
  if (!status) {
    spn_t* spn = (spn_t*)unit;
    spn_config_t* configure = (spn_config_t*)unit;
    unit->on_configure(spn, configure);
  }
  else {
    // @spader @log
    // What else can we get from TCC here?
    spn_event_buffer_push(spn.events, (spn_build_event_t) {
      .kind = SPN_EVENT_BUILD_SCRIPT_CRASHED,
      .pkg = unit->info,
      .io = &unit->logs.io,
      .crashed.path = sp_str_lit("")
    });
    return SPN_ERROR;
  }

  unit->time.configure = sp_tm_read_timer(&timer);

  spn_event_buffer_push(spn.events, (spn_build_event_t) {
    .kind = SPN_EVENT_BUILD_SCRIPT_CONFIGURE_OK,
    .pkg = unit->info,
    .io = &unit->logs.io,
    .configure.time = unit->time.configure,
  });

  return SPN_OK;
}

s32 on_configure_package(spn_bg_cmd_t* cmd, void* user_data) {
  spn_pkg_unit_t* unit = (spn_pkg_unit_t*)user_data;

  // package() still lives in the TCC script for now, so it compiles either way;
  // the flag only decides who runs configuration
  spn_try(compile_package(unit->session, unit));

  if (configure_use_wasm()) {
    spn_try(compile_wasm(unit->session, unit));
    if (sp_fs_is_file(unit->paths.configure)) {
      spn_try(spn_wasm_run_configure(unit));
    }
    return SPN_OK;
  }

  spn_try(configure_package(unit));
  return SPN_OK;
}

spn_task_result_t spn_task_init_configure_graph(spn_app_t* app) {
  spn_session_t* session = &app->session;
  spn_build_graph_t* graph = &session->configure.graph;
  spn_bg_init(graph, spn.mem);
  spn_pkg_unit_t* root = spn_session_find_root(&app->session);

  if (spn_wasm_init_stupid_global_runtime()) {
    return SPN_TASK_ERROR;
  }

  // Add a graph node for each package
  sp_om_for(session->units.packages, it) {
    spn_pkg_unit_t* unit = sp_om_at(session->units.packages, it);

    unit->nodes.configure.run = spn_bg_add_fn(graph, on_configure_package, unit);
    unit->nodes.configure.stamp = spn_bg_add_file(graph, unit->paths.stamp.configure);
    spn_try(spn_bg_cmd_add_output(graph, unit->nodes.configure.run, unit->nodes.configure.stamp));
  }

  // The root configures last, after every other package; only now do all units
  // have their nodes, so this can't be folded into the loop above
  sp_om_for(session->units.packages, it) {
    spn_pkg_unit_t* unit = sp_om_at(session->units.packages, it);
    spn_loaded_pkg_t* pkg = sp_str_ht_get(session->packages, unit->info->qualified);

    if (pkg->source != SPN_PKG_SOURCE_ROOT) {
      spn_try(spn_bg_cmd_add_input(graph, root->nodes.configure.run, unit->nodes.configure.stamp));
    }
  }

  // Add links between packages
  sp_om_for(session->units.packages, it) {
    spn_pkg_unit_t* unit = sp_str_om_at(session->units.packages, it);

    sp_da(spn_pkg_unit_t*) deps = *sp_om_get(session->units.graph, unit->id);
    sp_da_for(deps, j) {
      spn_pkg_unit_t* parent = deps[j];
      spn_try(spn_bg_cmd_add_input(graph, unit->nodes.configure.run, parent->nodes.configure.stamp));
    }
  }

  session->configure.dirty = spn_bg_compute_forced_dirty(graph);
  session->configure.executor = spn_bg_executor_new(
    graph,
    session->configure.dirty,
    (spn_bg_executor_config_t) {
      .num_threads = 16
    }
  );
  spn_bg_executor_run(session->configure.executor);

  return SPN_TASK_DONE;
}

spn_task_result_t spn_task_update_configure_graph(spn_app_t* app) {
  spn_bg_ctx_t* build = &app->session.configure;

  if (sp_atomic_s32_get(&build->executor->shutdown)) {
    return sp_da_empty(build->executor->errors) ?
      SPN_TASK_DONE :
      SPN_TASK_ERROR;
  }

  return SPN_TASK_CONTINUE;
}

