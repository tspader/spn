#include "app/types.h"
#include "ctx/types.h"
#include "resolve/types.h"

#include "cli/cli.h"
#include "enum/enum.h"
#include "event/event.h"
#include "log/log.h"
#include "index/cache.h"
#include "pkg/id.h"
#include "pkg/pkg.h"
#include "resolve/resolve.h"
#include "semver/convert.h"
#include "session/session.h"
#include "task/task.h"
#include "toolchain/types.h"

static void overlay_profile(spn_profile_t* dst, spn_profile_t* src) {
  if (!sp_str_empty(src->name))      dst->name = src->name;
  if (!sp_str_empty(src->toolchain)) dst->toolchain = src->toolchain;
  if (src->linkage)                  dst->linkage = src->linkage;
  if (src->standard)                 dst->standard = src->standard;
  if (src->mode)                     dst->mode = src->mode;
  if (src->os)                       dst->os = src->os;
  if (src->arch)                     dst->arch = src->arch;
}

static spn_profile_t resolve_profile(spn_app_t* app) {
  spn_profile_t profile = {
    .name = sp_str_lit("default"),
    .linkage = SPN_LIB_KIND_SHARED,
    .standard = SPN_C11,
    .mode = SPN_BUILD_MODE_DEBUG,
  };

  if (!sp_om_empty(app->package.profiles)) {
    spn_profile_t* def = spn_pkg_get_default_profile(&app->package);
    overlay_profile(&profile, def);
  }

  if (!sp_str_empty(app->config.overrides.profile)) {
    spn_profile_t* named = sp_om_get(app->package.profiles, app->config.overrides.profile);
    if (named) {
      overlay_profile(&profile, named);
    }
  }

  if (!sp_str_empty(app->config.overrides.toolchain)) {
    if (!sp_str_ht_exists(app->session.toolchains, app->config.overrides.toolchain)) {
      spn_log_error("{:fg brightcyan} toolchain isn't defined",
        SP_FMT_STR(app->config.overrides.toolchain)
      );
      return (spn_profile_t) { .mode = SPN_BUILD_MODE_NONE };
    }
    profile.toolchain = app->config.overrides.toolchain;
  }

  if (!sp_str_empty(app->config.overrides.mode)) {
    profile.mode = spn_dep_build_mode_from_str(app->config.overrides.mode);
  }

  return profile;
}

spn_task_result_t spn_task_resolve(spn_app_t* app) {
  spn_session_t* session = &app->session;
  session->profile = resolve_profile(app);
  if (session->profile.mode == SPN_BUILD_MODE_NONE) {
    return SPN_TASK_ERROR;
  }
 SP_LOG("resolved profile: name={} toolchain={} mode={} linkage={} standard={}",
        SP_FMT_STR(session->profile.name),
        SP_FMT_STR(session->profile.toolchain),
        SP_FMT_STR(spn_dep_build_mode_to_str(session->profile.mode)),
        SP_FMT_STR(spn_pkg_linkage_to_str(session->profile.linkage)),
        SP_FMT_STR(spn_c_standard_to_str(session->profile.standard))
      );
      exit(0);
  if (!sp_str_empty(session->profile.toolchain)) {
    spn_cli_set_toolchain(app, session->profile.toolchain);
  }

  session->paths.profile = sp_fs_join_path(session->paths.build, session->profile.name);
  spn_session_set_filter(session, app->config.filter);

  spn_index_cache_t index = SP_ZERO_INITIALIZE();
  spn_index_cache_init(&index, &spn.indexes);

  spn_resolver_t* resolver = sp_alloc_type(spn_resolver_t);
  spn_resolver_init(resolver, &index, &app->package, spn.events);
  app->resolver = resolver;

  sp_ht_for_kv(app->package.deps, it) {
    spn_resolver_add(resolver, *it.val);
  }

  spn_toolchain_entry_t toolchain = *app->config.toolchain;
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

  // Resolve
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

  // Emit events for the results of the resolve (per-package and overall)
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
