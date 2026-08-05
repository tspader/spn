#include "ctx/ctx.h"
#include "intern/intern.h"

sp_intern_t* spn_ctx_get_intern() {
  return spn.intern;
}

void spn_ctx_cancel(spn_ctx_t* ctx) {
  sp_atomic_s32_set(&ctx->aborted, 1);
}

bool spn_ctx_cancelled(spn_ctx_t* ctx) {
  return sp_atomic_s32_get(&ctx->aborted) != 0;
}

spn_index_info_t* spn_find_index(sp_str_t name) {
  sp_da_for(spn.indexes, it) {
    if (sp_str_equal(spn.indexes[it].name, name)) {
      return &spn.indexes[it];
    }
  }
  return SP_NULLPTR;
}

sp_str_t spn_intern(sp_str_t str) {
  return sp_intern_get_or_insert_str(spn_ctx_get_intern(), str);
}

sp_str_t spn_intern_cstr(const c8* cstr) {
  return sp_intern_get_or_insert_str(spn_ctx_get_intern(), sp_cstr_as_str(cstr));
}

sp_str_t spn_intern_str(sp_intern_id_t id) {
  return sp_intern_str_from_id(spn_ctx_get_intern(), id);
}

bool spn_intern_is_equal(sp_str_t a, sp_str_t b) {
  return sp_intern_is_equal_str(spn_ctx_get_intern(), a, b);
}

bool spn_intern_is_equal_cstr(sp_str_t str, const c8* cstr) {
  return sp_intern_is_equal_str(spn_ctx_get_intern(), str, sp_cstr_as_str(cstr));
}

