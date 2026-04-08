#ifndef SPN_INTERN_TYPES_H
#define SPN_INTERN_TYPES_H

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

#endif
