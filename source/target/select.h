#ifndef SPN_TARGET_SELECT_H
#define SPN_TARGET_SELECT_H

#include "sp.h"
#include "spn.h"

#include "target/types.h"

// Selects which artifact kind a lib target is built as for this session.
//
// - config is the root manifest's [config.<pkg>] kind. It's a hard requirement: if the lib
//   doesn't support it, selection fails.
// - linkage is the profile's linkage. SPN_LIB_KIND_STATIC and SPN_LIB_KIND_SOURCE promise a
//   build with no runtime library dependencies, so a shared-only lib fails selection. Otherwise
//   it's a preference, and any supported kind can satisfy it.
typedef struct {
  sp_opt_spn_linkage_t config;
  spn_linkage_t linkage;
} spn_kind_query_t;

spn_err_t spn_target_select_lib_kind(spn_target_info_t* info, spn_kind_query_t query, spn_linkage_t* out);

#endif
