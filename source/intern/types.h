#ifndef SE_INTERN_TYPES_H
#define SE_INTERN_TYPES_H

#include "sp.h"

#define SP_INTERN_INVALID_STR (SP_RVAL(sp_str_t) { 0 })
#define SP_INTERN_INVALID_ID 0

typedef u32 sp_intern_id_t;
typedef sp_str_t sp_intern_str_t;

typedef u32 (*sp_intern_hash_fn_t)(sp_str_t str);

typedef struct {
  u32 hash;
  u32 len;
  u32 id;
  const c8* data;
} sp_intern_slot_t;

typedef struct {
  sp_mem_t mem;
  sp_intern_slot_t* slots;
  u32 count;
  u32 capacity;
} sp_intern_index_t;

typedef struct {
  sp_mem_t mem;
  sp_mem_arena_t* data;
  sp_intern_index_t index;
  sp_intern_hash_fn_t hash;
  u32 next_id;
} sp_intern_t;

#endif
