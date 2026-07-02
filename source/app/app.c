#include "ctx/types.h"

#include "app/app.h"
#include "lock/lock.h"
#include "pkg/mutate.h"
#include "pkg/pkg.h"
#include "semver/convert.h"

void spn_app_update_lock_file(spn_app_t* app) {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  spn_lock_file_t lock = spn_build_lock_file(scratch.mem, app->session.resolve, &app->package);

  sp_da_for(app->package.system_deps, i) {
    sp_ht_insert(lock.system_deps, app->package.system_deps[i], true);
  }

  sp_str_t output = spn_lock_file_to_str(scratch.mem, &lock);
  sp_io_file_writer_t file;
  sp_io_file_writer_from_path(&file, app->paths.lock);
  sp_io_write_str(&file.base, output, SP_NULLPTR);
  sp_io_file_writer_close(&file);
  sp_mem_end_scratch(scratch);
}

void spn_app_write_manifest(spn_pkg_info_t* pkg, sp_str_t path) {
}
