#include "session/registry/registry.h"

#include "ctx/types.h"

#include "codegen/codegen.h"
#include "codegen/lower.h"

spn_loaded_pkg_t* spn_registry_load_file_pkg(spn_pkg_registry_t* registry, sp_mem_t mem, sp_intern_t* intern, sp_str_t qualified, sp_str_t path, spn_registry_err_t* err) {
  spn_loaded_pkg_t* existing = sp_str_ht_get(*registry, qualified);
  if (existing) return existing;

  sp_str_t manifest = path;
  if (sp_str_starts_with(manifest, SP_LIT("file://"))) {
    manifest = sp_str_strip_left(manifest, SP_LIT("file://"));
  }
  if (!sp_str_starts_with(manifest, SP_LIT("/"))) {
    manifest = sp_fs_join_path(mem, spn.paths.project, manifest);
  }
  manifest = sp_fs_normalize_path(mem, manifest);
  err->manifest = manifest;

  if (!sp_fs_exists(manifest)) {
    err->error = sp_str_lit("manifest file is missing");
    return SP_NULLPTR;
  }

  spn_pkg_info_t* info = sp_alloc_type(mem, spn_pkg_info_t);
  spn_codegen_ctx_t ctx = sp_zero;
  spn_codegen_ctx_init(&ctx, mem, mem, intern);
  if (spn_codegen_load_pkg(&ctx, manifest, info)) {
    err->error = spn_codegen_issues_message(mem, ctx.issues);
    return SP_NULLPTR;
  }

  sp_str_ht_insert(*registry, qualified, sp_zero_struct(spn_loaded_pkg_t));
  spn_loaded_pkg_t* loaded = sp_str_ht_get(*registry, qualified);
  loaded->source = SPN_PKG_SOURCE_FILE;
  loaded->info = info;
  loaded->paths.manifest = manifest;
  loaded->paths.source = sp_fs_parent_path(manifest);
  loaded->paths.script = sp_fs_join_path(mem, loaded->paths.source, sp_str_lit("spn.c"));
  loaded->paths.build = sp_fs_join_path(mem, loaded->paths.source, info->build);
  loaded->paths.configure = sp_fs_join_path(mem, loaded->paths.source, info->configure);

  return loaded;
}
