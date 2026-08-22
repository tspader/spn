#include "spn_test.h"

#include "external/tom.h"
#include "version.h"

sp_test(core, version_matches_manifest) {
  sp_mem_t mem = sp_test_arena(t);

  toml_table_t* root = spn_toml_parse(sp_str_lit("spn.toml"));
  sp_must(t, root != SP_NULLPTR);

  toml_table_t* package = toml_table_table(root, "package");
  sp_must(t, package != SP_NULLPTR);

  sp_expect_str_eq_c(t, spn_toml_str(mem, package, "version"), SPN_VERSION);

  toml_free(root);
  return SP_OK;
}
