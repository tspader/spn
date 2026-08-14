#include "pkg/load.h"

#include "codegen/lower.h"
#include "toml/loader.h"

spn_err_t spn_pkg_load(sp_mem_t mem, sp_intern_t* intern, sp_str_t path, spn_manifest_role_t role, spn_pkg_info_t* pkg, spn_codegen_issues_t* issues) {
  *issues = SP_NULLPTR;
  if (!sp_fs_exists(path)) {
    return SPN_ERR_NO_MANIFEST;
  }

  spn_toml_loader_t t = sp_zero;
  spn_toml_loader_init(&t, mem, intern);

  spn_err_t err = spn_codegen_load_pkg(&t, path, pkg);
  if (!err) {
    err = role == SPN_MANIFEST_ROOT ? spn_pkg_lower_patch_hashes(&t, pkg) : spn_pkg_reject_patches(&t, pkg);
  }

  if (err) {
    *issues = t.issues;
    return SPN_ERR_MANIFEST_ISSUES;
  }
  return SPN_OK;
}
