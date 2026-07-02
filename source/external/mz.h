#ifndef MZ_H
#define MZ_H

#define MZ_BACKEND_CJSON

#include <assert.h>
#include <stddef.h>

#include "sp.h"

#ifndef mz_assert
#define mz_assert(expr) assert(expr)
#endif

#if !defined(MZ_API)
  #if defined(MZ_SHARED_LIB)
    #if defined (MZ_IMPLEMENTATION)
      #define MZ_API MZ_EXPORT
    #else
      #define MZ_API MZ_IMPORT
    #endif
  #else
    #define MZ_API extern
  #endif
#endif

SP_BEGIN_EXTERN_C()

typedef sp_str_t mz_str_t;
typedef sp_mem_buffer_t mz_buf_t;
#define mz_zero SP_ZERO_INITIALIZE()
#define mz_zero_s(s) SP_ZERO_STRUCT(s)
#define mz_null 0
#define mz_nullptr 0

typedef struct {
  const c8* data;
  u64 len;
} mz_nstr_t;




typedef struct mz_schema_t mz_schema_t;
typedef struct mz_ctx_t mz_ctx_t;
typedef struct mz_json_cursor_t mz_json_cursor_t;

/*
Define exactly one backend macro when compiling with MZ_IMPLEMENTATION:
  - MZ_BACKEND_JANSSON
  - MZ_BACKEND_CJSON
  - MZ_BACKEND_YYJSON
  - MZ_BACKEND_RAPIDJSON
  - MZ_BACKEND_GLAZE
  - MZ_BACKEND_SIMDJSON
  - MZ_BACKEND_CUSTOM

When using MZ_BACKEND_CUSTOM, provide implementations for the static backend
contract functions declared in this header.
*/

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

#define mz_try(expr) do { mz_err_t _mz_result = (expr); if (_mz_result != MZ_OK) return _mz_result; } while (0)
#define mz_try_diag(ctx, key, expr) do { \
  mz_err_t _mz_result = (expr); \
  if (_mz_result != MZ_OK) { \
    mz_diag_set((ctx), _mz_result); \
    mz_diag_push_key((ctx), (key)); \
    return _mz_result; \
  } \
} while (0)

