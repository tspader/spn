#ifndef SPN_EVENT_TYPES_H
#define SPN_EVENT_TYPES_H

#include "err.h"
#include "pkg/types.h"
#include "sp.h"
#include "node.h"
#include "resolve/types.h"
#include "unit/types.h"

typedef enum {
  SPN_EVENT_FETCH,
  SPN_EVENT_ERR,
  SPN_EVENT_ERR_CIRCULAR_DEP,
  SPN_EVENT_ERR_UNKNOWN_PKG,
  SPN_EVENT_ERR_UNSATISFIABLE_VERSION,
  SPN_EVENT_RESOLVE,
  SPN_EVENT_SYNC,
  SPN_EVENT_CHECKOUT,
  SPN_EVENT_BUILD_SCRIPT_COMPILE,
  SPN_EVENT_BUILD_SCRIPT_COMPILE_FAILED,
  SPN_EVENT_BUILD_SCRIPT_CONFIGURE,
  SPN_EVENT_BUILD_SCRIPT_CONFIGURE_OK,
  SPN_EVENT_BUILD_SCRIPT_BUILD,
  SPN_EVENT_BUILD_SCRIPT_PACKAGE,
  SPN_EVENT_BUILD_SCRIPT_FAILED,
  SPN_EVENT_BUILD_SCRIPT_CRASHED,
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
  SPN_EVENT_PREPARE_BUILD_GRAPH_FAILED,
  SPN_EVENT_LINK_START,
  SPN_EVENT_LINK_PASSED,
  SPN_EVENT_LINK_FAILED,
  SPN_EVENT_TRACE_DEBUG,
  SPN_EVENT_TRACE_INFO,
  SPN_EVENT_TRACE_WARN,
  SPN_EVENT_TRACE_ERROR,
  SPN_EVENT_CLI_ENTRY,
  SPN_EVENT_RESOLVE_START,
  SPN_EVENT_RESOLVE_PACKAGE,
  SPN_EVENT_RESOLVE_END,
  SPN_EVENT_SYNC_START,
  SPN_EVENT_SYNC_PACKAGE,
  SPN_EVENT_SYNC_FAILED,
  SPN_EVENT_SYNC_END,
  SPN_EVENT_API_CALL,
  SPN_EVENT_USER_LOG,
  SPN_EVENT_EMBED_START,
  SPN_EVENT_EMBED_PASSED,
  SPN_EVENT_EMBED_FAILED,
  SPN_EVENT_DIRTY_SUMMARY,
  SPN_EVENT_BUILD_FAILED,
  SPN_EVENT_BUILD_SUMMARY,
  SPN_EVENT_BUILD_SCRIPT_PACKAGE_OK,
  SPN_EVENT_COUNT,
} spn_build_event_kind_t;

// ============================================================================
// Named event variant types (used in the union and for bind schemas)
// ============================================================================

typedef struct { spn_user_node_t* info; } spn_evt_node_t;

typedef struct { u64 time; } spn_evt_dep_passed_t;
typedef struct { sp_str_t out; sp_str_t err; } spn_evt_dep_failed_t;

typedef struct { sp_str_t name; } spn_evt_target_add_t;
typedef struct {
  sp_str_t source_file;
  sp_str_t object_file;
  sp_str_t compiler;
  s32 compiler_kind;
  sp_str_t args;
} spn_evt_target_build_t;
typedef struct {
  sp_str_t source_file;
  sp_str_t object_file;
  u64 time;
} spn_evt_target_passed_t;
typedef struct {
  sp_str_t source_file;
  sp_str_t object_file;
  s32 rc;
  sp_str_t out;
  sp_str_t err;
  u64 time;
} spn_evt_target_failed_t;

typedef struct { spn_profile_t* profile; u64 time; } spn_evt_build_passed_t;

typedef struct { u32 n; spn_profile_t* profile; u64 time; } spn_evt_tests_passed_t;

