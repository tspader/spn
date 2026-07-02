#ifndef SPN_TOOLCHAIN_PROVISION_H
#define SPN_TOOLCHAIN_PROVISION_H

#include "toolchain/types.h"

typedef spn_err_t (*spn_fetch_fn)(sp_str_t url, sp_str_t dest, void* user_data);

typedef enum {
  SPN_TOOLCHAIN_PROVISION_OK,
  SPN_TOOLCHAIN_PROVISION_ERR_FETCH,
  SPN_TOOLCHAIN_PROVISION_ERR_SHA,
  SPN_TOOLCHAIN_PROVISION_ERR_EXTRACT,
} spn_toolchain_provision_status_t;

typedef struct {
  spn_toolchain_provision_status_t status;
  sp_str_t url;
  sp_str_t expected;
  sp_str_t actual;
} spn_toolchain_provision_err_t;

typedef struct {
  sp_mem_t mem;
  sp_str_t dir;
  sp_str_t mirror;
  spn_fetch_fn fetch;
  void* fetch_user_data;
} spn_toolchain_store_t;

spn_err_t spn_fetch_curl(sp_str_t url, sp_str_t dest, void* user_data);

sp_str_t  spn_toolchain_store_path(spn_toolchain_store_t* store, spn_artifact_t artifact);
sp_str_t  spn_artifact_resolve_url(sp_mem_t mem, spn_artifact_t artifact, sp_str_t mirror);
spn_toolchain_provision_err_t spn_toolchain_provision(spn_toolchain_store_t* store, spn_toolchain_t* toolchain, sp_str_t* root);

#endif
