#include "spn.h"

SPN_EXPORT
s32 run_cmake(spn_t* spn, spn_node_ctx_t* ctx) {
  spn_cmake_t* cmake = spn_cmake_new(spn);
  spn_cmake_add_define(cmake, "BUILD_SHARED_LIBS", "OFF");
  spn_cmake_add_define(cmake, "CPPTRACE_GET_SYMBOLS_WITH_LIBDL", "ON");
  spn_cmake_add_define(cmake, "CPPTRACE_UNWIND_WITH_UNWIND", "ON");
  spn_cmake_add_define(cmake, "CPPTRACE_DEMANGLE_WITH_CXXABI", "ON");
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