typedef struct { sp_str_t commit; spn_semver_t version; sp_str_t message; } spn_evt_checkout_t;
typedef struct { spn_resolve_strategy_t strategy; } spn_evt_resolve_t;
typedef struct { spn_pkg_id_t id; } spn_evt_circular_t;
typedef struct { spn_pkg_req_t low; spn_pkg_req_t high; } spn_evt_unsatisfiable_t;
typedef struct { spn_pkg_req_t request; } spn_evt_unknown_t;
typedef struct { sp_str_t path; } spn_evt_clean_t;
typedef struct { sp_str_t path; } spn_evt_generate_t;
typedef struct { sp_str_t script_path; u64 time; bool has_configure; bool has_package; } spn_evt_script_compile_t;
typedef struct { u64 time; } spn_evt_script_package_t;
typedef struct { sp_str_t script_path; sp_str_t error; } spn_evt_compile_failed_t;
typedef struct { sp_str_t path; } spn_evt_script_crashed_t;
typedef struct { sp_str_t url; } spn_evt_sync_t;
typedef struct { sp_str_t message; } spn_evt_debug_t;
typedef struct { s32 kind; } spn_evt_target_add_debug_t;
typedef struct { sp_str_t source; } spn_evt_target_source_t;
typedef struct { sp_str_t profile; bool force; u32 packages; } spn_evt_graph_init_t;
typedef struct {
  s32 kind;
  u32 num_objects;
  sp_str_t output_path;
  sp_str_t linker;
  sp_str_t args;
  bool has_embeds;
} spn_evt_link_start_t;
typedef struct {
  sp_str_t output_path;
  u64 time;
} spn_evt_link_passed_t;
typedef struct {
  s32 exit_code;
  sp_str_t out;
  sp_str_t err;
  sp_str_t linker;
  sp_str_t args;
} spn_evt_link_failed_t;
typedef struct { u64 time; } spn_evt_configure_t;
typedef struct { sp_str_t message; sp_str_t file; u32 line; } spn_evt_trace_t;
typedef struct { sp_str_t command; sp_str_t profile; sp_str_t target; bool force; sp_str_t cwd; sp_str_t manifest; } spn_evt_cli_entry_t;
typedef struct { s32 strategy; u32 num_deps; } spn_evt_resolve_start_t;
typedef struct { sp_str_t name; sp_str_t version; s32 kind; } spn_evt_resolve_pkg_t;
typedef struct { u32 num_resolved; u64 time; } spn_evt_resolve_end_t;
typedef struct { u32 num_packages; u32 num_index; u32 num_file; } spn_evt_sync_start_t;
typedef struct { sp_str_t name; s32 kind; sp_str_t url; sp_str_t source_path; u64 time; } spn_evt_sync_pkg_t;
typedef struct { sp_str_t name; sp_str_t url; sp_str_t error; } spn_evt_sync_failed_t;
typedef struct { u32 num_synced; u64 time; } spn_evt_sync_end_t;
typedef struct { sp_str_t fn; sp_str_t args; } spn_evt_api_call_t;
typedef struct { sp_str_t message; } spn_evt_user_log_t;
typedef struct { u32 num_files; } spn_evt_embed_start_t;
typedef struct { sp_str_t object_path; sp_str_t header_path; u64 time; } spn_evt_embed_passed_t;
typedef struct { sp_str_t path; sp_str_t error; } spn_evt_embed_failed_t;
typedef struct { u32 total_commands; u32 dirty_commands; u32 total_files; u32 dirty_files; bool forced; } spn_evt_dirty_summary_t;
typedef struct { sp_str_t profile; u64 time; u32 num_errors; sp_str_t first_error; } spn_evt_build_failed_t;
typedef struct { bool success; u32 num_dirty; u32 total_commands; u64 time; sp_str_t profile; } spn_evt_build_summary_t;
typedef struct { u64 time; } spn_evt_package_ok_t;

typedef struct spn_build_event_t spn_build_event_t;

struct spn_build_event_t {
  spn_build_event_kind_t kind;
  spn_pkg_t* pkg;
  spn_build_io_t* io;
  u64 thread_id;

  union {
    sp_str_t tcc;
    spn_err_union_t err;
    spn_evt_node_t node;
    union {
      spn_evt_dep_passed_t passed;
      spn_evt_dep_failed_t failed;
    } dep;
    struct {
      sp_str_t name;
      union {
        spn_evt_target_add_t add;
        spn_evt_target_build_t build;
        spn_evt_target_passed_t passed;
        spn_evt_target_failed_t failed;
        spn_evt_target_add_debug_t add_debug;
        spn_evt_target_source_t source;
        spn_evt_link_start_t link_start;
        spn_evt_link_passed_t link_passed;
        spn_evt_link_failed_t link_failed;
      };
    } target;
    union {
      spn_evt_build_passed_t passed;
    } build;
    union {
      spn_evt_tests_passed_t passed;
    } test;
    spn_evt_checkout_t checkout;
    spn_evt_resolve_t resolve;
    spn_evt_circular_t circular;
    spn_evt_unsatisfiable_t unsatisfiable;
    spn_evt_unknown_t unknown;
    spn_evt_clean_t clean;
    spn_evt_generate_t generate;
    spn_evt_script_compile_t script_compile;
    spn_evt_compile_failed_t compile_failed;
    spn_evt_script_crashed_t crashed;
    spn_evt_script_package_t script_package;
    spn_evt_sync_t sync;
    spn_evt_debug_t debug;
    spn_evt_graph_init_t graph_init;
    spn_evt_configure_t configure;
    spn_evt_trace_t trace;
    spn_evt_cli_entry_t cli_entry;
    spn_evt_resolve_start_t resolve_start;
    spn_evt_resolve_pkg_t resolve_pkg;
    spn_evt_resolve_end_t resolve_end;
    spn_evt_sync_start_t sync_start;
    spn_evt_sync_pkg_t sync_pkg;
    spn_evt_sync_failed_t sync_failed;
    spn_evt_sync_end_t sync_end;
    spn_evt_api_call_t api_call;
    spn_evt_user_log_t user_log;
    spn_evt_embed_start_t embed_start;
    spn_evt_embed_passed_t embed_passed;
    spn_evt_embed_failed_t embed_failed;
    spn_evt_dirty_summary_t dirty_summary;
    spn_evt_build_failed_t build_failed;
    spn_evt_build_summary_t build_summary;
    spn_evt_package_ok_t package_ok;
  };
};

struct spn_event_buffer_t {
  sp_rb(spn_build_event_t) buffer;
  sp_mutex_t mutex;
  sp_cv_t condition;
};

#endif
