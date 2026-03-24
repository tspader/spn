#include "ctx/ctx.h"
#include "event/event.h"
#include "target/types.h"

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

void spn_ctx_push_target_source_event(spn_target_t* target, sp_str_t source) {
  spn_event_buffer_push_ex(spn.events, target->pkg, SP_NULLPTR, (spn_build_event_t) {
    .kind = SPN_EVENT_ADD_SOURCE,
    .target = {
      .name = target->name,
      .source = { .source = source },
    }
  });
}
