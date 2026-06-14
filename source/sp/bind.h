#ifndef SPN_BIND_H
#define SPN_BIND_H

#include "sp.h"

// ============================================================================
// Types
// ============================================================================

typedef enum {
  SP_BIND_NONE = 0,
  SP_BIND_STR,
  SP_BIND_BOOL,
  SP_BIND_S32,
  SP_BIND_S64,
  SP_BIND_U32,
  SP_BIND_U64,
  SP_BIND_F64,
  SP_BIND_OBJECT,
  SP_BIND_ARRAY,
  SP_BIND_MAP,
} sp_bind_kind_t;

typedef struct sp_bind_field_t sp_bind_field_t;

struct sp_bind_field_t {
  const c8* key;
  sp_bind_kind_t kind;
  u32 offset;
  void* user_data;
  union {
    struct {
      sp_da(sp_bind_field_t) fields;
    } object;
    struct {
      sp_bind_field_t* element;
      u32 element_size;
    } array;
    struct {
      sp_bind_field_t* value;
      u32 entry_size;
      u32 key_offset;
    } map;
  } as;
};

typedef sp_bind_field_t sp_bind_t;

// ============================================================================
// Builder
// ============================================================================

#define SP_BIND_NO_OFFSET SP_LIMIT_U32_MAX
#define SP_BIND_STACK_MAX 32

typedef struct {
  sp_bind_t* root;
  sp_bind_field_t* stack[SP_BIND_STACK_MAX];
  u32 depth;
} sp_bind_builder_t;

sp_bind_builder_t  sp_bind_builder_begin();
sp_bind_t*         sp_bind_builder_end(sp_bind_builder_t* b);
void               sp_bind_builder_push_object(sp_bind_builder_t* b, const c8* key, u32 offset);
void               sp_bind_builder_push_array(sp_bind_builder_t* b, const c8* key, u32 offset, u32 element_size);
void               sp_bind_builder_push_map(sp_bind_builder_t* b, const c8* key, u32 offset, u32 entry_size, u32 key_offset);
void               sp_bind_builder_push_entry_object(sp_bind_builder_t* b);
void               sp_bind_builder_add_entry(sp_bind_builder_t* b, sp_bind_kind_t kind);
void               sp_bind_builder_add_field(sp_bind_builder_t* b, const c8* key, sp_bind_kind_t kind, u32 offset);
void               sp_bind_builder_pop(sp_bind_builder_t* b);

// ============================================================================
// Query
// ============================================================================

sp_bind_field_t*   sp_bind_find(sp_bind_t* bind, const c8* key);
u32                sp_bind_field_count(sp_bind_t* bind);
void*              sp_bind_field_ptr(sp_bind_field_t* field, void* base);
u32                sp_bind_kind_size(sp_bind_kind_t kind);
void               sp_bind_free(sp_bind_t* bind);

// ============================================================================
// Macros
// ============================================================================

#define SP_BIND_SCOPE_PUSH(b, push_expr) \
  for (bool _sp_open = ((push_expr), true); _sp_open; _sp_open = false, sp_bind_builder_pop((b)))

#define SP_BIND_SCHEMA(b) \
  SP_BIND_SCOPE_PUSH(b, sp_bind_builder_push_object((b), SP_NULLPTR, SP_BIND_NO_OFFSET))

#define SP_BIND(b, type, field, key, kind) \
  sp_bind_builder_add_field((b), (key), (kind), offsetof(type, field))

#define SP_BIND_OBJECT(b, type, field, key) \
  SP_BIND_SCOPE_PUSH(b, sp_bind_builder_push_object((b), (key), offsetof(type, field)))

#define SP_BIND_ARRAY(b, type, field, key, elem_type) \
  SP_BIND_SCOPE_PUSH(b, sp_bind_builder_push_array((b), (key), offsetof(type, field), sizeof(elem_type)))

#define SP_BIND_MAP(b, type, field, key, entry_type, key_field) \
  SP_BIND_SCOPE_PUSH(b, sp_bind_builder_push_map((b), (key), offsetof(type, field), sizeof(entry_type), offsetof(entry_type, key_field)))

#define SP_BIND_ENTRY(b, kind) \
  sp_bind_builder_add_entry((b), (kind))

