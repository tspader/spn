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
typedef struct mz_obj_it_t mz_obj_it_t;
typedef struct mz_arr_it_t mz_arr_it_t;

#ifdef SP_CPP
#define MZ_ZERO_LOCAL(type) type{}
#else
#define MZ_ZERO_LOCAL(type) (type){0}
#endif

static mz_err_t mz_backend_load_file(mz_nstr_t path, mz_json_doc_t** result);
static mz_err_t mz_backend_load_str(mz_nstr_t json, mz_json_doc_t** result);
static void mz_backend_free_document(mz_json_doc_t* doc);
static mz_json_cursor_t mz_backend_get_root(const mz_json_doc_t* doc);
static mz_json_kind_t mz_backend_get_value_kind(const mz_json_cursor_t* cursor);
static mz_err_t mz_backend_get_bool(const mz_json_cursor_t* cursor, bool* value);
static mz_err_t mz_backend_get_string(const mz_json_cursor_t* cursor, const c8** result);
static mz_err_t mz_backend_get_s64_exact(const mz_json_cursor_t* cursor, s64* result);
static mz_err_t mz_backend_get_u64_exact(const mz_json_cursor_t* cursor, u64* result);
static mz_err_t mz_backend_get_f64(const mz_json_cursor_t* cursor, f64* result);
static mz_obj_it_t mz_backend_obj_it_begin(const mz_json_cursor_t* cursor);
static bool mz_backend_obj_it_next(mz_obj_it_t* it, const c8** key, mz_json_cursor_t* cursor);
static mz_arr_it_t mz_backend_arr_it_begin(const mz_json_cursor_t* cursor);
static bool        mz_backend_arr_it_next(mz_arr_it_t* it, mz_json_cursor_t* cursor, u32* index);

#if !defined(MZ_BACKEND_JANSSON) && !defined(MZ_BACKEND_CJSON) && !defined(MZ_BACKEND_YYJSON) && !defined(MZ_BACKEND_RAPIDJSON) && !defined(MZ_BACKEND_GLAZE) && !defined(MZ_BACKEND_SIMDJSON) && !defined(MZ_BACKEND_CUSTOM)
#error "Please select a backend with MZ_BACKEND_JANSSON, MZ_BACKEND_CJSON, MZ_BACKEND_YYJSON, MZ_BACKEND_RAPIDJSON, MZ_BACKEND_GLAZE, MZ_BACKEND_SIMDJSON or MZ_BACKEND_CUSTOM"
#endif

#if ((defined(MZ_BACKEND_JANSSON) ? 1 : 0) + \
     (defined(MZ_BACKEND_CJSON) ? 1 : 0) + \
     (defined(MZ_BACKEND_YYJSON) ? 1 : 0) + \
     (defined(MZ_BACKEND_RAPIDJSON) ? 1 : 0) + \
     (defined(MZ_BACKEND_GLAZE) ? 1 : 0) + \
     (defined(MZ_BACKEND_SIMDJSON) ? 1 : 0) + \
     (defined(MZ_BACKEND_CUSTOM) ? 1 : 0)) != 1
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

struct mz_obj_it_t {
  json_t* object;
  void* iter;
};

struct mz_arr_it_t {
  json_t* array;
  u32 index;
  u32 len;
};

static mz_json_cursor_t mz_backend_get_root(const mz_json_doc_t* doc) {
  return (mz_json_cursor_t) { .value = doc->root };
}