typedef enum {
  MZ_BACKEND_KIND_JANSSON = 0,
  MZ_BACKEND_KIND_CJSON,
  MZ_BACKEND_KIND_CUSTOM,
  MZ_BACKEND_KIND_SIMDJSON,
  MZ_BACKEND_KIND_YYJSON,
  MZ_BACKEND_KIND_RAPIDJSON,
  MZ_BACKEND_KIND_GLAZE,
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

struct mz_ctx_t {
  sp_mem_t allocator;
  mz_diag_t diag;
  mz_diag_part_t diag_parts[64];
  u32 diag_parts_len;
  c8 diag_path[512];
  u32 max_input_bytes;
};

typedef union {
  const c8* str;
  u32 u32;
} mz_key_t;

typedef mz_err_t(*mz_on_alloc_fn_t)(mz_ctx_t* ctx, void* parent_out, mz_key_t key, u32 size, void** value_out);
typedef mz_err_t(*mz_on_parse_fn_t)(mz_ctx_t* ctx, void* parent_out, mz_key_t key, void* field_out, const void* parsed_out);

typedef struct {
  bool required;
  u32 offset;
  u32 ptr_size;
  mz_on_alloc_fn_t on_alloc;
  mz_on_parse_fn_t on_parse;
} mz_bind_opts_t;

typedef struct {
  mz_on_parse_fn_t on_parse;
} mz_entry_opts_t;

typedef struct {
  sp_mem_t mem;
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
#define MZ_REQUIRED true
#define MZ_OPTIONAL false
#define MZ_DEFAULT_ALLOC mz_nullptr
#define MZ_DEFAULT_PARSE mz_nullptr

SP_API mz_schema_t* mz_schema_object(sp_mem_t mem, mz_object_mode_t mode);
SP_API mz_schema_t* mz_schema_string(sp_mem_t mem);
SP_API mz_schema_t* mz_schema_any(sp_mem_t mem);
SP_API mz_schema_t* mz_schema_bool(sp_mem_t mem);
SP_API mz_schema_t* mz_schema_s32(sp_mem_t mem);
SP_API mz_schema_t* mz_schema_s32_ex(sp_mem_t mem, s32 min, s32 max);
SP_API mz_schema_t* mz_schema_u64(sp_mem_t mem);
SP_API mz_schema_t* mz_schema_u64_ex(sp_mem_t mem, u64 min, u64 max);
SP_API mz_schema_t* mz_schema_f64(sp_mem_t mem);
SP_API mz_schema_t* mz_schema_f64_ex(sp_mem_t mem, f64 min, f64 max);
SP_API mz_schema_t* mz_schema_array(sp_mem_t mem, mz_schema_t* element, mz_on_alloc_fn_t on_alloc);
SP_API mz_schema_t* mz_schema_array_ex(sp_mem_t mem, mz_schema_t* element, mz_on_alloc_fn_t on_alloc, mz_on_parse_fn_t on_parse, u32 min_len, u32 max_len);
SP_API mz_schema_t* mz_schema_map(sp_mem_t mem, mz_schema_t* value, mz_on_alloc_fn_t on_alloc);
SP_API mz_schema_t* mz_schema_map_ex(sp_mem_t mem, mz_schema_t* value, mz_on_alloc_fn_t on_alloc, mz_on_parse_fn_t on_parse);
SP_API mz_schema_t* mz_schema_tagged_union(sp_mem_t mem, const c8* key, mz_tag_kind_t kind);

SP_API mz_tag_value_t mz_tag_str(const c8* str);
SP_API mz_tag_value_t mz_tag_s32(s32 s32);
SP_API mz_tag_value_t mz_tag_u64(u64 u64);
SP_API void mz_tagged_union_add(mz_schema_t* tagged, mz_tag_value_t tag, mz_schema_t* schema);

SP_API void mz_object_add_field_ex(mz_schema_t* object, const c8* key, mz_schema_t* field, mz_bind_opts_t opts);
SP_API void mz_object_add_field_ptr_ex(mz_schema_t* object, const c8* key, mz_schema_t* field, mz_bind_opts_t opts);
SP_API void mz_schema_free(mz_schema_t* schema);

SP_API mz_builder_t mz_builder_begin(sp_mem_t mem);
SP_API mz_schema_t* mz_builder_end(mz_builder_t* builder);
SP_API void mz_builder_push_object_ex(mz_builder_t* builder, const c8* key, mz_object_mode_t mode, mz_bind_opts_t opts);
SP_API void mz_builder_push_object_ptr_ex(mz_builder_t* builder, const c8* key, mz_object_mode_t mode, mz_bind_opts_t opts);
SP_API void mz_builder_push_tagged_union_ex(mz_builder_t* builder, const c8* key, const c8* tag_key, mz_tag_kind_t kind, mz_bind_opts_t opts);
SP_API void mz_builder_push_tagged_case_ex(mz_builder_t* builder, mz_tag_value_t tag, mz_object_mode_t mode);
SP_API void mz_builder_push_array_ex(mz_builder_t* builder, const c8* key, mz_bind_opts_t opts);
SP_API void mz_builder_push_map_ex(mz_builder_t* builder, const c8* key, mz_bind_opts_t opts);
SP_API void mz_builder_add_entry_ex(mz_builder_t* builder, mz_schema_t* entry, mz_entry_opts_t opts);
SP_API void mz_builder_push_entry_object_ex(mz_builder_t* builder, mz_object_mode_t mode, mz_entry_opts_t opts);
SP_API void mz_builder_add_field_ex(mz_builder_t* builder, const c8* key, mz_schema_t* field, mz_bind_opts_t opts);
SP_API void mz_builder_add_field_ptr_ex(mz_builder_t* builder, const c8* key, mz_schema_t* field, mz_bind_opts_t opts);
SP_API void mz_builder_pop(mz_builder_t* builder);

SP_API mz_ctx_t* mz_ctx_create(sp_mem_t allocator);
SP_API void mz_ctx_init(mz_ctx_t* ctx, sp_mem_t allocator);
SP_API void mz_ctx_destroy(mz_ctx_t* ctx);
SP_API void mz_ctx_clear(mz_ctx_t* ctx);

MZ_API mz_err_t     mz_validate_str_ex(mz_schema_t* schema, const c8* json_source, mz_ctx_t* ctx);
MZ_API mz_err_t     mz_parse_str_ex(mz_schema_t* schema, const c8* json_source, void* out, mz_ctx_t* ctx);
MZ_API mz_backend_t mz_query_backend(void);

#define MZ_SCOPE_PUSH(b, push_expr) \
  for ( \
    bool _mz_open = ((push_expr), true); \
    _mz_open; \
    _mz_open = false, mz_builder_pop((b)) \
  )

#define MZ_PUSH(b, key, mode, req, off, size, alloc_fn, parse_fn) \
  mz_builder_push_object_ex((b), (key), (mode), (mz_bind_opts_t) { (req), (off), (size), (alloc_fn), (parse_fn) })

#define MZ_SCOPE(b, key, mode, req, off, size, alloc_fn, parse_fn) \
  MZ_SCOPE_PUSH((b), MZ_PUSH((b), (key), (mode), (req), (off), (size), (alloc_fn), (parse_fn)))

#define MZ_SCHEMA(b, mode) \
  MZ_SCOPE(b, mz_nullptr, mode, MZ_REQUIRED, MZ_NO_OFFSET, 0, mz_nullptr, mz_nullptr)

// #define MZ_SCHEMA(b, mode) \
//   MZ_SCOPE_PUSH((b), mz_builder_push_object_ex((b), mz_nullptr, (mode), (mz_bind_opts_t) { .required = MZ_REQUIRED, .offset = MZ_NO_OFFSET }))

#define MZ_BIND_EX(b, type, field, key, schema, req, alloc_fn, parse_fn) \
  mz_builder_add_field_ex((b), (key), (schema), (mz_bind_opts_t) { .required = (req), .offset = offsetof(type, field), .on_alloc = (alloc_fn), .on_parse = (parse_fn) })

#define MZ_BIND(b, type, field, key, schema) \
  MZ_BIND_EX((b), type, field, (key), (schema), MZ_REQUIRED, MZ_DEFAULT_ALLOC, MZ_DEFAULT_PARSE)

#define MZ_BIND_OPT(b, type, field, key, schema) \
  MZ_BIND_EX((b), type, field, (key), (schema), MZ_OPTIONAL, MZ_DEFAULT_ALLOC, MZ_DEFAULT_PARSE)

#define MZ_BIND_PARSE(b, type, field, key, schema, parse_fn) \
  MZ_BIND_EX((b), type, field, (key), (schema), MZ_REQUIRED, MZ_DEFAULT_ALLOC, (parse_fn))

#define MZ_BIND_PTR_EX(b, type, field, key, schema, req, alloc_fn, parse_fn) \
  mz_builder_add_field_ptr_ex((b), (key), (schema), (mz_bind_opts_t) { .required = (req), .offset = offsetof(type, field), .ptr_size = sizeof(*(((type*)0)->field)), .on_alloc = (alloc_fn), .on_parse = (parse_fn) })

#define MZ_BIND_PTR(b, type, field, key, schema) \
  MZ_BIND_PTR_EX((b), type, field, (key), (schema), MZ_REQUIRED, MZ_DEFAULT_ALLOC, MZ_DEFAULT_PARSE)

#define MZ_BIND_PTR_ALLOC(b, type, field, key, schema, alloc_fn) \
  MZ_BIND_PTR_EX((b), type, field, (key), (schema), MZ_REQUIRED, (alloc_fn), MZ_DEFAULT_PARSE)

#define MZ_BIND_PTR_PARSE_ALLOC(b, type, field, key, schema, alloc_fn, parse_fn) \
  MZ_BIND_PTR_EX((b), type, field, (key), (schema), MZ_REQUIRED, (alloc_fn), (parse_fn))

#define MZ_BIND_OBJECT_EX(b, type, field, key, mode, req, alloc_fn) \
  MZ_SCOPE(b, key, mode, req, offsetof(type, field), 0, alloc_fn, mz_nullptr)

#define MZ_BIND_OBJECT(b, type, field, key, mode) \
  MZ_BIND_OBJECT_EX((b), type, field, (key), (mode), MZ_REQUIRED, MZ_DEFAULT_ALLOC)

#define MZ_BIND_OBJECT_PTR_EX(b, type, field, key, mode, req, alloc_fn) \
  MZ_SCOPE_PUSH((b), mz_builder_push_object_ptr_ex((b), (key), (mode), (mz_bind_opts_t) { .required = (req), .offset = offsetof(type, field), .ptr_size = sizeof(*(((type*)0)->field)), .on_alloc = (alloc_fn) }))

#define MZ_BIND_OBJECT_PTR(b, type, field, key, mode) \
  MZ_BIND_OBJECT_PTR_EX((b), type, field, (key), (mode), MZ_REQUIRED, MZ_DEFAULT_ALLOC)

#define MZ_BIND_OBJECT_PTR_ALLOC(b, type, field, key, mode, alloc_fn) \
  MZ_BIND_OBJECT_PTR_EX((b), type, field, (key), (mode), MZ_REQUIRED, (alloc_fn))

#define MZ_BIND_TAGGED_EX(b, key, tag_key, kind, req, off) \
  MZ_SCOPE_PUSH((b), mz_builder_push_tagged_union_ex((b), (key), (tag_key), (kind), (mz_bind_opts_t) { .required = (req), .offset = (off) }))

#define MZ_BIND_TAGGED(b, type, field, key, tag_key, kind) \
  MZ_BIND_TAGGED_EX((b), (key), (tag_key), (kind), MZ_REQUIRED, offsetof(type, field))

#define MZ_TAGGED_SCHEMA(b, tag_key, kind) \
  MZ_BIND_TAGGED_EX((b), mz_nullptr, (tag_key), (kind), MZ_REQUIRED, MZ_NO_OFFSET)

#define MZ_TAG_CASE(b, tag, mode) \
  MZ_SCOPE_PUSH((b), mz_builder_push_tagged_case_ex((b), (tag), (mode)))

#define MZ_BIND_ARRAY_EX(b, type, field, key, req, alloc_fn) \
  MZ_SCOPE_PUSH((b), mz_builder_push_array_ex((b), (key), (mz_bind_opts_t) { .required = (req), .offset = offsetof(type, field), .on_alloc = (alloc_fn) }))

#define MZ_BIND_ARRAY(b, type, field, key, alloc_fn) \
  MZ_BIND_ARRAY_EX((b), type, field, (key), MZ_REQUIRED, (alloc_fn))

#define MZ_BIND_MAP_EX(b, type, field, key, req, alloc_fn) \
  MZ_SCOPE_PUSH((b), mz_builder_push_map_ex((b), (key), (mz_bind_opts_t) { .required = (req), .offset = offsetof(type, field), .on_alloc = (alloc_fn) }))

#define MZ_BIND_MAP(b, type, field, key, alloc_fn) \
  MZ_BIND_MAP_EX((b), type, field, (key), MZ_REQUIRED, (alloc_fn))

#define MZ_ENTRY_EX(b, schema, parse_fn) \
  mz_builder_add_entry_ex((b), (schema), (mz_entry_opts_t) { .on_parse = (parse_fn) })

#define MZ_ENTRY(b, schema) \
  MZ_ENTRY_EX((b), (schema), MZ_DEFAULT_PARSE)

#define MZ_ENTRY_PARSE(b, schema, parse_fn) \
  MZ_ENTRY_EX((b), (schema), (parse_fn))

#define MZ_ENTRY_OBJECT(b, mode) \
  MZ_SCOPE_PUSH((b), mz_builder_push_entry_object_ex((b), (mode), (mz_entry_opts_t) {0}))

SP_END_EXTERN_C()

#endif
