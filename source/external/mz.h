#ifndef MZ_H
#define MZ_H

#include <stddef.h>

#include "sp.h"

typedef struct mz_schema_t mz_schema_t;
typedef struct mz_ctx_t mz_ctx_t;
typedef struct mz_json_cursor_t mz_json_cursor_t;

typedef enum {
  MZ_TAG_KIND_STR = 0,
  MZ_TAG_KIND_S32,
  MZ_TAG_KIND_U64,
} mz_tag_kind_t;

typedef struct {
  mz_tag_kind_t kind;
  union {
    const c8* str;
    s32 s32;
    u64 u64;
  } as;
} mz_tag_value_t;

typedef enum {
  MZ_OK = 0,
  MZ_ERR_JSON,
  MZ_ERR_TYPE,
  MZ_ERR_MISSING_KEY,
  MZ_ERR_UNKNOWN_KEY,
  MZ_ERR_RANGE,
  MZ_ERR_LIMIT,
} mz_err_t;

typedef enum {
  MZ_BACKEND_KIND_JANSSON = 0,
  MZ_BACKEND_KIND_CJSON,
  MZ_BACKEND_KIND_CUSTOM,
} mz_backend_t;

typedef enum {
  MZ_OBJECT_STRICT,
  MZ_OBJECT_LOOSE,
} mz_object_mode_t;

typedef struct {
  mz_err_t kind;
  const c8* path;
  const c8* message;
} mz_diag_t;

typedef struct {
  bool is_index;
  union {
    const c8* key;
    u32 index;
  } as;
} mz_diag_part_t;

typedef struct mz_ctx_t {
  sp_allocator_t allocator;
  sp_mem_arena_t* arena;
  u32 arena_block_size;
  mz_diag_t diag;
  mz_diag_part_t diag_parts[64];
  u32 diag_parts_len;
  c8 diag_path[512];
  u32 max_input_bytes;
} mz_ctx_t;

typedef mz_err_t(*mz_map_insert_fn_t)(mz_ctx_t* ctx, void* out, const c8* key, void** value_out);
typedef mz_err_t(*mz_field_ptr_fn_t)(mz_ctx_t* ctx, void* parent_out, const c8* key, void** value_out);

typedef struct {
  mz_schema_t* root;
  mz_schema_t* stack[32];
  u32 depth;
} mz_builder_t;

typedef enum {
  MZ_JSON_KIND_INVALID = 0,
  MZ_JSON_KIND_NULL,
  MZ_JSON_KIND_BOOL,
  MZ_JSON_KIND_STRING,
  MZ_JSON_KIND_INTEGER,
  MZ_JSON_KIND_REAL,
  MZ_JSON_KIND_ARRAY,
  MZ_JSON_KIND_OBJECT,
} mz_json_kind_t;

#define MZ_NO_OFFSET SP_LIMIT_U32_MAX

SP_API mz_schema_t* mz_schema_object(mz_object_mode_t mode);
SP_API mz_schema_t* mz_schema_string();
SP_API mz_schema_t* mz_schema_any();
SP_API mz_schema_t* mz_schema_bool();
SP_API mz_schema_t* mz_schema_s32();
SP_API mz_schema_t* mz_schema_s32_ex(s32 min, s32 max);
SP_API mz_schema_t* mz_schema_u64();
SP_API mz_schema_t* mz_schema_u64_ex(u64 min, u64 max);
SP_API mz_schema_t* mz_schema_f64();
SP_API mz_schema_t* mz_schema_f64_ex(f64 min, f64 max);
SP_API mz_schema_t* mz_schema_array(mz_schema_t* element);
SP_API mz_schema_t* mz_schema_array_ex(mz_schema_t* element, u32 min_len, u32 max_len);
SP_API mz_schema_t* mz_schema_map(mz_schema_t* value, mz_map_insert_fn_t on_insert);
SP_API mz_schema_t* mz_schema_tagged_union(const c8* key, mz_tag_kind_t kind);