#define SP_BIND_ENTRY_OBJECT(b) \
  SP_BIND_SCOPE_PUSH(b, sp_bind_builder_push_entry_object((b)))

// ============================================================================
// Implementation
// ============================================================================

sp_bind_builder_t sp_bind_builder_begin() {
  sp_bind_builder_t b = SP_ZERO_INITIALIZE();
  return b;
}

sp_bind_t* sp_bind_builder_end(sp_bind_builder_t* b) {
  SP_ASSERT(b->depth == 0);
  return b->root;
}

SP_PRIVATE sp_bind_field_t* sp_bind_builder_current(sp_bind_builder_t* b) {
  SP_ASSERT(b->depth > 0);
  return b->stack[b->depth - 1];
}

SP_PRIVATE void sp_bind_builder_push(sp_bind_builder_t* b, sp_bind_field_t* field) {
  SP_ASSERT(b->depth < SP_BIND_STACK_MAX);
  b->stack[b->depth++] = field;
}

void sp_bind_builder_push_object(sp_bind_builder_t* b, const c8* key, u32 offset) {
  sp_bind_field_t field = (sp_bind_field_t) {
    .key = key,
    .kind = SP_BIND_OBJECT,
    .offset = offset,
  };

  if (b->depth == 0) {
    sp_bind_field_t* heap = SP_ALLOC(sp_bind_field_t);
    *heap = field;
    b->root = heap;
    sp_bind_builder_push(b, heap);
  } else {
    sp_bind_field_t* parent = sp_bind_builder_current(b);
    SP_ASSERT(parent->kind == SP_BIND_OBJECT);
    sp_da_push(parent->as.object.fields, field);
    sp_bind_field_t* pushed = &parent->as.object.fields[sp_da_size(parent->as.object.fields) - 1];
    sp_bind_builder_push(b, pushed);
  }
}

void sp_bind_builder_push_array(sp_bind_builder_t* b, const c8* key, u32 offset, u32 element_size) {
  sp_bind_field_t* parent = sp_bind_builder_current(b);
  SP_ASSERT(parent->kind == SP_BIND_OBJECT);

  sp_da_push(parent->as.object.fields, ((sp_bind_field_t) {
    .key = key,
    .kind = SP_BIND_ARRAY,
    .offset = offset,
    .as.array.element_size = element_size,
  }));

  sp_bind_field_t* field = &parent->as.object.fields[sp_da_size(parent->as.object.fields) - 1];
  sp_bind_builder_push(b, field);
}

void sp_bind_builder_push_map(sp_bind_builder_t* b, const c8* key, u32 offset, u32 entry_size, u32 key_offset) {
  sp_bind_field_t* parent = sp_bind_builder_current(b);
  SP_ASSERT(parent->kind == SP_BIND_OBJECT);

  sp_da_push(parent->as.object.fields, ((sp_bind_field_t) {
    .key = key,
    .kind = SP_BIND_MAP,
    .offset = offset,
    .as.map.entry_size = entry_size,
    .as.map.key_offset = key_offset,
  }));

  sp_bind_field_t* field = &parent->as.object.fields[sp_da_size(parent->as.object.fields) - 1];
  sp_bind_builder_push(b, field);
}

void sp_bind_builder_push_entry_object(sp_bind_builder_t* b) {
  sp_bind_field_t* parent = sp_bind_builder_current(b);

  sp_bind_field_t* entry = SP_ALLOC(sp_bind_field_t);
  *entry = (sp_bind_field_t) {
    .kind = SP_BIND_OBJECT,
    .offset = SP_BIND_NO_OFFSET,
  };

  if (parent->kind == SP_BIND_ARRAY) {
    parent->as.array.element = entry;
  } else if (parent->kind == SP_BIND_MAP) {
    parent->as.map.value = entry;
  } else {
    SP_ASSERT(false);
  }

  sp_bind_builder_push(b, entry);
}

