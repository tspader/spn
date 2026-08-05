#ifndef SPN_OP_TYPES_H
#define SPN_OP_TYPES_H

#include "sp.h"

#include "error/types.h"
#include "forward/types.h"
#include "semver/types.h"
#include "session/types.h"

typedef enum {
  SPN_ADD_DEP_PACKAGE,
  SPN_ADD_DEP_TEST,
  SPN_ADD_DEP_BUILD,
} spn_add_dep_t;

typedef struct {
  sp_str_t key;
  sp_str_t requested;
  spn_semver_range_t range;
  spn_add_dep_t dep;
} spn_add_request_t;

typedef struct {
  bool force;
  sp_str_t only;
} spn_index_refresh_t;

typedef struct {
  sp_str_t index;
  sp_str_t url;
  sp_str_t revision;
  bool dry;
  bool allow_dirty;
} spn_publish_request_t;

typedef enum {
  SPN_OP_NONE,
  SPN_OP_REACH,
  SPN_OP_ADD,
  SPN_OP_CLEAN,
  SPN_OP_PUBLISH,
  SPN_OP_SYNC_INDEXES,
} spn_op_kind_t;

typedef struct {
  spn_op_kind_t kind;
  union {
    spn_phase_t reach;
    spn_add_request_t add;
    spn_publish_request_t publish;
    spn_index_refresh_t refresh;
    struct {
      bool whole;
    } clean;
  };
} spn_op_desc_t;

struct spn_op_t {
  spn_ctx_t* ctx;
  spn_op_desc_t desc;
  sp_thread_t thread;
  sp_atomic_s32_t done;
  spn_err_union_t result;
};

#endif
