#include "app.h"
#include "resolve.h"

void add_pkg_unit(spn_session_t* session, spn_resolved_pkg_t resolved) {
  sp_om_insert(session->units.packages, resolved.pkg->name, SP_ZERO_STRUCT(spn_pkg_unit_t));
  spn_pkg_unit_t* unit = sp_om_back(session->units.packages);
  spn_init_pkg_unit_for_session(session, unit, resolved.pkg, resolved.kind, resolved.version);
}

spn_task_result_t spn_task_resolve(spn_app_t* app) {
  spn_session_t* session = &app->session;
  spn_session_init(session, &app->package, app->config.profile, sp_str_lit("build"));
  spn_session_set_filter(session, app->config.filter);

  spn_init_pkg_unit_for_session(session, &session->units.root, &app->package, SPN_PACKAGE_KIND_ROOT, app->package.version);

  spn_app_resolve(app);

  sp_str_ht_for_kv(app->resolver->resolved, it) {
    add_pkg_unit(session, *it.val);
  }

  sp_om_for(session->units.packages, it) {
    spn_pkg_t* pkg = sp_om_at(session->units.packages, it)->ctx.pkg;
    spn.tui.info.max_name = SP_MAX(spn.tui.info.max_name, pkg->name.len);
  }

  return SPN_TASK_DONE;
}
