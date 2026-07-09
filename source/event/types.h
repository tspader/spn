#ifndef SPN_EVENT_TYPES_H
#define SPN_EVENT_TYPES_H

#include "sp.h"

#include "error/types.h"
#include "forward/types.h"
#include "pkg/types.h"
#include "unit/types.h"

// One row per event kind: enum, jsonl name, TUI verb, TUI verbosity, jsonl
// log level, whether it renders as an error, whether that error is terminal,
// and the union arm carrying its payload (SPN_EVT_NONE when it has none).
// Every consumer table derives from this list, so a new event is one row.
#define SPN_EVENT_LIST(X) \
  X(SPN_EVENT_ERR,                          "err",                        "error",       QUIET,   ERROR, true,  true,  SPN_EVT_NONE) \
  X(SPN_EVENT_ERR_CIRCULAR_DEP,             "err_circular_dep",           "error",       QUIET,   ERROR, true,  true,  SPN_EVT(circular)) \
  X(SPN_EVENT_ERR_UNKNOWN_PKG,              "err_unknown_pkg",            "error",       QUIET,   ERROR, true,  true,  SPN_EVT(unknown)) \
  X(SPN_EVENT_ERR_UNSATISFIABLE_VERSION,    "err_unsatisfiable_version",  "error",       QUIET,   ERROR, true,  true,  SPN_EVT(unsatisfiable)) \
  X(SPN_EVENT_ERR_MANIFEST,                 "err_manifest",               "error",       QUIET,   ERROR, true,  true,  SPN_EVT(manifest_err)) \
  X(SPN_EVENT_ERR_UNIT_CYCLE,               "err_unit_cycle",             "error",       QUIET,   ERROR, true,  true,  SPN_EVT(unit_cycle)) \
  X(SPN_EVENT_ERR_DYNAMIC_DUPLICATE,        "err_dynamic_duplicate",      "error",       QUIET,   ERROR, true,  true,  SPN_EVT(dynamic_dup)) \
  X(SPN_EVENT_ERR_RESOLUTION_TOO_COMPLEX,   "err_resolution_too_complex", "error",       QUIET,   ERROR, true,  true,  SPN_EVT(too_complex)) \
  X(SPN_EVENT_ERR_OPTION,                   "err_option",                 "error",       QUIET,   ERROR, true,  true,  SPN_EVT(option)) \
  X(SPN_EVENT_RESOLVE_START,                "resolve_start",              "Resolving",   NORMAL,  INFO,  false, false, SPN_EVT_NONE) \
  X(SPN_EVENT_RESOLVE_PACKAGE,              "resolve_package",            "Resolving",   DEBUG,   INFO,  false, false, SPN_EVT(resolve_pkg)) \
  X(SPN_EVENT_RESOLVE_END,                  "resolve_end",                "Resolved",    NORMAL,  INFO,  false, false, SPN_EVT(resolve_end)) \
  X(SPN_EVENT_SYNC,                         "sync",                       "Downloading", NORMAL,  INFO,  false, false, SPN_EVT(sync)) \
  X(SPN_EVENT_SYNC_START,                   "sync_start",                 "Syncing",     DEBUG,   INFO,  false, false, SPN_EVT(sync_start)) \
  X(SPN_EVENT_SYNC_PACKAGE,                 "sync_package",               "Synced",      DEBUG,   INFO,  false, false, SPN_EVT(sync_pkg)) \
  X(SPN_EVENT_SYNC_FAILED,                  "sync_failed",                "error",       QUIET,   ERROR, true,  true,  SPN_EVT(sync_failed)) \
  X(SPN_EVENT_SYNC_STALE,                   "sync_stale",                 "Stale",       NORMAL,  INFO,  false, false, SPN_EVT(sync)) \
  X(SPN_EVENT_SYNC_END,                     "sync_end",                   "Downloaded",  NORMAL,  INFO,  false, false, SPN_EVT(sync_end)) \
  X(SPN_EVENT_BUILD_SCRIPT_COMPILE,         "script_compile",             "Compiling",   VERBOSE, INFO,  false, false, SPN_EVT(script_compile)) \
  X(SPN_EVENT_BUILD_SCRIPT_COMPILE_FAILED,  "script_compile_failed",      "error",       QUIET,   ERROR, true,  true,  SPN_EVT(compile_failed)) \
  X(SPN_EVENT_BUILD_SCRIPT_CONFIGURE,       "configure",                  "Configuring", DEBUG,   INFO,  false, false, SPN_EVT_NONE) \
  X(SPN_EVENT_BUILD_SCRIPT_CONFIGURE_OK,    "configure_ok",               "Configured",  DEBUG,   DEBUG, false, false, SPN_EVT(configure)) \
  X(SPN_EVENT_BUILD_SCRIPT_PACKAGE,         "script_package",             "Packaging",   DEBUG,   INFO,  false, false, SPN_EVT(script_package)) \
  X(SPN_EVENT_BUILD_SCRIPT_PACKAGE_OK,      "package_ok",                 "Packaged",    DEBUG,   INFO,  false, false, SPN_EVT(package_ok)) \
  X(SPN_EVENT_BUILD_SCRIPT_CRASHED,         "script_crashed",             "error",       QUIET,   ERROR, true,  true,  SPN_EVT(crashed)) \
  X(SPN_EVENT_BUILD_SCRIPT_USER_FN,         "script_user_fn",             "Running",     DEBUG,   DEBUG, false, false, SPN_EVT_NONE) \
  X(SPN_EVENT_COMPILE_START,                "compile_start",              "Compiling",   NORMAL,  INFO,  false, false, SPN_EVT_NONE) \
  X(SPN_EVENT_TARGET_BUILD_PASSED,          "target_build_passed",        "Compiled",    DEBUG,   INFO,  false, false, SPN_EVT(target.passed)) \
  X(SPN_EVENT_TARGET_BUILD_FAILED,          "target_build_failed",        "error",       QUIET,   ERROR, true,  false, SPN_EVT(target.failed)) \
  X(SPN_EVENT_TARGET_RUN,                   "target_run",                 "Running",     NORMAL,  INFO,  false, false, SPN_EVT(run)) \
  X(SPN_EVENT_LINK_START,                   "link_start",                 "Linking",     VERBOSE, INFO,  false, false, SPN_EVT(target.link_start)) \
  X(SPN_EVENT_LINK_PASSED,                  "link_passed",                "Linked",      DEBUG,   INFO,  false, false, SPN_EVT(target.link_passed)) \
  X(SPN_EVENT_LINK_FAILED,                  "link_failed",                "error",       QUIET,   ERROR, true,  false, SPN_EVT(target.link_failed)) \
  X(SPN_EVENT_EMBED_START,                  "embed_start",                "Embedding",   VERBOSE, INFO,  false, false, SPN_EVT(embed_start)) \
  X(SPN_EVENT_EMBED_PASSED,                 "embed_passed",               "Embedded",    DEBUG,   INFO,  false, false, SPN_EVT(embed_passed)) \
  X(SPN_EVENT_EMBED_FAILED,                 "embed_failed",               "error",       QUIET,   ERROR, true,  false, SPN_EVT(embed_failed)) \
  X(SPN_EVENT_INIT_BUILD_GRAPH,             "init_build_graph",           "Planning",    DEBUG,   INFO,  false, false, SPN_EVT(graph_init)) \
  X(SPN_EVENT_PREPARE_BUILD_GRAPH_FAILED,   "prepare_build_graph_failed", "error",       QUIET,   ERROR, true,  true,  SPN_EVT(err.build_graph)) \
  X(SPN_EVENT_DIRTY_SUMMARY,                "dirty_summary",              "Planned",     DEBUG,   INFO,  false, false, SPN_EVT(dirty_summary)) \
  X(SPN_EVENT_BUILD_PASSED,                 "build_passed",               "Finished",    NORMAL,  INFO,  false, false, SPN_EVT(build.passed)) \
  X(SPN_EVENT_BUILD_FAILED,                 "build_failed",               "error",       QUIET,   ERROR, true,  true,  SPN_EVT(build_failed)) \
  X(SPN_EVENT_BUILD_SUMMARY,                "build_summary",              "Summary",     DEBUG,   INFO,  false, false, SPN_EVT(build_summary)) \
  X(SPN_EVENT_API_CALL,                     "api_call",                   "Calling",     DEBUG,   DEBUG, false, false, SPN_EVT(api_call)) \
  X(SPN_EVENT_USER_LOG,                     "user_log",                   "",            VERBOSE, INFO,  false, false, SPN_EVT(user_log)) \
  X(SPN_EVENT_ADDED,                        "added",                      "Added",       NORMAL,  INFO,  false, false, SPN_EVT(added))

