#include "mz.h"

#include <string.h>
#include <sys/stat.h>
#include <float.h>
#include <stdio.h>

/*
Backend contract:
- Cursor key matching is case-sensitive.
- Strict object schemas reject unknown keys.
- Integer schemas accept only integral JSON tokens.
- Diagnostic path building stays backend-independent (root.key[0].child).
*/

typedef struct mz_json_doc_t mz_json_doc_t;
typedef struct mz_json_object_iter_t mz_json_object_iter_t;
typedef struct mz_json_array_iter_t mz_json_array_iter_t;

static mz_json_cursor_t mz_json_doc_root_cursor(const mz_json_doc_t* doc);
static mz_err_t mz_json_doc_parse_source(const c8* source, bool from_file, mz_json_doc_t** out_doc);
static void mz_json_doc_release(mz_json_doc_t* doc);

#define MZ_BACKEND_CJSON

#if !defined(MZ_BACKEND_JANSSON) && !defined(MZ_BACKEND_CJSON) && !defined(MZ_BACKEND_CUSTOM)
#error "Please select a backend with MZ_BACKEND_JANSSON, MZ_BACKEND_CJSON or MZ_BACKEND_CUSTOM"
#endif

#if (defined(MZ_BACKEND_JANSSON) && defined(MZ_BACKEND_CJSON)) || \
    (defined(MZ_BACKEND_JANSSON) && defined(MZ_BACKEND_CUSTOM)) || \
    (defined(MZ_BACKEND_CJSON) && defined(MZ_BACKEND_CUSTOM))
#error "Please define exactly one backend macro"
#endif

#if defined(MZ_BACKEND_JANSSON)
#include "jansson.h"

struct mz_json_doc_t {
  json_t* root;
};

struct mz_json_cursor_t {
  json_t* value;
};

struct mz_json_object_iter_t {
  json_t* object;
  void* iter;
};

struct mz_json_array_iter_t {
  json_t* array;
  u32 index;
  u32 len;
};

static mz_json_cursor_t mz_json_doc_root_cursor(const mz_json_doc_t* doc) {
  mz_json_cursor_t cursor = {0};
  if (doc) {
    cursor.value = doc->root;
  }
  return cursor;
}

static mz_json_kind_t mz_json_cursor_kind(const mz_json_cursor_t* cursor) {
  if (!cursor || !cursor->value) {
    return MZ_JSON_KIND_INVALID;
  }

  json_t* value = cursor->value;
  if (json_is_object(value)) {
    return MZ_JSON_KIND_OBJECT;
  }
  if (json_is_array(value)) {
    return MZ_JSON_KIND_ARRAY;
  }
  if (json_is_string(value)) {
    return MZ_JSON_KIND_STRING;
  }
  if (json_is_boolean(value)) {
    return MZ_JSON_KIND_BOOL;
  }
  if (json_is_integer(value)) {
    return MZ_JSON_KIND_INTEGER;
  }
  if (json_is_real(value)) {
    return MZ_JSON_KIND_REAL;
  }
  if (json_is_null(value)) {
    return MZ_JSON_KIND_NULL;
  }

  return MZ_JSON_KIND_INVALID;
}

static bool mz_json_cursor_get_bool(const mz_json_cursor_t* cursor, bool* out_value) {
  if (!cursor || !cursor->value || !json_is_boolean(cursor->value)) {
    return false;
  }

  if (out_value) {
    *out_value = json_is_true(cursor->value);
  }
  return true;
}

static bool mz_json_cursor_get_string(const mz_json_cursor_t* cursor, const c8** out_value) {
  if (!cursor || !cursor->value || !out_value || !json_is_string(cursor->value)) {
    return false;
  }

  const c8* str = json_string_value(cursor->value);
  if (!str) {
    return false;
  }

  *out_value = str;
  return true;
}

static bool mz_json_cursor_get_s64_exact(const mz_json_cursor_t* cursor, s64* out_value) {
  if (!cursor || !cursor->value || !out_value || !json_is_integer(cursor->value)) {
    return false;
  }

  *out_value = (s64)json_integer_value(cursor->value);
  return true;
}

static bool mz_json_cursor_get_f64(const mz_json_cursor_t* cursor, f64* out_value) {
  if (!cursor || !cursor->value || !out_value) {
    return false;
  }

  if (!json_is_real(cursor->value) && !json_is_integer(cursor->value)) {
    return false;
  }

  *out_value = (f64)json_number_value(cursor->value);
  return true;
}

static mz_json_object_iter_t mz_json_cursor_object_iter_begin(const mz_json_cursor_t* cursor) {
  mz_json_object_iter_t it = {0};
  if (!cursor || !cursor->value || !json_is_object(cursor->value)) {
    return it;
  }

  it.object = cursor->value;
  it.iter = json_object_iter(cursor->value);
  return it;
}

static bool mz_json_cursor_object_iter_next(mz_json_object_iter_t* it, const c8** out_key, mz_json_cursor_t* out_cursor) {
  if (!it || !it->iter || !out_key || !out_cursor) {
    return false;
  }

  json_t* object = (json_t*)it->object;
  void* iter = it->iter;

  *out_key = json_object_iter_key(iter);
  out_cursor->value = json_object_iter_value(iter);
  it->iter = json_object_iter_next(object, iter);
  return true;
}

static mz_json_array_iter_t mz_json_cursor_array_iter_begin(const mz_json_cursor_t* cursor) {
  mz_json_array_iter_t it = {0};
  if (!cursor || !cursor->value || !json_is_array(cursor->value)) {
    return it;
  }

  it.array = cursor->value;
  it.index = 0;
  it.len = (u32)json_array_size(cursor->value);
  return it;
}

static bool mz_json_cursor_array_iter_next(mz_json_array_iter_t* it, mz_json_cursor_t* out_cursor, u32* out_index) {
  if (!it || !out_cursor || it->index >= it->len) {
    return false;
  }

  json_t* child = json_array_get((json_t*)it->array, it->index);
  if (!child) {
    return false;
  }

  out_cursor->value = child;
  if (out_index) {
    *out_index = it->index;
  }
  it->index++;
  return true;
}

static mz_err_t mz_json_doc_parse_source(const c8* source, bool from_file, mz_json_doc_t** out_doc) {
  json_error_t error = SP_ZERO_INITIALIZE();
  json_t* root = from_file ? json_load_file(source, 0, &error) : json_loads(source, 0, &error);
  if (!root) {
    return MZ_ERR_JSON;
  }

  mz_json_doc_t* doc = sp_alloc_type(mz_json_doc_t);
  doc->root = root;
  *out_doc = doc;
  return MZ_OK;
}

static void mz_json_doc_release(mz_json_doc_t* doc) {
  if (!doc) {
    return;
  }
  if (doc->root) {
    json_decref(doc->root);
  }
  sp_free(doc);
}

#elif defined(MZ_BACKEND_CJSON)
#include "cJSON.h"

struct mz_json_doc_t {
  cJSON* root;
};

struct mz_json_cursor_t {
  cJSON* value;
};

struct mz_json_object_iter_t {
  cJSON* iter;
};

struct mz_json_array_iter_t {
  cJSON* iter;
  u32 index;
};

static mz_json_cursor_t mz_json_doc_root_cursor(const mz_json_doc_t* doc) {
  mz_json_cursor_t cursor = {0};
  if (doc) {
    cursor.value = doc->root;
  }
  return cursor;
}

static bool mz_json_number_to_s64_exact(const mz_json_cursor_t* cursor, s64* out_value) {
  if (!cursor || !cursor->value || !cJSON_IsNumber(cursor->value)) {
    return false;
  }

  f64 raw = cJSON_GetNumberValue(cursor->value);
  if (raw != raw) {
    return false;
  }

  if (raw < (f64)SP_LIMIT_S64_MIN || raw > (f64)SP_LIMIT_S64_MAX) {
    return false;
  }

  s64 parsed = (s64)raw;
  if ((f64)parsed != raw) {
    return false;
  }

  if (out_value) {
    *out_value = parsed;
  }

  return true;
}

