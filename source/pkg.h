#ifndef SPN_PKG_H
#define SPN_PKG_H

#include "sp.h"
#include "spn.h"

#include "option.h"
#include "ordered_map.h"
#include "profile.h"
#include "semver.h"
#include "target.h"

#define SPN_PACKAGE_KIND(X) \
  X(SPN_PACKAGE_KIND_NONE, "none") \
  X(SPN_PACKAGE_KIND_ROOT, "root") \
  X(SPN_PACKAGE_KIND_WORKSPACE, "workspace") \
  X(SPN_PACKAGE_KIND_FILE, "file") \
  X(SPN_PACKAGE_KIND_INDEX, "index")

typedef enum {
  SPN_PACKAGE_KIND(SP_X_NAMED_ENUM_DEFINE)
} spn_pkg_kind_t;

typedef struct {
  sp_str_t name;
  spn_pkg_kind_t kind;
  spn_visibility_t visibility;
  union {
    spn_semver_range_t range;
    sp_str_t file;
  };
} spn_pkg_req_t;

typedef struct {
  spn_semver_t version;
  sp_str_t commit;
} spn_pkg_metadata_t;

sp_str_t spn_package_kind_to_str(spn_pkg_kind_t kind);
spn_pkg_kind_t spn_package_kind_from_str(sp_str_t str);
spn_pkg_dir_t spn_cache_dir_kind_from_str(sp_str_t str);

struct spn_pkg {
  sp_str_t name;
  sp_str_t repo;
  sp_str_t url;
  sp_str_t author;
  sp_str_t maintainer;
  spn_semver_t version;
  sp_om(spn_target_t) libs;
  sp_om(spn_target_t) exes;
  sp_om(spn_target_t) tests;
  sp_om(spn_profile_t) profiles;
  sp_om(spn_index_t) indexes;
  sp_ht(sp_str_t, spn_pkg_req_t) deps;
  sp_ht(sp_str_t, spn_dep_option_t) options;
  sp_ht(sp_str_t, spn_dep_options_t) config;
  sp_ht(spn_semver_t, spn_pkg_metadata_t) metadata;
  sp_da(spn_semver_t) versions;
  sp_da(sp_str_t) include;
  sp_da(sp_str_t) define;
  sp_da(sp_str_t) system_deps;
  spn_pkg_kind_t kind;

  sp_mem_arena_t* arena;

  struct {
    sp_str_t root;
    sp_str_t manifest;
    sp_str_t metadata;
    sp_str_t script;
    struct {
      sp_str_t source;
      sp_str_t work;
      sp_str_t store;
    } cache;
  } paths;
};

spn_pkg_t spn_pkg_new(sp_str_t path);
spn_pkg_t spn_pkg_from_default(sp_str_t path, sp_str_t name);
void spn_pkg_load(spn_pkg_t* pkg, sp_str_t manifest_path);
void spn_pkg_from_index(spn_pkg_t* pkg, sp_str_t index_path);
void spn_pkg_from_manifest(spn_pkg_t* pkg, sp_str_t manifest_path);
void spn_pkg_init(spn_pkg_t* pkg, sp_str_t name);
void spn_pkg_set_index(spn_pkg_t* pkg, sp_str_t path);
void spn_pkg_set_manifest(spn_pkg_t* pkg, sp_str_t path);
void spn_pkg_set_name(spn_pkg_t* pkg, const c8* name);
void spn_pkg_set_name_ex(spn_pkg_t* pkg, sp_str_t name);
void spn_pkg_set_repo(spn_pkg_t* pkg, const c8* repo);
void spn_pkg_set_repo_ex(spn_pkg_t* pkg, sp_str_t repo);
void spn_pkg_set_url(spn_pkg_t* pkg, const c8* url);
void spn_pkg_set_url_ex(spn_pkg_t* pkg, sp_str_t url);
void spn_pkg_set_author(spn_pkg_t* pkg, const c8* author);
void spn_pkg_set_author_ex(spn_pkg_t* pkg, sp_str_t author);
void spn_pkg_set_maintainer(spn_pkg_t* pkg, const c8* maintainer);
void spn_pkg_set_maintainer_ex(spn_pkg_t* pkg, sp_str_t maintainer);
void spn_pkg_add_version(spn_pkg_t* pkg, const c8* version, const c8* commit);
void spn_pkg_add_version_ex(spn_pkg_t* pkg, spn_semver_t version, sp_str_t commit);
void spn_pkg_add_include(spn_pkg_t* pkg, const c8* path);
void spn_pkg_add_include_ex(spn_pkg_t* pkg, sp_str_t path);
void spn_pkg_add_define(spn_pkg_t* pkg, const c8* define);
void spn_pkg_add_define_ex(spn_pkg_t* pkg, sp_str_t define);
void spn_pkg_add_system_dep(spn_pkg_t* pkg, const c8* dep);
void spn_pkg_add_system_dep_ex(spn_pkg_t* pkg, sp_str_t dep);
void spn_pkg_add_linkage(spn_pkg_t* pkg, spn_linkage_t linkage);
spn_profile_t* spn_pkg_add_profile(spn_pkg_t* pkg, const c8* name);
spn_profile_t* spn_pkg_add_profile_ex(spn_pkg_t* pkg, spn_profile_t profile);
spn_index_t* spn_pkg_add_index(spn_pkg_t* pkg, const c8* name, const c8* location);
spn_index_t* spn_pkg_add_index_ex(spn_pkg_t* pkg, sp_str_t name, sp_str_t location);
spn_target_t* spn_pkg_add_exe(spn_pkg_t* pkg, const c8* name);
spn_target_t* spn_pkg_add_exe_ex(spn_pkg_t* pkg, sp_str_t name);
spn_target_t* spn_pkg_add_test(spn_pkg_t* pkg, const c8* name);
spn_target_t* spn_pkg_add_test_ex(spn_pkg_t* pkg, sp_str_t name);
spn_target_t* spn_pkg_add_lib(spn_pkg_t* pkg, const c8* name, spn_linkage_t kind);
spn_target_t* spn_pkg_add_lib_ex(spn_pkg_t* pkg, sp_str_t name, spn_linkage_t kind);
bool spn_pkg_has_lib_kind(spn_pkg_t* pkg, spn_linkage_t kind);
void spn_pkg_add_dep(spn_pkg_t* pkg, sp_str_t name, sp_str_t version, spn_visibility_t visibility);
void spn_pkg_add_dep_latest(spn_pkg_t* pkg, sp_str_t name, spn_visibility_t visibility);
sp_str_t spn_pkg_get_url(spn_pkg_t* pkg);
spn_profile_t* spn_pkg_get_default_profile(spn_pkg_t* pkg);
spn_profile_t* spn_pkg_get_profile_or_default(spn_pkg_t* pkg, sp_str_t name);
spn_target_t* spn_pkg_get_target(spn_pkg_t* pkg, const c8* name);
spn_target_t* spn_pkg_get_target_ex(spn_pkg_t* pkg, sp_str_t name);

#endif
