#include "ordered_map.h"
#include "intern/intern.h"
#include "ctx/ctx.h"

sp_intern_t* sp_intern_new() {
  sp_intern_t* intern = sp_alloc_type(sp_intern_t);
  sp_intern_init(intern);
  return intern;
}

void sp_intern_init(sp_intern_t* intern) {
  sp_require(intern);
  intern->arenas.data = sp_mem_arena_new_ex(512, SP_MEM_ARENA_MODE_NO_REALLOC, 0);
  intern->arenas.metadata = sp_mem_arena_new(512);

  sp_mem_arena_alloc(intern->arenas.data, 1);
  sp_da_push(intern->order, (sp_str_lit("")));
}

sp_intern_id_t sp_intern_get(sp_intern_t* intern, sp_str_t str) {
  sp_intern_id_t* id = sp_str_ht_get(intern->index, str);
  if (!id) {
    return SP_INTERN_INVALID_ID;
  }

  return *id;
}

sp_str_t sp_intern_get_str(sp_intern_t* intern, sp_str_t str) {
  sp_intern_id_t id = sp_intern_get(intern, str);
  return sp_intern_resolve_id(intern, id);
}

sp_str_t sp_intern_resolve_id(sp_intern_t* intern, sp_intern_id_t id) {
  sp_require_as(id < sp_da_size(intern->order), SP_INTERN_INVALID_STR);
  return intern->order[id];
}

sp_intern_id_t sp_intern_get_or_insert(sp_intern_t* intern, sp_str_t str) {
  sp_require_as(str.len, SP_INTERN_INVALID_ID);
  sp_require_as(intern, SP_INTERN_INVALID_ID);

  {
    sp_intern_id_t id = sp_intern_get(intern, str);
    if (id) {
      return id;
    }
  }

  sp_context_push_arena(intern->arenas.data);
  const c8* cstr = sp_str_to_cstr(str);
  sp_context_pop();

  sp_str_t key = sp_str(cstr, str.len);
  sp_intern_id_t id = sp_da_size(intern->order);

  sp_context_push_arena(intern->arenas.metadata);
  sp_da_push(intern->order, key);
  sp_str_ht_insert(intern->index, key, id);
  sp_context_pop();

  return id;
}

sp_str_t sp_intern_get_or_insert_str(sp_intern_t* intern, sp_str_t str) {
  sp_intern_id_t id = sp_intern_get_or_insert(intern, str);
  return sp_intern_resolve_id(intern, id);
}

bool sp_intern_is_equal(sp_intern_t* intern, sp_intern_id_t a, sp_intern_id_t b) {
  return a == b;
}

bool sp_intern_is_equal_str(sp_intern_t* intern, sp_str_t a, sp_str_t b) {
  sp_intern_id_t ia = sp_intern_get(intern, a);
  if (!ia) {
    return false;
  }

  sp_intern_id_t ib = sp_intern_get(intern, b);
  if (!ib) {
    return false;
  }

  return ia == ib;
}

u64 sp_intern_size(sp_intern_t* intern) {
  sp_require_as(intern, 0);
  return sp_da_size(intern->order);
}

u64 sp_intern_bytes_used(sp_intern_t* intern) {
  sp_require_as(intern, 0);
  return sp_mem_arena_bytes_used(intern->arenas.data);
}

u64 sp_intern_bytes_allocated(sp_intern_t* intern) {
  sp_require_as(intern, 0);
  return sp_mem_arena_capacity(intern->arenas.data);
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
  sp_intern_id_t is = sp_intern_get_or_insert(spn_ctx_get_intern(), str);
  sp_intern_id_t ic = sp_intern_get_or_insert(spn_ctx_get_intern(), sp_str_view(cstr));
  return is == ic;
}