static mz_json_kind_t mz_json_cursor_kind(const mz_json_cursor_t* cursor) {
  if (!cursor || !cursor->value) {
    return MZ_JSON_KIND_INVALID;
  }

  const cJSON* value = cursor->value;
  if (cJSON_IsObject(value)) {
    return MZ_JSON_KIND_OBJECT;
  }
  if (cJSON_IsArray(value)) {
    return MZ_JSON_KIND_ARRAY;
  }
  if (cJSON_IsString(value)) {
    return MZ_JSON_KIND_STRING;
  }
  if (cJSON_IsBool(value)) {
    return MZ_JSON_KIND_BOOL;
  }
  if (cJSON_IsNumber(value)) {
    if (mz_json_number_to_s64_exact(cursor, SP_NULLPTR)) {
      return MZ_JSON_KIND_INTEGER;
    }
    return MZ_JSON_KIND_REAL;
  }
  if (cJSON_IsNull(value)) {
    return MZ_JSON_KIND_NULL;
  }

  return MZ_JSON_KIND_INVALID;
}

static bool mz_json_cursor_get_bool(const mz_json_cursor_t* cursor, bool* out_value) {
  if (!cursor || !cursor->value || !cJSON_IsBool(cursor->value)) {
    return false;
  }

  if (out_value) {
    *out_value = cJSON_IsTrue(cursor->value);
  }
  return true;
}

static bool mz_json_cursor_get_string(const mz_json_cursor_t* cursor, const c8** out_value) {
  if (!cursor || !cursor->value || !out_value) {
    return false;
  }

  const c8* str = cJSON_GetStringValue(cursor->value);
  if (!str) {
    return false;
  }

  *out_value = str;
  return true;
}

static bool mz_json_cursor_get_s64_exact(const mz_json_cursor_t* cursor, s64* out_value) {
  if (!out_value) {
    return false;
  }

  bool ok = mz_json_number_to_s64_exact(cursor, out_value);
  return ok;
}

static bool mz_json_cursor_get_f64(const mz_json_cursor_t* cursor, f64* out_value) {
  if (!cursor || !cursor->value || !out_value || !cJSON_IsNumber(cursor->value)) {
    return false;
  }

  *out_value = cJSON_GetNumberValue(cursor->value);
  return true;
}

static cJSON* mz_json_cjson_next_entry(cJSON* entry) {
  while (entry && !entry->string) {
    entry = entry->next;
  }

  return entry;
}

static mz_json_object_iter_t mz_json_cursor_object_iter_begin(const mz_json_cursor_t* cursor) {
  mz_json_object_iter_t it = {0};
  if (!cursor || !cursor->value || !cJSON_IsObject(cursor->value)) {
    return it;
  }

  it.iter = mz_json_cjson_next_entry(cursor->value->child);
  return it;
}

static bool mz_json_cursor_object_iter_next(mz_json_object_iter_t* it, const c8** out_key, mz_json_cursor_t* out_cursor) {
  if (!it || !it->iter || !out_key || !out_cursor) {
    return false;
  }

  cJSON* entry = (cJSON*)it->iter;
  *out_key = entry->string;
  out_cursor->value = entry;
  it->iter = mz_json_cjson_next_entry(entry->next);
  return *out_key != SP_NULLPTR;
}

static mz_json_array_iter_t mz_json_cursor_array_iter_begin(const mz_json_cursor_t* cursor) {
  mz_json_array_iter_t it = {0};
  if (!cursor || !cursor->value || !cJSON_IsArray(cursor->value)) {
    return it;
  }

  it.iter = cursor->value->child;
  it.index = 0;
  return it;
}

static bool mz_json_cursor_array_iter_next(mz_json_array_iter_t* it, mz_json_cursor_t* out_cursor, u32* out_index) {
  if (!it || !it->iter || !out_cursor) {
    return false;
  }

  cJSON* entry = (cJSON*)it->iter;
  out_cursor->value = entry;
  if (out_index) {
    *out_index = it->index;
  }

  it->iter = entry->next;
  it->index++;
  return true;
}

static mz_err_t mz_json_cjson_read_file(const c8* path, c8** out_buffer) {
  FILE* file = fopen(path, "rb");
  if (!file) {
    return MZ_ERR_JSON;
  }

  if (fseek(file, 0, SEEK_END) != 0) {
    fclose(file);
    return MZ_ERR_JSON;
  }

  long file_size = ftell(file);
  if (file_size < 0 || (u64)file_size >= (u64)SP_LIMIT_U32_MAX) {
    fclose(file);
    return MZ_ERR_JSON;
  }

  if (fseek(file, 0, SEEK_SET) != 0) {
    fclose(file);
    return MZ_ERR_JSON;
  }

  u32 size = (u32)file_size;
  c8* buffer = (c8*)sp_alloc(size + 1);
  size_t read_len = fread(buffer, 1, size, file);
  fclose(file);
  if (read_len != size) {
    sp_free(buffer);
    return MZ_ERR_JSON;
  }

  buffer[size] = '\0';
  *out_buffer = buffer;
  return MZ_OK;
}

static mz_err_t mz_json_doc_parse_source(const c8* source, bool from_file, mz_json_doc_t** out_doc) {
  if (!source || !out_doc) {
    return MZ_ERR_JSON;
  }

  c8* file_buffer = SP_NULLPTR;
  const c8* parse_source = source;
  if (from_file) {
    mz_err_t read_err = mz_json_cjson_read_file(source, &file_buffer);
    if (read_err != MZ_OK) {
      return read_err;
    }
    parse_source = file_buffer;
  }

  cJSON* root = cJSON_ParseWithOpts(parse_source, SP_NULLPTR, true);
  if (file_buffer) {
    sp_free(file_buffer);
  }
  if (!root) {
    return MZ_ERR_JSON;
  }

  mz_json_doc_t* doc = sp_alloc_type(mz_json_doc_t);
  doc->root = root;
  *out_doc = doc;
  return MZ_OK;
}

static void mz_json_doc_release(mz_json_doc_t* doc) {
  if (!doc) {
    return;
  }

  if (doc->root) {
    cJSON_Delete(doc->root);
  }

  sp_free(doc);
}

#elif defined(MZ_BACKEND_CUSTOM)
#ifndef MZ_BACKEND_CUSTOM_HEADER
#error "MZ_BACKEND_CUSTOM requires MZ_BACKEND_CUSTOM_HEADER to provide backend contract implementations"
#endif

#include MZ_BACKEND_CUSTOM_HEADER
#endif

typedef enum {
  MZ_SCHEMA_OBJECT,
  MZ_SCHEMA_STRING,
  MZ_SCHEMA_ANY,
  MZ_SCHEMA_BOOL,
  MZ_SCHEMA_S32,
  MZ_SCHEMA_U64,
  MZ_SCHEMA_F64,
  MZ_SCHEMA_ARRAY,
  MZ_SCHEMA_MAP,
  MZ_SCHEMA_TAGGED,
} mz_schema_kind_t;

typedef struct {
  mz_tag_value_t tag;
  mz_schema_t* schema;
} mz_tag_case_t;

typedef struct {
  const c8* key;
  mz_schema_t* schema;
  bool required;
  u32 offset;
  bool is_ptr;
  u32 ptr_size;
  mz_field_ptr_fn_t on_resolve;
} mz_field_t;

typedef struct {
  u32 hash;
  u32 field_index;
} mz_object_slot_t;

struct mz_schema_t {
  mz_schema_kind_t kind;
  union {
    struct {
      mz_object_mode_t mode;
      sp_da(mz_field_t) fields;
      mz_object_slot_t* slots;
      u32 slots_cap;
    } object;
    struct {
      s32 min;
      s32 max;
    } s32_range;
    struct {
      u64 min;
      u64 max;
    } u64_range;
    struct {
      f64 min;
      f64 max;
    } f64_range;
    struct {
      mz_schema_t* element;
      u32 min_len;
      u32 max_len;
      u32 elem_size;
    } array;
    struct {
      mz_schema_t* value;
      mz_map_insert_fn_t on_insert;
    } map;
    struct {
      const c8* key;
      mz_tag_kind_t kind;
      sp_da(mz_tag_case_t) cases;
    } tagged;
  } as;
};

static mz_err_t mz_diag_set(mz_ctx_t* ctx, mz_err_t kind) {
  if (ctx) {
    ctx->diag.kind = kind;
  }

  return kind;
}

static void mz_diag_push_key(mz_ctx_t* ctx, const c8* key) {
  if (!ctx) {
    return;
  }

  if (ctx->diag_parts_len >= SP_CARR_LEN(ctx->diag_parts)) {
    return;
  }

  ctx->diag_parts[ctx->diag_parts_len++] = (mz_diag_part_t) {
    .is_index = false,
    .as.key = key,
  };
}

static void mz_diag_push_index(mz_ctx_t* ctx, u32 index) {
  if (!ctx) {
    return;
  }

  if (ctx->diag_parts_len >= SP_CARR_LEN(ctx->diag_parts)) {
    return;
  }

  ctx->diag_parts[ctx->diag_parts_len++] = (mz_diag_part_t) {
    .is_index = true,
    .as.index = index,
  };
}

