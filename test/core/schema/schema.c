#include "spn_test.h"

#include "jtd.h"
#include "enum/enum.h"
#include "yyjson.h"

#define SCHEMA_ENUMS \
  X(arch,           "manifest.jtd.json") \
  X(os,             "manifest.jtd.json") \
  X(abi,            "manifest.jtd.json") \
  X(cc_driver,      "toolchains.jtd.json") \
  X(linkage,        "manifest.jtd.json") \
  X(c_standard,     "manifest.jtd.json") \
  X(cxx_standard,   "manifest.jtd.json") \
  X(build_mode,     "manifest.jtd.json") \
  X(option_type,    "manifest.jtd.json") \
  X(index_protocol, "manifest.jtd.json") \
  X(index_dep_kind, "release.jtd.json")

#define X(ID, FILE) \
  static u32 schema_##ID##_from(sp_str_t str) { return spn_##ID##_from_str(str); } \
  static sp_str_t schema_##ID##_to(u32 value) { return spn_##ID##_to_str((spn_##ID##_t)value); }
SCHEMA_ENUMS
#undef X

typedef struct {
  const c8* name;
  const c8* file;
  const c8* def;
  u32 (*from)(sp_str_t);
  sp_str_t (*to)(u32);
} test_t;

static const test_t tests [] = {
  #define X(ID, FILE) { .name = #ID, .file = FILE, .def = #ID, .from = schema_##ID##_from, .to = schema_##ID##_to },
  SCHEMA_ENUMS
  #undef X
};

sp_test_each(schema, roundtrip, test_t, tests) {
  sp_mem_t mem = sp_test_arena(t);

  sp_str_t path = sp_fs_join_path(mem, test_repo_path(mem, sp_str_lit(SCHEMA_GEN_DIR)), sp_cstr_as_str(it->file));
  jtd_result_t jtd = sp_zero;
  sp_must_eq(t, JTD_OK, jtd_parse_file(mem, path, &jtd));

  jtd_schema_t* def = jtd_definition(&jtd, sp_cstr_as_str(it->def));
  sp_must(t, def != SP_NULLPTR);
  sp_must_eq(t, JTD_FORM_ENUM, def->form);

  sp_expect(t, !sp_da_empty(def->as.enumeration.values));
  sp_da_for(def->as.enumeration.values, v) {
    sp_str_t value = def->as.enumeration.values[v];
    sp_expect_str_eq(t, it->to(it->from(value)), value);
  }

  return SP_OK;
}

sp_test(schema, abi_handles) {
  const c8* kinds [] = { "ctx", "config", "target", "node", "node_ctx", "profile", "make", "autoconf", "cmake" };

  sp_mem_t mem = sp_test_arena(t);
  sp_str_t path = sp_fs_join_path(mem, test_repo_path(mem, sp_str_lit(SCHEMA_DIR)), sp_str_lit("abi.json"));
  sp_str_t json = sp_zero;
  sp_must_eq(t, SP_OK, sp_io_read_file(mem, path, &json));

  yyjson_doc* doc = yyjson_read(json.data, json.len, 0);
  sp_must(t, doc != SP_NULLPTR);

  yyjson_val* handles = yyjson_obj_get(yyjson_doc_get_root(doc), "handles");
  sp_must_eq(t, (u32)sp_carr_len(kinds), (u32)yyjson_arr_size(handles));
  sp_carr_for(kinds, k) {
    sp_expect(t, sp_cstr_equal(kinds[k], yyjson_get_str(yyjson_arr_get(handles, k))));
  }
  yyjson_doc_free(doc);

  return SP_OK;
}
