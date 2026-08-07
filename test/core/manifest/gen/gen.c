#include "spn_test.h"

#include "codegen/codegen.h"
#include "intern/intern.h"
#include "toml/loader.h"
#include "manifest.gen.h"

typedef struct {
  const c8* name;
} test_t;

static const test_t tests [] = {
  { "all_target_kinds" },
  { "array_empty_string_element" },
  { "bin_single" },
  { "deps_all_empty_omitted" },
  { "deps_all_tables" },
  { "deps_empty_value" },
  { "deps_mixed_empty" },
  { "deps_package" },
  { "empty_manifest" },
  { "escape_control_high" },
  { "escape_control" },
  { "escape_special" },
  { "escape_unicode" },
  { "field_order_normalized" },
  { "index_optional_source" },
  { "index_multiple" },
  { "index_single" },
  { "kitchen_sink" },
  { "lib_cxx" },
  { "malformed" },
  { "minimal" },
  { "missing_required_version" },
  { "named_sorted" },
  { "optional_empty_string_omitted" },
  { "option_invalid" },
  { "options" },
  { "package_all_scalars" },
  { "package_arrays" },
  { "package_empty_arrays_omitted" },
  { "profile_empty_object" },
  { "profile_multiple" },
  { "profile_opt_sanitize" },
  { "profile_opt_wrong_type" },
  { "profile_sanitize_invalid" },
  { "profile_single" },
  { "required_empty_string_kept" },
  { "semver_normalize" },
  { "target_all_fields" },
  { "target_link_false" },
  { "target_multiple" },
  { "toolchain_launcher_args" },
  { "toolchain_scalars" },
  { "toolchain_triples" },
  { "triple_subset" },
  { "when_dep" },
  { "when_empty" },
  { "when_invalid" },
  { "when_not" },
  { "when_source" },
  { "when_values" },
};

static spn_toml_loader_t loader_new(sp_mem_t mem) {
  spn_toml_loader_t ctx = sp_zero;
  spn_toml_loader_init(&ctx, mem, sp_intern_new(mem));
  return ctx;
}

static sp_str_t manifest_render(sp_mem_t mem, sp_str_t path) {
  spn_toml_loader_t ctx = loader_new(mem);

  spn_cg_manifest_t manifest = sp_zero;
  if (spn_codegen_load(&ctx, path, &manifest)) {
    return spn_codegen_issues_to_str(mem, ctx.issues);
  }

  return spn_manifest_write(mem, &manifest);
}

sp_test_each(manifest_gen, corpus, test_t, tests) {
  sp_mem_t mem = sp_test_arena(t);

  sp_str_t path = test_repo_path(mem, sp_fmt(mem, "test/core/manifest/gen/manifests/{}.toml", sp_fmt_cstr(it->name)).value);
  sp_expect_golden(t, sp_test_format(t, "golden/{}.json", sp_fmt_cstr(it->name)), manifest_render(mem, path));

  return SP_OK;
}

sp_test(manifest_gen, missing_file) {
  sp_mem_t mem = sp_test_arena(t);
  spn_toml_loader_t ctx = loader_new(mem);

  spn_cg_manifest_t manifest = sp_zero;
  sp_must(t, spn_codegen_load(&ctx, sp_str_lit("/nonexistent/missing.toml"), &manifest));
  sp_must_eq(t, 1, sp_da_size(ctx.issues));
  sp_expect_eq(t, SPN_ERR_CODEGEN_FILE_MISSING, ctx.issues[0].code);
  sp_expect_str_eq_c(t, ctx.issues[0].detail, "missing file");

  return SP_OK;
}