static const c8* mz_err_message(mz_err_t kind) {
  switch (kind) {
    case MZ_OK: {
      return "ok";
    }
    case MZ_ERR_JSON: {
      return "invalid json";
    }
    case MZ_ERR_TYPE: {
      return "type mismatch";
    }
    case MZ_ERR_MISSING_KEY: {
      return "missing required key";
    }
    case MZ_ERR_UNKNOWN_KEY: {
      return "unknown key";
    }
    case MZ_ERR_RANGE: {
      return "value out of range";
    }
    case MZ_ERR_LIMIT: {
      return "input exceeds configured limit";
    }
  }

  return "unknown error";
}

static void mz_diag_reset(mz_ctx_t* ctx);

static void mz_diag_finalize(mz_ctx_t* ctx) {
  if (!ctx) {
    return;
  }

  c8* dst = ctx->diag_path;
  u32 rem = SP_CARR_LEN(ctx->diag_path);

  if (rem > 0) {
    s32 n = snprintf(dst, rem, "root");
    if (n > 0) {
      u32 used = (u32)n < rem ? (u32)n : rem;
      dst += used;
      rem -= used;
    }
  }

  for (s32 it = (s32)ctx->diag_parts_len - 1; it >= 0; it--) {
    mz_diag_part_t part = ctx->diag_parts[it];
    if (rem == 0) {
      break;
    }

    s32 n = 0;
    if (part.is_index) {
      n = snprintf(dst, rem, "[%u]", part.as.index);
    }
    else {
      n = snprintf(dst, rem, ".%s", part.as.key);
    }

    if (n > 0) {
      u32 used = (u32)n < rem ? (u32)n : rem;
      dst += used;
      rem -= used;
    }
  }

  ctx->diag.path = ctx->diag_path;
  ctx->diag.message = mz_err_message(ctx->diag.kind);
}

static bool mz_schema_is_object(mz_schema_t* schema) {
  return schema && schema->kind == MZ_SCHEMA_OBJECT;
}

static u32 mz_object_key_hash(const c8* key) {
  SP_ASSERT(key);
  u64 h = sp_hash_cstr(key);
  u32 folded = (u32)h ^ (u32)(h >> 32);
  return folded ? folded : 1;
}

static void mz_object_rebuild_index(mz_schema_t* schema) {
  if (!mz_schema_is_object(schema)) {
    return;
  }

  if (schema->as.object.slots) {
    sp_free(schema->as.object.slots);
    schema->as.object.slots = SP_NULLPTR;
    schema->as.object.slots_cap = 0;
  }

  u32 field_count = sp_da_size(schema->as.object.fields);
  if (field_count == 0) {
    return;
  }

  u32 cap = 8;
  while (cap < field_count * 2) {
    cap <<= 1;
  }

  schema->as.object.slots = sp_alloc(sizeof(mz_object_slot_t) * cap);
  if (!schema->as.object.slots) {
    schema->as.object.slots_cap = 0;
    return;
  }
  schema->as.object.slots_cap = cap;

  sp_for(it, cap) {
    schema->as.object.slots[it] = (mz_object_slot_t) {
      .hash = 0,
      .field_index = SP_LIMIT_U32_MAX,
    };
  }

  u32 mask = cap - 1;
  sp_da_for(schema->as.object.fields, field_it) {
    mz_field_t* field = &schema->as.object.fields[field_it];
    u32 hash = mz_object_key_hash(field->key);
    u32 slot_index = hash & mask;
    while (true) {
      mz_object_slot_t* slot = &schema->as.object.slots[slot_index];
      if (slot->field_index == SP_LIMIT_U32_MAX) {
        slot->hash = hash;
        slot->field_index = field_it;
        break;
      }

      mz_field_t* existing = &schema->as.object.fields[slot->field_index];
      SP_ASSERT(!sp_cstr_equal(existing->key, field->key));
      if (sp_cstr_equal(existing->key, field->key)) {
        break;
      }

      slot_index = (slot_index + 1) & mask;
    }
  }
}

static mz_field_t* mz_object_find_field_indexed(mz_schema_t* schema, const c8* key, u32* out_field_index) {
  if (!mz_schema_is_object(schema) || !key) {
    return SP_NULLPTR;
  }

  if (!schema->as.object.slots) {
    sp_da_for(schema->as.object.fields, it) {
      mz_field_t* field = &schema->as.object.fields[it];
      if (sp_cstr_equal(field->key, key)) {
        if (out_field_index) {
          *out_field_index = it;
        }
        return field;
      }
    }
    return SP_NULLPTR;
  }

  u32 cap = schema->as.object.slots_cap;
  if (cap == 0) {
    return SP_NULLPTR;
  }

  u32 hash = mz_object_key_hash(key);
  u32 slot_index = hash & (cap - 1);

  while (true) {
    mz_object_slot_t* slot = &schema->as.object.slots[slot_index];
    if (slot->field_index == SP_LIMIT_U32_MAX) {
      return SP_NULLPTR;
    }

    if (slot->hash == hash) {
      mz_field_t* field = &schema->as.object.fields[slot->field_index];
      if (sp_cstr_equal(field->key, key)) {
        if (out_field_index) {
          *out_field_index = slot->field_index;
        }
        return field;
      }
    }

    slot_index = (slot_index + 1) & (cap - 1);
  }
}

static mz_field_t* mz_object_find_field(mz_schema_t* schema, const c8* key) {
  mz_field_t* indexed = mz_object_find_field_indexed(schema, key, SP_NULLPTR);
  if (indexed) {
    return indexed;
  }

  if (!mz_schema_is_object(schema)) {
    return SP_NULLPTR;
  }

  sp_da_for(schema->as.object.fields, it) {
    mz_field_t* field = &schema->as.object.fields[it];
    if (sp_cstr_equal(field->key, key)) {
      return field;
    }
  }

  return SP_NULLPTR;
}

static mz_err_t mz_eval_schema(mz_ctx_t* ctx, mz_schema_t* schema, const mz_json_cursor_t* cursor, void* out);

static bool mz_bitset_has(u32* bits, u32 index) {
  if (!bits) {
    return false;
  }
  return (bits[index / 32] & (1u << (index % 32))) != 0;
}

static void mz_bitset_set(u32* bits, u32 index) {
  if (!bits) {
    return;
  }
  bits[index / 32] |= (1u << (index % 32));
}

static mz_err_t mz_eval_object_member(mz_ctx_t* ctx, mz_schema_t* schema, const c8* key, const mz_json_cursor_t* child, void* out, u32* seen) {
  u32 field_index = 0;
  mz_field_t* field = mz_object_find_field_indexed(schema, key, &field_index);
  if (!field) {
    if (schema->as.object.mode == MZ_OBJECT_STRICT) {
      mz_err_t err = mz_diag_set(ctx, MZ_ERR_UNKNOWN_KEY);
      mz_diag_push_key(ctx, key);
      return err;
    }
    return MZ_OK;
  }

  if (mz_bitset_has(seen, field_index)) {
    return MZ_OK;
  }

  void* child_out = SP_NULLPTR;
  if (out && field->offset != MZ_NO_OFFSET) {
    if (!field->is_ptr) {
      child_out = ((u8*)out) + field->offset;
    }
    else {
      void** slot = (void**)(((u8*)out) + field->offset);
      if (!*slot) {
        if (field->on_resolve) {
          sp_context_push_allocator(ctx->allocator);
          mz_err_t bind_err = field->on_resolve(ctx, out, field->key, slot);
          sp_context_pop();
          if (bind_err != MZ_OK) {
            mz_diag_push_key(ctx, field->key);
            return bind_err;
          }
        }
        else {
          if (field->ptr_size == 0) {
            mz_err_t bind_err = mz_diag_set(ctx, MZ_ERR_TYPE);
            mz_diag_push_key(ctx, field->key);
            return bind_err;
          }

          sp_context_push_allocator(ctx->allocator);
          *slot = sp_alloc(field->ptr_size);
          sp_context_pop();
        }
      }

      child_out = *slot;
    }
  }

  mz_err_t err = mz_eval_schema(ctx, field->schema, child, child_out);
  if (err != MZ_OK) {
    mz_diag_push_key(ctx, field->key);
    return err;
  }

  mz_bitset_set(seen, field_index);
  return MZ_OK;
}

