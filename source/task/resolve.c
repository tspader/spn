#include "app/types.h"
#include "ctx/types.h"
#include "resolve/types.h"

#include "event/event.h"
#include "index/cache.h"
#include "resolve/resolve.h"
#include "semver/convert.h"
#include "session/session.h"
#include "task/task.h"

spn_task_result_t spn_task_resolve(spn_app_t* app) {
  // @spader
  // This initialization is part of the mess of how we decide whether we're
  // doing simple synchronous work or asynchronous work. The RESOLVE task is
  // the first one where we know we need to initialize everything.
  spn_session_t* session = &app->session;
  spn_session_init(session, &app->package, app->config.profile, sp_str_lit("build"));
  spn_session_set_filter(session, app->config.filter);

  spn_index_cache_t index = SP_ZERO_INITIALIZE();
  spn_index_cache_init(&index, &spn.indexes);

  spn_resolver_t* resolver = sp_alloc_type(spn_resolver_t);
  spn_resolver_init(resolver, &index, &app->package, spn.events);
  app->resolver = resolver;

  spn_init_pkg_unit_for_session(session, &session->units.root, &app->package, SPN_PACKAGE_KIND_ROOT, app->package.version);

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
    case SPN_RESOLVE_STRATEGY_LOCK_FILE: spn_try_as(spn_resolve_from_lock_file(resolver, &app->lock.value), SPN_TASK_ERROR); break;
    case SPN_RESOLVE_STRATEGY_SOLVER:    spn_try_as(spn_resolve_from_solver(resolver), SPN_TASK_ERROR); break;
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