static mz_json_kind_t mz_backend_get_value_kind(const mz_json_cursor_t* cursor) {
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

static mz_err_t mz_backend_get_bool(const mz_json_cursor_t* cursor, bool* value) {
  if (!json_is_boolean(cursor->value)) return MZ_ERR_TYPE;
  *value = json_is_true(cursor->value);
  return MZ_OK;
}

static mz_err_t mz_backend_get_string(const mz_json_cursor_t* cursor, const c8** result) {
  if (!json_is_string(cursor->value)) return MZ_ERR_TYPE;
  *result = json_string_value(cursor->value);
  return *result ? MZ_OK : MZ_ERR_JSON;
}

static mz_err_t mz_backend_get_s64_exact(const mz_json_cursor_t* cursor, s64* result) {
  if (!json_is_integer(cursor->value)) return MZ_ERR_TYPE;
  *result = (s64)json_integer_value(cursor->value);
  return MZ_OK;
}

static mz_err_t mz_backend_get_u64_exact(const mz_json_cursor_t* cursor, u64* result) {
  if (!json_is_integer(cursor->value)) return MZ_ERR_TYPE;
  s64 raw = (s64)json_integer_value(cursor->value);
  if (raw < 0) {
    return MZ_ERR_RANGE;
  }

  *result = (u64)raw;
  return MZ_OK;
}

static mz_err_t mz_backend_get_f64(const mz_json_cursor_t* cursor, f64* result) {
  if (!json_is_real(cursor->value) && !json_is_integer(cursor->value)) return MZ_ERR_TYPE;
  *result = (f64)json_number_value(cursor->value);
  return MZ_OK;
}

static mz_obj_it_t mz_backend_obj_it_begin(const mz_json_cursor_t* cursor) {
  mz_obj_it_t it = MZ_ZERO_LOCAL(mz_obj_it_t);
  if (!json_is_object(cursor->value)) return it;
  it.object = cursor->value;
  it.iter = json_object_iter(cursor->value);
  return it;
}

static bool mz_backend_obj_it_next(mz_obj_it_t* it, const c8** key, mz_json_cursor_t* cursor) {
  if (!it->iter) return false;
  json_t* object = (json_t*)it->object;
  void* iter = it->iter;

  *key = json_object_iter_key(iter);
  cursor->value = json_object_iter_value(iter);
  it->iter = json_object_iter_next(object, iter);
  return true;
}

static mz_arr_it_t mz_backend_arr_it_begin(const mz_json_cursor_t* cursor) {
  mz_arr_it_t it = MZ_ZERO_LOCAL(mz_arr_it_t);
  if (!json_is_array(cursor->value)) return it;
  it.array = cursor->value;
  it.index = 0;
  it.len = (u32)json_array_size(cursor->value);
  return it;
}

static bool mz_backend_arr_it_next(mz_arr_it_t* it, mz_json_cursor_t* cursor, u32* index) {
  if (it->index >= it->len) return false;
  json_t* child = json_array_get((json_t*)it->array, it->index);
  if (!child) {
    return false;
  }

  cursor->value = child;
  if (index) {
    *index = it->index;
  }
  it->index++;
  return true;
}

static mz_err_t mz_backend_load_str(mz_nstr_t json, mz_json_doc_t** result) {
  json_error_t error = SP_ZERO_INITIALIZE();
  json_t* root = json_loadb(json.data, (size_t)json.len, 0, &error);
  if (!root) {
    return MZ_ERR_JSON;
  }

  mz_json_doc_t* doc = sp_alloc_type(mz_json_doc_t);
  doc->root = root;
  *result = doc;
  return MZ_OK;
}

static mz_err_t mz_backend_load_file(mz_nstr_t path, mz_json_doc_t** result) {
  json_error_t error = SP_ZERO_INITIALIZE();
  json_t* root = json_load_file(path.data, 0, &error);
  if (!root) {
    return MZ_ERR_JSON;
  }

  mz_json_doc_t* doc = sp_alloc_type(mz_json_doc_t);
  doc->root = root;
  *result = doc;
  return MZ_OK;
}

static void mz_backend_free_document(mz_json_doc_t* doc) {
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

struct mz_obj_it_t {
  cJSON* iter;
};

struct mz_arr_it_t {
  cJSON* iter;
  u32 index;
};

static mz_json_cursor_t mz_backend_get_root(const mz_json_doc_t* doc) {
  return (mz_json_cursor_t) { .value = doc->root };
}

static mz_err_t mz_json_number_to_s64_exact(const mz_json_cursor_t* cursor, s64* result) {
  if (!cJSON_IsNumber(cursor->value)) return MZ_ERR_TYPE;
  f64 raw = cJSON_GetNumberValue(cursor->value);
  if (raw != raw) {
    return MZ_ERR_JSON;
  }

  if (raw < (f64)SP_LIMIT_S64_MIN || raw > (f64)SP_LIMIT_S64_MAX) {
    return MZ_ERR_RANGE;
  }

  s64 parsed = (s64)raw;
  if ((f64)parsed != raw) {
    return MZ_ERR_TYPE;
  }

  *result = parsed;
  return MZ_OK;
}

static mz_err_t mz_json_number_to_u64_exact(const mz_json_cursor_t* cursor, u64* result) {
  if (!cJSON_IsNumber(cursor->value)) return MZ_ERR_TYPE;
  f64 raw = cJSON_GetNumberValue(cursor->value);
  if (raw != raw) {
    return MZ_ERR_JSON;
  }

  if (raw < 0) {
    return MZ_ERR_RANGE;
  }

  if (raw > (f64)SP_LIMIT_U64_MAX) {
    return MZ_ERR_RANGE;
  }

  u64 parsed = (u64)raw;
  if ((f64)parsed != raw) {
    return MZ_ERR_TYPE;
  }

  *result = parsed;
  return MZ_OK;
}

static mz_json_kind_t mz_backend_get_value_kind(const mz_json_cursor_t* cursor) {
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
    if (mz_json_number_to_s64_exact(cursor, mz_nullptr) == MZ_OK) {
      return MZ_JSON_KIND_INTEGER;
    }
    return MZ_JSON_KIND_REAL;
  }
  if (cJSON_IsNull(value)) {
    return MZ_JSON_KIND_NULL;
  }

  return MZ_JSON_KIND_INVALID;
}

static mz_err_t mz_backend_get_bool(const mz_json_cursor_t* cursor, bool* value) {
  if (!cJSON_IsBool(cursor->value)) return MZ_ERR_TYPE;
  *value = cJSON_IsTrue(cursor->value);
  return MZ_OK;
}

static mz_err_t mz_backend_get_string(const mz_json_cursor_t* cursor, const c8** result) {
  const c8* str = cJSON_GetStringValue(cursor->value);
  if (!str) {
    return MZ_ERR_TYPE;
  }

  *result = str;
  return MZ_OK;
}

static mz_err_t mz_backend_get_s64_exact(const mz_json_cursor_t* cursor, s64* result) {
  return mz_json_number_to_s64_exact(cursor, result);
}

static mz_err_t mz_backend_get_u64_exact(const mz_json_cursor_t* cursor, u64* result) {
  return mz_json_number_to_u64_exact(cursor, result);
}

static mz_err_t mz_backend_get_f64(const mz_json_cursor_t* cursor, f64* result) {
  if (!cJSON_IsNumber(cursor->value)) return MZ_ERR_TYPE;
  *result = cJSON_GetNumberValue(cursor->value);
  return MZ_OK;
}

static cJSON* mz_json_cjson_next_entry(cJSON* entry) {
  while (entry && !entry->string) {
    entry = entry->next;
  }

  return entry;
}

static mz_obj_it_t mz_backend_obj_it_begin(const mz_json_cursor_t* cursor) {
  mz_obj_it_t it = MZ_ZERO_LOCAL(mz_obj_it_t);
  if (!cJSON_IsObject(cursor->value)) return it;
  it.iter = mz_json_cjson_next_entry(cursor->value->child);
  return it;
}

static bool mz_backend_obj_it_next(mz_obj_it_t* it, const c8** key, mz_json_cursor_t* cursor) {
  if (!it->iter) return false;
  cJSON* entry = (cJSON*)it->iter;
  *key = entry->string;
  cursor->value = entry;
  it->iter = mz_json_cjson_next_entry(entry->next);
  return *key != mz_nullptr;
}

static mz_arr_it_t mz_backend_arr_it_begin(const mz_json_cursor_t* cursor) {
  mz_arr_it_t it = MZ_ZERO_LOCAL(mz_arr_it_t);
  if (!cJSON_IsArray(cursor->value)) return it;
  it.iter = cursor->value->child;
  it.index = 0;
  return it;
}

static bool mz_backend_arr_it_next(mz_arr_it_t* it, mz_json_cursor_t* cursor, u32* index) {
  if (!it->iter) return false;
  cJSON* entry = (cJSON*)it->iter;
  cursor->value = entry;
  if (index) {
    *index = it->index;
  }

  it->iter = entry->next;
  it->index++;
  return true;
}

static mz_err_t mz_json_cjson_read_file(const c8* path, c8** result) {
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
  *result = buffer;
  return MZ_OK;
}

static mz_err_t mz_backend_load_str(mz_nstr_t json, mz_json_doc_t** result) {
  cJSON* root = cJSON_ParseWithOpts(json.data, mz_nullptr, true);
  if (!root) {
    return MZ_ERR_JSON;
  }

  mz_json_doc_t* doc = sp_alloc_type(mz_json_doc_t);
  doc->root = root;
  *result = doc;
  return MZ_OK;
}

static mz_err_t mz_backend_load_file(mz_nstr_t path, mz_json_doc_t** result) {
  c8* json = mz_nullptr;
  mz_try(mz_json_cjson_read_file(path.data, &json));

  cJSON* root = cJSON_ParseWithOpts(json, mz_nullptr, true);
  sp_free(json);
  if (!root) {
    return MZ_ERR_JSON;
  }

  mz_json_doc_t* doc = sp_alloc_type(mz_json_doc_t);
  doc->root = root;
  *result = doc;
  return MZ_OK;
}

static void mz_backend_free_document(mz_json_doc_t* doc) {
  if (doc->root) {
    cJSON_Delete(doc->root);
  }

  sp_free(doc);
}

#elif defined(MZ_BACKEND_YYJSON)
#include "yyjson.h"

struct mz_json_doc_t {
  yyjson_doc* root;
};

struct mz_json_cursor_t {
  yyjson_val* value;
};

struct mz_obj_it_t {
  yyjson_obj_iter value;
};

struct mz_arr_it_t {
  yyjson_arr_iter value;
};

static mz_json_cursor_t mz_backend_get_root(const mz_json_doc_t* doc) {
  return (mz_json_cursor_t) {
    .value = yyjson_doc_get_root(doc->root)
  };
}

static mz_json_kind_t mz_backend_get_value_kind(const mz_json_cursor_t* cursor) {
  switch (unsafe_yyjson_get_type(cursor->value)) {
    case YYJSON_TYPE_NULL: return MZ_JSON_KIND_NULL;
    case YYJSON_TYPE_BOOL: return MZ_JSON_KIND_BOOL;
    case YYJSON_TYPE_STR: return MZ_JSON_KIND_STRING;
    case YYJSON_TYPE_ARR: return MZ_JSON_KIND_ARRAY;
    case YYJSON_TYPE_OBJ: return MZ_JSON_KIND_OBJECT;
    case YYJSON_TYPE_NUM: {
      switch (unsafe_yyjson_get_subtype(cursor->value)) {
        case YYJSON_SUBTYPE_REAL: return MZ_JSON_KIND_REAL;
        case YYJSON_SUBTYPE_UINT:
        case YYJSON_SUBTYPE_SINT: return MZ_JSON_KIND_INTEGER;
      }
    }
    case YYJSON_TYPE_NONE:
    case YYJSON_TYPE_RAW: return MZ_JSON_KIND_INVALID;
  }

  return MZ_JSON_KIND_INVALID;
}

static mz_err_t mz_backend_get_bool(const mz_json_cursor_t* cursor, bool* value) {
  if (!yyjson_is_bool(cursor->value)) return MZ_ERR_TYPE;
  *value = yyjson_get_bool(cursor->value);
  return MZ_OK;
}

static mz_err_t mz_backend_get_string(const mz_json_cursor_t* cursor, const c8** result) {
  if (!yyjson_is_str(cursor->value)) return MZ_ERR_TYPE;
  *result = yyjson_get_str(cursor->value);
  return *result ? MZ_OK : MZ_ERR_JSON;
}

static mz_err_t mz_backend_get_s64_exact(const mz_json_cursor_t* cursor, s64* result) {
  if (yyjson_is_sint(cursor->value)) {
    *result = yyjson_get_sint(cursor->value);
    return MZ_OK;
  }
  if (yyjson_is_uint(cursor->value)) {
    u64 raw = yyjson_get_uint(cursor->value);
    if (raw > (u64)SP_LIMIT_S64_MAX) {
      return MZ_ERR_RANGE;
    }
    *result = (s64)raw;
    return MZ_OK;
  }
  if (yyjson_is_num(cursor->value)) {
    return MZ_ERR_TYPE;
  }

  return MZ_ERR_TYPE;
}

static mz_err_t mz_backend_get_u64_exact(const mz_json_cursor_t* cursor, u64* result) {
  if (yyjson_is_uint(cursor->value)) {
    *result = yyjson_get_uint(cursor->value);
    return MZ_OK;
  }
  if (yyjson_is_sint(cursor->value)) {
    s64 raw = (s64)yyjson_get_sint(cursor->value);
    if (raw < 0) {
      return MZ_ERR_RANGE;
    }
    *result = (u64)raw;
    return MZ_OK;
  }
  if (yyjson_is_num(cursor->value)) {
    return MZ_ERR_TYPE;
  }

  return MZ_ERR_TYPE;
}

static mz_err_t mz_backend_get_f64(const mz_json_cursor_t* cursor, f64* result) {
  if (!yyjson_is_num(cursor->value)) return MZ_ERR_TYPE;
  *result = yyjson_get_num(cursor->value);
  return MZ_OK;
}

static mz_obj_it_t mz_backend_obj_it_begin(const mz_json_cursor_t* cursor) {
  mz_obj_it_t it = mz_zero;

  if (!yyjson_is_obj(cursor->value)) return it;
  it.value = yyjson_obj_iter_with(cursor->value);
  return it;
}

static bool mz_backend_obj_it_next(mz_obj_it_t* it, const c8** key, mz_json_cursor_t* value) {
  yyjson_val* next = yyjson_obj_iter_next(&it->value);
  if (!next) {
    return false;
  }

  *key = yyjson_get_str(next);
  value->value = yyjson_obj_iter_get_val(next);
  return *key && value->value;
}

static mz_arr_it_t mz_backend_arr_it_begin(const mz_json_cursor_t* cursor) {
  mz_arr_it_t it = mz_zero;

  if (!yyjson_is_arr(cursor->value)) return it;
  it.value = yyjson_arr_iter_with(cursor->value);
  return it;
}

static bool mz_backend_arr_it_next(mz_arr_it_t* it, mz_json_cursor_t* cursor, u32* index) {
  u64 idx = it->value.idx;
  yyjson_val* child = yyjson_arr_iter_next(&it->value);
  if (!child) {
    return false;
  }

  cursor->value = child;
  if (index) {
    *index = (u32)idx;
  }
  return true;
}

static mz_err_t mz_backend_load_str(mz_nstr_t json, mz_json_doc_t** result) {
  yyjson_doc* root = yyjson_read(json.data, json.len, 0);
  if (!root) {
    return MZ_ERR_JSON;
  }

  *result = sp_alloc_type(mz_json_doc_t);
  (*result)->root = root;
  return MZ_OK;
}

static mz_err_t mz_backend_load_file(mz_nstr_t path, mz_json_doc_t** result) {
  yyjson_doc* root = yyjson_read_file(path.data, 0, mz_nullptr, mz_nullptr);
  if (!root) {
    return MZ_ERR_JSON;
  }

  *result = sp_alloc_type(mz_json_doc_t);
  (*result)->root = root;
  return MZ_OK;
}

static void mz_backend_free_document(mz_json_doc_t* doc) {
  if (doc->root) {
    yyjson_doc_free(doc->root);
  }

  sp_free(doc);
}

#elif defined(MZ_BACKEND_RAPIDJSON)
#ifndef SP_CPP
#error "MZ_BACKEND_RAPIDJSON requires C++ compilation"
#endif

#include <new>
#include <string>

#include "rapidjson/document.h"

struct mz_json_doc_t {
  rapidjson::Document root;
};

struct mz_json_cursor_t {
  const rapidjson::Value* value;
};

struct mz_obj_it_t {
  const rapidjson::Value* object;
  rapidjson::Value::ConstMemberIterator iter;
  rapidjson::Value::ConstMemberIterator end;
};

struct mz_arr_it_t {
  const rapidjson::Value* array;
  rapidjson::SizeType index;
};

static bool mz_json_rapidjson_read_file(const c8* path, std::string* result) {
  FILE* file = fopen(path, "rb");
  if (!file) {
    return false;
  }

  if (fseek(file, 0, SEEK_END) != 0) {
    fclose(file);
    return false;
  }

  long file_size = ftell(file);
  if (file_size < 0) {
    fclose(file);
    return false;
  }

  if (fseek(file, 0, SEEK_SET) != 0) {
    fclose(file);
    return false;
  }

  result->resize((size_t)file_size);
  size_t read_len = file_size > 0 ? fread(result->data(), 1, (size_t)file_size, file) : 0;
  fclose(file);

  return read_len == (size_t)file_size;
}

static mz_json_cursor_t mz_backend_get_root(const mz_json_doc_t* doc) {
  return (mz_json_cursor_t) { .value = &doc->root };
}

static mz_json_kind_t mz_backend_get_value_kind(const mz_json_cursor_t* cursor) {
  const rapidjson::Value& value = *cursor->value;
  if (value.IsObject()) {
    return MZ_JSON_KIND_OBJECT;
  }
  if (value.IsArray()) {
    return MZ_JSON_KIND_ARRAY;
  }
  if (value.IsString()) {
    return MZ_JSON_KIND_STRING;
  }
  if (value.IsBool()) {
    return MZ_JSON_KIND_BOOL;
  }
  if (value.IsNumber()) {
    return value.IsDouble() ? MZ_JSON_KIND_REAL : MZ_JSON_KIND_INTEGER;
  }
  if (value.IsNull()) {
    return MZ_JSON_KIND_NULL;
  }

  return MZ_JSON_KIND_INVALID;
}

static mz_err_t mz_backend_get_bool(const mz_json_cursor_t* cursor, bool* value) {
  if (!cursor->value->IsBool()) return MZ_ERR_TYPE;
  *value = cursor->value->GetBool();
  return MZ_OK;
}

static mz_err_t mz_backend_get_string(const mz_json_cursor_t* cursor, const c8** result) {
  if (!cursor->value->IsString()) return MZ_ERR_TYPE;
  *result = cursor->value->GetString();
  return MZ_OK;
}

static mz_err_t mz_backend_get_s64_exact(const mz_json_cursor_t* cursor, s64* result) {
  const rapidjson::Value& value = *cursor->value;
  if (value.IsInt64()) {
    *result = (s64)value.GetInt64();
    return MZ_OK;
  }
  if (value.IsUint64()) {
    u64 raw = (u64)value.GetUint64();
    if (raw > (u64)SP_LIMIT_S64_MAX) {
      return MZ_ERR_RANGE;
    }
    *result = (s64)raw;
    return MZ_OK;
  }
  if (value.IsNumber()) {
    return MZ_ERR_TYPE;
  }

  return MZ_ERR_TYPE;
}

static mz_err_t mz_backend_get_u64_exact(const mz_json_cursor_t* cursor, u64* result) {
  const rapidjson::Value& value = *cursor->value;
  if (value.IsUint64()) {
    *result = (u64)value.GetUint64();
    return MZ_OK;
  }
  if (value.IsInt64()) {
    s64 raw = (s64)value.GetInt64();
    if (raw < 0) {
      return MZ_ERR_RANGE;
    }
    *result = (u64)raw;
    return MZ_OK;
  }
  if (value.IsNumber()) {
    return MZ_ERR_TYPE;
  }

  return MZ_ERR_TYPE;
}

static mz_err_t mz_backend_get_f64(const mz_json_cursor_t* cursor, f64* result) {
  if (!cursor->value->IsNumber()) return MZ_ERR_TYPE;
  *result = (f64)cursor->value->GetDouble();
  return MZ_OK;
}

static mz_obj_it_t mz_backend_obj_it_begin(const mz_json_cursor_t* cursor) {
  mz_obj_it_t it = MZ_ZERO_LOCAL(mz_obj_it_t);
  if (!cursor->value->IsObject()) return it;
  it.object = cursor->value;
  it.iter = cursor->value->MemberBegin();
  it.end = cursor->value->MemberEnd();
  return it;
}

static bool mz_backend_obj_it_next(mz_obj_it_t* it, const c8** key, mz_json_cursor_t* cursor) {
  if (it->iter == it->end) return false;
  const rapidjson::Value::ConstMemberIterator entry = it->iter++;
  *key = entry->name.GetString();
  cursor->value = &entry->value;
  return true;
}

static mz_arr_it_t mz_backend_arr_it_begin(const mz_json_cursor_t* cursor) {
  mz_arr_it_t it = MZ_ZERO_LOCAL(mz_arr_it_t);
  if (!cursor->value->IsArray()) return it;
  it.array = cursor->value;
  it.index = 0;
  return it;
}

static bool mz_backend_arr_it_next(mz_arr_it_t* it, mz_json_cursor_t* cursor, u32* index) {
  if (it->index >= it->array->Size()) return false;
  cursor->value = &((*it->array)[it->index]);
  if (index) {
    *index = (u32)it->index;
  }
  it->index++;
  return true;
}

static mz_err_t mz_backend_load_str(mz_nstr_t json, mz_json_doc_t** result) {
  mz_json_doc_t* doc = new (std::nothrow) mz_json_doc_t();
  if (!doc) {
    return MZ_ERR_JSON;
  }

  std::string buffer;
  doc->root.Parse<rapidjson::kParseFullPrecisionFlag>(json.data, (size_t)json.len);
  if (doc->root.HasParseError()) {
    delete doc;
    return MZ_ERR_JSON;
  }

  *result = doc;
  return MZ_OK;
}

static mz_err_t mz_backend_load_file(mz_nstr_t path, mz_json_doc_t** result) {
  mz_json_doc_t* doc = new (std::nothrow) mz_json_doc_t();
  if (!doc) {
    return MZ_ERR_JSON;
  }

  std::string json;
  if (!mz_json_rapidjson_read_file(path.data, &json)) {
    delete doc;
    return MZ_ERR_JSON;
  }

  doc->root.Parse<rapidjson::kParseFullPrecisionFlag>(json.data(), json.size());
  if (doc->root.HasParseError()) {
    delete doc;
    return MZ_ERR_JSON;
  }

  *result = doc;
  return MZ_OK;
}

static void mz_backend_free_document(mz_json_doc_t* doc) {
  delete doc;
}

#elif defined(MZ_BACKEND_GLAZE)
#ifndef SP_CPP
#error "MZ_BACKEND_GLAZE requires C++ compilation"
#endif

#include <new>
#include <string>

#include "glaze/json/generic.hpp"

struct mz_json_doc_t {
  glz::generic_u64 root;
  std::string buffer;
};

struct mz_json_cursor_t {
  const glz::generic_u64* value;
};

struct mz_obj_it_t {
  const glz::generic_u64::object_t* object;
  glz::generic_u64::object_t::const_iterator iter;
  glz::generic_u64::object_t::const_iterator end;
};

struct mz_arr_it_t {
  const glz::generic_u64::array_t* array;
  size_t index;
};

static mz_json_cursor_t mz_backend_get_root(const mz_json_doc_t* doc) {
  return (mz_json_cursor_t) { .value = &doc->root };
}

static mz_json_kind_t mz_backend_get_value_kind(const mz_json_cursor_t* cursor) {
  const glz::generic_u64& value = *cursor->value;
  if (value.is_object()) {
    return MZ_JSON_KIND_OBJECT;
  }
  if (value.is_array()) {
    return MZ_JSON_KIND_ARRAY;
  }
  if (value.is_string()) {
    return MZ_JSON_KIND_STRING;
  }
  if (value.is_boolean()) {
    return MZ_JSON_KIND_BOOL;
  }
  if (value.is_number()) {
    return value.is_double() ? MZ_JSON_KIND_REAL : MZ_JSON_KIND_INTEGER;
  }
  if (value.is_null()) {
    return MZ_JSON_KIND_NULL;
  }

  return MZ_JSON_KIND_INVALID;
}

static mz_err_t mz_backend_get_bool(const mz_json_cursor_t* cursor, bool* value) {
  if (!cursor->value->is_boolean()) return MZ_ERR_TYPE;
  *value = cursor->value->get_boolean();
  return MZ_OK;
}

static mz_err_t mz_backend_get_string(const mz_json_cursor_t* cursor, const c8** result) {
  if (!cursor->value->is_string()) return MZ_ERR_TYPE;
  *result = cursor->value->get_string().c_str();
  return MZ_OK;
}

static mz_err_t mz_backend_get_s64_exact(const mz_json_cursor_t* cursor, s64* result) {
  if (cursor->value->is_int64()) {
    *result = (s64)cursor->value->template get<int64_t>();
    return MZ_OK;
  }
  if (cursor->value->is_uint64()) {
    u64 raw = (u64)cursor->value->template get<uint64_t>();
    if (raw > (u64)SP_LIMIT_S64_MAX) {
      return MZ_ERR_RANGE;
    }
    *result = (s64)raw;
    return MZ_OK;
  }
  if (cursor->value->is_number()) {
    return MZ_ERR_TYPE;
  }

  return MZ_ERR_TYPE;
}

static mz_err_t mz_backend_get_u64_exact(const mz_json_cursor_t* cursor, u64* result) {
  if (cursor->value->is_uint64()) {
    *result = (u64)cursor->value->template get<uint64_t>();
    return MZ_OK;
  }
  if (cursor->value->is_int64()) {
    s64 raw = (s64)cursor->value->template get<int64_t>();
    if (raw < 0) {
      return MZ_ERR_RANGE;
    }
    *result = (u64)raw;
    return MZ_OK;
  }
  if (cursor->value->is_number()) {
    return MZ_ERR_TYPE;
  }

  return MZ_ERR_TYPE;
}

static mz_err_t mz_backend_get_f64(const mz_json_cursor_t* cursor, f64* result) {
  if (!cursor->value->is_number()) return MZ_ERR_TYPE;
  if (cursor->value->is_uint64()) {
    *result = (f64)cursor->value->template get<uint64_t>();
  }
  else if (cursor->value->is_int64()) {
    *result = (f64)cursor->value->template get<int64_t>();
  }
  else {
    *result = (f64)cursor->value->template get<double>();
  }
  return MZ_OK;
}

static mz_obj_it_t mz_backend_obj_it_begin(const mz_json_cursor_t* cursor) {
  mz_obj_it_t it = MZ_ZERO_LOCAL(mz_obj_it_t);
  if (!cursor->value->is_object()) return it;
  it.object = &cursor->value->get_object();
  it.iter = it.object->begin();
  it.end = it.object->end();
  return it;
}

static bool mz_backend_obj_it_next(mz_obj_it_t* it, const c8** key, mz_json_cursor_t* cursor) {
  if (it->iter == it->end) return false;
  const glz::generic_u64::object_t::const_iterator entry = it->iter++;
  *key = entry->first.c_str();
  cursor->value = &entry->second;
  return true;
}

static mz_arr_it_t mz_backend_arr_it_begin(const mz_json_cursor_t* cursor) {
  mz_arr_it_t it = MZ_ZERO_LOCAL(mz_arr_it_t);
  if (!cursor->value->is_array()) return it;
  it.array = &cursor->value->get_array();
  it.index = 0;
  return it;
}

static bool mz_backend_arr_it_next(mz_arr_it_t* it, mz_json_cursor_t* cursor, u32* index) {
  if (it->index >= it->array->size()) return false;
  cursor->value = &((*it->array)[it->index]);
  if (index) {
    *index = (u32)it->index;
  }
  it->index++;
  return true;
}

static mz_err_t mz_backend_load_str(mz_nstr_t json, mz_json_doc_t** result) {
  mz_json_doc_t* doc = new (std::nothrow) mz_json_doc_t();
  if (!doc) {
    return MZ_ERR_JSON;
  }

  glz::error_ctx err = {};
  doc->buffer.assign(json.data, (size_t)json.len);
  err = glz::read_json(doc->root, doc->buffer);

  if (err) {
    delete doc;
    return MZ_ERR_JSON;
  }

  *result = doc;
  return MZ_OK;
}

static mz_err_t mz_backend_load_file(mz_nstr_t path, mz_json_doc_t** result) {
  mz_json_doc_t* doc = new (std::nothrow) mz_json_doc_t();
  if (!doc) {
    return MZ_ERR_JSON;
  }

  glz::error_ctx err = glz::read_file_json(doc->root, path.data, doc->buffer);
  if (err) {
    delete doc;
    return MZ_ERR_JSON;
  }

  *result = doc;
  return MZ_OK;
}

static void mz_backend_free_document(mz_json_doc_t* doc) {
  delete doc;
}

#elif defined(MZ_BACKEND_SIMDJSON)
#ifndef SP_CPP
#error "MZ_BACKEND_SIMDJSON requires C++ compilation"
#endif

#include <new>
#include <string>

#include "external/simdjson/singleheader/simdjson.h"

struct mz_json_doc_t {
  simdjson::dom::parser parser;
  simdjson::padded_string json;
  simdjson::dom::element root;
};

struct mz_json_cursor_t {
  simdjson::dom::element value;
};

struct mz_obj_it_t {
  simdjson::dom::object::iterator iter;
  simdjson::dom::object::iterator end;
};

struct mz_arr_it_t {
  simdjson::dom::array::iterator iter;
  simdjson::dom::array::iterator end;
  u32 index;
};

static mz_err_t mz_json_simdjson_err(simdjson::error_code err) {
  switch (err) {
    case simdjson::SUCCESS: {
      return MZ_OK;
    }
    case simdjson::INCORRECT_TYPE: {
      return MZ_ERR_TYPE;
    }
    case simdjson::NUMBER_OUT_OF_RANGE:
    case simdjson::BIGINT_ERROR: {
      return MZ_ERR_RANGE;
    }
    default: {
      return MZ_ERR_JSON;
    }
  }
}

static mz_json_cursor_t mz_backend_get_root(const mz_json_doc_t* doc) {
  return (mz_json_cursor_t) { .value = doc->root };
}

static mz_json_kind_t mz_backend_get_value_kind(const mz_json_cursor_t* cursor) {
  switch (cursor->value.type()) {
    case simdjson::dom::element_type::ARRAY: {
      return MZ_JSON_KIND_ARRAY;
    }
    case simdjson::dom::element_type::OBJECT: {
      return MZ_JSON_KIND_OBJECT;
    }
    case simdjson::dom::element_type::INT64:
    case simdjson::dom::element_type::UINT64: {
      return MZ_JSON_KIND_INTEGER;
    }
    case simdjson::dom::element_type::DOUBLE: {
      return MZ_JSON_KIND_REAL;
    }
    case simdjson::dom::element_type::STRING: {
      return MZ_JSON_KIND_STRING;
    }
    case simdjson::dom::element_type::BOOL: {
      return MZ_JSON_KIND_BOOL;
    }
    case simdjson::dom::element_type::NULL_VALUE: {
      return MZ_JSON_KIND_NULL;
    }
  }

  return MZ_JSON_KIND_INVALID;
}

static mz_err_t mz_backend_get_bool(const mz_json_cursor_t* cursor, bool* value) {
  return mz_json_simdjson_err(cursor->value.get_bool().get(*value));
}

static mz_err_t mz_backend_get_string(const mz_json_cursor_t* cursor, const c8** result) {
  return mz_json_simdjson_err(cursor->value.get_c_str().get(*result));
}

static mz_err_t mz_backend_get_s64_exact(const mz_json_cursor_t* cursor, s64* result) {
  return mz_json_simdjson_err(cursor->value.get_int64().get(*result));
}

static mz_err_t mz_backend_get_u64_exact(const mz_json_cursor_t* cursor, u64* result) {
  return mz_json_simdjson_err(cursor->value.get_uint64().get(*result));
}

static mz_err_t mz_backend_get_f64(const mz_json_cursor_t* cursor, f64* result) {
  return mz_json_simdjson_err(cursor->value.get_double().get(*result));
}

static mz_obj_it_t mz_backend_obj_it_begin(const mz_json_cursor_t* cursor) {
  mz_obj_it_t it = MZ_ZERO_LOCAL(mz_obj_it_t);
  simdjson::dom::object object;
  if (cursor->value.get_object().get(object)) {
    return it;
  }

  it.iter = object.begin();
  it.end = object.end();
  return it;
}

static bool mz_backend_obj_it_next(mz_obj_it_t* it, const c8** key, mz_json_cursor_t* cursor) {
  if (it->iter == it->end) return false;
  simdjson::dom::key_value_pair entry = *it->iter;
  *key = entry.key.data();
  cursor->value = entry.value;
  ++it->iter;
  return true;
}

static mz_arr_it_t mz_backend_arr_it_begin(const mz_json_cursor_t* cursor) {
  mz_arr_it_t it = MZ_ZERO_LOCAL(mz_arr_it_t);
  simdjson::dom::array array;
  if (cursor->value.get_array().get(array)) {
    return it;
  }

  it.iter = array.begin();
  it.end = array.end();
  it.index = 0;
  return it;
}

static bool mz_backend_arr_it_next(mz_arr_it_t* it, mz_json_cursor_t* cursor, u32* index) {
  if (it->iter == it->end) return false;
  cursor->value = *it->iter;
  if (index) {
    *index = it->index;
  }

  ++it->iter;
  it->index++;
  return true;
}

static mz_err_t mz_backend_load_str(mz_nstr_t json, mz_json_doc_t** result) {
  mz_json_doc_t* doc = new (std::nothrow) mz_json_doc_t();
  if (!doc) {
    return MZ_ERR_JSON;
  }

  simdjson::error_code error = doc->parser.parse(json.data, (size_t)json.len).get(doc->root);
  if (error) {
    delete doc;
    return mz_json_simdjson_err(error);
  }

  *result = doc;
  return MZ_OK;
}

static mz_err_t mz_backend_load_file(mz_nstr_t path, mz_json_doc_t** result) {
  mz_json_doc_t* doc = new (std::nothrow) mz_json_doc_t();
  if (!doc) {
    return MZ_ERR_JSON;
  }

  std::string file_path(path.data, (size_t)path.len);
  simdjson::error_code error = doc->parser.load(file_path).get(doc->root);
  if (error) {
    delete doc;
    return mz_json_simdjson_err(error);
  }

  *result = doc;
  return MZ_OK;
}

static void mz_backend_free_document(mz_json_doc_t* doc) {
  delete doc;
}

#elif defined(MZ_BACKEND_CUSTOM)
#ifndef MZ_BACKEND_CUSTOM_HEADER
#error "MZ_BACKEND_CUSTOM requires MZ_BACKEND_CUSTOM_HEADER to provide backend contract implementations"
#endif

#include MZ_BACKEND_CUSTOM_HEADER
#endif

SP_BEGIN_EXTERN_C()

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
  mz_on_alloc_fn_t on_alloc;
  mz_on_parse_fn_t on_parse;
} mz_field_t;