static mz_err_t mz_eval_object_from_iter(
  mz_ctx_t* ctx,
  mz_schema_t* schema,
  mz_json_object_iter_t* iter,
  bool has_first,
  const c8* first_key,
  const mz_json_cursor_t* first_child,
  void* out
) {
  u32 field_count = sp_da_size(schema->as.object.fields);
  u32 seen_inline[2] = {0};
  u32 seen_words = (field_count + 31) / 32;
  u32* seen = seen_words <= SP_CARR_LEN(seen_inline) ? seen_inline : SP_NULLPTR;
  if (!seen && seen_words > 0) {
    sp_context_push_allocator(ctx->allocator);
    seen = sp_alloc(sizeof(u32) * seen_words);
    sp_context_pop();
    if (!seen) {
      return mz_diag_set(ctx, MZ_ERR_TYPE);
    }
    memset(seen, 0, sizeof(u32) * seen_words);
  }

  if (has_first) {
    mz_err_t first_err = mz_eval_object_member(ctx, schema, first_key, first_child, out, seen);
    if (first_err != MZ_OK) {
      return first_err;
    }
  }

  const c8* key = SP_NULLPTR;
  mz_json_cursor_t child = {0};
  while (mz_json_cursor_object_iter_next(iter, &key, &child)) {
    mz_err_t err = mz_eval_object_member(ctx, schema, key, &child, out, seen);
    if (err != MZ_OK) {
      return err;
    }
  }

  sp_da_for(schema->as.object.fields, field_it) {
    mz_field_t* field = &schema->as.object.fields[field_it];
    if (!field->required) {
      continue;
    }

    if (!mz_bitset_has(seen, field_it)) {
      mz_err_t err = mz_diag_set(ctx, MZ_ERR_MISSING_KEY);
      mz_diag_push_key(ctx, field->key);
      return err;
    }
  }

  return MZ_OK;
}

static mz_err_t mz_eval_object(mz_ctx_t* ctx, mz_schema_t* schema, const mz_json_cursor_t* cursor, void* out) {
  if (mz_json_cursor_kind(cursor) != MZ_JSON_KIND_OBJECT) {
    return mz_diag_set(ctx, MZ_ERR_TYPE);
  }

  mz_json_object_iter_t iter = mz_json_cursor_object_iter_begin(cursor);
  return mz_eval_object_from_iter(ctx, schema, &iter, false, SP_NULLPTR, SP_NULLPTR, out);
}

static mz_err_t mz_eval_string(mz_ctx_t* ctx, const mz_json_cursor_t* cursor, void* out) {
  const c8* json_value = SP_NULLPTR;
  if (!mz_json_cursor_get_string(cursor, &json_value)) {
    return mz_diag_set(ctx, MZ_ERR_TYPE);
  }

  if (out) {
    SP_ASSERT(ctx);
    SP_ASSERT(ctx->arena);

    sp_context_push_allocator(ctx->allocator);
    c8* copy = sp_cstr_copy(json_value);
    sp_context_pop();

    *((const c8**)out) = copy;
  }

  return MZ_OK;
}

static mz_err_t mz_eval_any(mz_ctx_t* ctx, const mz_json_cursor_t* cursor, void* out) {
  SP_UNUSED(ctx);
  SP_UNUSED(cursor);
  SP_UNUSED(out);
  return MZ_OK;
}

static mz_err_t mz_eval_bool(mz_ctx_t* ctx, const mz_json_cursor_t* cursor, void* out) {
  bool parsed = false;
  if (!mz_json_cursor_get_bool(cursor, &parsed)) {
    return mz_diag_set(ctx, MZ_ERR_TYPE);
  }

  if (out) {
    *((bool*)out) = parsed;
  }

  return MZ_OK;
}

static mz_err_t mz_eval_s32(mz_ctx_t* ctx, mz_schema_t* schema, const mz_json_cursor_t* cursor, void* out) {
  s64 raw = 0;
  if (!mz_json_cursor_get_s64_exact(cursor, &raw)) {
    return mz_diag_set(ctx, MZ_ERR_TYPE);
  }

  s32 parsed = (s32)raw;
  if ((s64)parsed != raw) {
    return mz_diag_set(ctx, MZ_ERR_RANGE);
  }

  if (parsed < schema->as.s32_range.min || parsed > schema->as.s32_range.max) {
    return mz_diag_set(ctx, MZ_ERR_RANGE);
  }

  if (out) {
    *((s32*)out) = parsed;
  }

  return MZ_OK;
}

static mz_err_t mz_eval_u64(mz_ctx_t* ctx, mz_schema_t* schema, const mz_json_cursor_t* cursor, void* out) {
  s64 raw = 0;
  if (!mz_json_cursor_get_s64_exact(cursor, &raw)) {
    return mz_diag_set(ctx, MZ_ERR_TYPE);
  }
  if (raw < 0) {
    return mz_diag_set(ctx, MZ_ERR_RANGE);
  }

  u64 parsed = (u64)raw;
  if (parsed < schema->as.u64_range.min || parsed > schema->as.u64_range.max) {
    return mz_diag_set(ctx, MZ_ERR_RANGE);
  }

  if (out) {
    *((u64*)out) = parsed;
  }

  return MZ_OK;
}

static mz_err_t mz_eval_f64(mz_ctx_t* ctx, mz_schema_t* schema, const mz_json_cursor_t* cursor, void* out) {
  f64 parsed = 0;
  if (!mz_json_cursor_get_f64(cursor, &parsed)) {
    return mz_diag_set(ctx, MZ_ERR_TYPE);
  }

  if (parsed < schema->as.f64_range.min || parsed > schema->as.f64_range.max) {
    return mz_diag_set(ctx, MZ_ERR_RANGE);
  }

  if (out) {
    *((f64*)out) = parsed;
  }

  return MZ_OK;
}

