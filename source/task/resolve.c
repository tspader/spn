#include "app/types.h"
#include "ctx/types.h"
#include "intern/types.h"
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
#include "unit/types.h"

spn_err_t init_session(spn_session_t* session, spn_pkg_info_t* root) {
  // @spader Clean this up
  sp_str_om_for(root->toolchains, it) {
    spn_toolchain_entry_t entry = *sp_str_om_at(root->toolchains, it);
    if (entry.kind == SPN_TOOLCHAIN_INDEX) continue;
    sp_str_ht_insert(session->toolchains, entry.name, entry);
  }

  sp_da_for(spn.toolchains, it) {
    sp_str_ht_insert(session->toolchains, spn.toolchains[it].name, spn.toolchains[it]);
  }

  spn_toolchain_entry_t builtin_toolchains[] = {
    {
      .name = sp_str_lit("builtin"),
      .kind = SPN_TOOLCHAIN_BUILTIN,
      .info = {
        .compiler = { .program = sp_str_lit("cc") },
        .linker   = { .program = sp_str_lit("cc") },
        .archiver = { .program = sp_str_lit("ar") },
        .driver = SPN_CC_DRIVER_GCC,
      },
    }
  };
  sp_carr_for(builtin_toolchains, it) {
    spn_toolchain_entry_t entry = builtin_toolchains[it];
    sp_str_ht_insert(session->toolchains, entry.name, entry);
  }

  // Build the list of available profiles
  spn_profile_populate(&session->profiles, root);

  session->pkg = root;
  session->paths.root = spn.paths.project;
  session->paths.build = sp_fs_join_path(session->mem, spn.paths.project, sp_str_lit("build"));
  session->events = spn.events;
  session->intern = spn.intern;
  sp_mutex_init(&session->mutex, SP_MUTEX_PLAIN);

  return SPN_OK;
}

spn_err_t apply_config(spn_session_t* session, spn_app_config_t config) {
  if (spn_profile_resolve(session->profiles, &config.overrides, &session->profile)) {
    sp_str_t name = spn_profile_select_name(&config.overrides);
    spn_log_error("profile {.cyan} isn't defined", SP_FMT_STR(name));
    return SPN_ERROR;
  }

  if (!sp_str_ht_exists(session->toolchains, session->profile.toolchain)) {
    sp_str_t name = session->profile.toolchain;
    spn_log_error("toolchain {.cyan} isn't defined", SP_FMT_STR(name));
    return SPN_ERROR;
  }

  session->paths.profile = sp_fs_join_path(session->mem, session->paths.build, session->profile.name);
  session->filter = config.filter;

  return SPN_OK;
}

void add_root(spn_session_t* session, spn_resolve_query_t* query) {
  spn_resolve_query_add(query, (spn_requested_pkg_t) {
    .qualified = session->pkg->qualified,
    .source = SPN_PKG_SOURCE_ROOT,
  });

}

void emit_resolved(spn_resolve_query_t* query) {
  sp_str_ht_for_kv(query->result, it) {
    spn_event_buffer_push(spn.events, (spn_build_event_t) {
      .kind = SPN_EVENT_RESOLVE_PACKAGE,
      .resolve_pkg = {
        .name = *it.key,
        .version = spn_semver_to_str(it.val->version),
      }
    });
  }

  spn_event_buffer_push(spn.events, (spn_build_event_t) {
    .kind = SPN_EVENT_RESOLVE_END,
    .resolve_end = {
      .num_resolved = sp_str_ht_size(query->result),
      .time = query->time,
    }
  });

}

spn_err_t load_root_package(spn_session_t* session);

spn_task_result_t spn_task_resolve(spn_app_t* app) {
  spn_session_t* session = &app->session;
  spn_pkg_info_t* pkg = &app->package;

  spn_try_as(init_session(session, pkg), SPN_TASK_ERROR);
  spn_try_as(apply_config(session, app->config), SPN_TASK_ERROR);

  // The solver reads local packages' deps from the registry, so the root must
  // be registered before we resolve.
  spn_try_as(load_root_package(session), SPN_TASK_ERROR);

  spn_index_cache_t index = SP_ZERO_INITIALIZE();
  spn_index_cache_init(&index, &spn.indexes);

  spn_resolver_t* resolver = sp_alloc_type(app->session.mem, spn_resolver_t);
  spn_resolver_init(resolver, &index, &session->packages, spn.events);
  app->resolver = resolver;

  spn_resolve_query_t query = sp_zero_initialize();
  add_root(session, &query);

  // Resolve
  spn_resolve_strategy_t strategy = app->lock.some == SP_OPT_SOME ?
    SPN_RESOLVE_STRATEGY_LOCK_FILE :
    SPN_RESOLVE_STRATEGY_SOLVER;

  spn_try_as(spn_resolve_from_solver(resolver, &query), SPN_TASK_ERROR);
  session->resolve = query.result;

  // switch (strategy) {
  //   case SPN_RESOLVE_STRATEGY_LOCK_FILE: {
  //     spn_try_as(spn_resolve_from_lock_file(resolver, &app->lock.value), SPN_TASK_ERROR);
  //     break;
  //   }
  //   case SPN_RESOLVE_STRATEGY_SOLVER: {
  //     spn_try_as(spn_resolve_from_solver(resolver), SPN_TASK_ERROR);
  //     break;
  //   }
  // }

  emit_resolved(&query);

  return SPN_TASK_DONE;
}
