#ifndef SPN_UNIT_UNIT_H
#define SPN_UNIT_UNIT_H

#include "sp.h"
#include "spn/core.h"
#include "session/types.h"
#include "target/types.h"
#include "unit/types.h"

spn_profile_info_t spn_build_metaprogram_profile();
spn_build_id_t     spn_build_id(const spn_profile_info_t* profile);
spn_err_t          spn_build_add(spn_session_t* session, spn_profile_info_t profile, spn_path_t root, spn_toolchain_info_t* toolchain, spn_build_unit_t** build);

spn_err_t spn_units_add_packages(spn_session_t* session);

// The metaprogram scope runs before the configure graph, the target scope after
typedef enum {
  SPN_UNIT_SCOPE_METAPROGRAM,
  SPN_UNIT_SCOPE_TARGET,
} spn_unit_scope_t;
spn_err_t spn_units_add_targets(spn_session_t* session, spn_unit_scope_t scope);

spn_err_t spn_target_link_invocation(sp_mem_t mem, spn_target_unit_t* target, const spn_cc_link_files_t* files, spn_invocation_t* invocation);

sp_da(spn_closure_entry_t) spn_target_link_closure(sp_mem_t mem, spn_target_unit_t* root);
sp_da(spn_target_unit_t*)  spn_target_runtime_libs(sp_mem_t mem, spn_target_unit_t* root);
sp_da(spn_link_lib_t)      spn_closure_get_linked_libs(sp_mem_t mem, sp_da(spn_closure_entry_t) closure);
bool spn_dep_kind_applies(spn_dep_kind_t dep, spn_target_kind_t target);

void spn_unit_paths_init(spn_pkg_unit_t* unit, spn_loaded_pkg_t* loaded);
spn_path_t spn_target_unit_object_dir(sp_mem_t mem, spn_target_unit_t* target);

// A script host exists in the metaprogram build only to compile its package's
// scripts; it is not consumed there, so it configures and packages nothing
static inline bool spn_pkg_unit_is_script_host(spn_pkg_unit_t* unit) {
  return !(unit->kinds & (spn_dep_kind_bit(SPN_DEP_KIND_PACKAGE) | spn_dep_kind_bit(SPN_DEP_KIND_TEST)));
}

sp_hash_t spn_unit_fingerprint(spn_session_t* session, spn_build_unit_t* build, spn_pkg_id_t id);
sp_str_t  spn_unit_fingerprint_str(sp_mem_t mem, sp_hash_t fingerprint);

#endif
