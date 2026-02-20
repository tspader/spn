#ifndef SPN_EVENT_H
#define SPN_EVENT_H

#include "sp.h"
#include "pkg.h"
#include "node.h"
#include "resolve.h"

typedef struct spn_build_ctx spn_build_ctx_t;
typedef struct spn_build_io_t spn_build_io_t;

typedef enum {
  SPN_EVENT_FETCH,
  SPN_EVENT_ERR_CIRCULAR_DEP,
  SPN_EVENT_ERR_UNKNOWN_PKG,
  SPN_EVENT_RESOLVE,
  SPN_EVENT_SYNC,
  SPN_EVENT_CHECKOUT,
  SPN_EVENT_BUILD_SCRIPT_COMPILE,
  SPN_EVENT_BUILD_SCRIPT_COMPILE_FAILED,
  SPN_EVENT_BUILD_SCRIPT_CONFIGURE,
  SPN_EVENT_BUILD_SCRIPT_BUILD,
  SPN_EVENT_BUILD_SCRIPT_PACKAGE,
  SPN_EVENT_BUILD_SCRIPT_FAILED,
  SPN_EVENT_BUILD_SCRIPT_CRASHED,
  SPN_EVENT_BUILD_SCRIPT_CONFIGURE_FAILED,
  SPN_EVENT_BUILD_SCRIPT_BUILD_FAILED,
  SPN_EVENT_BUILD_SCRIPT_PACKAGE_FAILED,
  SPN_EVENT_BUILD_SCRIPT_USER_FN,
  SPN_EVENT_DEP_BUILD,
  SPN_EVENT_DEP_BUILD_PASSED,
  SPN_EVENT_DEP_BUILD_FAILED,
  SPN_EVENT_TARGET_BUILD,
  SPN_EVENT_TARGET_BUILD_PASSED,
  SPN_EVENT_TARGET_BUILD_FAILED,
  SPN_EVENT_BUILD_PASSED,
  SPN_EVENT_TCC_ERROR,
  SPN_EVENT_TEST_RUN,
  SPN_EVENT_TEST_PASSED,
  SPN_EVENT_TESTS_PASSED,
  SPN_EVENT_TEST_FAILED,
  SPN_EVENT_CLEAN,
  SPN_EVENT_GENERATE,
  SPN_EVENT_ADD_TARGET,
  SPN_EVENT_DEBUG,
  SPN_EVENT_ADD_SOURCE,
  SPN_EVENT_INIT_BUILD_GRAPH,
  SPN_EVENT_LINK_TARGET,
  SPN_EVENT_RUN_CONFIGURE,
} spn_build_event_kind_t;

typedef struct spn_build_event_t {
  spn_build_event_kind_t kind;
  spn_pkg_t* pkg;
  spn_build_io_t* io;

  union {
    sp_str_t tcc;
    struct {
      spn_user_node_t* info;
    } node;
    union {
      struct { u64 time; } passed;
      struct { sp_str_t out; sp_str_t err; } failed;
    } dep;
    union {
      struct { sp_str_t name; } add;
      struct { sp_str_t args; } build;
      struct { sp_str_t out; sp_str_t err; } failed;
    } target;
    union {
      struct { spn_profile_t* profile; u64 time; } passed;
    } build;
    union {
      struct { u32 n; spn_profile_t* profile; u64 time; } passed;
    } test;
    struct { sp_str_t commit; spn_semver_t version; sp_str_t message; } checkout;
    struct { spn_resolve_strategy_t strategy; } resolve;
    struct { spn_pkg_t* pkg; } circular;
    struct { spn_pkg_req_t request; } unknown;
    struct { sp_str_t path; } clean;
    struct { sp_str_t path; } generate;
    struct { sp_str_t error; } compile_failed;
    struct { sp_str_t url; } sync;
    struct { sp_str_t message; } debug;
    struct { sp_str_t target; s32 kind; } target_add;
    struct { sp_str_t target; sp_str_t source; } target_source;
    struct { sp_str_t profile; bool force; u32 packages; } graph_init;
    struct { sp_str_t target; u32 objects; } target_link;
    struct { bool exists; s32 result; u64 time; } configure;
  };
} spn_build_event_t;

typedef struct spn_event_buffer_t {
  sp_rb(spn_build_event_t) buffer;
  sp_mutex_t mutex;
  sp_cv_t condition;
} spn_event_buffer_t;

void                spn_build_event_init(spn_build_event_t* event, spn_build_event_kind_t kind, spn_build_ctx_t* ctx);
spn_event_buffer_t* spn_event_buffer_new(void);
void                spn_event_buffer_push(spn_event_buffer_t* evs, spn_build_ctx_t* ctx, spn_build_event_kind_t k);
void                spn_event_buffer_push_ctx(spn_event_buffer_t* evs, spn_build_ctx_t* ctx, spn_build_event_t e);
void                spn_event_buffer_push_ex(spn_event_buffer_t* evs, spn_pkg_t* pkg, spn_build_io_t* io, spn_build_event_t e);
spn_build_event_t   spn_build_event_make(spn_build_ctx_t* ctx, spn_build_event_kind_t kind);
void                spn_push_event_ex(spn_build_event_t event);
void                spn_push_event(spn_build_event_kind_t kind);
sp_da(spn_build_event_t) spn_event_buffer_drain(spn_event_buffer_t* events);
sp_str_t        spn_build_event_kind_to_str(spn_build_event_kind_t kind);
spn_verbosity_t spn_build_event_get_verbosity(spn_build_event_kind_t kind);



#endif
