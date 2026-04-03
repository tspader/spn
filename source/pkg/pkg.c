#include "pkg/pkg.h"

#include "pkg/load.h"
#include "pkg/mutate.h"
#include "target/mutate.h"

spn_pkg_t spn_pkg_new(sp_str_t name) {
  spn_pkg_t pkg = SP_ZERO_INITIALIZE();
  spn_pkg_init(&pkg, name);
  return pkg;
}

spn_pkg_t spn_pkg_from_bare_default(sp_str_t path, sp_str_t name) {
  spn_pkg_t package = spn_pkg_new(name);
  spn_pkg_set_manifest(&package, sp_fs_join_path(path, sp_str_lit("spn.toml")));
  package.version = (spn_semver_t) {0, 1, 0};
  sp_dyn_array_push(package.versions, package.version);
  return package;
}

spn_pkg_t spn_pkg_from_default(sp_str_t path, sp_str_t name) {
  spn_pkg_t pkg = spn_pkg_new(name);
  spn_pkg_set_manifest(&pkg, sp_fs_join_path(path, sp_str_lit("spn.toml")));
  spn_pkg_set_repo(&pkg, "");
  spn_pkg_add_version(&pkg, "0.1.0", "");

  spn_target_t* bin = spn_pkg_add_exe_ex(&pkg, pkg.name);
  spn_target_add_source_ex(bin, sp_str_lit("main.c"));

  return pkg;
}

spn_err_t spn_pkg_from_index(spn_pkg_t* pkg, sp_str_t path) {
  (void)pkg;
  (void)path;

  SP_FATAL("Legacy index package loading was removed. New index-backed resolution is not implemented yet.");
  SP_UNREACHABLE_RETURN(SPN_ERROR);
}

spn_err_t spn_pkg_from_manifest(spn_pkg_t* pkg, sp_str_t manifest) {
  SP_ASSERT(sp_fs_exists(manifest)); // @spader Return an error code instead? Or is this an invariant?

  spn_err_union_t result = spn_pkg_load(pkg, manifest);
  if (result.kind) {
    return result.kind;
  }
  spn_pkg_set_manifest(pkg, manifest);
  return SPN_OK;
}

bool spn_pkg_has_lib_kind(spn_pkg_t* pkg, spn_linkage_t kind) {
  sp_om_for(pkg->libs, it) {
    spn_target_t* lib = sp_om_at(pkg->libs, it);
    if (spn_linkage_set_has(lib->linkages, kind)) {
      return true;
    }
  }

  return false;
}

sp_str_t spn_pkg_get_url(spn_pkg_t* pkg) {
  return pkg->url;
}

spn_target_t* spn_pkg_get_target(spn_pkg_t* pkg, const c8* name) {
  return spn_pkg_get_target_ex(pkg, sp_str_view(name));
}

// @spader
// This doesn't look quite right. It's suspicious that we'd need to get a target without caring
// where it came from specifically.
spn_target_t* spn_pkg_get_target_ex(spn_pkg_t* pkg, sp_str_t name) {
  if (sp_om_has(pkg->exes, name)) {
    return sp_om_get(pkg->exes, name);
  }
  if (sp_om_has(pkg->scripts, name)) {
    return sp_om_get(pkg->scripts, name);
  }
  if (sp_om_has(pkg->tests, name)) {
    return sp_om_get(pkg->tests, name);
  }
  if (sp_om_has(pkg->libs, name)) {
    return sp_om_get(pkg->libs, name);
  }

  return SP_NULLPTR;
}

spn_profile_info_t* spn_pkg_get_default_profile(spn_pkg_t* pkg) {
  sp_om_for(pkg->profiles, it) {
    return sp_om_at(pkg->profiles, it);
  }

  SP_UNREACHABLE_RETURN(SP_NULLPTR);
}