typedef struct {
  const c8* str;
  bool boolean;
  s32 s32;
  u64 u64;
  f64 f64;
} mz_field_temp_t;

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
      mz_on_alloc_fn_t on_alloc;
      mz_on_parse_fn_t on_parse;
    } array;
    struct {
      mz_schema_t* value;
      mz_on_alloc_fn_t on_alloc;
      mz_on_parse_fn_t on_parse;
    } map;
    struct {
      const c8* key;
      mz_tag_kind_t kind;
      sp_da(mz_tag_case_t) cases;
    } tagged;
  } as;
};

static mz_diag_part_t mz_diag_part_make_key(const c8* key) {
  mz_diag_part_t part = SP_ZERO_STRUCT(mz_diag_part_t);
  part.is_index = false;
  part.as.key = key;
  return part;
}

static mz_diag_part_t mz_diag_part_make_index(u32 index) {
  mz_diag_part_t part = SP_ZERO_STRUCT(mz_diag_part_t);
  part.is_index = true;
  part.as.index = index;
  return part;
}

static mz_object_slot_t mz_object_slot_make(u32 hash, u32 field_index) {
  mz_object_slot_t slot = SP_ZERO_STRUCT(mz_object_slot_t);
  slot.hash = hash;
  slot.field_index = field_index;
  return slot;
}

