#include "sp.h"
#include "sp/macro.h"
#include "app/types.h"
#include "ctx/types.h"
#include "intern/types.h"
#include "resolve/types.h"

#include "event/event.h"
#include "index/cache.h"
#include "intern/intern.h"
#include "pkg/id.h"
#include "pkg/pkg.h"
#include "resolve/resolve.h"
#include "semver/convert.h"
#include "session/session.h"
#include "task/task.h"
#include "unit/types.h"

void add_root(spn_session_t* session, spn_resolve_query_t* query) {
  spn_resolve_query_add(query, (spn_requested_dep_t) {
    .qualified = session->pkg->qualified,
    .source = SPN_PKG_SOURCE_ROOT,
  });

}

void emit_resolved(sp_mem_t mem, spn_resolve_query_t* query) {
  sp_ht_for_kv(query->result, it) {
    spn_event_buffer_push(spn.events, (spn_build_event_t) {
      .kind = SPN_EVENT_RESOLVE_PACKAGE,
      .resolve_pkg = {
        .name = spn_intern_str(it.val->id.qualified),
        .version = spn_semver_to_str(mem, it.val->id.version),
      }
    });
  }

  spn_event_buffer_push(spn.events, (spn_build_event_t) {
    .kind = SPN_EVENT_RESOLVE_END,
    .resolve_end = {
      .num_resolved = sp_ht_size(query->result),
      .time = query->time,
    }
  });

}

spn_task_step_t spn_task_resolve(spn_app_t* app) {
  spn_session_t* session = &app->session;
  spn_pkg_info_t* pkg = &app->package;

  spn_event_buffer_push(spn.events, (spn_build_event_t) {
    .kind = SPN_EVENT_RESOLVE_START,
    .pkg = pkg,
  });

  // The solver reads local packages' deps from the registry, so the root must
  // be registered before we resolve.
  sp_ht_insert(session->registry, spn_pkg_id(session->intern, session->pkg->qualified), ((spn_registry_pkg_t) {
    .source = SPN_PKG_SOURCE_ROOT,
    .info = session->pkg,
    .manifest = spn.paths.manifest,
  }));

  spn_index_cache_t index = SP_ZERO_INITIALIZE();
  spn_index_cache_init(&index, session->mem, session->intern, &spn.indexes);

  spn_resolver_t resolver = sp_zero;
  spn_resolver_init(&resolver, spn.mem, session->intern, &index, &session->registry, spn.events, session->profile, session->pkg->config, 0);
  resolver.seeds = session->gates.seeds;

  spn_resolve_query_t query = sp_zero_initialize();
  spn_resolve_query_init(session->mem, &query);
  add_root(session, &query);

  if (spn_resolve_from_solver(&resolver, &query)) return spn_task_fail(SPN_ERROR);
  session->resolve = query.result;

  emit_resolved(session->mem, &query);

  return spn_task_done();
}
