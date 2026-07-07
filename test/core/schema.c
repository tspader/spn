#define SP_IMPLEMENTATION
#include "sp.h"

#define UTEST_IMPLEMENTATION
#include "utest.h"

#include "jtd.h"
#include "enum/enum.h"
#include "yyjson.h"

UTEST_MAIN();

typedef struct {
  const c8* file;
  const c8* def;
  u32 (*from)(sp_str_t);
  sp_str_t (*to)(u32);
} schema_enum_t;

static u32 schema_arch_from(sp_str_t str) { return spn_arch_from_str(str); }
static u32 schema_os_from(sp_str_t str) { return spn_os_from_str(str); }
static u32 schema_abi_from(sp_str_t str) { return spn_abi_from_str(str); }
static u32 schema_cc_driver_from(sp_str_t str) { return spn_cc_driver_from_str(str); }
static u32 schema_linkage_from(sp_str_t str) { return spn_linkage_from_str(str); }
static u32 schema_c_standard_from(sp_str_t str) { return spn_c_standard_from_str(str); }
static u32 schema_cxx_standard_from(sp_str_t str) { return spn_cxx_standard_from_str(str); }
static u32 schema_build_mode_from(sp_str_t str) { return spn_build_mode_from_str(str); }
static u32 schema_index_protocol_from(sp_str_t str) { return spn_index_protocol_from_str(str); }
static u32 schema_option_type_from(sp_str_t str) { return spn_option_type_from_str(str); }
static u32 schema_index_dep_kind_from(sp_str_t str) { return spn_index_dep_kind_from_str(str); }

static sp_str_t schema_arch_to(u32 value) { return spn_arch_to_str((spn_arch_t)value); }
static sp_str_t schema_os_to(u32 value) { return spn_os_to_str((spn_os_t)value); }
static sp_str_t schema_abi_to(u32 value) { return spn_abi_to_str((spn_abi_t)value); }
static sp_str_t schema_cc_driver_to(u32 value) { return spn_cc_driver_to_str((spn_cc_driver_t)value); }
static sp_str_t schema_linkage_to(u32 value) { return spn_linkage_to_str((spn_linkage_t)value); }
static sp_str_t schema_c_standard_to(u32 value) { return spn_c_standard_to_str((spn_c_standard_t)value); }
static sp_str_t schema_cxx_standard_to(u32 value) { return spn_cxx_standard_to_str((spn_cxx_standard_t)value); }
static sp_str_t schema_build_mode_to(u32 value) { return spn_build_mode_to_str((spn_build_mode_t)value); }
static sp_str_t schema_index_protocol_to(u32 value) { return spn_index_protocol_to_str((spn_index_protocol_t)value); }
static sp_str_t schema_option_type_to(u32 value) { return spn_option_type_to_str((spn_option_type_t)value); }
static sp_str_t schema_index_dep_kind_to(u32 value) { return spn_index_dep_kind_to_str((spn_index_dep_kind_t)value); }

void run_schema_enum_test(s32* utest_result, schema_enum_t t) {
  sp_mem_t mem = sp_mem_os_new();

  sp_str_t path = sp_fs_join_path(mem, sp_str_lit(SCHEMA_GEN_DIR), sp_cstr_as_str(t.file));
  jtd_result_t jtd = sp_zero;
  EXPECT_EQ(JTD_OK, jtd_parse_file(mem, path, &jtd));
  if (!jtd.ok) {
    return;
  }

  jtd_schema_t* def = jtd_definition(&jtd, sp_cstr_as_str(t.def));
  EXPECT_TRUE(def != SP_NULLPTR);
  if (!def) {
    return;
  }
  EXPECT_EQ(JTD_FORM_ENUM, def->form);
  if (def->form != JTD_FORM_ENUM) {
    return;
  }

  EXPECT_FALSE(sp_da_empty(def->as.enumeration.values));
  sp_da_for(def->as.enumeration.values, it) {
    sp_str_t value = def->as.enumeration.values[it];
    EXPECT_TRUE(sp_str_equal(t.to(t.from(value)), value));
  }
}