static mz_err_t mz_eval_array(mz_ctx_t* ctx, mz_schema_t* schema, const mz_json_cursor_t* cursor, void* out) {
  if (mz_json_cursor_kind(cursor) != MZ_JSON_KIND_ARRAY) {
    return mz_diag_set(ctx, MZ_ERR_TYPE);
  }

  mz_schema_t* element_schema = schema->as.array.element;
  u32 min_len = schema->as.array.min_len;
  u32 max_len = schema->as.array.max_len;

  if (!out) {
    u32 count = 0;
    mz_json_array_iter_t it = mz_json_cursor_array_iter_begin(cursor);
    mz_json_cursor_t child = {0};
    u32 index = 0;
    while (mz_json_cursor_array_iter_next(&it, &child, &index)) {
      if (max_len != SP_LIMIT_U32_MAX && count >= max_len) {
        return mz_diag_set(ctx, MZ_ERR_RANGE);
      }

      mz_err_t err = mz_eval_schema(ctx, element_schema, &child, SP_NULLPTR);
      if (err != MZ_OK) {
        mz_diag_push_index(ctx, index);
        return err;
      }

      count++;
    }

    if (count < min_len) {
      return mz_diag_set(ctx, MZ_ERR_RANGE);
    }

    return MZ_OK;
  }

  sp_context_push_allocator(ctx->allocator);
  switch (element_schema->kind) {
    case MZ_SCHEMA_STRING: {
      const c8*** outp = (const c8***)out;
      const c8** result = *outp;
      u32 count = 0;
      mz_json_array_iter_t it = mz_json_cursor_array_iter_begin(cursor);
      mz_json_cursor_t child = {0};
      u32 index = 0;
      while (mz_json_cursor_array_iter_next(&it, &child, &index)) {
        if (max_len != SP_LIMIT_U32_MAX && count >= max_len) {
          *outp = result;
          sp_context_pop();
          return mz_diag_set(ctx, MZ_ERR_RANGE);
        }

        const c8* parsed = SP_NULLPTR;
        mz_err_t err = mz_eval_schema(ctx, element_schema, &child, &parsed);
        if (err != MZ_OK) {
          *outp = result;
          mz_diag_push_index(ctx, index);
          sp_context_pop();
          return err;
        }
        sp_da_push(result, parsed);
        *outp = result;
        count++;
      }

      if (count < min_len) {
        *outp = result;
        sp_context_pop();
        return mz_diag_set(ctx, MZ_ERR_RANGE);
      }

      sp_context_pop();
      return MZ_OK;
    }
    case MZ_SCHEMA_BOOL: {
      bool** outp = (bool**)out;
      bool* result = *outp;
      u32 count = 0;
      mz_json_array_iter_t it = mz_json_cursor_array_iter_begin(cursor);
      mz_json_cursor_t child = {0};
      u32 index = 0;
      while (mz_json_cursor_array_iter_next(&it, &child, &index)) {
        if (max_len != SP_LIMIT_U32_MAX && count >= max_len) {
          *outp = result;
          sp_context_pop();
          return mz_diag_set(ctx, MZ_ERR_RANGE);
        }

        bool parsed = false;
        mz_err_t err = mz_eval_schema(ctx, element_schema, &child, &parsed);
        if (err != MZ_OK) {
          *outp = result;
          mz_diag_push_index(ctx, index);
          sp_context_pop();
          return err;
        }
        sp_da_push(result, parsed);
        *outp = result;
        count++;
      }

      if (count < min_len) {
        *outp = result;
        sp_context_pop();
        return mz_diag_set(ctx, MZ_ERR_RANGE);
      }

      sp_context_pop();
      return MZ_OK;
    }
    case MZ_SCHEMA_S32: {
      s32** outp = (s32**)out;
      s32* result = *outp;
      u32 count = 0;
      mz_json_array_iter_t it = mz_json_cursor_array_iter_begin(cursor);
      mz_json_cursor_t child = {0};
      u32 index = 0;
      while (mz_json_cursor_array_iter_next(&it, &child, &index)) {
        if (max_len != SP_LIMIT_U32_MAX && count >= max_len) {
          *outp = result;
          sp_context_pop();
          return mz_diag_set(ctx, MZ_ERR_RANGE);
        }

        s32 parsed = 0;
        mz_err_t err = mz_eval_schema(ctx, element_schema, &child, &parsed);
        if (err != MZ_OK) {
          *outp = result;
          mz_diag_push_index(ctx, index);
          sp_context_pop();
          return err;
        }
        sp_da_push(result, parsed);
        *outp = result;
        count++;
      }

      if (count < min_len) {
        *outp = result;
        sp_context_pop();
        return mz_diag_set(ctx, MZ_ERR_RANGE);
      }

      sp_context_pop();
      return MZ_OK;
    }
    case MZ_SCHEMA_U64: {
      u64** outp = (u64**)out;
      u64* result = *outp;
      u32 count = 0;
      mz_json_array_iter_t it = mz_json_cursor_array_iter_begin(cursor);
      mz_json_cursor_t child = {0};
      u32 index = 0;
      while (mz_json_cursor_array_iter_next(&it, &child, &index)) {
        if (max_len != SP_LIMIT_U32_MAX && count >= max_len) {
          *outp = result;
          sp_context_pop();
          return mz_diag_set(ctx, MZ_ERR_RANGE);
        }

        u64 parsed = 0;
        mz_err_t err = mz_eval_schema(ctx, element_schema, &child, &parsed);
        if (err != MZ_OK) {
          *outp = result;
          mz_diag_push_index(ctx, index);
          sp_context_pop();
          return err;
        }
        sp_da_push(result, parsed);
        *outp = result;
        count++;
      }

      if (count < min_len) {
        *outp = result;
        sp_context_pop();
        return mz_diag_set(ctx, MZ_ERR_RANGE);
      }

      sp_context_pop();
      return MZ_OK;
    }
    case MZ_SCHEMA_F64: {
      f64** outp = (f64**)out;
      f64* result = *outp;
      u32 count = 0;
      mz_json_array_iter_t it = mz_json_cursor_array_iter_begin(cursor);
      mz_json_cursor_t child = {0};
      u32 index = 0;
      while (mz_json_cursor_array_iter_next(&it, &child, &index)) {
        if (max_len != SP_LIMIT_U32_MAX && count >= max_len) {
          *outp = result;
          sp_context_pop();
          return mz_diag_set(ctx, MZ_ERR_RANGE);
        }

        f64 parsed = 0;
        mz_err_t err = mz_eval_schema(ctx, element_schema, &child, &parsed);
        if (err != MZ_OK) {
          *outp = result;
          mz_diag_push_index(ctx, index);
          sp_context_pop();
          return err;
        }
        sp_da_push(result, parsed);
        *outp = result;
        count++;
      }

      if (count < min_len) {
        *outp = result;
        sp_context_pop();
        return mz_diag_set(ctx, MZ_ERR_RANGE);
      }

      sp_context_pop();
      return MZ_OK;
    }
    case MZ_SCHEMA_OBJECT: {
      if (schema->as.array.elem_size == 0) {
        sp_context_pop();
        return mz_diag_set(ctx, MZ_ERR_TYPE);
      }

      void** outp = (void**)out;
      void* result = *outp;
      u32 count = 0;
      mz_json_array_iter_t it = mz_json_cursor_array_iter_begin(cursor);
      mz_json_cursor_t child = {0};
      u32 index = 0;
      while (mz_json_cursor_array_iter_next(&it, &child, &index)) {
        if (max_len != SP_LIMIT_U32_MAX && count >= max_len) {
          *outp = result;
          sp_context_pop();
          return mz_diag_set(ctx, MZ_ERR_RANGE);
        }

        void* parsed = sp_alloc(schema->as.array.elem_size);
        memset(parsed, 0, schema->as.array.elem_size);

        mz_err_t err = mz_eval_schema(ctx, element_schema, &child, parsed);
        if (err != MZ_OK) {
          sp_free(parsed);
          *outp = result;
          mz_diag_push_index(ctx, index);
          sp_context_pop();
          return err;
        }

        sp_dyn_array_push_f(&result, parsed, schema->as.array.elem_size);
        sp_free(parsed);
        *outp = result;
        count++;
      }

      if (count < min_len) {
        *outp = result;
        sp_context_pop();
        return mz_diag_set(ctx, MZ_ERR_RANGE);
      }

      sp_context_pop();
      return MZ_OK;
    }
    default: {
      sp_context_pop();
      return mz_diag_set(ctx, MZ_ERR_TYPE);
    }
  }
}

static bool mz_tag_matches_cursor(mz_tag_value_t tag, const mz_json_cursor_t* cursor) {
  switch (tag.kind) {
    case MZ_TAG_KIND_STR: {
      const c8* str = SP_NULLPTR;
      if (!mz_json_cursor_get_string(cursor, &str)) {
        return false;
      }
      return str && sp_cstr_equal(tag.as.str, str);
    }
    case MZ_TAG_KIND_S32: {
      s64 raw = 0;
      if (!mz_json_cursor_get_s64_exact(cursor, &raw)) {
        return false;
      }
      s32 parsed = (s32)raw;
      if ((s64)parsed != raw) {
        return false;
      }
      return tag.as.s32 == parsed;
    }
    case MZ_TAG_KIND_U64: {
      s64 raw = 0;
      if (!mz_json_cursor_get_s64_exact(cursor, &raw)) {
        return false;
      }
      if (raw < 0) {
        return false;
      }
      return tag.as.u64 == (u64)raw;
    }
  }

  return false;
}

static mz_err_t mz_eval_tagged(mz_ctx_t* ctx, mz_schema_t* schema, const mz_json_cursor_t* cursor, void* out) {
  if (mz_json_cursor_kind(cursor) != MZ_JSON_KIND_OBJECT) {
    return mz_diag_set(ctx, MZ_ERR_TYPE);
  }

  mz_json_object_iter_t iter = mz_json_cursor_object_iter_begin(cursor);
  const c8* first_key = SP_NULLPTR;
  mz_json_cursor_t first_child = {0};
  if (!mz_json_cursor_object_iter_next(&iter, &first_key, &first_child)) {
    mz_err_t err = mz_diag_set(ctx, MZ_ERR_MISSING_KEY);
    mz_diag_push_key(ctx, schema->as.tagged.key);
    return err;
  }

  if (!sp_cstr_equal(first_key, schema->as.tagged.key)) {
    mz_err_t err = mz_diag_set(ctx, MZ_ERR_MISSING_KEY);
    mz_diag_push_key(ctx, schema->as.tagged.key);
    return err;
  }

  mz_schema_t* selected = SP_NULLPTR;
  sp_da_for(schema->as.tagged.cases, case_it) {
    mz_tag_case_t* c = &schema->as.tagged.cases[case_it];
    if (mz_tag_matches_cursor(c->tag, &first_child)) {
      selected = c->schema;
      break;
    }
  }

  if (!selected) {
    mz_err_t err = mz_diag_set(ctx, MZ_ERR_TYPE);
    mz_diag_push_key(ctx, schema->as.tagged.key);
    return err;
  }

  if (selected->kind != MZ_SCHEMA_OBJECT) {
    mz_err_t err = mz_diag_set(ctx, MZ_ERR_TYPE);
    mz_diag_push_key(ctx, schema->as.tagged.key);
    return err;
  }

  return mz_eval_object_from_iter(ctx, selected, &iter, true, first_key, &first_child, out);
}

