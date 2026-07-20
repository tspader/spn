#include "toml/loader.h"
#include "codegen/lower.h"
#include "error/types.h"
#include "spn.h"

#define foo(_expr, __err, __u) \
  do { \
    spn_err_t __err = (_expr); \
    if (__err) { \
      return (spn_err_union_t) { \
        __err, __u, \
      }; \
    } \
  } while (0)

spn_err_union_t spn_toml_load_manifest(sp_mem_t mem, sp_intern_t* intern, sp_str_t path, spn_pkg_info_t* pkg) {
  spn_err_t err = SPN_OK;

  spn_toml_loader_t t = sp_zero;
  spn_toml_loader_init(&t, mem, intern);

  sp_try_goto(spn_codegen_load_pkg(&t, path, pkg), err, done);
  sp_try_goto(spn_pkg_lower_patch_hashes(&t, pkg), err, done);

done:
  if (err) {
    return (spn_err_union_t) {
      .kind = SPN_ERR_MANIFEST_ISSUES,
      .issues = t.issues
    };
  }
  return sp_zero_s(spn_err_union_t);
}
