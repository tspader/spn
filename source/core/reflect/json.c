#include "reflect/reflect.h"
#include "yyjson.h"

static sp_str_t str_value(yyjson_val* value, sp_mem_t mem) {
  const c8* chars = yyjson_get_str(value);
  if (!chars) {
    return sp_str_lit("");
  }
  return sp_str_from_cstr_n(mem, chars, (u32)yyjson_get_len(value));
}

static void read_fields(yyjson_val* obj, const spn_reflect_type_t* type, void* value, sp_mem_t mem) {
  sp_for(it, type->num_fields) {
    const spn_reflect_field_t* field = &type->fields[it];
    void* slot = (u8*)value + field->offset;
    yyjson_val* json = yyjson_obj_get(obj, field->key);
    switch (field->kind) {
      case SPN_REFLECT_STR: {
        *(sp_str_t*)slot = str_value(json, mem);
        break;
      }
      case SPN_REFLECT_OBJECT: {
        if (yyjson_is_obj(json)) {
          read_fields(json, field->object, slot, mem);
        }
        break;
      }
      case SPN_REFLECT_STR_ARRAY: {
        sp_da(sp_str_t) items = sp_da_new(mem, sp_str_t);
        size_t idx, max;
        yyjson_val* element;
        yyjson_arr_foreach(json, idx, max, element) {
          if (yyjson_is_str(element)) {
            sp_da_push(items, str_value(element, mem));
          }
        }
        *(sp_da(sp_str_t)*)slot = items;
        break;
      }
      case SPN_REFLECT_OBJECT_ARRAY: {
        u32 stride = field->object->size;
        void* items = sp_da_init_ex(mem, stride);
        size_t idx, max;
        yyjson_val* element;
        yyjson_arr_foreach(json, idx, max, element) {
          if (!yyjson_is_obj(element)) {
            continue;
          }
          items = sp_da_grow_ex(items, stride, 1);
          void* entry = (u8*)items + sp_da_head(items)->size * stride;
          sp_mem_zero(entry, stride);
          read_fields(element, field->object, entry, mem);
          sp_da_head(items)->size += 1;
        }
        *(void**)slot = items;
        break;
      }
    }
  }
}

spn_err_t spn_reflect_json_read(sp_str_t json, const spn_reflect_type_t* type, void* value, sp_mem_t mem) {
  yyjson_doc* doc = yyjson_read(json.data, json.len, 0);
  if (!doc) {
    return SPN_ERROR;
  }
  yyjson_val* root = yyjson_doc_get_root(doc);
  if (!yyjson_is_obj(root)) {
    yyjson_doc_free(doc);
    return SPN_ERROR;
  }
  read_fields(root, type, value, mem);
  yyjson_doc_free(doc);
  return SPN_OK;
}
