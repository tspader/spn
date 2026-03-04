#include "app/app.h"
#include "ctx/ctx.h"
#include "event/event.h"
#include "index/cache.h"
#include "log.h"
#include "resolve/resolve.h"
#include "session/session.h"

spn_task_result_t spn_task_resolve(spn_app_t* app) {
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

  spn_trace_info(spn.events, root->ctx.pkg, &root->ctx.logs,
    "resolving package {} with {} deps",
    SP_FMT_STR(app->package.name),
    SP_FMT_U32(sp_ht_size(app->package.deps)));

  if (app->lock.some == SP_OPT_SOME) {
    spn_event_buffer_push_ctx(spn.events, &root->ctx, (spn_build_event_t) {
      .kind = SPN_EVENT_RESOLVE,
      .resolve = { .strategy = SPN_RESOLVE_STRATEGY_LOCK_FILE }
    });

    spn_trace_debug(spn.events, root->ctx.pkg, &root->ctx.logs,
      "using lock file for resolution", SP_FMT_U32(0));

    if (spn_resolve_from_lock_file(resolver, &app->lock.value)) {
      spn_trace_error(spn.events, root->ctx.pkg, &root->ctx.logs,
        "resolution from lock file failed", SP_FMT_U32(0));
      return SPN_TASK_ERROR;
    }
  }
  else {
    spn_event_buffer_push_ctx(spn.events, &root->ctx, (spn_build_event_t) {
      .kind = SPN_EVENT_RESOLVE,
      .resolve = { .strategy = SPN_RESOLVE_STRATEGY_SOLVER }
    });

    spn_trace_debug(spn.events, root->ctx.pkg, &root->ctx.logs,
      "using solver for resolution", SP_FMT_U32(0));

    if (spn_resolve_from_solver(resolver)) {
      spn_trace_error(spn.events, root->ctx.pkg, &root->ctx.logs,
        "resolution from solver failed", SP_FMT_U32(0));
      return SPN_TASK_ERROR;
    }
  }

  spn_trace_info(spn.events, root->ctx.pkg, &root->ctx.logs,
    "resolution complete, {} packages resolved",
    SP_FMT_U32(sp_str_ht_size(resolver->resolved)));

  return SPN_TASK_DONE;
}
