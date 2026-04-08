#ifndef SPN_PKG_MUTATE_H
#define SPN_PKG_MUTATE_H

#include "pkg/types.h"
#include "error/types.h"

void             spn_pkg_init(spn_pkg_t* pkg, sp_str_t name);
void             spn_pkg_set_name(spn_pkg_t* pkg, const c8* name);
void             spn_pkg_set_name_ex(spn_pkg_t* pkg, sp_str_t name);
void             spn_pkg_set_repo(spn_pkg_t* pkg, const c8* repo);
void             spn_pkg_set_repo_ex(spn_pkg_t* pkg, sp_str_t repo);
void             spn_pkg_set_url(spn_pkg_t* pkg, const c8* url);
void             spn_pkg_set_url_ex(spn_pkg_t* pkg, sp_str_t url);
void             spn_pkg_set_author(spn_pkg_t* pkg, const c8* author);
void             spn_pkg_set_author_ex(spn_pkg_t* pkg, sp_str_t author);
void             spn_pkg_set_maintainer(spn_pkg_t* pkg, const c8* maintainer);
void             spn_pkg_set_maintainer_ex(spn_pkg_t* pkg, sp_str_t maintainer);
void             spn_pkg_add_version(spn_pkg_t* pkg, const c8* version, const c8* commit);
void             spn_pkg_add_version_ex(spn_pkg_t* pkg, spn_semver_t version, sp_str_t commit);
void             spn_pkg_add_include(spn_pkg_t* pkg, const c8* path);
void             spn_pkg_add_include_ex(spn_pkg_t* pkg, sp_str_t path);
void             spn_pkg_add_define(spn_pkg_t* pkg, const c8* define);
void             spn_pkg_add_define_ex(spn_pkg_t* pkg, sp_str_t define);
void             spn_pkg_add_system_dep(spn_pkg_t* pkg, const c8* dep);
void             spn_pkg_add_system_dep_ex(spn_pkg_t* pkg, sp_str_t dep);
void             spn_pkg_add_linkage(spn_pkg_t* pkg, spn_linkage_t linkage);
spn_profile_info_t* spn_pkg_add_profile(spn_pkg_t* pkg, const c8* name);
spn_profile_info_t* spn_pkg_add_profile_ex(spn_pkg_t* pkg, spn_profile_info_t profile);
spn_index_t*     spn_pkg_add_index(spn_pkg_t* pkg, const c8* name, const c8* location);
spn_index_t*     spn_pkg_add_index_ex(spn_pkg_t* pkg, sp_str_t name, sp_str_t location);
spn_target_t*    spn_pkg_add_exe(spn_pkg_t* pkg, const c8* name);
spn_target_t*    spn_pkg_add_exe_ex(spn_pkg_t* pkg, sp_str_t name);
spn_target_t*    spn_pkg_add_script(spn_pkg_t* pkg, const c8* name);
spn_target_t*    spn_pkg_add_script_ex(spn_pkg_t* pkg, sp_str_t name);
spn_target_t*    spn_pkg_add_test(spn_pkg_t* pkg, const c8* name);
spn_target_t*    spn_pkg_add_test_ex(spn_pkg_t* pkg, sp_str_t name);
spn_target_t*    spn_pkg_add_lib(spn_pkg_t* pkg, const c8* name, spn_linkage_t kind);
spn_target_t*    spn_pkg_add_lib_ex(spn_pkg_t* pkg, sp_str_t name, spn_linkage_t kind);
spn_err_t        spn_pkg_add_toolchain(spn_pkg_t* pkg, spn_toolchain_entry_t entry);

#endif
