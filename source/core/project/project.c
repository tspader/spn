#include "project/project.h"

#include "error/error.h"
#include "intern/intern.h"
#include "lock/lock.h"
#include "pkg/load.h"
#include "toml/issue.h"
#include "atomic_file/atomic_file.h"

sp_str_t spn_project_manifest_path(sp_mem_t mem, sp_str_t root) {
  return sp_fs_join_path(mem, root, sp_str_lit("spn.toml"));
}

spn_err_t spn_project_load(spn_ctx_t* ctx, sp_str_t root, spn_project_t** project) {
  *project = SP_NULLPTR;

  sp_str_t manifest = spn_project_manifest_path(ctx->heap, root);
  if (!sp_fs_exists(manifest)) {
    return SPN_OK;
  }

  spn_project_t* loaded = sp_alloc_type(ctx->heap, spn_project_t);
  loaded->paths.manifest = manifest;
  loaded->paths.lock = sp_fs_join_path(ctx->heap, root, sp_str_lit("spn.lock"));

  spn_codegen_issues_t issues = sp_zero;
  spn_err_t parsed = spn_pkg_load(ctx->heap, ctx->intern, manifest, SPN_MANIFEST_ROOT, &loaded->package, &issues);
  if (parsed == SPN_ERR_NO_MANIFEST) {
    return spn_err_emit(ctx, (spn_err_union_t) {
      .kind = SPN_ERR_NO_MANIFEST,
      .no_manifest = { .path = manifest },
    });
  }
  if (parsed) {
    return spn_err_emit(ctx, (spn_err_union_t) {
      .kind = SPN_ERR_MANIFEST_ISSUES,
      .manifest = { .path = manifest, .issues = spn_codegen_issues_to_err(ctx->heap, issues) },
    });
  }

  if (sp_fs_exists(loaded->paths.lock)) {
    sp_opt_set(loaded->lock, spn_lock_file_load(ctx->heap, loaded->paths.lock, ctx->events));
  }

  *project = loaded;
  return SPN_OK;
}

spn_err_t spn_project_update_lock(spn_ctx_t* ctx, spn_project_t* project, spn_resolve_t resolve) {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  spn_lock_file_t lock = spn_build_lock_file(scratch.mem, ctx->intern, resolve, &project->package);

  sp_da_for(project->package.system_deps, it) {
    sp_ht_insert(lock.system_deps, project->package.system_deps[it], true);
  }

  sp_str_t output = spn_lock_file_to_str(scratch.mem, &lock);
  sp_err_t written = sp_fs_write_atomic(project->paths.lock, output);
  sp_mem_end_scratch(scratch);

  if (written != SP_OK) {
    return spn_err_emit(ctx, (spn_err_union_t) { .kind = SPN_ERR_FS_WRITE, .fs = { .path = project->paths.lock } });
  }
  return SPN_OK;
}
