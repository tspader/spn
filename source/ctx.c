#include "ctx.h"

#include "app.h"
#include "unit.h"

sp_str_t spn_app_project_dir(void) {
  return app.paths.dir;
}

sp_str_t spn_pkg_unit_get_include_dir(spn_pkg_unit_t* unit) {
  return spn_build_ctx_get_include_dir(&unit->ctx);
}

sp_intern_t* spn_ctx_get_intern(void) {
  return spn.intern;
}

spn_log_level_t spn_ctx_get_log_level(void) {
  return spn.log_level;
}

sp_io_writer_t* spn_ctx_get_log_out(void) {
  return &spn.logger.out;
}

sp_io_writer_t* spn_ctx_get_log_err(void) {
  return &spn.logger.err;
}

sp_str_t spn_ctx_source_cache_root(void) {
  return spn.paths.source;
}

sp_str_t spn_ctx_build_cache_root(void) {
  return spn.paths.build;
}

sp_str_t spn_ctx_store_cache_root(void) {
  return spn.paths.store;
}

sp_str_t spn_ctx_project_root(void) {
  return spn.paths.project;
}

sp_str_t spn_ctx_build_source_dir(spn_build_ctx_t* build) {
  return build->paths.source;
}

sp_str_t spn_ctx_build_work_dir(spn_build_ctx_t* build) {
  return build->paths.work;
}

sp_str_t spn_ctx_build_store_dir(spn_build_ctx_t* build) {
  return build->paths.store;
}

sp_str_t spn_ctx_build_include_dir(spn_build_ctx_t* build) {
  return build->paths.include;
}

sp_str_t spn_ctx_build_lib_dir(spn_build_ctx_t* build) {
  return build->paths.lib;
}

spn_linkage_t spn_ctx_build_linkage(spn_build_ctx_t* build) {
  return build->linkage;
}

spn_build_mode_t spn_ctx_build_mode(spn_build_ctx_t* build) {
  return build->profile->mode;
}

sp_ps_output_t spn_ctx_build_subprocess(spn_build_ctx_t* build, sp_ps_config_t cfg) {
  return spn_build_ctx_subprocess(build, cfg);
}

sp_da(sp_str_t) spn_ctx_build_lib_entries(spn_build_ctx_t* build) {
  sp_da(sp_str_t) entries = SP_NULLPTR;
  sp_om_for(build->pkg->libs, it) {
    spn_target_t* lib = sp_om_at(build->pkg->libs, it);
    sp_da_push(entries, spn_build_ctx_get_lib_path(build, lib));
  }
  return entries;
}

sp_da(spn_build_ctx_t*) spn_ctx_all_build_contexts(void) {
  sp_da(spn_build_ctx_t*) builds = SP_NULLPTR;
  sp_om_for(app.session.units.packages, it) {
    spn_pkg_unit_t* unit = sp_om_at(app.session.units.packages, it);
    sp_da_push(builds, &unit->ctx);
  }
  return builds;
}

spn_pkg_t* spn_ctx_ensure_package(spn_pkg_req_t req) {
  return spn_app_ensure_package(&app, req);
}

spn_pkg_t* spn_ctx_root_package(void) {
  return &app.package;
}

spn_resolver_t* spn_ctx_resolver(void) {
  return app.resolver;
}

void spn_ctx_push_target_source_event(spn_target_t* target, sp_str_t source) {
  spn_event_buffer_push_ex(spn.events, target->pkg, SP_NULLPTR, (spn_build_event_t) {
    .kind = SPN_EVENT_ADD_SOURCE,
    .target_source = {
      .target = target->name,
      .source = source,
    }
  });
}

void spn_push_event(spn_build_event_kind_t kind) {
  spn_push_event_ex((spn_build_event_t) {
    .kind = kind
  });
}

void spn_push_event_ex(spn_build_event_t event) {
  spn_event_buffer_push_ctx(spn.events, &spn_session_find_root(&app.session)->ctx, event);
}