static mz_schema_t* mz_schema_array_ex_internal(mz_schema_t* element, u32 min_len, u32 max_len, u32 elem_size) {
  SP_ASSERT(element);

  mz_schema_t* schema = sp_alloc_type(mz_schema_t);
  *schema = (mz_schema_t) {
    .kind = MZ_SCHEMA_ARRAY,
    .as = {
      .array = {
        .element = element,
        .min_len = min_len,
        .max_len = max_len,
        .elem_size = elem_size,
      },
    },
  };
  return schema;
}

static mz_err_t mz_eval_map(mz_ctx_t* ctx, mz_schema_t* schema, const mz_json_cursor_t* cursor, void* out) {
  if (mz_json_cursor_kind(cursor) != MZ_JSON_KIND_OBJECT) {
    return mz_diag_set(ctx, MZ_ERR_TYPE);
  }

  const c8* key = SP_NULLPTR;
  mz_json_cursor_t child = {0};
  mz_json_object_iter_t it = mz_json_cursor_object_iter_begin(cursor);
  while (mz_json_cursor_object_iter_next(&it, &key, &child)) {
    mz_schema_t* value_schema = schema->as.map.value;

    if (!out) {
      mz_err_t err = mz_eval_schema(ctx, value_schema, &child, SP_NULLPTR);
      if (err != MZ_OK) {
        mz_diag_push_key(ctx, key);
        return err;
      }

      continue;
    }

    if (!schema->as.map.on_insert) {
      return mz_diag_set(ctx, MZ_ERR_TYPE);
    }

    void* value_out = SP_NULLPTR;
    sp_context_push_allocator(ctx->allocator);
    mz_err_t err = schema->as.map.on_insert(ctx, out, key, &value_out);
    sp_context_pop();
    if (err != MZ_OK) {
      mz_diag_push_key(ctx, key);
      return err;
    }

    if (!value_out) {
      err = mz_diag_set(ctx, MZ_ERR_TYPE);
      mz_diag_push_key(ctx, key);
      return err;
    }

    err = mz_eval_schema(ctx, value_schema, &child, value_out);
    if (err != MZ_OK) {
      mz_diag_push_key(ctx, key);
      return err;
    }
  }

  return MZ_OK;
}

static mz_err_t mz_eval_schema(mz_ctx_t* ctx, mz_schema_t* schema, const mz_json_cursor_t* cursor, void* out) {
  if (!schema) {
    return mz_diag_set(ctx, MZ_ERR_TYPE);
  }

  switch (schema->kind) {
    case MZ_SCHEMA_OBJECT: {
      return mz_eval_object(ctx, schema, cursor, out);
    }
    case MZ_SCHEMA_STRING: {
      return mz_eval_string(ctx, cursor, out);
    }
    case MZ_SCHEMA_ANY: {
      return mz_eval_any(ctx, cursor, out);
    }
    case MZ_SCHEMA_BOOL: {
      return mz_eval_bool(ctx, cursor, out);
    }
    case MZ_SCHEMA_S32: {
      return mz_eval_s32(ctx, schema, cursor, out);
    }
    case MZ_SCHEMA_U64: {
      return mz_eval_u64(ctx, schema, cursor, out);
    }
    case MZ_SCHEMA_F64: {
      return mz_eval_f64(ctx, schema, cursor, out);
    }
    case MZ_SCHEMA_ARRAY: {
      return mz_eval_array(ctx, schema, cursor, out);
    }
    case MZ_SCHEMA_MAP: {
      return mz_eval_map(ctx, schema, cursor, out);
    }
    case MZ_SCHEMA_TAGGED: {
      return mz_eval_tagged(ctx, schema, cursor, out);
    }
  }

  return mz_diag_set(ctx, MZ_ERR_TYPE);
}

static void mz_schema_free_internal(mz_schema_t* schema) {
  if (!schema) {
    return;
  }

  if (schema->kind == MZ_SCHEMA_OBJECT) {
    sp_da_for(schema->as.object.fields, it) {
      if (schema->as.object.fields[it].key) {
        sp_free((void*)schema->as.object.fields[it].key);
      }
      mz_schema_free_internal(schema->as.object.fields[it].schema);
    }
    sp_da_free(schema->as.object.fields);
    if (schema->as.object.slots) {
      sp_free(schema->as.object.slots);
      schema->as.object.slots = SP_NULLPTR;
      schema->as.object.slots_cap = 0;
    }
  }
  if (schema->kind == MZ_SCHEMA_ARRAY) {
    mz_schema_free_internal(schema->as.array.element);
  }
  if (schema->kind == MZ_SCHEMA_MAP) {
    mz_schema_free_internal(schema->as.map.value);
  }
  if (schema->kind == MZ_SCHEMA_TAGGED) {
    if (schema->as.tagged.key) {
      sp_free((void*)schema->as.tagged.key);
    }
    sp_da_for(schema->as.tagged.cases, it) {
      if (schema->as.tagged.cases[it].tag.kind == MZ_TAG_KIND_STR && schema->as.tagged.cases[it].tag.as.str) {
        sp_free((void*)schema->as.tagged.cases[it].tag.as.str);
      }
      mz_schema_free_internal(schema->as.tagged.cases[it].schema);
    }
    sp_da_free(schema->as.tagged.cases);
  }

  sp_free(schema);
}

mz_schema_t* mz_schema_object(mz_object_mode_t mode) {
  mz_schema_t* schema = sp_alloc_type(mz_schema_t);
  *schema = (mz_schema_t) {
    .kind = MZ_SCHEMA_OBJECT,
    .as = {
      .object = {
        .mode = mode,
      },
    },
  };
  return schema;
}

mz_schema_t* mz_schema_string() {
  mz_schema_t* schema = sp_alloc_type(mz_schema_t);
  *schema = (mz_schema_t) {
    .kind = MZ_SCHEMA_STRING,
  };
  return schema;
}

mz_schema_t* mz_schema_any() {
  mz_schema_t* schema = sp_alloc_type(mz_schema_t);
  *schema = (mz_schema_t) {
    .kind = MZ_SCHEMA_ANY,
  };
  return schema;
}

mz_schema_t* mz_schema_bool() {
  mz_schema_t* schema = sp_alloc_type(mz_schema_t);
  *schema = (mz_schema_t) {
    .kind = MZ_SCHEMA_BOOL,
  };
  return schema;
}

mz_schema_t* mz_schema_s32_ex(s32 min, s32 max) {
  mz_schema_t* schema = sp_alloc_type(mz_schema_t);
  *schema = (mz_schema_t) {
    .kind = MZ_SCHEMA_S32,
    .as = {
      .s32_range = {
        .min = min,
        .max = max,
      },
    },
  };
  return schema;
}

mz_schema_t* mz_schema_s32() {
  return mz_schema_s32_ex(SP_LIMIT_S32_MIN, SP_LIMIT_S32_MAX);
}

mz_schema_t* mz_schema_u64_ex(u64 min, u64 max) {
  mz_schema_t* schema = sp_alloc_type(mz_schema_t);
  *schema = (mz_schema_t) {
    .kind = MZ_SCHEMA_U64,
    .as = {
      .u64_range = {
        .min = min,
        .max = max,
      },
    },
  };
  return schema;
}

mz_schema_t* mz_schema_u64() {
  return mz_schema_u64_ex(0, SP_LIMIT_U64_MAX);
}

mz_schema_t* mz_schema_f64_ex(f64 min, f64 max) {
  mz_schema_t* schema = sp_alloc_type(mz_schema_t);
  *schema = (mz_schema_t) {
    .kind = MZ_SCHEMA_F64,
    .as = {
      .f64_range = {
        .min = min,
        .max = max,
      },
    },
  };
  return schema;
}

mz_schema_t* mz_schema_f64() {
  return mz_schema_f64_ex(-SP_LIMIT_F64_MAX, SP_LIMIT_F64_MAX);
}

mz_schema_t* mz_schema_array_ex(mz_schema_t* element, u32 min_len, u32 max_len) {
  return mz_schema_array_ex_internal(element, min_len, max_len, 0);
}

mz_schema_t* mz_schema_array(mz_schema_t* element) {
  return mz_schema_array_ex(element, 0, SP_LIMIT_U32_MAX);
}