static mz_schema_t mz_schema_make_object_value(mz_object_mode_t mode) {
  mz_schema_t schema = mz_zero_s(mz_schema_t);
  schema.kind = MZ_SCHEMA_OBJECT;
  schema.as.object.mode = mode;
  return schema;
}

static mz_schema_t mz_schema_make_s32_value(s32 min, s32 max) {
  mz_schema_t schema = mz_zero_s(mz_schema_t);
  schema.kind = MZ_SCHEMA_S32;
  schema.as.s32_range.min = min;
  schema.as.s32_range.max = max;
  return schema;
}

static mz_schema_t mz_schema_make_u64_value(u64 min, u64 max) {
  mz_schema_t schema = mz_zero_s(mz_schema_t);
  schema.kind = MZ_SCHEMA_U64;
  schema.as.u64_range.min = min;
  schema.as.u64_range.max = max;
  return schema;
}

static mz_schema_t mz_schema_make_f64_value(f64 min, f64 max) {
  mz_schema_t schema = mz_zero_s(mz_schema_t);
  schema.kind = MZ_SCHEMA_F64;
  schema.as.f64_range.min = min;
  schema.as.f64_range.max = max;
  return schema;
}

static mz_schema_t mz_schema_make_array_value(mz_schema_t* element, mz_on_alloc_fn_t on_alloc, mz_on_parse_fn_t on_parse, u32 min_len, u32 max_len) {
  mz_schema_t schema = mz_zero_s(mz_schema_t);
  schema.kind = MZ_SCHEMA_ARRAY;
  schema.as.array.element = element;
  schema.as.array.min_len = min_len;
  schema.as.array.max_len = max_len;
  schema.as.array.on_alloc = on_alloc;
  schema.as.array.on_parse = on_parse;
  return schema;
}

static mz_schema_t mz_schema_make_map_value(mz_schema_t* value, mz_on_alloc_fn_t on_alloc, mz_on_parse_fn_t on_parse) {
  mz_schema_t schema = mz_zero_s(mz_schema_t);
  schema.kind = MZ_SCHEMA_MAP;
  schema.as.map.value = value;
  schema.as.map.on_alloc = on_alloc;
  schema.as.map.on_parse = on_parse;
  return schema;
}

static mz_schema_t mz_schema_make_tagged_value(const c8* key, mz_tag_kind_t kind) {
  mz_schema_t schema = mz_zero_s(mz_schema_t);
  schema.kind = MZ_SCHEMA_TAGGED;
  schema.as.tagged.key = key;
  schema.as.tagged.kind = kind;
  return schema;
}

static mz_tag_value_t mz_tag_value_make_str(const c8* str) {
  mz_tag_value_t value = mz_zero_s(mz_tag_value_t);
  value.kind = MZ_TAG_KIND_STR;
  value.as.str = str;
  return value;
}

static mz_tag_value_t mz_tag_value_make_s32(s32 value_in) {
  mz_tag_value_t value = mz_zero_s(mz_tag_value_t);
  value.kind = MZ_TAG_KIND_S32;
  value.as.s32 = value_in;
  return value;
}

static mz_tag_value_t mz_tag_value_make_u64(u64 value_in) {
  mz_tag_value_t value = mz_zero_s(mz_tag_value_t);
  value.kind = MZ_TAG_KIND_U64;
  value.as.u64 = value_in;
  return value;
}

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

  ctx->diag_parts[ctx->diag_parts_len++] = mz_diag_part_make_key(key);
}

static void mz_diag_push_index(mz_ctx_t* ctx, u32 index) {
  if (!ctx) {
    return;
  }

  if (ctx->diag_parts_len >= SP_CARR_LEN(ctx->diag_parts)) {
    return;
  }

  ctx->diag_parts[ctx->diag_parts_len++] = mz_diag_part_make_index(index);
}

static const c8* mz_err_message(mz_err_t kind) {
  switch (kind) {
    case MZ_OK:       return "ok";
    case MZ_ERR_JSON: return "invalid json";
    case MZ_ERR_TYPE: return "type mismatch";
    case MZ_ERR_MISSING_KEY: return "missing required key";
    case MZ_ERR_UNKNOWN_KEY: return "unknown key";
    case MZ_ERR_RANGE: return "value out of range";
    case MZ_ERR_LIMIT: return "input exceeds configured limit";
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
  mz_assert(key);
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
    schema->as.object.slots = mz_nullptr;
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

  schema->as.object.slots = (mz_object_slot_t*)sp_alloc(sizeof(mz_object_slot_t) * cap);
  if (!schema->as.object.slots) {
    schema->as.object.slots_cap = 0;
    return;
  }
  schema->as.object.slots_cap = cap;

  sp_for(it, cap) {
    schema->as.object.slots[it] = mz_object_slot_make(0, SP_LIMIT_U32_MAX);
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
      mz_assert(!sp_cstr_equal(existing->key, field->key));
      if (sp_cstr_equal(existing->key, field->key)) {
        break;
      }

      slot_index = (slot_index + 1) & mask;
    }
  }
}

