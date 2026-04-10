#include "error/types.h"
#include "index/types.h"
#include "pkg/types.h"
#include "profile/types.h"
#include "toolchain/types.h"

#include "enum/enum.h"
#include "intern/intern.h"
#include "pkg/mutate.h"
#include "semver/compare.h"
#include "semver/convert.h"

void spn_pkg_init(spn_pkg_info_t* pkg, sp_str_t name) {
  pkg->arena = sp_mem_arena_new(4096);
  pkg->name = spn_intern(name);

  sp_ht_set_fns(pkg->deps, sp_ht_on_hash_str_key, sp_ht_on_compare_str_key);
  sp_ht_set_fns(pkg->options, sp_ht_on_hash_str_key, sp_ht_on_compare_str_key);
  sp_ht_set_fns(pkg->config, sp_ht_on_hash_str_key, sp_ht_on_compare_str_key);
}

void spn_pkg_set_name(spn_pkg_info_t* pkg, const c8* name) {
  spn_pkg_set_name_ex(pkg, sp_str_view(name));
}

void spn_pkg_set_name_ex(spn_pkg_info_t* pkg, sp_str_t name) {
  pkg->name = spn_intern(name);
}

void spn_pkg_set_repo(spn_pkg_info_t* pkg, const c8* repo) {
  spn_pkg_set_repo_ex(pkg, sp_str_view(repo));
}

void spn_pkg_set_repo_ex(spn_pkg_info_t* pkg, sp_str_t repo) {
  sp_context_push_arena(pkg->arena);
  pkg->repo = sp_str_copy(repo);
  sp_context_pop();
}

void spn_pkg_set_url(spn_pkg_info_t* pkg, const c8* url) {
  spn_pkg_set_url_ex(pkg, sp_str_view(url));
}

void spn_pkg_set_url_ex(spn_pkg_info_t* pkg, sp_str_t url) {
  sp_context_push_arena(pkg->arena);
  pkg->url = sp_str_copy(url);
  sp_context_pop();
}

void spn_pkg_set_author(spn_pkg_info_t* pkg, const c8* author) {
  spn_pkg_set_author_ex(pkg, sp_str_view(author));
}

void spn_pkg_set_author_ex(spn_pkg_info_t* pkg, sp_str_t author) {
  sp_context_push_arena(pkg->arena);
  pkg->author = sp_str_copy(author);
  sp_context_pop();
}

void spn_pkg_set_maintainer(spn_pkg_info_t* pkg, const c8* maintainer) {
  spn_pkg_set_maintainer_ex(pkg, sp_str_view(maintainer));
}

void spn_pkg_set_maintainer_ex(spn_pkg_info_t* pkg, sp_str_t maintainer) {
  sp_context_push_arena(pkg->arena);
  pkg->maintainer = sp_str_copy(maintainer);
  sp_context_pop();
}

void spn_pkg_add_version(spn_pkg_info_t* pkg, const c8* version, const c8* commit) {
  spn_pkg_add_version_ex(pkg, spn_semver_from_str(sp_str_view(version)), sp_str_view(commit));
}

void spn_pkg_add_version_ex(spn_pkg_info_t* pkg, spn_semver_t version, sp_str_t commit) {
  if (spn_semver_is_empty(pkg->version)) {
    pkg->version = version;
  }

  sp_context_push_arena(pkg->arena);
  spn_pkg_metadata_t metadata = { version, sp_str_copy(commit) };
  sp_ht_insert(pkg->metadata, version, metadata);
  sp_da_push(pkg->versions, version);
  sp_context_pop();
}

void spn_pkg_add_include(spn_pkg_info_t* pkg, const c8* include) {
  spn_pkg_add_include_ex(pkg, sp_str_view(include));
}

void spn_pkg_add_include_ex(spn_pkg_info_t* pkg, sp_str_t path) {
  sp_context_push_arena(pkg->arena);
  sp_da_push(pkg->include, sp_str_copy(path));
  sp_context_pop();
}

void spn_pkg_add_define(spn_pkg_info_t* pkg, const c8* define) {
  spn_pkg_add_define_ex(pkg, sp_str_view(define));
}

void spn_pkg_add_define_ex(spn_pkg_info_t* pkg, sp_str_t define) {
  sp_context_push_arena(pkg->arena);
  sp_da_push(pkg->define, sp_str_copy(define));
  sp_context_pop();
}