mz_schema_t* mz_schema_map(mz_schema_t* value, mz_map_insert_fn_t on_insert) {
  SP_ASSERT(value);

  mz_schema_t* schema = sp_alloc_type(mz_schema_t);
  *schema = (mz_schema_t) {
    .kind = MZ_SCHEMA_MAP,
    .as = {
      .map = {
        .value = value,
        .on_insert = on_insert,
      },
    },
  };
  return schema;
}

mz_schema_t* mz_schema_tagged_union(const c8* key, mz_tag_kind_t kind) {
  SP_ASSERT(key);

  mz_schema_t* schema = sp_alloc_type(mz_schema_t);
  *schema = (mz_schema_t) {
    .kind = MZ_SCHEMA_TAGGED,
    .as = {
      .tagged = {
        .key = sp_cstr_copy(key),
        .kind = kind,
      },
    },
  };
  return schema;
}

mz_tag_value_t mz_tag_str(const c8* str) {
  SP_ASSERT(str);
  return (mz_tag_value_t) {
    .kind = MZ_TAG_KIND_STR,
    .as.str = str,
  };
}

mz_tag_value_t mz_tag_s32(s32 s32) {
  return (mz_tag_value_t) {
    .kind = MZ_TAG_KIND_S32,
    .as.s32 = s32,
  };
}

mz_tag_value_t mz_tag_u64(u64 u64) {
  return (mz_tag_value_t) {
    .kind = MZ_TAG_KIND_U64,
    .as.u64 = u64,
  };
}

void mz_tagged_union_add(mz_schema_t* tagged, mz_tag_value_t tag, mz_schema_t* schema) {
  SP_ASSERT(tagged);
  SP_ASSERT(tagged->kind == MZ_SCHEMA_TAGGED);
  SP_ASSERT(schema);

  SP_ASSERT(tagged->as.tagged.kind == tag.kind);
  if (tagged->as.tagged.kind != tag.kind) {
    return;
  }

  if (schema->kind == MZ_SCHEMA_OBJECT) {
    mz_field_t* found = mz_object_find_field(schema, tagged->as.tagged.key);
    if (!found) {
      mz_schema_t* tag_schema = SP_NULLPTR;
      switch (tagged->as.tagged.kind) {
        case MZ_TAG_KIND_STR: {
          tag_schema = mz_schema_string();
          break;
        }
        case MZ_TAG_KIND_S32: {
          tag_schema = mz_schema_s32();
          break;
        }
        case MZ_TAG_KIND_U64: {
          tag_schema = mz_schema_u64();
          break;
        }
      }

      SP_ASSERT(tag_schema);
      if (tag_schema) {
        mz_object_add_field(schema, tagged->as.tagged.key, tag_schema, true, MZ_NO_OFFSET);
      }
    }
  }

  switch (tag.kind) {
    case MZ_TAG_KIND_STR: {
      tag.as.str = sp_cstr_copy(tag.as.str); break;
    }
    case MZ_TAG_KIND_U64:
    case MZ_TAG_KIND_S32: {
      break;
    }
  }

  mz_tag_case_t c = {
    .tag = tag,
    .schema = schema,
  };
  sp_da_push(tagged->as.tagged.cases, c);
}

void mz_object_add_field(mz_schema_t* object, const c8* key, mz_schema_t* field, bool required, u32 offset) {
  SP_ASSERT(object);
  SP_ASSERT(object->kind == MZ_SCHEMA_OBJECT);
  SP_ASSERT(key);
  SP_ASSERT(field);

  mz_field_t entry = {
    .key = sp_cstr_copy(key),
    .schema = field,
    .required = required,
    .offset = offset,
    .is_ptr = false,
  };
  sp_da_push(object->as.object.fields, entry);
  mz_object_rebuild_index(object);
}

void mz_object_add_field_ptr(mz_schema_t* object, const c8* key, mz_schema_t* field, bool required, u32 offset, u32 ptr_size, mz_field_ptr_fn_t on_resolve) {
  SP_ASSERT(object);
  SP_ASSERT(object->kind == MZ_SCHEMA_OBJECT);
  SP_ASSERT(key);
  SP_ASSERT(field);

  mz_field_t entry = {
    .key = sp_cstr_copy(key),
    .schema = field,
    .required = required,
    .offset = offset,
    .is_ptr = true,
    .ptr_size = ptr_size,
    .on_resolve = on_resolve,
  };
  sp_da_push(object->as.object.fields, entry);
  mz_object_rebuild_index(object);
}

mz_builder_t mz_builder_begin() {
  mz_builder_t builder = SP_ZERO_INITIALIZE();
  return builder;
}

mz_schema_t* mz_builder_end(mz_builder_t* builder) {
  SP_ASSERT(builder);
  SP_ASSERT(builder->depth == 0);
  return builder->root;
}

void mz_builder_push_object(mz_builder_t* builder, const c8* key, mz_object_mode_t mode, bool required, u32 offset) {
  SP_ASSERT(builder);
  SP_ASSERT(builder->depth < SP_CARR_LEN(builder->stack));

  mz_schema_t* object = mz_schema_object(mode);
  if (builder->depth == 0) {
    builder->root = object;
  }
  else {
    SP_ASSERT(key);
    mz_builder_add_field(builder, key, object, required, offset);
  }

  builder->stack[builder->depth++] = object;
}

void mz_builder_push_object_ptr(mz_builder_t* builder, const c8* key, mz_object_mode_t mode, bool required, u32 offset, u32 ptr_size, mz_field_ptr_fn_t on_resolve) {
  SP_ASSERT(builder);
  SP_ASSERT(builder->depth < SP_CARR_LEN(builder->stack));

  mz_schema_t* object = mz_schema_object(mode);
  if (builder->depth == 0) {
    builder->root = object;
  }
  else {
    SP_ASSERT(key);
    mz_builder_add_field_ptr(builder, key, object, required, offset, ptr_size, on_resolve);
  }

  builder->stack[builder->depth++] = object;
}

void mz_builder_push_array_object(mz_builder_t* builder, const c8* key, mz_object_mode_t element_mode, bool required, u32 offset, u32 elem_size) {
  SP_ASSERT(builder);
  SP_ASSERT(builder->depth > 0);
  SP_ASSERT(builder->depth < SP_CARR_LEN(builder->stack));
  SP_ASSERT(key);
  SP_ASSERT(elem_size > 0);

  mz_schema_t* element_object = mz_schema_object(element_mode);
  mz_schema_t* array_schema = mz_schema_array_ex_internal(element_object, 0, SP_LIMIT_U32_MAX, elem_size);
  mz_builder_add_field(builder, key, array_schema, required, offset);
  builder->stack[builder->depth++] = element_object;
}

void mz_builder_push_map_object(mz_builder_t* builder, const c8* key, mz_map_insert_fn_t on_insert, mz_object_mode_t value_mode, bool required, u32 offset) {
  SP_ASSERT(builder);
  SP_ASSERT(builder->depth > 0);
  SP_ASSERT(builder->depth < SP_CARR_LEN(builder->stack));
  SP_ASSERT(key);

  mz_schema_t* value_object = mz_schema_object(value_mode);
  mz_schema_t* map_schema = mz_schema_map(value_object, on_insert);
  mz_builder_add_field(builder, key, map_schema, required, offset);
  builder->stack[builder->depth++] = value_object;
}

void mz_builder_add_field(mz_builder_t* builder, const c8* key, mz_schema_t* field, bool required, u32 offset) {
  SP_ASSERT(builder);
  SP_ASSERT(builder->depth > 0);
  mz_schema_t* current = builder->stack[builder->depth - 1];
  mz_object_add_field(current, key, field, required, offset);
}

void mz_builder_add_field_ptr(mz_builder_t* builder, const c8* key, mz_schema_t* field, bool required, u32 offset, u32 ptr_size, mz_field_ptr_fn_t on_resolve) {
  SP_ASSERT(builder);
  SP_ASSERT(builder->depth > 0);
  mz_schema_t* current = builder->stack[builder->depth - 1];
  mz_object_add_field_ptr(current, key, field, required, offset, ptr_size, on_resolve);
}

void mz_builder_pop(mz_builder_t* builder) {
  SP_ASSERT(builder);
  SP_ASSERT(builder->depth > 0);
  builder->depth--;
}

void mz_schema_free(mz_schema_t* schema) {
  mz_schema_free_internal(schema);
}

