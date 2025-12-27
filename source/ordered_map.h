#ifndef SP_OM_H
#define SP_OM_H

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
  sp_om_new_ex(om, sp_mem_arena_new_ex(4096, SP_MEM_ARENA_MODE_NO_REALLOC), sp_mem_arena_new(4096))


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
#define sp_om_for(om, it)         for (u32 it = 0; it < sp_om_size(om); it++)

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


// id get_or_insert();
// str get_or_insert();
// id get();
// str get();
void           sp_intern_init(sp_intern_t* intern);
sp_intern_id_t sp_intern_get_or_insert(sp_intern_t* intern, sp_str_t str);
sp_str_t       sp_intern_get_or_insert_str(sp_intern_t* intern, sp_str_t str);
sp_intern_id_t sp_intern_get(sp_intern_t* intern, sp_str_t str);
sp_str_t       sp_intern_get_str(sp_intern_t* intern, sp_str_t str);
sp_str_t       sp_intern_resolve_id(sp_intern_t* intern, sp_intern_id_t id);
u64            sp_intern_size(sp_intern_t* intern);
u64            sp_intern_bytes_used(sp_intern_t* intern);
u64            sp_intern_bytes_allocated(sp_intern_t* intern);
//u64 sp_intern_size(sp_intern_t* intern);

#ifdef SP_OM_IMPLEMENTATION

void sp_intern_init(sp_intern_t* intern) {
  sp_require(intern);
  intern->arenas.data = sp_mem_arena_new_ex(512, SP_MEM_ARENA_MODE_NO_REALLOC);
  intern->arenas.metadata = sp_mem_arena_new(512);

  sp_mem_arena_alloc(intern->arenas.data, 1);
  sp_da_push(intern->order, (sp_str_lit("")));
}

sp_intern_id_t sp_intern_get(sp_intern_t* intern, sp_str_t str) {
  sp_intern_id_t* id = sp_str_ht_get(intern->index, str);
  if (!id) return SP_INTERN_INVALID_ID;

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
    if (id) return id;
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

#endif
#endif