void spn_pkg_add_system_dep(spn_pkg_info_t* pkg, const c8* dep) {
  spn_pkg_add_system_dep_ex(pkg, sp_str_view(dep));
}

void spn_pkg_add_system_dep_ex(spn_pkg_info_t* pkg, sp_str_t dep) {
  sp_context_push_arena(pkg->arena);
  sp_da_push(pkg->system_deps, sp_str_copy(dep));
  sp_context_pop();
}

void spn_pkg_add_linkage(spn_pkg_info_t* pkg, spn_linkage_t linkage) {
  (void)pkg;
  (void)linkage;
}

spn_profile_info_t* spn_pkg_add_profile(spn_pkg_info_t* pkg, const c8* name) {
  spn_profile_info_t profile = {
    .name = spn_intern_cstr(name),
  };

  return spn_pkg_add_profile_ex(pkg, profile);
}

spn_profile_info_t* spn_pkg_add_profile_ex(spn_pkg_info_t* pkg, spn_profile_info_t profile) {
  sp_str_om_insert(pkg->profiles, profile.name, profile);
  return sp_str_om_get(pkg->profiles, profile.name);
}


spn_target_info_t* spn_pkg_add_exe(spn_pkg_info_t* pkg, const c8* name) {
  return spn_pkg_add_exe_ex(pkg, spn_intern_cstr(name));
}

spn_target_info_t* spn_pkg_add_exe_ex(spn_pkg_info_t* pkg, sp_str_t name) {
  spn_target_info_t exe = {
    .name = spn_intern(name),
    .kind = SPN_TARGET_EXE,
  };
  sp_str_om_insert(pkg->exes, exe.name, exe);
  return sp_str_om_get(pkg->exes, exe.name);
}

spn_target_info_t* spn_pkg_add_script(spn_pkg_info_t* pkg, const c8* name) {
  return spn_pkg_add_script_ex(pkg, spn_intern_cstr(name));
}

spn_target_info_t* spn_pkg_add_script_ex(spn_pkg_info_t* pkg, sp_str_t name) {
  spn_target_info_t script = {
    .name = spn_intern(name),
    .kind = SPN_TARGET_EXE,
  };
  sp_str_om_insert(pkg->scripts, script.name, script);
  return sp_str_om_get(pkg->scripts, script.name);
}

spn_target_info_t* spn_pkg_add_test(spn_pkg_info_t* pkg, const c8* name) {
  return spn_pkg_add_test_ex(pkg, spn_intern_cstr(name));
}

spn_target_info_t* spn_pkg_add_test_ex(spn_pkg_info_t* pkg, sp_str_t name) {
  spn_target_info_t test = {
    .name = spn_intern(name),
    .kind = SPN_TARGET_TEST,
  };
  sp_str_om_insert(pkg->tests, test.name, test);
  return sp_str_om_get(pkg->tests, test.name);
}

spn_target_info_t* spn_pkg_add_lib_ex(spn_pkg_info_t* pkg, sp_str_t name, spn_linkage_set_t linkage) {
  spn_target_info_t lib = {
    .name = spn_intern(name),
    .kind = SPN_TARGET_LIB,
    .linkages = linkage
  };
  sp_str_om_insert(pkg->libs, lib.name, lib);
  return sp_str_om_get(pkg->libs, lib.name);
}

spn_index_info_t* spn_pkg_add_index(spn_pkg_info_t* pkg, const c8* name, const c8* location) {
  return spn_pkg_add_index_ex(pkg, spn_intern_cstr(name), spn_intern_cstr(location));
}

spn_index_info_t* spn_pkg_add_index_ex(spn_pkg_info_t* pkg, sp_str_t name, sp_str_t location) {
  spn_index_info_t index = {
    .name = spn_intern(name),
    .location = spn_intern(location),
    .kind = SPN_INDEX_WORKSPACE,
  };
  sp_str_om_insert(pkg->indexes, index.name, index);
  return sp_str_om_get(pkg->indexes, index.name);
}

spn_err_t spn_pkg_add_toolchain(spn_pkg_info_t* pkg, spn_toolchain_entry_t entry) {
  sp_str_om_insert(pkg->toolchains, entry.name, entry);
  return SPN_OK;
}

