#ifndef SPN_REFLECT_TYPES_H
#define SPN_REFLECT_TYPES_H

#include "sp.h"

#define spn_reflect_offset(type, field) ((u32)__builtin_offsetof(type, field))

typedef enum {
  SPN_REFLECT_STR,
  SPN_REFLECT_OBJECT,
  SPN_REFLECT_STR_ARRAY,
  SPN_REFLECT_OBJECT_ARRAY,
} spn_reflect_kind_t;

typedef struct spn_reflect_type spn_reflect_type_t;

typedef struct {
  const c8* key;
  u32 offset;
  spn_reflect_kind_t kind;
  const spn_reflect_type_t* object;
} spn_reflect_field_t;

struct spn_reflect_type {
  u32 size;
  const spn_reflect_field_t* fields;
  u32 num_fields;
};

#endif