void sp_bind_builder_add_entry(sp_bind_builder_t* b, sp_bind_kind_t kind) {
  sp_bind_field_t* parent = sp_bind_builder_current(b);

  sp_bind_field_t* entry = SP_ALLOC(sp_bind_field_t);
  *entry = (sp_bind_field_t) {
    .kind = kind,
    .offset = SP_BIND_NO_OFFSET,
  };

  if (parent->kind == SP_BIND_ARRAY) {
    parent->as.array.element = entry;
  } else if (parent->kind == SP_BIND_MAP) {
    parent->as.map.value = entry;
  } else {
    SP_ASSERT(false);
  }
}

void sp_bind_builder_add_field(sp_bind_builder_t* b, const c8* key, sp_bind_kind_t kind, u32 offset) {
  sp_bind_field_t* parent = sp_bind_builder_current(b);
  SP_ASSERT(parent->kind == SP_BIND_OBJECT);

  sp_da_push(parent->as.object.fields, ((sp_bind_field_t) {
    .key = key,
    .kind = kind,
    .offset = offset,
  }));
}

void sp_bind_builder_pop(sp_bind_builder_t* b) {
  SP_ASSERT(b->depth > 0);
  b->depth--;
}

// ============================================================================
// Query implementation
// ============================================================================

sp_bind_field_t* sp_bind_find(sp_bind_t* bind, const c8* key) {
  SP_ASSERT(bind);
  SP_ASSERT(bind->kind == SP_BIND_OBJECT);

  sp_da_for(bind->as.object.fields, i) {
    sp_bind_field_t* f = &bind->as.object.fields[i];
    if (f->key && sp_cstr_equal(f->key, key)) {
      return f;
    }
  }
  return SP_NULLPTR;
}

u32 sp_bind_field_count(sp_bind_t* bind) {
  SP_ASSERT(bind);
  SP_ASSERT(bind->kind == SP_BIND_OBJECT);
  return sp_da_size(bind->as.object.fields);
}

void* sp_bind_field_ptr(sp_bind_field_t* field, void* base) {
  SP_ASSERT(field);
  SP_ASSERT(base);
  SP_ASSERT(field->offset != SP_BIND_NO_OFFSET);
  return (u8*)base + field->offset;
}

u32 sp_bind_kind_size(sp_bind_kind_t kind) {
  switch (kind) {
    case SP_BIND_STR:    { return sizeof(sp_str_t); }
    case SP_BIND_BOOL:   { return sizeof(bool); }
    case SP_BIND_S32:    { return sizeof(s32); }
    case SP_BIND_S64:    { return sizeof(s64); }
    case SP_BIND_U32:    { return sizeof(u32); }
    case SP_BIND_U64:    { return sizeof(u64); }
    case SP_BIND_F64:    { return sizeof(f64); }
    case SP_BIND_NONE:
    case SP_BIND_OBJECT:
    case SP_BIND_ARRAY:
    case SP_BIND_MAP:    { SP_ASSERT(false); return 0; }
  }
  SP_ASSERT(false);
  return 0;
}

SP_PRIVATE void sp_bind_free_field(sp_bind_field_t* field);

SP_PRIVATE void sp_bind_free_fields(sp_da(sp_bind_field_t) fields) {
  sp_da_for(fields, i) {
    sp_bind_free_field(&fields[i]);
  }
  sp_da_free(fields);
}

SP_PRIVATE void sp_bind_free_field(sp_bind_field_t* field) {
  switch (field->kind) {
    case SP_BIND_OBJECT: {
      sp_bind_free_fields(field->as.object.fields);
      break;
    }
    case SP_BIND_ARRAY: {
      if (field->as.array.element) {
        sp_bind_free_field(field->as.array.element);
        spn_free(field->as.array.element);
      }
      break;
    }
    case SP_BIND_MAP: {
      if (field->as.map.value) {
        sp_bind_free_field(field->as.map.value);
        spn_free(field->as.map.value);
      }
      break;
    }
    case SP_BIND_NONE:
    case SP_BIND_STR:
    case SP_BIND_BOOL:
    case SP_BIND_S32:
    case SP_BIND_S64:
    case SP_BIND_U32:
    case SP_BIND_U64:
    case SP_BIND_F64: {
      break;
    }
  }
}

void sp_bind_free(sp_bind_t* bind) {
  SP_ASSERT(bind);
  sp_bind_free_field(bind);
  spn_free(bind);
}

#endif // SPN_BIND_H
