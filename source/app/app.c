#include "ctx/types.h"

#include "app/app.h"
#include "lock/lock.h"
#include "pkg/mutate.h"
#include "pkg/pkg.h"
#include "semver/convert.h"

void spn_app_update_lock_file(spn_app_t* app) {
  spn_lock_file_t lock = spn_build_lock_file(app->resolver, &app->package);

  sp_da_for(app->package.system_deps, i) {
    sp_ht_insert(lock.system_deps, sp_str_copy(app->package.system_deps[i]), true);
  }

  sp_str_t output = spn_lock_file_to_str(&lock);
  sp_io_writer_t file = sp_io_writer_from_file(app->paths.lock, SP_IO_WRITE_MODE_OVERWRITE);
  sp_io_write_str(&file, output);
  sp_io_writer_close(&file);
}

void spn_app_write_manifest(spn_pkg_info_t* pkg, sp_str_t path) {
}
