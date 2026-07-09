#include "sp.h"
#include "sp/macro.h"
#include "session/registry/registry.h"

#include "ctx/types.h"

#include "toml/loader.h"
#include "toml/issue.h"
#include "codegen/lower.h"
#include "pkg/id.h"

sp_str_t parse(sp_mem_t mem, sp_str_t path) {
  if (sp_str_starts_with(path, SP_LIT("file://"))) {
    path = sp_str_strip_left(path, SP_LIT("file://"));
  }
  if (!sp_str_starts_with(path, SP_LIT("/"))) {
    path = sp_fs_join_path(mem, spn.paths.project, path);
  }
  return sp_fs_normalize_path(mem, path);
}

spn_registry_pkg_t* spn_registry_load_file_pkg(spn_pkg_registry_t* registry, sp_mem_t mem, sp_intern_t* intern, sp_str_t qualified, sp_str_t path, spn_registry_err_t* err) {
  spn_pkg_id_t id = spn_pkg_id(intern, qualified);
  spn_registry_pkg_t* existing = sp_ht_getp(*registry, id);
  if (existing) return existing;

  sp_str_t manifest = parse(mem, path);
  err->manifest = manifest;

  if (!sp_fs_exists(manifest)) {
    err->error = sp_str_lit("manifest file is missing");
    return SP_NULLPTR;
  }

  spn_pkg_info_t* info = sp_alloc_type(mem, spn_pkg_info_t);
  spn_toml_loader_t ctx = sp_zero;
  spn_toml_loader_init(&ctx, mem, intern);
  if (spn_codegen_load_pkg(&ctx, manifest, info)) {
    err->error = spn_codegen_issues_message(mem, ctx.issues);
    err->issues = ctx.issues;
    return SP_NULLPTR;
  }

  sp_ht_insert(*registry, id, ((spn_registry_pkg_t) {
    .source = SPN_PKG_SOURCE_FILE,
    .info = info,
    .manifest = manifest,
  }));

  return sp_ht_getp(*registry, id);
}
