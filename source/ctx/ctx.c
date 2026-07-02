#include "ctx/ctx.h"
#include "intern/intern.h"

sp_intern_t* spn_ctx_get_intern(void) {
  return spn.intern;
}

spn_log_level_t spn_ctx_get_log_level(void) {
  return spn.logger.level;
}

sp_io_writer_t* spn_ctx_get_log_out(void) {
  return &spn.logger.out.base;
}

sp_io_writer_t* spn_ctx_get_log_err(void) {
  return &spn.logger.err.base;
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

sp_str_t spn_intern(sp_str_t str) {
  return sp_intern_get_or_insert_str(spn_ctx_get_intern(), str);
}

sp_str_t spn_intern_cstr(const c8* cstr) {
  return sp_intern_get_or_insert_str(spn_ctx_get_intern(), sp_str_view(cstr));
}

bool spn_intern_is_equal(sp_str_t a, sp_str_t b) {
  return sp_intern_is_equal_str(spn_ctx_get_intern(), a, b);
}

bool spn_intern_is_equal_cstr(sp_str_t str, const c8* cstr) {
  return sp_intern_is_equal_str(spn_ctx_get_intern(), str, sp_str_view(cstr));
}