#define SPN_EVENT_ENUM(kind, ...) kind,
typedef enum {
  SPN_EVENT_LIST(SPN_EVENT_ENUM)
  SPN_EVENT_COUNT,
} spn_build_event_kind_t;
#undef SPN_EVENT_ENUM

// ============================================================================
// Named event variant types (used in the union and for bind schemas)
// ============================================================================

typedef struct { spn_user_node_t* info; } spn_evt_node_t;

typedef struct {
  sp_str_t source_file;
  sp_str_t object_file;
  spn_invocation_t* invocation;
  sp_str_t out;
  u64 time;
} spn_evt_target_passed_t;
typedef struct {
  sp_str_t source_file;
  sp_str_t object_file;
  s32 rc;
  sp_str_t out;
  sp_str_t err;
  spn_invocation_t* invocation;
  u64 time;
} spn_evt_target_failed_t;

typedef struct { spn_profile_info_t* profile; u64 time; } spn_evt_build_passed_t;

typedef struct { sp_str_t name; sp_str_t command; } spn_evt_run_t;
typedef struct {
  spn_requested_pkg_t request;
  sp_str_t requester;
  spn_semver_t requester_version;
  bool conflict;
  spn_semver_t selected;
} spn_evt_unsatisfiable_t;
typedef struct { spn_pkg_name_t id; } spn_evt_circular_t;
typedef struct { spn_pkg_name_t id; spn_semver_t version; } spn_evt_unit_cycle_t;
typedef struct { spn_pkg_name_t id; spn_semver_t low; spn_semver_t high; } spn_evt_dynamic_dup_t;
typedef struct { spn_pkg_name_t id; } spn_evt_too_complex_t;

