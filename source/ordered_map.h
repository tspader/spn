#ifndef SP_OM_H
#define SP_OM_H

#include "sp.h"

#define sp_om(T)                                                                   \
  struct {                                                                         \
    struct { sp_mem_arena_t* data; sp_mem_arena_t* metadata; } arenas; \
    sp_da(T*) order;                                                               \
    sp_str_ht(T*) index;                                                           \
    T* temp;                                                                       \
  }*

#define sp_om_new_ex(om, darena, marena)                                                    \
  do {                                                                             \
    (om) = sp_alloc_hint(&(om), sizeof(*(om)));                                      \
    (om)->arenas.data = darena;                                                           \
    (om)->arenas.metadata = marena;                                                           \
  } while (0)

#define sp_om_new(om)                                                              \
  sp_om_new_ex(om, sp_mem_arena_new_ex(4096, SP_MEM_ARENA_MODE_NO_REALLOC, 0), sp_mem_arena_new(4096))


#define sp_om_ensure(om)                                                           \
  if (!(om)) { sp_om_new(om); }

#define sp_om_alloc_entry(om)                                                      \
  sp_alloc_hint(&(om)->temp, sizeof(*(om)->temp))

#define sp_om_insert(om, key, val)                                                 \
  do {                                                                             \
    sp_om_ensure(om);                                                              \
    if (sp_str_ht_exists((om)->index, (key))) {                                    \
      break;                                                                       \
    }                                                                              \
    sp_context_push_allocator(sp_mem_arena_as_allocator((om)->arenas.data));             \
    (om)->temp = sp_om_alloc_entry(om);                                            \
    sp_context_pop();                                                              \
    sp_context_push_allocator(sp_mem_arena_as_allocator((om)->arenas.metadata));   \
    *(om)->temp = (val);                                                           \
    sp_da_push((om)->order, (om)->temp);                                           \
    sp_str_ht_insert((om)->index, sp_str_copy(key), (om)->temp);                   \
    sp_context_pop();                                                              \
  } while (0)

#define sp_om_free(om)                                                             \
  do {                                                                             \
    if ((om)) {                                                                    \
      sp_mem_arena_destroy((om)->arenas.data);                                     \
      sp_mem_arena_destroy((om)->arenas.metadata);                                 \
      sp_free(om);                                                                 \
      (om) = SP_NULLPTR;                                                           \
    }                                                                              \
  } while (0)

#define sp_om_get(om, key)        (!(om) ? SP_NULLPTR : *sp_str_ht_get((om)->index, (key)))
#define sp_om_getp(om, key)       (!(om) ? SP_NULLPTR : sp_str_ht_get((om)->index, (key)))
#define sp_om_has(om, key)        ((om) && sp_str_ht_exists((om)->index, (key)))
#define sp_om_at(om, n)           ((om)->order[(n)])
#define sp_om_size(om)            (!(om) ? 0 : sp_da_size((om)->order))
#define sp_om_empty(om)           (!(om) ? true : (sp_da_size((om)->order) == 0))
#define sp_om_for(om, it)         for (u32 it = 0; it < sp_om_size(om); it++)
#define sp_om_back(om)            (*sp_da_back((om)->order))

#if defined(SP_CPP)
SP_END_EXTERN_C()
template<typename T> static T* sp_alloc_hint(T** dummy, size_t size) {
  (void)dummy;
  return (T*)sp_alloc(size);
}
SP_BEGIN_EXTERN_C()
#else
#define sp_alloc_hint(dummy, size) sp_alloc(size)
#endif

#define SP_INTERN_INVALID_ID 0
#define SP_INTERN_INVALID_STR SP_ZERO_STRUCT(sp_str_t)
typedef u32 sp_intern_id_t;
//typedef sp_om(sp_str_t) sp_intern_t;

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
//u64 sp_intern_size(sp_intern_t* intern);

#endif