static mz_field_t* mz_object_find_field_indexed(mz_schema_t* schema, const c8* key, u32* out_field_index) {
  if (!mz_schema_is_object(schema) || !key) {
    return mz_nullptr;
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
    return mz_nullptr;
  }

  u32 cap = schema->as.object.slots_cap;
  if (cap == 0) {
    return mz_nullptr;
  }

  u32 hash = mz_object_key_hash(key);
  u32 slot_index = hash & (cap - 1);

  while (true) {
    mz_object_slot_t* slot = &schema->as.object.slots[slot_index];
    if (slot->field_index == SP_LIMIT_U32_MAX) {
      return mz_nullptr;
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
  mz_field_t* indexed = mz_object_find_field_indexed(schema, key, mz_nullptr);
  if (indexed) {
    return indexed;
  }

  if (!mz_schema_is_object(schema)) {
    return mz_nullptr;
  }

  sp_da_for(schema->as.object.fields, it) {
    mz_field_t* field = &schema->as.object.fields[it];
    if (sp_cstr_equal(field->key, key)) {
      return field;
    }
  }

  return mz_nullptr;
}

#define mz_free(ptr) sp_free((void*)(ptr))

static c8* mz_alloc_cstr(sp_allocator_t allocator, const c8* value) {
  u32 len = sp_cstr_len(value);
  c8* result = (c8*)sp_mem_allocator_alloc(allocator, len + 1);
  if (!result) {
    return mz_nullptr;
  }

  sp_mem_copy(value, result, len + 1);
  return result;
}

static mz_err_t mz_eval_schema(mz_ctx_t* ctx, mz_schema_t* schema, const mz_json_cursor_t* cursor, void* out);

static u32 mz_schema_parse_out_size(mz_schema_t* schema) {
  if (!schema) {
    return 0;
  }

  switch (schema->kind) {
    case MZ_SCHEMA_STRING: {
      return sizeof(const c8*);
    }
    case MZ_SCHEMA_BOOL: {
      return sizeof(bool);
    }
    case MZ_SCHEMA_S32: {
      return sizeof(s32);
    }
    case MZ_SCHEMA_U64: {
      return sizeof(u64);
    }
    case MZ_SCHEMA_F64: {
      return sizeof(f64);
    }
    case MZ_SCHEMA_ANY:
    case MZ_SCHEMA_OBJECT:
    case MZ_SCHEMA_ARRAY:
    case MZ_SCHEMA_MAP:
    case MZ_SCHEMA_TAGGED: {
      return 0;
    }
  }

  return 0;
}

static void* mz_field_temp_out_for_schema(mz_schema_t* schema, mz_field_temp_t* temp) {
  mz_assert(temp);

  switch (schema->kind) {
    case MZ_SCHEMA_STRING: {
      return &temp->str;
    }
    case MZ_SCHEMA_BOOL: {
      return &temp->boolean;
    }
    case MZ_SCHEMA_S32: {
      return &temp->s32;
    }
    case MZ_SCHEMA_U64: {
      return &temp->u64;
    }
    case MZ_SCHEMA_F64: {
      return &temp->f64;
    }
    case MZ_SCHEMA_ANY:
    case MZ_SCHEMA_OBJECT:
    case MZ_SCHEMA_ARRAY:
    case MZ_SCHEMA_MAP:
    case MZ_SCHEMA_TAGGED: {
      return mz_nullptr;
    }
  }

  return mz_nullptr;
}

static bool mz_schema_collection_has_entry(mz_schema_t* schema) {
  if (!schema) {
    return false;
  }

  switch (schema->kind) {
    case MZ_SCHEMA_ARRAY: {
      return schema->as.array.element != mz_nullptr;
    }
    case MZ_SCHEMA_MAP: {
      return schema->as.map.value != mz_nullptr;
    }
    case MZ_SCHEMA_OBJECT:
    case MZ_SCHEMA_STRING:
    case MZ_SCHEMA_ANY:
    case MZ_SCHEMA_BOOL:
    case MZ_SCHEMA_S32:
    case MZ_SCHEMA_U64:
    case MZ_SCHEMA_F64:
    case MZ_SCHEMA_TAGGED: {
      return false;
    }
  }

  return false;
}

static void mz_schema_collection_set_entry(mz_schema_t* schema, mz_schema_t* entry, mz_on_parse_fn_t on_parse) {
  mz_assert(schema);
  mz_assert(entry);

  switch (schema->kind) {
    case MZ_SCHEMA_ARRAY: {
      mz_assert(!schema->as.array.element);
      schema->as.array.element = entry;
      schema->as.array.on_parse = on_parse;
      return;
    }
    case MZ_SCHEMA_MAP: {
      mz_assert(!schema->as.map.value);
      schema->as.map.value = entry;
      schema->as.map.on_parse = on_parse;
      return;
    }
    case MZ_SCHEMA_OBJECT:
    case MZ_SCHEMA_STRING:
    case MZ_SCHEMA_ANY:
    case MZ_SCHEMA_BOOL:
    case MZ_SCHEMA_S32:
    case MZ_SCHEMA_U64:
    case MZ_SCHEMA_F64:
    case MZ_SCHEMA_TAGGED: {
      mz_assert(false);
      return;
    }
  }
}

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

static mz_err_t mz_json_cursor_normalize_type_err(const mz_json_cursor_t* cursor, mz_err_t err) {
  SP_UNUSED(cursor);
  if (err != MZ_ERR_TYPE) {
    return err;
  }

  return err;
}

// @nasty
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
    SP_UNUSED(child);
    return MZ_OK;
  }

  mz_key_t mz_key = { .str = key };
  u32 alloc_size = field->is_ptr ? field->ptr_size : mz_schema_parse_out_size(field->schema);

  void* field_out = mz_nullptr;
  void* parsed_out = mz_nullptr;
  mz_field_temp_t temp = SP_ZERO_INITIALIZE();

  void* bound_field = mz_nullptr;
  if (out && field->offset != MZ_NO_OFFSET) {
    bound_field = ((u8*)out) + field->offset;
  }

  if (out && field->is_ptr) {
    void* ptr_value = mz_nullptr;
    if (field->on_alloc) {
      sp_context_push_allocator(ctx->allocator);
      mz_err_t alloc_err = field->on_alloc(ctx, out, mz_key, alloc_size, &ptr_value);
      sp_context_pop();
      if (alloc_err != MZ_OK) {
        mz_diag_set(ctx, alloc_err);
        mz_diag_push_key(ctx, field->key);
        return alloc_err;
      }
    }
    else {
      if (alloc_size == 0) {
        mz_err_t alloc_err = mz_diag_set(ctx, MZ_ERR_TYPE);
        mz_diag_push_key(ctx, field->key);
        return alloc_err;
      }
      ptr_value = sp_mem_allocator_alloc(ctx->allocator, alloc_size);

      // @nasty don't check for OOM, this wouldnt be a type error anyway?
      if (!ptr_value) {
        mz_err_t alloc_err = mz_diag_set(ctx, MZ_ERR_TYPE);
        mz_diag_push_key(ctx, field->key);
        return alloc_err;
      }

      sp_mem_zero(ptr_value, alloc_size);
    }

    if (!ptr_value) {
      mz_err_t alloc_err = mz_diag_set(ctx, MZ_ERR_TYPE);
      mz_diag_push_key(ctx, field->key);
      return alloc_err;
    }

    if (bound_field) {
      *((void**)bound_field) = ptr_value;
    }
    field_out = ptr_value;
  }
  else if (out && field->on_alloc) {
    sp_context_push_allocator(ctx->allocator);
    mz_err_t alloc_err = field->on_alloc(ctx, out, mz_key, alloc_size, &field_out);
    sp_context_pop();
    if (alloc_err != MZ_OK) {
      mz_diag_set(ctx, alloc_err);
      mz_diag_push_key(ctx, field->key);
      return alloc_err;
    }
  }
  else if (bound_field) {
    field_out = bound_field;
  }

  parsed_out = field_out;
  if (field->on_parse) {
    void* temp_out = mz_field_temp_out_for_schema(field->schema, &temp);
    if (temp_out) {
      parsed_out = temp_out;
    }
  }

  mz_err_t err = mz_eval_schema(ctx, field->schema, child, parsed_out);
  if (err != MZ_OK) {
    mz_diag_push_key(ctx, field->key);
    return err;
  }

  if (field->on_parse) {
    //mz_try_diag(ctx, field->key, field->on_parse(ctx, out, mz_key, field_out, parsed_out));
    mz_err_t parse_err = field->on_parse(ctx, out, mz_key, field_out, parsed_out);
    if (parse_err != MZ_OK) {
      mz_diag_set(ctx, parse_err);
      mz_diag_push_key(ctx, field->key);
      return parse_err;
    }
  }

  mz_bitset_set(seen, field_index);
  return MZ_OK;
}

static mz_err_t mz_eval_string_tag(mz_ctx_t* ctx, const c8* value, void* out) {
  if (out) {
    mz_assert(ctx);
    mz_assert(ctx->arena);

    c8* copy = mz_alloc_cstr(ctx->allocator, value);

    *((const c8**)out) = copy;
  }

  return MZ_OK;
}

static mz_err_t mz_eval_schema_tag(mz_ctx_t* ctx, mz_schema_t* schema, mz_tag_value_t tag, void* out) {
  if (!schema) {
    return mz_diag_set(ctx, MZ_ERR_TYPE);
  }

  switch (schema->kind) {
    case MZ_SCHEMA_STRING: {
      if (tag.kind != MZ_TAG_KIND_STR) {
        return mz_diag_set(ctx, MZ_ERR_TYPE);
      }
      return mz_eval_string_tag(ctx, tag.as.str, out);
    }
    case MZ_SCHEMA_ANY: {
      return MZ_OK;
    }
    case MZ_SCHEMA_S32: {
      s32 parsed = 0;
      if (tag.kind == MZ_TAG_KIND_STR) {
        return mz_diag_set(ctx, MZ_ERR_TYPE);
      }
      if (tag.kind == MZ_TAG_KIND_S32) {
        parsed = tag.as.s32;
      }
      else {
        if (tag.as.u64 > (u64)SP_LIMIT_S32_MAX) {
          return mz_diag_set(ctx, MZ_ERR_RANGE);
        }
        parsed = (s32)tag.as.u64;
      }

      if (parsed < schema->as.s32_range.min || parsed > schema->as.s32_range.max) {
        return mz_diag_set(ctx, MZ_ERR_RANGE);
      }

      if (out) {
        *((s32*)out) = parsed;
      }
      return MZ_OK;
    }
    case MZ_SCHEMA_U64: {
      u64 parsed = 0;
      if (tag.kind == MZ_TAG_KIND_STR) {
        return mz_diag_set(ctx, MZ_ERR_TYPE);
      }
      if (tag.kind == MZ_TAG_KIND_S32) {
        if (tag.as.s32 < 0) {
          return mz_diag_set(ctx, MZ_ERR_RANGE);
        }
        parsed = (u64)tag.as.s32;
      }
      else {
        parsed = tag.as.u64;
      }

      if (parsed < schema->as.u64_range.min || parsed > schema->as.u64_range.max) {
        return mz_diag_set(ctx, MZ_ERR_RANGE);
      }

      if (out) {
        *((u64*)out) = parsed;
      }
      return MZ_OK;
    }
    case MZ_SCHEMA_F64: {
      f64 parsed = 0;
      if (tag.kind == MZ_TAG_KIND_STR) {
        return mz_diag_set(ctx, MZ_ERR_TYPE);
      }
      if (tag.kind == MZ_TAG_KIND_S32) {
        parsed = (f64)tag.as.s32;
      }
      else {
        parsed = (f64)tag.as.u64;
      }

      if (parsed < schema->as.f64_range.min || parsed > schema->as.f64_range.max) {
        return mz_diag_set(ctx, MZ_ERR_RANGE);
      }

      if (out) {
        *((f64*)out) = parsed;
      }
      return MZ_OK;
    }
    case MZ_SCHEMA_OBJECT:
    case MZ_SCHEMA_BOOL:
    case MZ_SCHEMA_ARRAY:
    case MZ_SCHEMA_MAP:
    case MZ_SCHEMA_TAGGED: {
      return mz_diag_set(ctx, MZ_ERR_TYPE);
    }
  }

  return mz_diag_set(ctx, MZ_ERR_TYPE);
}

static mz_err_t mz_eval_object_tag_member(mz_ctx_t* ctx, mz_schema_t* schema, const c8* key, mz_tag_value_t tag, void* out, u32* seen) {
  u32 field_index = 0;
  mz_field_t* field = mz_object_find_field_indexed(schema, key, &field_index);
  if (!field) {
    return MZ_OK;
  }

  if (mz_bitset_has(seen, field_index)) {
    return MZ_OK;
  }

  mz_key_t mz_key = { .str = key };
  u32 alloc_size = field->is_ptr ? field->ptr_size : mz_schema_parse_out_size(field->schema);

  void* field_out = mz_nullptr;
  void* parsed_out = mz_nullptr;
  mz_field_temp_t temp = SP_ZERO_INITIALIZE();

  void* bound_field = mz_nullptr;
  if (out && field->offset != MZ_NO_OFFSET) {
    bound_field = ((u8*)out) + field->offset;
  }

  if (out && field->is_ptr) {
    void* ptr_value = mz_nullptr;
    if (field->on_alloc) {
      sp_context_push_allocator(ctx->allocator);
      mz_err_t alloc_err = field->on_alloc(ctx, out, mz_key, alloc_size, &ptr_value);
      sp_context_pop();
      if (alloc_err != MZ_OK) {
        mz_diag_set(ctx, alloc_err);
        mz_diag_push_key(ctx, field->key);
        return alloc_err;
      }
    }
    else {
      if (alloc_size == 0) {
        mz_err_t alloc_err = mz_diag_set(ctx, MZ_ERR_TYPE);
        mz_diag_push_key(ctx, field->key);
        return alloc_err;
      }
      ptr_value = sp_mem_allocator_alloc(ctx->allocator, alloc_size);
      if (!ptr_value) {
        mz_err_t alloc_err = mz_diag_set(ctx, MZ_ERR_TYPE);
        mz_diag_push_key(ctx, field->key);
        return alloc_err;
      }
      sp_mem_zero(ptr_value, alloc_size);
    }

    if (!ptr_value) {
      mz_err_t alloc_err = mz_diag_set(ctx, MZ_ERR_TYPE);
      mz_diag_push_key(ctx, field->key);
      return alloc_err;
    }

    if (bound_field) {
      *((void**)bound_field) = ptr_value;
    }
    field_out = ptr_value;
  }
  else if (out && field->on_alloc) {
    sp_context_push_allocator(ctx->allocator);
    mz_err_t alloc_err = field->on_alloc(ctx, out, mz_key, alloc_size, &field_out);
    sp_context_pop();
    if (alloc_err != MZ_OK) {
      mz_diag_set(ctx, alloc_err);
      mz_diag_push_key(ctx, field->key);
      return alloc_err;
    }
  }
  else if (bound_field) {
    field_out = bound_field;
  }

  parsed_out = field_out;
  if (field->on_parse) {
    void* temp_out = mz_field_temp_out_for_schema(field->schema, &temp);
    if (temp_out) {
      parsed_out = temp_out;
    }
  }

  mz_err_t err = mz_eval_schema_tag(ctx, field->schema, tag, parsed_out);
  if (err != MZ_OK) {
    mz_diag_push_key(ctx, field->key);
    return err;
  }

  if (field->on_parse) {
    mz_err_t parse_err = field->on_parse(ctx, out, mz_key, field_out, parsed_out);
    if (parse_err != MZ_OK) {
      mz_diag_set(ctx, parse_err);
      mz_diag_push_key(ctx, field->key);
      return parse_err;
    }
  }

  mz_bitset_set(seen, field_index);
  return MZ_OK;
}