UTEST(schema, arch) {
  run_schema_enum_test(utest_result, (schema_enum_t) {
    .file = "manifest.jtd.json",
    .def = "arch",
    .from = schema_arch_from,
    .to = schema_arch_to,
  });
}

UTEST(schema, os) {
  run_schema_enum_test(utest_result, (schema_enum_t) {
    .file = "manifest.jtd.json",
    .def = "os",
    .from = schema_os_from,
    .to = schema_os_to,
  });
}

UTEST(schema, abi) {
  run_schema_enum_test(utest_result, (schema_enum_t) {
    .file = "manifest.jtd.json",
    .def = "abi",
    .from = schema_abi_from,
    .to = schema_abi_to,
  });
}

UTEST(schema, cc_driver) {
  run_schema_enum_test(utest_result, (schema_enum_t) {
    .file = "toolchains.jtd.json",
    .def = "cc_driver",
    .from = schema_cc_driver_from,
    .to = schema_cc_driver_to,
  });
}

UTEST(schema, linkage) {
  run_schema_enum_test(utest_result, (schema_enum_t) {
    .file = "manifest.jtd.json",
    .def = "linkage",
    .from = schema_linkage_from,
    .to = schema_linkage_to,
  });
}

UTEST(schema, c_standard) {
  run_schema_enum_test(utest_result, (schema_enum_t) {
    .file = "manifest.jtd.json",
    .def = "c_standard",
    .from = schema_c_standard_from,
    .to = schema_c_standard_to,
  });
}

UTEST(schema, cxx_standard) {
  run_schema_enum_test(utest_result, (schema_enum_t) {
    .file = "manifest.jtd.json",
    .def = "cxx_standard",
    .from = schema_cxx_standard_from,
    .to = schema_cxx_standard_to,
  });
}

UTEST(schema, build_mode) {
  run_schema_enum_test(utest_result, (schema_enum_t) {
    .file = "manifest.jtd.json",
    .def = "build_mode",
    .from = schema_build_mode_from,
    .to = schema_build_mode_to,
  });
}

UTEST(schema, option_type) {
  run_schema_enum_test(utest_result, (schema_enum_t) {
    .file = "manifest.jtd.json",
    .def = "option_type",
    .from = schema_option_type_from,
    .to = schema_option_type_to,
  });
}

UTEST(schema, index_protocol) {
  run_schema_enum_test(utest_result, (schema_enum_t) {
    .file = "manifest.jtd.json",
    .def = "index_protocol",
    .from = schema_index_protocol_from,
    .to = schema_index_protocol_to,
  });
}

UTEST(schema, index_dep_kind) {
  run_schema_enum_test(utest_result, (schema_enum_t) {
    .file = "release.jtd.json",
    .def = "index_dep_kind",
    .from = schema_index_dep_kind_from,
    .to = schema_index_dep_kind_to,
  });
}

UTEST(schema, abi_handles) {
  const c8* kinds [] = { "ctx", "config", "target", "node", "node_ctx", "profile", "make", "autoconf", "cmake" };

  sp_mem_t mem = sp_mem_os_new();
  sp_str_t path = sp_fs_join_path(mem, sp_str_lit(SCHEMA_DIR), sp_str_lit("abi.json"));
  sp_str_t json = sp_zero;
  ASSERT_EQ(SP_OK, sp_io_read_file(mem, path, &json));

  yyjson_doc* doc = yyjson_read(json.data, json.len, 0);
  ASSERT_TRUE(doc != SP_NULLPTR);

  yyjson_val* handles = yyjson_obj_get(yyjson_doc_get_root(doc), "handles");
  ASSERT_EQ((u32)sp_carr_len(kinds), (u32)yyjson_arr_size(handles));
  sp_carr_for(kinds, it) {
    EXPECT_TRUE(sp_cstr_equal(kinds[it], yyjson_get_str(yyjson_arr_get(handles, it))));
  }
  yyjson_doc_free(doc);
}