typedef enum {
  SPN_OPTION_ERR_UNDECLARED,
  SPN_OPTION_ERR_BAD_VALUE,
  SPN_OPTION_ERR_CONFLICT,
  SPN_OPTION_ERR_VETO,
  SPN_OPTION_ERR_NO_VALUE,
  SPN_OPTION_ERR_LATE_GATE,
  SPN_OPTION_ERR_UNKNOWN_PKG,
} spn_option_err_t;

typedef struct {
  spn_option_err_t err;
  sp_str_t pkg;
  sp_str_t option;
  sp_str_t value;
  sp_str_t a;
  sp_str_t b;
} spn_evt_option_t;
typedef struct { spn_requested_pkg_t request; } spn_evt_unknown_t;
typedef struct { sp_str_t script_path; u64 time; bool has_configure; bool has_package; } spn_evt_script_compile_t;
typedef struct { u64 time; } spn_evt_script_package_t;
typedef struct { sp_str_t script_path; sp_str_t error; } spn_evt_compile_failed_t;
typedef struct { sp_str_t path; sp_str_t error; } spn_evt_script_crashed_t;
typedef struct { sp_str_t name; sp_str_t url; } spn_evt_sync_t;
typedef struct { sp_str_t profile; bool force; u32 packages; } spn_evt_graph_init_t;
typedef struct {
  spn_target_unit_t* target;
} spn_evt_link_start_t;
typedef struct {
  sp_str_t output_path;
  spn_invocation_t* invocation;
  sp_str_t out;
  u64 time;
} spn_evt_link_passed_t;
typedef struct {
  s32 exit_code;
  sp_str_t out;
  sp_str_t err;
  spn_invocation_t* invocation;
} spn_evt_link_failed_t;
typedef struct { u64 time; } spn_evt_configure_t;
typedef struct { sp_str_t name; sp_str_t version; s32 kind; } spn_evt_resolve_pkg_t;
typedef struct { u32 num_resolved; u64 time; } spn_evt_resolve_end_t;
typedef struct { u32 num_packages; u32 num_index; u32 num_file; } spn_evt_sync_start_t;
typedef struct { sp_str_t name; sp_str_t url; sp_str_t source_path; u64 time; bool fetched; } spn_evt_sync_pkg_t;
typedef struct { sp_str_t name; sp_str_t url; sp_str_t error; } spn_evt_sync_failed_t;
typedef struct { sp_str_t name; sp_str_t path; sp_str_t error; sp_da(spn_codegen_issue_t) issues; } spn_evt_manifest_err_t;
typedef struct { u32 num_synced; u64 time; } spn_evt_sync_end_t;
typedef struct { sp_str_t fn; sp_str_t args; } spn_evt_api_call_t;
typedef struct { sp_str_t message; } spn_evt_user_log_t;
typedef struct { sp_str_t name; sp_str_t version; } spn_evt_added_t;
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
  spn_pkg_info_t* pkg;
  spn_build_io_t* io;
  u64 thread_id;

  union {
    spn_err_union_t err;
    spn_evt_node_t node;
    struct {
      sp_str_t name;
      union {
        spn_evt_target_passed_t passed;
        spn_evt_target_failed_t failed;
        spn_evt_link_start_t link_start;
        spn_evt_link_passed_t link_passed;
        spn_evt_link_failed_t link_failed;
      };
    } target;
    union {
      spn_evt_build_passed_t passed;
    } build;
    spn_evt_run_t run;
    spn_evt_circular_t circular;
    spn_evt_unit_cycle_t unit_cycle;
    spn_evt_dynamic_dup_t dynamic_dup;
    spn_evt_too_complex_t too_complex;
    spn_evt_option_t option;
    spn_evt_unsatisfiable_t unsatisfiable;
    spn_evt_unknown_t unknown;
    spn_evt_script_compile_t script_compile;
    spn_evt_compile_failed_t compile_failed;
    spn_evt_script_crashed_t crashed;
    spn_evt_script_package_t script_package;
    spn_evt_sync_t sync;
    spn_evt_graph_init_t graph_init;
    spn_evt_configure_t configure;
    spn_evt_resolve_pkg_t resolve_pkg;
    spn_evt_resolve_end_t resolve_end;
    spn_evt_sync_start_t sync_start;
    spn_evt_sync_pkg_t sync_pkg;
    spn_evt_sync_failed_t sync_failed;
    spn_evt_manifest_err_t manifest_err;
    spn_evt_sync_end_t sync_end;
    spn_evt_api_call_t api_call;
    spn_evt_user_log_t user_log;
    spn_evt_added_t added;
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