SP_API mz_tag_value_t mz_tag_str(const c8* str);
SP_API mz_tag_value_t mz_tag_s32(s32 s32);
SP_API mz_tag_value_t mz_tag_u64(u64 u64);
SP_API void mz_tagged_union_add(mz_schema_t* tagged, mz_tag_value_t tag, mz_schema_t* schema);

SP_API void mz_object_add_field(mz_schema_t* object, const c8* key, mz_schema_t* field, bool required, u32 offset);
SP_API void mz_object_add_field_ptr(mz_schema_t* object, const c8* key, mz_schema_t* field, bool required, u32 offset, u32 ptr_size, mz_field_ptr_fn_t on_resolve);
SP_API void mz_schema_free(mz_schema_t* schema);

SP_API mz_builder_t mz_builder_begin();
SP_API mz_schema_t* mz_builder_end(mz_builder_t* builder);
SP_API void mz_builder_push_object(mz_builder_t* builder, const c8* key, mz_object_mode_t mode, bool required, u32 offset);
SP_API void mz_builder_push_object_ptr(mz_builder_t* builder, const c8* key, mz_object_mode_t mode, bool required, u32 offset, u32 ptr_size, mz_field_ptr_fn_t on_resolve);
SP_API void mz_builder_push_array_object(mz_builder_t* builder, const c8* key, mz_object_mode_t element_mode, bool required, u32 offset, u32 elem_size);
SP_API void mz_builder_push_map_object(mz_builder_t* builder, const c8* key, mz_map_insert_fn_t on_insert, mz_object_mode_t value_mode, bool required, u32 offset);
SP_API void mz_builder_add_field(mz_builder_t* builder, const c8* key, mz_schema_t* field, bool required, u32 offset);
SP_API void mz_builder_add_field_ptr(mz_builder_t* builder, const c8* key, mz_schema_t* field, bool required, u32 offset, u32 ptr_size, mz_field_ptr_fn_t on_resolve);
SP_API void mz_builder_pop(mz_builder_t* builder);

SP_API void mz_parse_ctx_init(mz_ctx_t* ctx);
SP_API void mz_parse_ctx_init_ex(mz_ctx_t* ctx, sp_allocator_t allocator, u32 arena_block_size);
SP_API void mz_parse_ctx_clear(mz_ctx_t* ctx);
SP_API void mz_parse_ctx_destroy(mz_ctx_t* ctx);

SP_API mz_backend_t mz_query_backend(void);

SP_API mz_err_t mz_validate(mz_schema_t* schema, const mz_json_cursor_t* cursor);
SP_API mz_err_t mz_validate_ex(mz_schema_t* schema, const mz_json_cursor_t* cursor, mz_ctx_t* ctx);
SP_API mz_err_t mz_validate_str(mz_schema_t* schema, const c8* json_source);
SP_API mz_err_t mz_validate_str_ex(mz_schema_t* schema, const c8* json_source, mz_ctx_t* ctx);
SP_API mz_err_t mz_validate_file(mz_schema_t* schema, const c8* file_path);

SP_API mz_err_t mz_parse(mz_schema_t* schema, const mz_json_cursor_t* cursor, void* out);
SP_API mz_err_t mz_parse_ex(mz_schema_t* schema, const mz_json_cursor_t* cursor, void* out, mz_ctx_t* ctx);
SP_API mz_err_t mz_parse_str(mz_schema_t* schema, const c8* json_source, void* out);
SP_API mz_err_t mz_parse_str_ex(mz_schema_t* schema, const c8* json_source, void* out, mz_ctx_t* ctx);
SP_API mz_err_t mz_parse_file(mz_schema_t* schema, const c8* file_path, void* out);

#define MZ_SCHEMA(BUILDER_PTR, MODE) \
  for (bool _mz_open = (mz_builder_push_object((BUILDER_PTR), SP_NULLPTR, (MODE), true, MZ_NO_OFFSET), true); _mz_open; _mz_open = false, mz_builder_pop((BUILDER_PTR)))

