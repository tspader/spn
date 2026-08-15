#include "paths/paths_test.h"

const spn_path_roots_t* paths_test_roots_build(paths_test_roots_t spec, spn_path_roots_t* storage) {
  const c8* dirs [SPN_PATH_ROOT_COUNT] = {
    [SPN_PATH_ROOT_PROJECT] = spec.project,
    [SPN_PATH_ROOT_STORE] = spec.store,
    [SPN_PATH_ROOT_BUILD] = spec.build,
    [SPN_PATH_ROOT_CHECKOUT] = spec.checkout,
    [SPN_PATH_ROOT_TOOLCHAIN] = spec.toolchain,
    [SPN_PATH_ROOT_INDEX] = spec.index,
    [SPN_PATH_ROOT_RUNTIME] = spec.runtime,
    [SPN_PATH_ROOT_CACHE] = spec.cache,
  };

  *storage = (spn_path_roots_t) sp_zero;
  sp_carr_for(dirs, it) {
    if (dirs[it]) {
      storage->dirs[it] = sp_str_view(dirs[it]);
    }
  }
  return storage;
}
