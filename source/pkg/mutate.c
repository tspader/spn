#include "error/types.h"
#include "index/types.h"
#include "pkg/types.h"
#include "profile/types.h"
#include "toolchain/types.h"

#include "enum/enum.h"
#include "intern/intern.h"
#include "pkg/mutate.h"
#include "target/mutate.h"

static sp_mem_t spn_pkg_mem(spn_pkg_info_t* pkg) {
  return sp_mem_arena_as_allocator(pkg->arena);
}

void spn_pkg_init(sp_mem_t mem, spn_pkg_info_t* pkg, sp_str_t name) {
  pkg->arena = sp_mem_arena_new(mem);
  pkg->name = spn_intern(name);

  sp_mem_t a = spn_pkg_mem(pkg);
  sp_str_om_init(pkg->libs);
  sp_str_om_init(pkg->exes);
  sp_str_om_init(pkg->scripts);
  sp_str_om_init(pkg->tests);
  sp_str_om_init(pkg->profiles);
  sp_str_om_init(pkg->indexes);
  sp_str_om_init(pkg->toolchains);
  sp_da_init(a, pkg->deps);
  sp_da_init(a, pkg->config);
  sp_da_init(a, pkg->include);
  sp_da_init(a, pkg->define);
  sp_da_init(a, pkg->public_define);
  sp_da_init(a, pkg->system_deps);
  sp_da_init(a, pkg->gated.system_deps);
  sp_str_om_init(pkg->options);
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
  pkg->repo = sp_str_copy(spn_pkg_mem(pkg), repo);
}

void spn_pkg_set_author(spn_pkg_info_t* pkg, const c8* author) {
  spn_pkg_set_author_ex(pkg, sp_str_view(author));
}

void spn_pkg_set_author_ex(spn_pkg_info_t* pkg, sp_str_t author) {
  pkg->author = sp_str_copy(spn_pkg_mem(pkg), author);
}

void spn_pkg_set_maintainer(spn_pkg_info_t* pkg, const c8* maintainer) {
  spn_pkg_set_maintainer_ex(pkg, sp_str_view(maintainer));
}

void spn_pkg_set_maintainer_ex(spn_pkg_info_t* pkg, sp_str_t maintainer) {
  pkg->maintainer = sp_str_copy(spn_pkg_mem(pkg), maintainer);
}

void spn_pkg_add_include(spn_pkg_info_t* pkg, const c8* include) {
  spn_pkg_add_include_ex(pkg, sp_str_view(include));
}

void spn_pkg_add_include_ex(spn_pkg_info_t* pkg, sp_str_t path) {
  sp_da_push(pkg->include, sp_str_copy(spn_pkg_mem(pkg), path));
}

void spn_pkg_add_define(spn_pkg_info_t* pkg, const c8* define) {
  spn_pkg_add_define_ex(pkg, sp_str_view(define));
}

void spn_pkg_add_define_ex(spn_pkg_info_t* pkg, sp_str_t define) {
  sp_da_push(pkg->define, sp_str_copy(spn_pkg_mem(pkg), define));
}

void spn_pkg_add_system_dep(spn_pkg_info_t* pkg, const c8* dep) {
  spn_pkg_add_system_dep_ex(pkg, sp_str_view(dep));
}

void spn_pkg_add_system_dep_ex(spn_pkg_info_t* pkg, sp_str_t dep) {
  sp_da_push(pkg->system_deps, sp_str_copy(spn_pkg_mem(pkg), dep));
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
  spn_target_info_t* target = sp_str_om_get(pkg->exes, exe.name);
  spn_target_info_init(spn_pkg_mem(pkg), target);
  return target;
}

spn_target_info_t* spn_pkg_add_script(spn_pkg_info_t* pkg, const c8* name) {
  return spn_pkg_add_script_ex(pkg, spn_intern_cstr(name));
}

spn_target_info_t* spn_pkg_add_script_ex(spn_pkg_info_t* pkg, sp_str_t name) {
  spn_target_info_t script = {
    .name = spn_intern(name),
    .kind = SPN_TARGET_SCRIPT,
  };
  sp_str_om_insert(pkg->scripts, script.name, script);
  spn_target_info_t* target = sp_str_om_get(pkg->scripts, script.name);
  spn_target_info_init(spn_pkg_mem(pkg), target);
  return target;
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
  spn_target_info_t* target = sp_str_om_get(pkg->tests, test.name);
  spn_target_info_init(spn_pkg_mem(pkg), target);
  return target;
}

spn_target_info_t* spn_pkg_add_lib_ex(spn_pkg_info_t* pkg, sp_str_t name, spn_linkage_set_t linkage) {
  spn_target_info_t lib = {
    .name = spn_intern(name),
    .kind = SPN_TARGET_LIB,
    .linkages = linkage
  };
  sp_str_om_insert(pkg->libs, lib.name, lib);
  spn_target_info_t* target = sp_str_om_get(pkg->libs, lib.name);
  spn_target_info_init(spn_pkg_mem(pkg), target);
  return target;
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