static mz_err_t mz_eval_object_from_iter(
  mz_ctx_t* ctx,
  mz_schema_t* schema,
  mz_obj_it_t* iter,
  bool has_first,
  const c8* first_key,
  const mz_json_cursor_t* first_child,
  const mz_tag_value_t* first_tag,
  void* out
) {
  u32 field_count = sp_da_size(schema->as.object.fields);
  u32 seen_inline[2] = {0};
  u32 seen_words = (field_count + 31) / 32;
  u32* seen = seen_words <= SP_CARR_LEN(seen_inline) ? seen_inline : mz_nullptr;
  if (!seen && seen_words > 0) {
    seen = sp_mem_allocator_alloc_n(ctx->allocator, u32, seen_words);
    if (!seen) {
      return mz_diag_set(ctx, MZ_ERR_TYPE);
    }
    sp_mem_zero(seen, sizeof(u32) * seen_words);
  }

  if (has_first) {
    mz_try(first_tag ?
      mz_eval_object_tag_member(ctx, schema, first_key, *first_tag, out, seen) :
      mz_eval_object_member(ctx, schema, first_key, first_child, out, seen));
  }

  const c8* key = mz_nullptr;
  mz_json_cursor_t child = MZ_ZERO_LOCAL(mz_json_cursor_t);
  while (mz_backend_obj_it_next(iter, &key, &child)) {
    mz_try(mz_eval_object_member(ctx, schema, key, &child, out, seen));
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
  switch (mz_backend_get_value_kind(cursor)) {
    case MZ_JSON_KIND_OBJECT: {
      mz_obj_it_t it = mz_backend_obj_it_begin(cursor);
      return mz_eval_object_from_iter(ctx, schema, &it, false, mz_nullptr, mz_nullptr, mz_nullptr, out);
    }
    case MZ_JSON_KIND_INVALID: {
      return mz_diag_set(ctx, MZ_ERR_JSON);
    }
    default: {
      return mz_diag_set(ctx, mz_json_cursor_normalize_type_err(cursor, MZ_ERR_TYPE));
    }
  }
}

static mz_err_t mz_eval_string(mz_ctx_t* ctx, const mz_json_cursor_t* cursor, void* out) {
  const c8* json_value = mz_nullptr;
  mz_err_t err = mz_backend_get_string(cursor, &json_value);
  if (err != MZ_OK) {
    err = mz_json_cursor_normalize_type_err(cursor, err);
    return mz_diag_set(ctx, err);
  }

  if (out) {
    mz_assert(ctx);
    mz_assert(ctx->arena);

    c8* copy = mz_alloc_cstr(ctx->allocator, json_value);

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
  mz_err_t err = mz_backend_get_bool(cursor, &parsed);
  if (err != MZ_OK) {
    err = mz_json_cursor_normalize_type_err(cursor, err);
    return mz_diag_set(ctx, err);
  }
// @spader collapse: check type, ONCE, at top of function. stop useless defensive checks
  if (out) {
    *((bool*)out) = parsed;
  }

  return MZ_OK;
}

static mz_err_t mz_eval_s32(mz_ctx_t* ctx, mz_schema_t* schema, const mz_json_cursor_t* cursor, void* out) {
  s64 raw = 0;
  mz_err_t err = mz_backend_get_s64_exact(cursor, &raw);
  if (err != MZ_OK) {
    err = mz_json_cursor_normalize_type_err(cursor, err);
    return mz_diag_set(ctx, err);
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
  u64 parsed = 0;
  mz_err_t err = mz_backend_get_u64_exact(cursor, &parsed);
  if (err != MZ_OK) {
    err = mz_json_cursor_normalize_type_err(cursor, err);
    return mz_diag_set(ctx, err);
  }

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
  mz_err_t err = mz_backend_get_f64(cursor, &parsed);
  if (err != MZ_OK) {
    err = mz_json_cursor_normalize_type_err(cursor, err);
    return mz_diag_set(ctx, err);
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
  mz_json_kind_t kind = mz_backend_get_value_kind(cursor);
  if (kind == MZ_JSON_KIND_INVALID) {
    return mz_diag_set(ctx, MZ_ERR_JSON);
  }
  if (kind != MZ_JSON_KIND_ARRAY) {
    return mz_diag_set(ctx, mz_json_cursor_normalize_type_err(cursor, MZ_ERR_TYPE));
  }

  if (!schema->as.array.element) {
    return mz_diag_set(ctx, MZ_ERR_TYPE);
  }

  if (out && !schema->as.array.on_alloc) {
    return mz_diag_set(ctx, MZ_ERR_TYPE);
  }

  mz_schema_t* element_schema = schema->as.array.element;
  mz_on_alloc_fn_t on_alloc = schema->as.array.on_alloc;
  mz_on_parse_fn_t on_parse = schema->as.array.on_parse;
  u32 min_len = schema->as.array.min_len;
  u32 max_len = schema->as.array.max_len;

  u32 count = 0;
  mz_arr_it_t it = mz_backend_arr_it_begin(cursor);
  mz_json_cursor_t child = MZ_ZERO_LOCAL(mz_json_cursor_t);
  u32 index = 0;
  while (mz_backend_arr_it_next(&it, &child, &index)) {
    if (max_len != SP_LIMIT_U32_MAX && count >= max_len) {
      return mz_diag_set(ctx, MZ_ERR_RANGE);
    }

    mz_key_t key = { .u32 = index };
    void* elem_out = mz_nullptr;
    void* parsed_out = mz_nullptr;
    mz_field_temp_t temp = SP_ZERO_INITIALIZE();
    if (out && on_alloc) {
      sp_context_push_allocator(ctx->allocator);
      mz_err_t alloc_err = on_alloc(ctx, out, key, mz_schema_parse_out_size(element_schema), &elem_out);
      sp_context_pop();
      if (alloc_err != MZ_OK) {
        mz_diag_set(ctx, alloc_err);
        mz_diag_push_index(ctx, index);
        return alloc_err;
      }

      if (!elem_out) {
        mz_err_t alloc_err = mz_diag_set(ctx, MZ_ERR_TYPE);
        mz_diag_push_index(ctx, index);
        return alloc_err;
      }
    }

    parsed_out = elem_out;
    if (on_parse) {
      void* temp_out = mz_field_temp_out_for_schema(element_schema, &temp);
      if (temp_out) {
        parsed_out = temp_out;
      }
    }

    mz_err_t err = mz_eval_schema(ctx, element_schema, &child, parsed_out);
    if (err != MZ_OK) {
      mz_diag_push_index(ctx, index);
      return err;
    }

    if (on_parse) {
      mz_err_t parse_err = on_parse(ctx, out, key, elem_out, parsed_out);
      if (parse_err != MZ_OK) {
        mz_diag_set(ctx, parse_err);
        mz_diag_push_index(ctx, index);
        return parse_err;
      }
    }

    count++;
  }

  if (count < min_len) {
    return mz_diag_set(ctx, MZ_ERR_RANGE);
  }

  return MZ_OK;
}

static bool mz_tag_values_equal(mz_tag_value_t lhs, mz_tag_value_t rhs) {
  if (lhs.kind != rhs.kind) {
    return false;
  }

  switch (lhs.kind) {
    case MZ_TAG_KIND_STR: {
      return lhs.as.str && rhs.as.str && sp_cstr_equal(lhs.as.str, rhs.as.str);
    }
    case MZ_TAG_KIND_S32: {
      return lhs.as.s32 == rhs.as.s32;
    }
    case MZ_TAG_KIND_U64: {
      return lhs.as.u64 == rhs.as.u64;
    }
  }

  return false;
}

static mz_err_t mz_tag_value_from_cursor(mz_tag_kind_t kind, const mz_json_cursor_t* cursor, mz_tag_value_t* out_tag) {
  mz_assert(out_tag);
  out_tag->kind = kind;

  switch (kind) {
    case MZ_TAG_KIND_STR: {
      const c8* str = mz_nullptr;
      mz_err_t err = mz_backend_get_string(cursor, &str);
      err = mz_json_cursor_normalize_type_err(cursor, err);
      mz_try(err);
      out_tag->as.str = str;
      return MZ_OK;
    }
    case MZ_TAG_KIND_S32: {
      s64 raw = 0;
      mz_err_t err = mz_backend_get_s64_exact(cursor, &raw);
      err = mz_json_cursor_normalize_type_err(cursor, err);
      mz_try(err);
      s32 parsed = (s32)raw;
      if ((s64)parsed != raw) {
        return MZ_ERR_RANGE;
      }
      out_tag->as.s32 = parsed;
      return MZ_OK;
    }
    case MZ_TAG_KIND_U64: {
      u64 raw = 0;
      mz_err_t err = mz_backend_get_u64_exact(cursor, &raw);
      err = mz_json_cursor_normalize_type_err(cursor, err);
      mz_try(err);
      out_tag->as.u64 = raw;
      return MZ_OK;
    }
  }

  return MZ_ERR_TYPE;
}

static mz_err_t mz_eval_tagged(mz_ctx_t* ctx, mz_schema_t* schema, const mz_json_cursor_t* cursor, void* out) {
  mz_json_kind_t kind = mz_backend_get_value_kind(cursor);
  if (kind == MZ_JSON_KIND_INVALID) {
    return mz_diag_set(ctx, MZ_ERR_JSON);
  }
  if (kind != MZ_JSON_KIND_OBJECT) {
    return mz_diag_set(ctx, mz_json_cursor_normalize_type_err(cursor, MZ_ERR_TYPE));
  }

  mz_obj_it_t iter = mz_backend_obj_it_begin(cursor);
  const c8* first_key = mz_nullptr;
  mz_json_cursor_t first_child = MZ_ZERO_LOCAL(mz_json_cursor_t);
  if (!mz_backend_obj_it_next(&iter, &first_key, &first_child)) {
    mz_err_t err = mz_diag_set(ctx, MZ_ERR_MISSING_KEY);
    mz_diag_push_key(ctx, schema->as.tagged.key);
    return err;
  }

  if (!sp_cstr_equal(first_key, schema->as.tagged.key)) {
    mz_err_t err = mz_diag_set(ctx, MZ_ERR_MISSING_KEY);
    mz_diag_push_key(ctx, schema->as.tagged.key);
    return err;
  }

  mz_schema_t* selected = mz_nullptr;
  mz_tag_value_t selected_tag = SP_ZERO_STRUCT(mz_tag_value_t);
  mz_tag_value_t actual_tag = SP_ZERO_STRUCT(mz_tag_value_t);
  mz_err_t tag_err = mz_tag_value_from_cursor(schema->as.tagged.kind, &first_child, &actual_tag);
  if (tag_err == MZ_ERR_JSON) {
    mz_diag_set(ctx, tag_err);
    mz_diag_push_key(ctx, schema->as.tagged.key);
    return tag_err;
  }
  if (tag_err == MZ_OK) {
    sp_da_for(schema->as.tagged.cases, case_it) {
      mz_tag_case_t* c = &schema->as.tagged.cases[case_it];
      if (mz_tag_values_equal(c->tag, actual_tag)) {
        selected = c->schema;
        selected_tag = actual_tag;
        break;
      }
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

  if (mz_object_find_field(selected, schema->as.tagged.key)) {
    return mz_eval_object_from_iter(ctx, selected, &iter, true, first_key, mz_nullptr, &selected_tag, out);
  }

  return mz_eval_object_from_iter(ctx, selected, &iter, false, mz_nullptr, mz_nullptr, mz_nullptr, out);
}

static mz_schema_t* mz_schema_map_internal(mz_schema_t* value, mz_on_alloc_fn_t on_alloc, mz_on_parse_fn_t on_parse) {
  mz_schema_t* schema = sp_alloc_type(mz_schema_t);
  *schema = mz_schema_make_map_value(value, on_alloc, on_parse);
  return schema;
}

static mz_err_t mz_eval_map(mz_ctx_t* ctx, mz_schema_t* schema, const mz_json_cursor_t* cursor, void* out) {
  mz_json_kind_t kind = mz_backend_get_value_kind(cursor);
  if (kind == MZ_JSON_KIND_INVALID) {
    return mz_diag_set(ctx, MZ_ERR_JSON);
  }
  if (kind != MZ_JSON_KIND_OBJECT) {
    return mz_diag_set(ctx, mz_json_cursor_normalize_type_err(cursor, MZ_ERR_TYPE));
  }

  if (!schema->as.map.value) {
    return mz_diag_set(ctx, MZ_ERR_TYPE);
  }

  if (out && !schema->as.map.on_alloc) {
    return mz_diag_set(ctx, MZ_ERR_TYPE);
  }

  const c8* key = mz_nullptr;
  mz_json_cursor_t child = MZ_ZERO_LOCAL(mz_json_cursor_t);
  mz_obj_it_t it = mz_backend_obj_it_begin(cursor);
  while (mz_backend_obj_it_next(&it, &key, &child)) {
    mz_schema_t* value_schema = schema->as.map.value;
    mz_on_parse_fn_t on_parse = schema->as.map.on_parse;

    if (!out) {
      void* parsed_out = mz_nullptr;
      mz_field_temp_t temp = SP_ZERO_INITIALIZE();
      if (on_parse) {
        parsed_out = mz_field_temp_out_for_schema(value_schema, &temp);
      }

      mz_err_t err = mz_eval_schema(ctx, value_schema, &child, parsed_out);
      if (err != MZ_OK) {
        mz_diag_push_key(ctx, key);
        return err;
      }

      if (on_parse) {
        mz_key_t mz_key = { .str = key };
        mz_err_t parse_err = on_parse(ctx, out, mz_key, mz_nullptr, parsed_out);
        if (parse_err != MZ_OK) {
          mz_diag_set(ctx, parse_err);
          mz_diag_push_key(ctx, key);
          return parse_err;
        }
      }

      continue;
    }

    void* value_out = mz_nullptr;
    void* parsed_out = mz_nullptr;
    mz_field_temp_t temp = SP_ZERO_INITIALIZE();
    mz_key_t mz_key = { .str = key };
    sp_context_push_allocator(ctx->allocator);
    mz_err_t err = schema->as.map.on_alloc(ctx, out, mz_key, mz_schema_parse_out_size(value_schema), &value_out);
    sp_context_pop();
    if (err != MZ_OK) {
      mz_diag_set(ctx, err);
      mz_diag_push_key(ctx, key);
      return err;
    }

    if (!value_out) {
      err = mz_diag_set(ctx, MZ_ERR_TYPE);
      mz_diag_push_key(ctx, key);
      return err;
    }

    parsed_out = value_out;
    if (on_parse) {
      void* temp_out = mz_field_temp_out_for_schema(value_schema, &temp);
      if (temp_out) {
        parsed_out = temp_out;
      }
    }

    err = mz_eval_schema(ctx, value_schema, &child, parsed_out);
    if (err != MZ_OK) {
      mz_diag_push_key(ctx, key);
      return err;
    }

    if (on_parse) {
      mz_err_t parse_err = on_parse(ctx, out, mz_key, value_out, parsed_out);
      if (parse_err != MZ_OK) {
        mz_diag_set(ctx, parse_err);
        mz_diag_push_key(ctx, key);
        return parse_err;
      }
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

void mz_schema_free(mz_schema_t* schema) {
  if (!schema) {
    return;
  }

  if (schema->kind == MZ_SCHEMA_OBJECT) {
    sp_da_for(schema->as.object.fields, it) {
      if (schema->as.object.fields[it].key) {
        mz_free(schema->as.object.fields[it].key);
      }
      mz_schema_free(schema->as.object.fields[it].schema);
    }
    sp_da_free(schema->as.object.fields);
    if (schema->as.object.slots) {
      sp_free(schema->as.object.slots);
      schema->as.object.slots = mz_nullptr;
      schema->as.object.slots_cap = 0;
    }
  }
  if (schema->kind == MZ_SCHEMA_ARRAY) {
    mz_schema_free(schema->as.array.element);
  }
  if (schema->kind == MZ_SCHEMA_MAP) {
    mz_schema_free(schema->as.map.value);
  }
  if (schema->kind == MZ_SCHEMA_TAGGED) {
    if (schema->as.tagged.key) {
      mz_free(schema->as.tagged.key);
    }
    sp_da_for(schema->as.tagged.cases, it) {
      if (schema->as.tagged.cases[it].tag.kind == MZ_TAG_KIND_STR && schema->as.tagged.cases[it].tag.as.str) {
        mz_free(schema->as.tagged.cases[it].tag.as.str);
      }
      mz_schema_free(schema->as.tagged.cases[it].schema);
    }
    sp_da_free(schema->as.tagged.cases);
  }

  sp_free(schema);
}

mz_schema_t* mz_schema_object(mz_object_mode_t mode) {
  mz_schema_t* schema = sp_alloc_type(mz_schema_t);
  *schema = mz_schema_make_object_value(mode);
  return schema;
}

mz_schema_t* mz_schema_string() {
  mz_schema_t* schema = sp_alloc_type(mz_schema_t);
  *schema = (mz_schema_t) { .kind = MZ_SCHEMA_STRING };
  return schema;
}

mz_schema_t* mz_schema_any() {
  mz_schema_t* schema = sp_alloc_type(mz_schema_t);
  *schema = (mz_schema_t) { .kind = MZ_SCHEMA_ANY };
  return schema;
}

mz_schema_t* mz_schema_bool() {
  mz_schema_t* schema = sp_alloc_type(mz_schema_t);
  *schema = (mz_schema_t) { .kind = MZ_SCHEMA_BOOL };
  return schema;
}

mz_schema_t* mz_schema_s32_ex(s32 min, s32 max) {
  mz_schema_t* schema = sp_alloc_type(mz_schema_t);
  *schema = mz_schema_make_s32_value(min, max);
  return schema;
}

mz_schema_t* mz_schema_s32() {
  return mz_schema_s32_ex(SP_LIMIT_S32_MIN, SP_LIMIT_S32_MAX);
}

mz_schema_t* mz_schema_u64_ex(u64 min, u64 max) {
  mz_schema_t* schema = sp_alloc_type(mz_schema_t);
  *schema = mz_schema_make_u64_value(min, max);
  return schema;
}

mz_schema_t* mz_schema_u64() {
  return mz_schema_u64_ex(0, SP_LIMIT_U64_MAX);
}

mz_schema_t* mz_schema_f64_ex(f64 min, f64 max) {
  mz_schema_t* schema = sp_alloc_type(mz_schema_t);
  *schema = mz_schema_make_f64_value(min, max);
  return schema;
}

mz_schema_t* mz_schema_f64() {
  return mz_schema_f64_ex(-SP_LIMIT_F64_MAX, SP_LIMIT_F64_MAX);
}

mz_schema_t* mz_schema_array_ex(mz_schema_t* element, mz_on_alloc_fn_t on_alloc, mz_on_parse_fn_t on_parse, u32 min_len, u32 max_len) {
  mz_schema_t* schema = sp_alloc_type(mz_schema_t);
  *schema = mz_schema_make_array_value(element, on_alloc, on_parse, min_len, max_len);
  return schema;
}

mz_schema_t* mz_schema_array(mz_schema_t* element, mz_on_alloc_fn_t on_alloc) {
  return mz_schema_array_ex(element, on_alloc, mz_nullptr, 0, SP_LIMIT_U32_MAX);
}

mz_schema_t* mz_schema_map(mz_schema_t* value, mz_on_alloc_fn_t on_alloc) {
  mz_assert(value);
  return mz_schema_map_internal(value, on_alloc, mz_nullptr);
}

mz_schema_t* mz_schema_map_ex(mz_schema_t* value, mz_on_alloc_fn_t on_alloc, mz_on_parse_fn_t on_parse) {
  mz_assert(value);
  return mz_schema_map_internal(value, on_alloc, on_parse);
}

mz_schema_t* mz_schema_tagged_union(const c8* key, mz_tag_kind_t kind) {
  mz_assert(key);

  mz_schema_t* schema = sp_alloc_type(mz_schema_t);
  *schema = mz_schema_make_tagged_value(sp_cstr_copy(key), kind);
  return schema;
}

mz_tag_value_t mz_tag_str(const c8* str) {
  mz_assert(str);
  return mz_tag_value_make_str(str);
}

mz_tag_value_t mz_tag_s32(s32 s32) {
  return mz_tag_value_make_s32(s32);
}

mz_tag_value_t mz_tag_u64(u64 u64) {
  return mz_tag_value_make_u64(u64);
}

void mz_tagged_union_add(mz_schema_t* tagged, mz_tag_value_t tag, mz_schema_t* schema) {
  mz_assert(tagged);
  mz_assert(tagged->kind == MZ_SCHEMA_TAGGED);
  mz_assert(schema);

  mz_assert(tagged->as.tagged.kind == tag.kind);
  if (tagged->as.tagged.kind != tag.kind) {
    return;
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

  mz_tag_case_t c = SP_ZERO_STRUCT(mz_tag_case_t);
  c.tag = tag;
  c.schema = schema;
  sp_da_push(tagged->as.tagged.cases, c);
}

void mz_object_add_field_ex(mz_schema_t* object, const c8* key, mz_schema_t* field, mz_bind_opts_t opts) {
  mz_assert(object);
  mz_assert(object->kind == MZ_SCHEMA_OBJECT);
  mz_assert(key);
  mz_assert(field);

  mz_field_t entry = SP_ZERO_STRUCT(mz_field_t);
  entry.key = sp_cstr_copy(key);
  entry.schema = field;
  entry.required = opts.required;
  entry.offset = opts.offset;
  entry.is_ptr = false;
  entry.ptr_size = 0;
  entry.on_alloc = opts.on_alloc;
  entry.on_parse = opts.on_parse;
  sp_da_push(object->as.object.fields, entry);
  mz_object_rebuild_index(object);
}

void mz_object_add_field_ptr_ex(mz_schema_t* object, const c8* key, mz_schema_t* field, mz_bind_opts_t opts) {
  mz_assert(object);
  mz_assert(object->kind == MZ_SCHEMA_OBJECT);
  mz_assert(key);
  mz_assert(field);
  mz_assert(opts.ptr_size > 0);

  mz_field_t entry = SP_ZERO_STRUCT(mz_field_t);
  entry.key = sp_cstr_copy(key);
  entry.schema = field;
  entry.required = opts.required;
  entry.offset = opts.offset;
  entry.is_ptr = true;
  entry.ptr_size = opts.ptr_size;
  entry.on_alloc = opts.on_alloc;
  entry.on_parse = opts.on_parse;
  sp_da_push(object->as.object.fields, entry);
  mz_object_rebuild_index(object);
}

mz_builder_t mz_builder_begin() {
  mz_builder_t builder = SP_ZERO_INITIALIZE();
  return builder;
}

mz_schema_t* mz_builder_end(mz_builder_t* builder) {
  mz_assert(builder);
  mz_assert(builder->depth == 0);
  return builder->root;
}

void mz_builder_push_object_ex(mz_builder_t* builder, const c8* key, mz_object_mode_t mode, mz_bind_opts_t opts) {
  mz_assert(builder);
  mz_assert(builder->depth < SP_CARR_LEN(builder->stack));

  mz_schema_t* object = mz_schema_object(mode);
  if (builder->depth == 0) {
    builder->root = object;
  }
  else {
    mz_assert(key);
    mz_builder_add_field_ex(builder, key, object, opts);
  }

  builder->stack[builder->depth++] = object;
}

void mz_builder_push_object_ptr_ex(mz_builder_t* builder, const c8* key, mz_object_mode_t mode, mz_bind_opts_t opts) {
  mz_assert(builder);
  mz_assert(builder->depth < SP_CARR_LEN(builder->stack));

  mz_schema_t* object = mz_schema_object(mode);
  if (builder->depth == 0) {
    builder->root = object;
  }
  else {
    mz_assert(key);
    mz_builder_add_field_ptr_ex(builder, key, object, opts);
  }

  builder->stack[builder->depth++] = object;
}

void mz_builder_push_tagged_union_ex(mz_builder_t* builder, const c8* key, const c8* tag_key, mz_tag_kind_t kind, mz_bind_opts_t opts) {
  mz_assert(builder);
  mz_assert(builder->depth < SP_CARR_LEN(builder->stack));
  mz_assert(tag_key);

  mz_schema_t* tagged = mz_schema_tagged_union(tag_key, kind);
  if (builder->depth == 0) {
    builder->root = tagged;
  }
  else {
    mz_assert(key);
    mz_builder_add_field_ex(builder, key, tagged, opts);
  }

  builder->stack[builder->depth++] = tagged;
}

void mz_builder_push_tagged_case_ex(mz_builder_t* builder, mz_tag_value_t tag, mz_object_mode_t mode) {
  mz_assert(builder);
  mz_assert(builder->depth > 0);
  mz_assert(builder->depth < SP_CARR_LEN(builder->stack));

  mz_schema_t* current = builder->stack[builder->depth - 1];
  mz_assert(current->kind == MZ_SCHEMA_TAGGED);

  mz_schema_t* object = mz_schema_object(mode);
  mz_tagged_union_add(current, tag, object);
  builder->stack[builder->depth++] = object;
}

void mz_builder_push_array_ex(mz_builder_t* builder, const c8* key, mz_bind_opts_t opts) {
  mz_assert(builder);
  mz_assert(builder->depth > 0);
  mz_assert(builder->depth < SP_CARR_LEN(builder->stack));
  mz_assert(key);

  mz_schema_t* array_schema = mz_schema_array_ex(mz_nullptr, opts.on_alloc, mz_nullptr, 0, SP_LIMIT_U32_MAX);
  mz_bind_opts_t bind_opts = SP_ZERO_STRUCT(mz_bind_opts_t);
  bind_opts.required = opts.required;
  bind_opts.offset = opts.offset;
  mz_builder_add_field_ex(builder, key, array_schema, bind_opts);
  builder->stack[builder->depth++] = array_schema;
}

void mz_builder_push_map_ex(mz_builder_t* builder, const c8* key, mz_bind_opts_t opts) {
  mz_assert(builder);
  mz_assert(builder->depth > 0);
  mz_assert(builder->depth < SP_CARR_LEN(builder->stack));
  mz_assert(key);

  mz_schema_t* map_schema = mz_schema_map_internal(mz_nullptr, opts.on_alloc, mz_nullptr);
  mz_bind_opts_t bind_opts = SP_ZERO_STRUCT(mz_bind_opts_t);
  bind_opts.required = opts.required;
  bind_opts.offset = opts.offset;
  mz_builder_add_field_ex(builder, key, map_schema, bind_opts);
  builder->stack[builder->depth++] = map_schema;
}

void mz_builder_add_entry_ex(mz_builder_t* builder, mz_schema_t* entry, mz_entry_opts_t opts) {
  mz_assert(builder);
  mz_assert(builder->depth > 0);

  mz_schema_t* current = builder->stack[builder->depth - 1];
  mz_assert(current->kind == MZ_SCHEMA_ARRAY || current->kind == MZ_SCHEMA_MAP);
  mz_assert(!mz_schema_collection_has_entry(current));
  mz_schema_collection_set_entry(current, entry, opts.on_parse);
}

void mz_builder_push_entry_object_ex(mz_builder_t* builder, mz_object_mode_t mode, mz_entry_opts_t opts) {
  mz_assert(builder);
  mz_assert(builder->depth > 0);
  mz_assert(builder->depth < SP_CARR_LEN(builder->stack));

  mz_schema_t* object = mz_schema_object(mode);
  mz_builder_add_entry_ex(builder, object, opts);
  builder->stack[builder->depth++] = object;
}

void mz_builder_add_field_ex(mz_builder_t* builder, const c8* key, mz_schema_t* field, mz_bind_opts_t opts) {
  mz_assert(builder);
  mz_assert(builder->depth > 0);
  mz_schema_t* current = builder->stack[builder->depth - 1];
  mz_assert(current->kind == MZ_SCHEMA_OBJECT);
  mz_object_add_field_ex(current, key, field, opts);
}

void mz_builder_add_field_ptr_ex(mz_builder_t* builder, const c8* key, mz_schema_t* field, mz_bind_opts_t opts) {
  mz_assert(builder);
  mz_assert(builder->depth > 0);
  mz_schema_t* current = builder->stack[builder->depth - 1];
  mz_assert(current->kind == MZ_SCHEMA_OBJECT);
  mz_object_add_field_ptr_ex(current, key, field, opts);
}

void mz_builder_pop(mz_builder_t* builder) {
  mz_assert(builder);
  mz_assert(builder->depth > 0);

  mz_schema_t* current = builder->stack[builder->depth - 1];
  if (current->kind == MZ_SCHEMA_ARRAY || current->kind == MZ_SCHEMA_MAP) {
    mz_assert(mz_schema_collection_has_entry(current));
  }

  builder->depth--;
}

mz_ctx_t* mz_ctx_create() {
  mz_ctx_t* ctx = sp_alloc_type(mz_ctx_t);
  mz_ctx_init(ctx);
  return ctx;
}
mz_ctx_t* mz_ctx_create_ex(sp_allocator_t allocator) {
  mz_ctx_t* ctx = sp_alloc_type(mz_ctx_t);
  mz_ctx_init_ex(ctx, allocator);
  return ctx;

}

void mz_ctx_init_ex(mz_ctx_t* ctx, sp_allocator_t allocator) {
  *ctx = mz_zero_s(mz_ctx_t);
  ctx->allocator = allocator;
  ctx->diag.kind = MZ_OK;

  sp_context_push_allocator(ctx->allocator);
  ctx->arena = sp_mem_arena_new(SP_MEM_ARENA_BLOCK_SIZE);
  sp_context_pop();
}

void mz_ctx_init(mz_ctx_t* ctx) {
  mz_ctx_init_ex(ctx, sp_context_get()->allocator);
}

void mz_ctx_clear(mz_ctx_t* ctx) {
  sp_mem_arena_clear(ctx->arena);
  mz_diag_reset(ctx);
}

void mz_ctx_deinit(mz_ctx_t* ctx) {
  mz_assert(ctx);
  if (!ctx->arena) {
    return;
  }

  sp_mem_arena_destroy(ctx->arena);
  ctx->arena = mz_nullptr;
}

void mz_ctx_destroy(mz_ctx_t* ctx) {
  mz_ctx_deinit(ctx);
  sp_free(ctx);
}

static void mz_diag_reset(mz_ctx_t* ctx) {
  if (!ctx) { // @spader no?
    return;
  }

  ctx->diag = mz_zero_s(mz_diag_t); // @spader DI
  ctx->diag.kind = MZ_OK;

  ctx->diag_parts_len = 0;
  ctx->diag_path[0] = '\0';
}

static mz_err_t mz_eval_document(mz_ctx_t* ctx, mz_json_doc_t* doc, mz_schema_t* schema, void* out) {
  mz_diag_reset(ctx);

  mz_json_cursor_t cursor = mz_backend_get_root(doc);

  if (!schema) {
    mz_err_t err = mz_diag_set(ctx, MZ_ERR_TYPE);
    mz_diag_finalize(ctx);
    return err;
  }

  if (mz_backend_get_value_kind(&cursor) == MZ_JSON_KIND_INVALID) { // @spader merge into eval_schema?
    mz_err_t err = mz_diag_set(ctx, MZ_ERR_JSON);
    mz_diag_finalize(ctx);
    return err;
  }

  sp_context_push_allocator(sp_mem_arena_as_allocator(ctx->arena));
  mz_err_t err = mz_eval_schema(ctx, schema, &cursor, out);
  sp_context_pop();

  if (err != MZ_OK) {
    mz_diag_finalize(ctx);
  }

  return err;
}

mz_nstr_t mz_nstr_view(const c8* cstr) {
  return (mz_nstr_t) {
    .data = cstr,
    .len = sp_cstr_len(cstr),
  };
}

mz_buf_t mz_read_file(mz_str_t path) {
  sp_io_reader_t reader = sp_io_reader_from_file(path);
  if (reader.file.fd <= 0) {
    return mz_zero_s(mz_buf_t);
  }

  u64 size = sp_io_reader_size(&reader);
  if (size == 0) {
    sp_io_reader_close(&reader);
    return mz_zero_s(mz_buf_t);
  }

  u8* data = mz_alloc_n(u8, size + 1);
  u64 bytes_read = sp_io_read(&reader, data, size);
  sp_io_reader_close(&reader);

  return (mz_buf_t) {
    .data = data,
    .len = size,
    .capacity = size + 1
  };
}

static mz_err_t mz_run(mz_ctx_t* ctx, mz_schema_t* schema, mz_json_doc_t* doc, void* out) {
  mz_err_t err = mz_eval_document(ctx, doc, schema, out);
  mz_backend_free_document(doc);
  return err;
}

static mz_err_t mz_run_file_ex(mz_ctx_t* ctx, mz_schema_t* schema, mz_nstr_t file, void* out) {
  mz_json_doc_t* doc = mz_nullptr;
  mz_try(mz_backend_load_file(file, &doc));
  return mz_run(ctx, schema, doc, out);
}

static mz_err_t mz_run_str_ex(mz_ctx_t* ctx, mz_schema_t* schema, mz_nstr_t json, void* out) {
  mz_json_doc_t* doc = mz_nullptr;
  mz_try(mz_backend_load_str(json, &doc));
  return mz_run(ctx, schema, doc, out);
}

mz_err_t mz_validate_str_ex(mz_schema_t* schema, const c8* json, mz_ctx_t* ctx) {
  return mz_run_str_ex(ctx, schema, mz_nstr_view(json), mz_nullptr);
}

mz_err_t mz_parse_str_ex(mz_schema_t* schema, const c8* json, void* out, mz_ctx_t* ctx) {
  return mz_run_str_ex(ctx, schema, mz_nstr_view(json), out);
}

mz_backend_t mz_query_backend(void) {
  #if defined(MZ_BACKEND_JANSSON)
    return MZ_BACKEND_KIND_JANSSON;
  #elif defined(MZ_BACKEND_CJSON)
    return MZ_BACKEND_KIND_CJSON;
  #elif defined(MZ_BACKEND_YYJSON)
    return MZ_BACKEND_KIND_YYJSON;
  #elif defined(MZ_BACKEND_RAPIDJSON)
    return MZ_BACKEND_KIND_RAPIDJSON;
  #elif defined(MZ_BACKEND_GLAZE)
    return MZ_BACKEND_KIND_GLAZE;
  #elif defined(MZ_BACKEND_SIMDJSON)
    return MZ_BACKEND_KIND_SIMDJSON;
  #else
    return MZ_BACKEND_KIND_CUSTOM;
  #endif
}

SP_END_EXTERN_C()