#define MZ_OBJECT(BUILDER_PTR, KEY, MODE) \
  for (bool _mz_open = (mz_builder_push_object((BUILDER_PTR), (KEY), (MODE), true, MZ_NO_OFFSET), true); _mz_open; _mz_open = false, mz_builder_pop((BUILDER_PTR)))

#define MZ_OBJECT_BIND(BUILDER_PTR, TYPE, FIELD, KEY, MODE) \
  for (bool _mz_open = (mz_builder_push_object((BUILDER_PTR), (KEY), (MODE), true, offsetof(TYPE, FIELD)), true); _mz_open; _mz_open = false, mz_builder_pop((BUILDER_PTR)))

#define MZ_OBJECT_PTR_BIND(BUILDER_PTR, TYPE, FIELD, KEY, MODE) \
  for (bool _mz_open = (mz_builder_push_object_ptr((BUILDER_PTR), (KEY), (MODE), true, offsetof(TYPE, FIELD), sizeof(*(((TYPE*)0)->FIELD)), SP_NULLPTR), true); _mz_open; _mz_open = false, mz_builder_pop((BUILDER_PTR)))

#define MZ_OBJECT_PTR_BIND_ALLOC(BUILDER_PTR, TYPE, FIELD, KEY, MODE, ON_RESOLVE) \
  for (bool _mz_open = (mz_builder_push_object_ptr((BUILDER_PTR), (KEY), (MODE), true, offsetof(TYPE, FIELD), sizeof(*(((TYPE*)0)->FIELD)), (ON_RESOLVE)), true); _mz_open; _mz_open = false, mz_builder_pop((BUILDER_PTR)))

#define MZ_ARRAY(BUILDER_PTR, KEY, ELEM_TYPE, ELEMENT_MODE) \
  for (bool _mz_open = (mz_builder_push_array_object((BUILDER_PTR), (KEY), (ELEMENT_MODE), true, MZ_NO_OFFSET, sizeof(ELEM_TYPE)), true); _mz_open; _mz_open = false, mz_builder_pop((BUILDER_PTR)))

#define MZ_ARRAY_BIND(BUILDER_PTR, TYPE, FIELD, KEY, ELEMENT_MODE) \
  for (bool _mz_open = (mz_builder_push_array_object((BUILDER_PTR), (KEY), (ELEMENT_MODE), true, offsetof(TYPE, FIELD), sizeof(*(((TYPE*)0)->FIELD))), true); _mz_open; _mz_open = false, mz_builder_pop((BUILDER_PTR)))

#define MZ_ARRAY_OPT(BUILDER_PTR, KEY, ELEM_TYPE, ELEMENT_MODE) \
  for (bool _mz_open = (mz_builder_push_array_object((BUILDER_PTR), (KEY), (ELEMENT_MODE), false, MZ_NO_OFFSET, sizeof(ELEM_TYPE)), true); _mz_open; _mz_open = false, mz_builder_pop((BUILDER_PTR)))

#define MZ_ARRAY_OPT_BIND(BUILDER_PTR, TYPE, FIELD, KEY, ELEMENT_MODE) \
  for (bool _mz_open = (mz_builder_push_array_object((BUILDER_PTR), (KEY), (ELEMENT_MODE), false, offsetof(TYPE, FIELD), sizeof(*(((TYPE*)0)->FIELD))), true); _mz_open; _mz_open = false, mz_builder_pop((BUILDER_PTR)))

#define MZ_MAP(BUILDER_PTR, KEY, ON_INSERT, VALUE_MODE) \
  for (bool _mz_open = (mz_builder_push_map_object((BUILDER_PTR), (KEY), (ON_INSERT), (VALUE_MODE), true, MZ_NO_OFFSET), true); _mz_open; _mz_open = false, mz_builder_pop((BUILDER_PTR)))

