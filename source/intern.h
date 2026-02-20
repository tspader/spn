#ifndef SPN_INTERN_H
#define SPN_INTERN_H

#include "sp.h"

#define SP_INTERN_INVALID_ID 0
#define SP_INTERN_INVALID_STR SP_ZERO_STRUCT(sp_str_t)
typedef u32 sp_intern_id_t;

typedef struct {
  struct {
    sp_mem_arena_t* data;
    sp_mem_arena_t* metadata;
  } arenas;
  sp_da(sp_str_t) order;
  sp_str_ht(sp_intern_id_t) index;
} sp_intern_t;

sp_intern_t*   sp_intern_new();
void           sp_intern_init(sp_intern_t* intern);
sp_intern_id_t sp_intern_get_or_insert(sp_intern_t* intern, sp_str_t str);
sp_str_t       sp_intern_get_or_insert_str(sp_intern_t* intern, sp_str_t str);
sp_intern_id_t sp_intern_get(sp_intern_t* intern, sp_str_t str);
sp_str_t       sp_intern_get_str(sp_intern_t* intern, sp_str_t str);
sp_str_t       sp_intern_resolve_id(sp_intern_t* intern, sp_intern_id_t id);
bool           sp_intern_is_equal(sp_intern_t* intern, sp_intern_id_t a, sp_intern_id_t b);
bool           sp_intern_is_equal_str(sp_intern_t* intern, sp_str_t a, sp_str_t b);
u64            sp_intern_size(sp_intern_t* intern);
u64            sp_intern_bytes_used(sp_intern_t* intern);
u64            sp_intern_bytes_allocated(sp_intern_t* intern);
sp_str_t       spn_intern(sp_str_t str);
sp_str_t       spn_intern_cstr(const c8* cstr);
bool           spn_intern_is_equal(sp_str_t a, sp_str_t b);
bool           spn_intern_is_equal_cstr(sp_str_t str, const c8* cstr);

#endif
