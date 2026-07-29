#include "jtd_test.h"

static sp_err_t compare_ref_ok(sp_test_t* t, const jtd_result_t* root, const void* expect) {
  (void)expect;
  sp_expect_eq(t, (s32)JTD_FORM_REF, (s32)root->root->form);
  sp_expect_str_eq_c(t, root->root->as.ref.name, "str");

  jtd_schema_t* def = jtd_definition(root, sp_str_lit("str"));
  sp_must(t, def != SP_NULLPTR);
  sp_expect_eq(t, (s32)JTD_FORM_TYPE, (s32)def->form);
  sp_expect(t, jtd_resolve(root, root->root) == def);
  sp_expect(t, jtd_resolve(root, SP_NULLPTR) == SP_NULLPTR);
  sp_expect(t, jtd_definition(root, sp_str_lit("absent")) == SP_NULLPTR);
  return SP_OK;
}

static sp_err_t compare_ref_chain(sp_test_t* t, const jtd_result_t* root, const void* expect) {
  (void)expect;
  jtd_schema_t* shallow = jtd_resolve(root, root->root);
  sp_must(t, shallow != SP_NULLPTR);
  sp_expect_eq(t, (s32)JTD_FORM_REF, (s32)shallow->form);
  sp_expect_str_eq_c(t, shallow->as.ref.name, "b");

  jtd_diagnostic_t diag = sp_zero;
  jtd_schema_t* deep = jtd_resolve_deep(sp_test_arena(t), root, root->root, &diag);
  sp_must(t, deep != SP_NULLPTR);
  sp_expect_eq(t, (s32)JTD_FORM_TYPE, (s32)deep->form);
  sp_expect_eq(t, (s32)JTD_TYPE_STRING, (s32)deep->as.type);
  return SP_OK;
}

static sp_err_t compare_ref_cycle(sp_test_t* t, const jtd_result_t* root, const void* expect) {
  (void)expect;
  jtd_diagnostic_t diag = sp_zero;
  jtd_schema_t* deep = jtd_resolve_deep(sp_test_arena(t), root, root->root, &diag);
  sp_expect(t, deep == SP_NULLPTR);
  sp_expect_eq(t, (s32)JTD_ERR_REF_CYCLE, (s32)diag.code);
  sp_expect_str_eq_c(t, diag.path, "#/definitions/a");
  return SP_OK;
}

static sp_err_t compare_ref_recursive_properties(sp_test_t* t, const jtd_result_t* root, const void* expect) {
  (void)expect;
  jtd_schema_t* deep = jtd_resolve_deep(sp_test_arena(t), root, root->root, SP_NULLPTR);
  sp_must(t, deep != SP_NULLPTR);
  sp_expect_eq(t, (s32)JTD_FORM_PROPERTIES, (s32)deep->form);
  sp_must_eq(t, (u64)1, (u64)sp_da_size(deep->as.properties.optional));

  jtd_property_t* next = &deep->as.properties.optional[0];
  sp_expect_str_eq_c(t, next->key, "next");

  jtd_schema_t* resolved = jtd_resolve_deep(sp_test_arena(t), root, next->schema, SP_NULLPTR);
  sp_expect(t, resolved == deep);
  return SP_OK;
}

static const jtd_case_t cases [] = {
  {
    .name    = "resolves_definition",
    .json    = "ref.ok.json",
    .compare = compare_ref_ok,
  },
  {
    .name    = "resolves_ref_chain_deep",
    .json    = "ref.chain.json",
    .compare = compare_ref_chain,
  },
  {
    .name    = "detects_ref_cycle_deep",
    .json    = "ref.cycle.json",
    .compare = compare_ref_cycle,
  },
  {
    .name    = "resolves_recursive_schema_ref_deep",
    .json    = "ref.recursive_properties.json",
    .compare = compare_ref_recursive_properties,
  },
  {
    .name       = "not_string",
    .json       = "ref.not_string.json",
    .error      = JTD_ERR_REF_NOT_STRING,
    .error_path = "#",
  },
  {
    .name       = "unresolved",
    .json       = "ref.unresolved.json",
    .error      = JTD_ERR_REF_UNRESOLVED,
    .error_path = "#",
  },
  {
    .name       = "unresolved_nested",
    .json       = "ref.unresolved_nested.json",
    .error      = JTD_ERR_REF_UNRESOLVED,
    .error_path = "#/elements",
  },
  {
    .name       = "definitions_not_object",
    .json       = "ref.definitions_not_object.json",
    .error      = JTD_ERR_SCHEMA_NOT_OBJECT,
    .error_path = "#/definitions",
  },
  {
    .name       = "definition_nested_error_path",
    .json       = "ref.definition_nested_error.json",
    .error      = JTD_ERR_UNKNOWN_TYPE,
    .error_path = "#/definitions/foo",
  },
  {
    .name       = "definition_escaped_error_path",
    .json       = "ref.definition_escaped_error.json",
    .error      = JTD_ERR_UNKNOWN_TYPE,
    .error_path = "#/definitions/a~1b",
  },
  {
    .name       = "nested_definitions",
    .json       = "ref.nested_definitions.json",
    .error      = JTD_ERR_DEFINITIONS_NOT_ROOT,
    .error_path = "#/properties/x/definitions",
  },
};

sp_test_each_fn(ref, parse, jtd_case_t, cases, run_jtd_case);
