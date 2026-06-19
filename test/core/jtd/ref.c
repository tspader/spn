#include "jtd_test.h"

static void compare_ref_ok(s32* utest_result, const jtd_result_t* root, const void* expect) {
  EXPECT_EQ((s32)JTD_FORM_REF, (s32)root->root->form);
  jtd_expect_str(utest_result, root->root->as.ref.name, "str");

  jtd_schema_t* def = jtd_definition(root, sp_str_lit("str"));
  EXPECT_TRUE(def != SP_NULLPTR);
  if (def) {
    EXPECT_EQ((s32)JTD_FORM_TYPE, (s32)def->form);
    EXPECT_TRUE(jtd_resolve(root, root->root) == def);
  }
  EXPECT_TRUE(jtd_resolve(root, SP_NULLPTR) == SP_NULLPTR);
  EXPECT_TRUE(jtd_definition(root, sp_str_lit("absent")) == SP_NULLPTR);
}

static void compare_ref_chain(s32* utest_result, const jtd_result_t* root, const void* expect) {
  jtd_schema_t* shallow = jtd_resolve(root, root->root);
  EXPECT_TRUE(shallow != SP_NULLPTR);
  if (shallow) {
    EXPECT_EQ((s32)JTD_FORM_REF, (s32)shallow->form);
    jtd_expect_str(utest_result, shallow->as.ref.name, "b");
  }

  jtd_diagnostic_t diag = sp_zero;
  jtd_schema_t* deep = jtd_resolve_deep(sp_mem_get_scratch(), root, root->root, &diag);
  EXPECT_TRUE(deep != SP_NULLPTR);
  if (deep) {
    EXPECT_EQ((s32)JTD_FORM_TYPE, (s32)deep->form);
    EXPECT_EQ((s32)JTD_TYPE_STRING, (s32)deep->as.type);
  }
}

static void compare_ref_cycle(s32* utest_result, const jtd_result_t* root, const void* expect) {
  jtd_diagnostic_t diag = sp_zero;
  jtd_schema_t* deep = jtd_resolve_deep(sp_mem_get_scratch(), root, root->root, &diag);
  EXPECT_TRUE(deep == SP_NULLPTR);
  EXPECT_EQ((s32)JTD_ERR_REF_CYCLE, (s32)diag.code);
  jtd_expect_str(utest_result, diag.path, "#/definitions/a");
}

static void compare_ref_recursive_properties(s32* utest_result, const jtd_result_t* root, const void* expect) {
  jtd_schema_t* deep = jtd_resolve_deep(sp_mem_get_scratch(), root, root->root, SP_NULLPTR);
  EXPECT_TRUE(deep != SP_NULLPTR);
  if (deep) {
    EXPECT_EQ((s32)JTD_FORM_PROPERTIES, (s32)deep->form);
    EXPECT_EQ((u64)1, (u64)sp_da_size(deep->as.properties.optional));

    if (sp_da_size(deep->as.properties.optional) == 1) {
      jtd_property_t* next = &deep->as.properties.optional[0];
      jtd_expect_str(utest_result, next->key, "next");

      jtd_schema_t* resolved = jtd_resolve_deep(sp_mem_get_scratch(), root, next->schema, SP_NULLPTR);
      EXPECT_TRUE(resolved == deep);
    }
  }
}

UTEST(ref, resolves_definition) {
  run_jtd_case(utest_result, (jtd_case_t){
    .json    = "ref.ok.json",
    .compare = compare_ref_ok,
  });
}

UTEST(ref, resolves_ref_chain_deep) {
  run_jtd_case(utest_result, (jtd_case_t){
    .json    = "ref.chain.json",
    .compare = compare_ref_chain,
  });
}

UTEST(ref, detects_ref_cycle_deep) {
  run_jtd_case(utest_result, (jtd_case_t){
    .json    = "ref.cycle.json",
    .compare = compare_ref_cycle,
  });
}

UTEST(ref, resolves_recursive_schema_ref_deep) {
  run_jtd_case(utest_result, (jtd_case_t){
    .json    = "ref.recursive_properties.json",
    .compare = compare_ref_recursive_properties,
  });
}

UTEST(ref, not_string) {
  run_jtd_case(utest_result, (jtd_case_t){
    .json       = "ref.not_string.json",
    .error      = JTD_ERR_REF_NOT_STRING,
    .error_path = "#",
  });
}

UTEST(ref, unresolved) {
  run_jtd_case(utest_result, (jtd_case_t){
    .json       = "ref.unresolved.json",
    .error      = JTD_ERR_REF_UNRESOLVED,
    .error_path = "#",
  });
}

UTEST(ref, unresolved_nested) {
  run_jtd_case(utest_result, (jtd_case_t){
    .json       = "ref.unresolved_nested.json",
    .error      = JTD_ERR_REF_UNRESOLVED,
    .error_path = "#/elements",
  });
}

UTEST(ref, definitions_not_object) {
  run_jtd_case(utest_result, (jtd_case_t){
    .json       = "ref.definitions_not_object.json",
    .error      = JTD_ERR_SCHEMA_NOT_OBJECT,
    .error_path = "#/definitions",
  });
}

UTEST(ref, definition_nested_error_path) {
  run_jtd_case(utest_result, (jtd_case_t){
    .json       = "ref.definition_nested_error.json",
    .error      = JTD_ERR_INVALID_TYPE,
    .error_path = "#/definitions/foo",
  });
}

UTEST(ref, definition_escaped_error_path) {
  run_jtd_case(utest_result, (jtd_case_t){
    .json       = "ref.definition_escaped_error.json",
    .error      = JTD_ERR_INVALID_TYPE,
    .error_path = "#/definitions/a~1b",
  });
}

UTEST(ref, nested_definitions) {
  run_jtd_case(utest_result, (jtd_case_t){
    .json       = "ref.nested_definitions.json",
    .error      = JTD_ERR_DEFINITIONS_NOT_ROOT,
    .error_path = "#/properties/x/definitions",
  });
}
