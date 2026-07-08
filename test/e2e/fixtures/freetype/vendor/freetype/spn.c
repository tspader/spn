#include "spn.h"

SPN_EXPORT
s32 run_cmake(spn_t* spn, spn_node_ctx_t* ctx) {
  spn_cmake_t* cmake = spn_cmake_new(spn);
  spn_cmake_add_define(cmake, "CMAKE_POLICY_VERSION_MINIMUM", "3.5");
  spn_cmake_add_define(cmake, "DISABLE_FORCE_DEBUG_POSTFIX", "ON");
  spn_cmake_add_define(cmake, "BUILD_SHARED_LIBS", "OFF");
  spn_cmake_add_define(cmake, "FT_DISABLE_ZLIB", "ON");
  spn_cmake_add_define(cmake, "FT_DISABLE_BZIP2", "ON");
  spn_cmake_add_define(cmake, "FT_DISABLE_PNG", "ON");
  spn_cmake_add_define(cmake, "FT_DISABLE_HARFBUZZ", "ON");
  spn_cmake_add_define(cmake, "FT_DISABLE_BROTLI", "ON");
  if (spn_cmake_run(cmake)) {
    return 1;
  }
  return spn_cmake_install(cmake);
}

SPN_EXPORT
spn_err_t configure(spn_t* spn, spn_config_t* config) {
  spn_node_t* cmake = spn_add_node(config, "cmake");
  spn_node_set_fn(cmake, "run_cmake");
  return SPN_OK;
}
