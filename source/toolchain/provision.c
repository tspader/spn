#include "toolchain/provision.h"
#include "toolchain/sha256.h"

spn_err_t spn_fetch_curl(void* user_data, sp_str_t url, sp_str_t dest) {
  return SPN_ERROR;
}

sp_str_t spn_toolchain_store_path(spn_toolchain_store_t* store, spn_artifact_t artifact) {
  return sp_str_lit("");
}

sp_str_t spn_artifact_resolve_url(sp_mem_t mem, spn_artifact_t artifact, sp_str_t mirror) {
  return artifact.url;
}

spn_err_t spn_toolchain_provision(spn_toolchain_store_t* store, spn_toolchain_t* toolchain, sp_str_t* root) {
  *root = sp_str_lit("");
  return SPN_ERROR;
}
