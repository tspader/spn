#include "app/types.h"
#include "ctx/types.h"
#include "resolve/types.h"

#include "enum/enum.h"
#include "event/event.h"
#include "log/log.h"
#include "index/cache.h"
#include "pkg/id.h"
#include "pkg/pkg.h"
#include "profile/profile.h"
#include "resolve/resolve.h"
#include "semver/convert.h"
#include "session/session.h"
#include "task/task.h"
#include "toolchain/types.h"

spn_task_result_t spn_task_resolve(spn_app_t* app) {
  spn_session_t* session = &app->session;
  if (spn_profile_resolve(session->profiles, &app->config.overrides, &session->profile)) {
    sp_str_t name = spn_profile_select_name(&app->config.overrides);
    spn_log_error("{:fg brightcyan} profile isn't defined", SP_FMT_STR(name));
    return SPN_TASK_ERROR;
  }

  if (!sp_str_ht_exists(app->session.toolchains, session->profile.toolchain)) {
    spn_log_error("{:fg brightcyan} toolchain isn't defined",
      SP_FMT_STR(session->profile.toolchain)
    );
    return SPN_TASK_ERROR;
  }

  spn_log_info("{:fg brightgreen}", SP_FMT_CSTR("resolved profile:"));
  spn_log_info("  name:      {}", SP_FMT_STR(session->profile.name));
  spn_log_info("  toolchain: {}", SP_FMT_STR(session->profile.toolchain));
  spn_log_info("  linkage:   {}", SP_FMT_STR(spn_pkg_linkage_to_str(session->profile.linkage)));
  spn_log_info("  standard:  {}", SP_FMT_STR(spn_c_standard_to_str(session->profile.standard)));
  spn_log_info("  mode:      {}", SP_FMT_STR(spn_dep_build_mode_to_str(session->profile.mode)));
  spn_log_info("  os:        {}", SP_FMT_STR(spn_os_to_str(session->profile.os)));
  spn_log_info("  arch:      {}", SP_FMT_STR(spn_arch_to_str(session->profile.arch)));
  spn_log_info("  abi:       {}", SP_FMT_STR(spn_abi_to_str(session->profile.abi)));
  exit(0);

  session->paths.profile = sp_fs_join_path(session->paths.build, session->profile.name);
  session->events = spn.events;
  spn_session_set_filter(session, app->config.filter);

  spn_index_cache_t index = SP_ZERO_INITIALIZE();
  spn_index_cache_init(&index, &spn.indexes);

  spn_resolver_t* resolver = sp_alloc_type(spn_resolver_t);
  spn_resolver_init(resolver, &index, &app->package, spn.events);
  app->resolver = resolver;

  sp_ht_for_kv(app->package.deps, it) {
    spn_resolver_add(resolver, *it.val);
  }

  spn_toolchain_entry_t* tc_entry = sp_str_ht_get(app->session.toolchains, session->profile.toolchain);
  if (!tc_entry) {
    spn_log_error("toolchain {:fg brightcyan} not in session toolchains", SP_FMT_STR(session->profile.toolchain));
    return SPN_TASK_ERROR;
  }
  spn_toolchain_entry_t toolchain = *tc_entry;
  if (toolchain.kind == SPN_TOOLCHAIN_INDEX) {
    spn_resolver_add(resolver, (spn_pkg_req_t) {
      .id = spn_qualified_name_to_pkg_id(toolchain.request.package),
      .visibility = SPN_VISIBILITY_BUILD,
      .kind = SPN_PACKAGE_KIND_INDEX,
      .range = toolchain.request.range,
    });

  }

  spn_init_pkg_unit_for_session(session, &session->units.root, &app->package, SPN_PACKAGE_KIND_ROOT);

  spn_pkg_unit_t* root = spn_session_find_root(session);

  spn_resolve_strategy_t strategy = app->lock.some == SP_OPT_SOME ?
    SPN_RESOLVE_STRATEGY_LOCK_FILE :
    SPN_RESOLVE_STRATEGY_SOLVER;

  spn_event_buffer_push_ctx(spn.events, &root->ctx, (spn_build_event_t) {
    .kind = SPN_EVENT_RESOLVE_START,
    .resolve_start = {
      .strategy = strategy,
      .num_deps = sp_ht_size(app->package.deps),
    }
  });


  sp_tm_timer_t timer = sp_tm_start_timer();
  switch (strategy) {
    case SPN_RESOLVE_STRATEGY_LOCK_FILE: {
      spn_try_as(spn_resolve_from_lock_file(resolver, &app->lock.value), SPN_TASK_ERROR);
      break;
    }
    case SPN_RESOLVE_STRATEGY_SOLVER: {
      spn_try_as(spn_resolve_from_solver(resolver), SPN_TASK_ERROR);
      break;
    }
  }

  spn_event_buffer_push_ctx(spn.events, &root->ctx, (spn_build_event_t) {
    .kind = SPN_EVENT_RESOLVE_END,
    .resolve_end = {
      .num_resolved = sp_str_ht_size(resolver->resolved),
      .time = sp_tm_read_timer(&timer),
    }
  });

  sp_str_ht_for_kv(resolver->resolved, it) {
    spn_resolved_pkg_t* resolved = it.val;
    spn_event_buffer_push_ctx(spn.events, &root->ctx, (spn_build_event_t) {
      .kind = SPN_EVENT_RESOLVE_PACKAGE,
      .resolve_pkg = {
        .name = *it.key,
        .version = spn_semver_to_str(resolved->version),
        .kind = resolved->kind,
      }
    });
  }

  return SPN_TASK_DONE;
}
