#include "project/project.h"

#include "lock/lock.h"
#include "pkg/load.h"
#include "sp/atomic_file.h"

sp_str_t spn_project_manifest_path(sp_mem_t mem, sp_str_t root) {
  return sp_fs_join_path(mem, root, sp_str_lit("spn.toml"));
}

spn_err_union_t spn_project_load(sp_mem_t mem, sp_intern_t* intern, spn_event_buffer_t* events, sp_str_t root, spn_project_t** project) {
  *project = SP_NULLPTR;

  sp_str_t manifest = spn_project_manifest_path(mem, root);
  if (!sp_fs_exists(manifest)) {
    return spn_result(SPN_OK);
  }

  spn_project_t* loaded = sp_alloc_type(mem, spn_project_t);
  loaded->paths.root = root;
  loaded->paths.manifest = manifest;
  loaded->paths.lock = sp_fs_join_path(mem, root, sp_str_lit("spn.lock"));

  try_union(spn_pkg_load(mem, intern, manifest, SPN_MANIFEST_ROOT, sp_str_lit(""), &loaded->package));

  if (sp_fs_exists(loaded->paths.lock)) {
    sp_opt_set(loaded->lock, spn_lock_file_load(mem, loaded->paths.lock, events));
  }

  *project = loaded;
  return spn_result(SPN_OK);
}

spn_err_union_t spn_project_update_lock(spn_project_t* project, sp_intern_t* intern, spn_resolve_t resolve) {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  spn_lock_file_t lock = spn_build_lock_file(scratch.mem, intern, resolve, &project->package);

  sp_da_for(project->package.system_deps, it) {
    sp_ht_insert(lock.system_deps, project->package.system_deps[it], true);
  }

  sp_str_t output = spn_lock_file_to_str(scratch.mem, &lock);
  sp_err_t written = sp_fs_write_atomic(project->paths.lock, output);
  sp_mem_end_scratch(scratch);

  if (written != SP_OK) {
    return (spn_err_union_t) { .kind = SPN_ERR_FS_WRITE, .fs = { .path = project->paths.lock } };
  }
  return spn_result(SPN_OK);
}