#define MZ_MAP_BIND(BUILDER_PTR, TYPE, FIELD, KEY, ON_INSERT, VALUE_MODE) \
  for (bool _mz_open = (mz_builder_push_map_object((BUILDER_PTR), (KEY), (ON_INSERT), (VALUE_MODE), true, offsetof(TYPE, FIELD)), true); _mz_open; _mz_open = false, mz_builder_pop((BUILDER_PTR)))

#define MZ_MAP_OPT(BUILDER_PTR, KEY, ON_INSERT, VALUE_MODE) \
  for (bool _mz_open = (mz_builder_push_map_object((BUILDER_PTR), (KEY), (ON_INSERT), (VALUE_MODE), false, MZ_NO_OFFSET), true); _mz_open; _mz_open = false, mz_builder_pop((BUILDER_PTR)))

#define MZ_MAP_OPT_BIND(BUILDER_PTR, TYPE, FIELD, KEY, ON_INSERT, VALUE_MODE) \
  for (bool _mz_open = (mz_builder_push_map_object((BUILDER_PTR), (KEY), (ON_INSERT), (VALUE_MODE), false, offsetof(TYPE, FIELD)), true); _mz_open; _mz_open = false, mz_builder_pop((BUILDER_PTR)))

#define MZ(BUILDER_PTR, KEY, SCHEMA) \
  mz_builder_add_field((BUILDER_PTR), (KEY), (SCHEMA), true, MZ_NO_OFFSET)

#define MZ_OPT(BUILDER_PTR, KEY, SCHEMA) \
  mz_builder_add_field((BUILDER_PTR), (KEY), (SCHEMA), false, MZ_NO_OFFSET)

#define MZ_BIND(BUILDER_PTR, TYPE, FIELD, KEY, SCHEMA) \
  mz_builder_add_field((BUILDER_PTR), (KEY), (SCHEMA), true, offsetof(TYPE, FIELD))

#define MZ_OPT_BIND(BUILDER_PTR, TYPE, FIELD, KEY, SCHEMA) \
  mz_builder_add_field((BUILDER_PTR), (KEY), (SCHEMA), false, offsetof(TYPE, FIELD))

#define MZ_PTR(BUILDER_PTR, KEY, PTR_TYPE, SCHEMA) \
  mz_builder_add_field_ptr((BUILDER_PTR), (KEY), (SCHEMA), true, MZ_NO_OFFSET, sizeof(PTR_TYPE), SP_NULLPTR)

#define MZ_PTR_BIND(BUILDER_PTR, TYPE, FIELD, KEY, SCHEMA) \
  mz_builder_add_field_ptr((BUILDER_PTR), (KEY), (SCHEMA), true, offsetof(TYPE, FIELD), sizeof(*(((TYPE*)0)->FIELD)), SP_NULLPTR)

#define MZ_PTR_OPT(BUILDER_PTR, KEY, PTR_TYPE, SCHEMA) \
  mz_builder_add_field_ptr((BUILDER_PTR), (KEY), (SCHEMA), false, MZ_NO_OFFSET, sizeof(PTR_TYPE), SP_NULLPTR)

#define MZ_PTR_OPT_BIND(BUILDER_PTR, TYPE, FIELD, KEY, SCHEMA) \
  mz_builder_add_field_ptr((BUILDER_PTR), (KEY), (SCHEMA), false, offsetof(TYPE, FIELD), sizeof(*(((TYPE*)0)->FIELD)), SP_NULLPTR)

#define MZ_PTR_BIND_ALLOC(BUILDER_PTR, TYPE, FIELD, KEY, SCHEMA, ON_RESOLVE) \
  mz_builder_add_field_ptr((BUILDER_PTR), (KEY), (SCHEMA), true, offsetof(TYPE, FIELD), sizeof(*(((TYPE*)0)->FIELD)), (ON_RESOLVE))

#endif