void mz_parse_ctx_init_ex(mz_ctx_t* ctx, sp_allocator_t allocator, u32 arena_block_size) {
  SP_ASSERT(ctx);

  *ctx = (mz_ctx_t) {
    .allocator = allocator,
    .arena_block_size = arena_block_size ? arena_block_size : SP_MEM_ARENA_BLOCK_SIZE,
    .diag = {
      .kind = MZ_OK,
    },
  };

  sp_context_push_allocator(ctx->allocator);
  ctx->arena = sp_mem_arena_new(ctx->arena_block_size);
  sp_context_pop();
}

void mz_parse_ctx_init(mz_ctx_t* ctx) {
  SP_ASSERT(ctx);
  mz_parse_ctx_init_ex(ctx, sp_context_get()->allocator, SP_MEM_ARENA_BLOCK_SIZE);
}

void mz_parse_ctx_clear(mz_ctx_t* ctx) {
  SP_ASSERT(ctx);
  SP_ASSERT(ctx->arena);
  sp_mem_arena_clear(ctx->arena);
  mz_diag_reset(ctx);
}

void mz_parse_ctx_destroy(mz_ctx_t* ctx) {
  SP_ASSERT(ctx);
  if (!ctx->arena) {
    return;
  }

  sp_mem_arena_destroy(ctx->arena);
  ctx->arena = SP_NULLPTR;
}

static void mz_diag_reset(mz_ctx_t* ctx) {
  if (!ctx) {
    return;
  }

  ctx->diag = (mz_diag_t) {
    .kind = MZ_OK,
    .path = SP_NULLPTR,
    .message = SP_NULLPTR,
  };

  ctx->diag_parts_len = 0;
  ctx->diag_path[0] = '\0';
}

static mz_err_t mz_run_ex(mz_schema_t* schema, const mz_json_cursor_t* cursor, void* out, mz_ctx_t* parse_ctx) {
  mz_diag_reset(parse_ctx);

  if (!schema) {
    mz_err_t err = mz_diag_set(parse_ctx, MZ_ERR_TYPE);
    mz_diag_finalize(parse_ctx);
    return err;
  }

  if (!cursor || mz_json_cursor_kind(cursor) == MZ_JSON_KIND_INVALID) {
    mz_err_t err = mz_diag_set(parse_ctx, MZ_ERR_TYPE);
    mz_diag_finalize(parse_ctx);
    return err;
  }

  sp_context_push_allocator(sp_mem_arena_as_allocator(parse_ctx->arena));
  mz_err_t err = mz_eval_schema(parse_ctx, schema, cursor, out);
  sp_context_pop();

  if (err != MZ_OK) {
    mz_diag_finalize(parse_ctx);
  }

  return err;
}

mz_err_t mz_validate_ex(mz_schema_t* schema, const mz_json_cursor_t* cursor, mz_ctx_t* ctx) {
  SP_ASSERT(ctx);
  return mz_run_ex(schema, cursor, SP_NULLPTR, ctx);
}

mz_err_t mz_parse_ex(mz_schema_t* schema, const mz_json_cursor_t* cursor, void* out, mz_ctx_t* ctx) {
  SP_ASSERT(ctx);
  return mz_run_ex(schema, cursor, out, ctx);
}

mz_err_t mz_validate(mz_schema_t* schema, const mz_json_cursor_t* cursor) {
  mz_ctx_t ctx = SP_ZERO_INITIALIZE();
  mz_parse_ctx_init(&ctx);
  mz_err_t err = mz_validate_ex(schema, cursor, &ctx);
  mz_parse_ctx_destroy(&ctx);
  return err;
}

mz_err_t mz_parse(mz_schema_t* schema, const mz_json_cursor_t* cursor, void* out) {
  mz_ctx_t ctx = SP_ZERO_INITIALIZE();
  mz_parse_ctx_init(&ctx);
  mz_err_t err = mz_parse_ex(schema, cursor, out, &ctx);
  mz_parse_ctx_destroy(&ctx);
  return err;
}

static mz_err_t mz_load_json_source(const c8* source, bool from_file, mz_json_doc_t** out_doc) {
  return mz_json_doc_parse_source(source, from_file, out_doc);
}

static mz_err_t mz_load_json_source_ex(const c8* source, bool from_file, mz_json_doc_t** out_doc, mz_ctx_t* ctx) {
  SP_ASSERT(ctx);

  mz_diag_reset(ctx);

  if (ctx->max_input_bytes) {
    if (!from_file) {
      u32 source_len = (u32)strlen(source);
      if (source_len > ctx->max_input_bytes) {
        mz_err_t err = mz_diag_set(ctx, MZ_ERR_LIMIT);
        mz_diag_finalize(ctx);
        return err;
      }
    }
    else {
      struct stat st = {0};
      if (stat(source, &st) == 0 && (u64)st.st_size > ctx->max_input_bytes) {
        mz_err_t err = mz_diag_set(ctx, MZ_ERR_LIMIT);
        mz_diag_finalize(ctx);
        return err;
      }
    }
  }

  mz_err_t err = mz_json_doc_parse_source(source, from_file, out_doc);
  if (err != MZ_OK) {
    mz_err_t diag_err = mz_diag_set(ctx, MZ_ERR_JSON);
    mz_diag_finalize(ctx);
    return diag_err;
  }

  return MZ_OK;
}

mz_backend_t mz_query_backend(void) {
#if defined(MZ_BACKEND_JANSSON)
  return MZ_BACKEND_KIND_JANSSON;
#elif defined(MZ_BACKEND_CJSON)
  return MZ_BACKEND_KIND_CJSON;
#elif defined(MZ_BACKEND_CUSTOM)
  return MZ_BACKEND_KIND_CUSTOM;
#else
  SP_ASSERT(false);
  return MZ_BACKEND_KIND_CUSTOM;
#endif
}

mz_err_t mz_validate_str(mz_schema_t* schema, const c8* json_source) {
  mz_json_doc_t* doc = SP_NULLPTR;
  mz_err_t err = mz_load_json_source(json_source, false, &doc);
  if (err != MZ_OK) {
    return err;
  }

  mz_json_cursor_t root = mz_json_doc_root_cursor(doc);
  err = mz_validate(schema, &root);
  mz_json_doc_release(doc);
  return err;
}

mz_err_t mz_validate_str_ex(mz_schema_t* schema, const c8* json_source, mz_ctx_t* ctx) {
  SP_ASSERT(ctx);

  mz_json_doc_t* doc = SP_NULLPTR;
  mz_err_t err = mz_load_json_source_ex(json_source, false, &doc, ctx);
  if (err != MZ_OK) {
    return err;
  }

  mz_json_cursor_t root = mz_json_doc_root_cursor(doc);
  err = mz_validate_ex(schema, &root, ctx);
  mz_json_doc_release(doc);
  return err;
}

mz_err_t mz_validate_file(mz_schema_t* schema, const c8* file_path) {
  mz_json_doc_t* doc = SP_NULLPTR;
  mz_err_t err = mz_load_json_source(file_path, true, &doc);
  if (err != MZ_OK) {
    return err;
  }

  mz_json_cursor_t root = mz_json_doc_root_cursor(doc);
  err = mz_validate(schema, &root);
  mz_json_doc_release(doc);
  return err;
}

mz_err_t mz_parse_str(mz_schema_t* schema, const c8* json_source, void* out) {
  mz_json_doc_t* doc = SP_NULLPTR;
  mz_err_t err = mz_load_json_source(json_source, false, &doc);
  if (err != MZ_OK) {
    return err;
  }

  mz_json_cursor_t root = mz_json_doc_root_cursor(doc);
  err = mz_parse(schema, &root, out);
  mz_json_doc_release(doc);
  return err;
}

mz_err_t mz_parse_str_ex(mz_schema_t* schema, const c8* json_source, void* out, mz_ctx_t* ctx) {
  SP_ASSERT(ctx);

  mz_json_doc_t* doc = SP_NULLPTR;
  mz_err_t err = mz_load_json_source_ex(json_source, false, &doc, ctx);
  if (err != MZ_OK) {
    return err;
  }

  mz_json_cursor_t root = mz_json_doc_root_cursor(doc);
  err = mz_parse_ex(schema, &root, out, ctx);
  mz_json_doc_release(doc);
  return err;
}

mz_err_t mz_parse_file(mz_schema_t* schema, const c8* file_path, void* out) {
  mz_json_doc_t* doc = SP_NULLPTR;
  mz_err_t err = mz_load_json_source(file_path, true, &doc);
  if (err != MZ_OK) {
    return err;
  }

  mz_json_cursor_t root = mz_json_doc_root_cursor(doc);
  err = mz_parse(schema, &root, out);
  mz_json_doc_release(doc);
  return err;
}
