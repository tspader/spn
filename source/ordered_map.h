#ifndef SP_OM_H
#define SP_OM_H

#define sp_om(T)                                                                   \
  struct {                                                                         \
    sp_mem_arena_t* arena;                                                         \
    sp_da(T*) order;                                                               \
    sp_str_ht(T*) index;                                                           \
    T* temp;                                                                       \
  }*

#define sp_om_new_ex(om, a)                                                    \
  do {                                                                             \
    (om) = sp_om_alloc(&(om), sizeof(*(om)));                                      \
    (om)->arena = a;                                                           \
  } while (0)

#define sp_om_new(om)                                                              \
  sp_om_new_ex(om, sp_mem_arena_new(4096))


#define sp_om_ensure(om)                                                           \
  if (!(om)) { sp_om_new(om); }

#define sp_om_alloc_entry(om)                                                      \
  sp_om_alloc(&(om)->temp, sizeof(*(om)->temp))

#define sp_om_insert(om, key, val)                                                 \
  do {                                                                             \
    sp_om_ensure(om);                                                              \
    if (sp_str_ht_exists((om)->index, (key))) {                                    \
      break;                                                                       \
    }                                                                              \
    sp_context_push_allocator(sp_mem_arena_as_allocator((om)->arena));             \
    (om)->temp = sp_om_alloc_entry(om);                                            \
    *(om)->temp = (val);                                                           \
    sp_da_push((om)->order, (om)->temp);                                           \
    sp_str_ht_insert((om)->index, sp_str_copy(key), (om)->temp);                   \
    sp_context_pop();                                                              \
  } while (0)

#define sp_om_free(om)                                                             \
  do {                                                                             \
    if ((om)) {                                                                    \
      sp_mem_arena_destroy((om)->arena);                                           \
      sp_free(om);                                                                 \
      (om) = SP_NULLPTR;                                                           \
    }                                                                              \
  } while (0)

#define sp_om_get(om, key)   (!(om) ? SP_NULLPTR : *sp_str_ht_get((om)->index, (key)))
#define sp_om_getp(om, key)  (!(om) ? SP_NULLPTR : sp_str_ht_get((om)->index, (key)))
#define sp_om_has(om, key)   ((om) && sp_str_ht_exists((om)->index, (key)))
#define sp_om_at(om, n)      ((om)->order[(n)])
#define sp_om_size(om)       (!(om) ? 0 : sp_da_size((om)->order))
#define sp_om_for(om, it)    for (u32 it = 0; it < sp_om_size(om); it++)

#if defined(SP_CPP)
SP_END_EXTERN_C()
template<typename T> static T* sp_om_alloc(T** dummy, size_t size) {
  (void)dummy;
  return (T*)sp_alloc(size);
}
SP_BEGIN_EXTERN_C()
#else
#define sp_om_alloc(dummy, size) sp_alloc(size)
#endif


#endif
