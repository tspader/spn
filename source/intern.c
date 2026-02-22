#include "intern.h"

#include "ctx.h"

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
  sp_intern_id_t is = sp_intern_get_or_insert(spn_ctx_get_intern(), str);
  sp_intern_id_t ic = sp_intern_get_or_insert(spn_ctx_get_intern(), sp_str_view(cstr));
  return is == ic;
}
