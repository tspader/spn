#include "sp.h"
#include "sp/macro.h"
#include "event/log.h"

#include "event/event.h"

#include "log/types.h"
#include "sp/bind.h"

static const c8* level_name(s32 level) {
  switch (level) {
    case SPN_LOG_LEVEL_ERROR: return "error";
    case SPN_LOG_LEVEL_WARN:  return "warn";
    case SPN_LOG_LEVEL_INFO:  return "info";
    case SPN_LOG_LEVEL_DEBUG: return "debug";
    default:        return "unknown";
  }
}

static sp_bind_t* schemas[SPN_EVENT_COUNT] = {0};

static void build_schemas(sp_mem_t mem) {
  // SPN_EVENT_SYNC
  {
    sp_bind_builder_t b = sp_bind_builder_begin(mem);
    SP_BIND_SCHEMA(&b) {
      SP_BIND(&b, spn_evt_sync_t, name, "name", SP_BIND_STR);
      SP_BIND(&b, spn_evt_sync_t, url, "url", SP_BIND_STR);
    }
    schemas[SPN_EVENT_SYNC] = sp_bind_builder_end(&b);
  }

  // SPN_EVENT_BUILD_SCRIPT_PACKAGE
  {
    sp_bind_builder_t b = sp_bind_builder_begin(mem);
    SP_BIND_SCHEMA(&b) {
      SP_BIND(&b, spn_evt_script_package_t, time, "time_ns", SP_BIND_U64);
    }
    schemas[SPN_EVENT_BUILD_SCRIPT_PACKAGE] = sp_bind_builder_end(&b);
  }

  // SPN_EVENT_ADDED
  {
    sp_bind_builder_t b = sp_bind_builder_begin(mem);
    SP_BIND_SCHEMA(&b) {
      SP_BIND(&b, spn_evt_added_t, name, "name", SP_BIND_STR);
      SP_BIND(&b, spn_evt_added_t, version, "version", SP_BIND_STR);
    }
    schemas[SPN_EVENT_ADDED] = sp_bind_builder_end(&b);
  }

  // SPN_EVENT_PUBLISH, SPN_EVENT_PUBLISH_END
  {
    sp_bind_builder_t b = sp_bind_builder_begin(mem);
    SP_BIND_SCHEMA(&b) {
      SP_BIND(&b, spn_evt_publish_t, name, "name", SP_BIND_STR);
      SP_BIND(&b, spn_evt_publish_t, version, "version", SP_BIND_STR);
      SP_BIND(&b, spn_evt_publish_t, index, "index", SP_BIND_STR);
      SP_BIND(&b, spn_evt_publish_t, url, "url", SP_BIND_STR);
    }
    sp_bind_t* schema = sp_bind_builder_end(&b);
    schemas[SPN_EVENT_PUBLISH] = schema;
    schemas[SPN_EVENT_PUBLISH_END] = schema;
  }

  // SPN_EVENT_BUILD_SCRIPT_CRASHED
  {
    sp_bind_builder_t b = sp_bind_builder_begin(mem);
    SP_BIND_SCHEMA(&b) {
      SP_BIND(&b, spn_evt_script_crashed_t, path, "script_path", SP_BIND_STR);
      SP_BIND(&b, spn_evt_script_crashed_t, error, "error", SP_BIND_STR);
    }
    schemas[SPN_EVENT_BUILD_SCRIPT_CRASHED] = sp_bind_builder_end(&b);
  }

  // SPN_EVENT_BUILD_SCRIPT_COMPILE
  {
    sp_bind_builder_t b = sp_bind_builder_begin(mem);
    SP_BIND_SCHEMA(&b) {
      SP_BIND(&b, spn_evt_script_compile_t, script_path, "script_path", SP_BIND_STR);
      SP_BIND(&b, spn_evt_script_compile_t, time, "time_ns", SP_BIND_U64);
      SP_BIND(&b, spn_evt_script_compile_t, has_configure, "has_configure", SP_BIND_BOOL);
      SP_BIND(&b, spn_evt_script_compile_t, has_package, "has_package", SP_BIND_BOOL);
    }
    schemas[SPN_EVENT_BUILD_SCRIPT_COMPILE] = sp_bind_builder_end(&b);
  }

  // SPN_EVENT_BUILD_SCRIPT_COMPILE_FAILED
  {
    sp_bind_builder_t b = sp_bind_builder_begin(mem);
    SP_BIND_SCHEMA(&b) {
      SP_BIND(&b, spn_evt_compile_failed_t, script_path, "script_path", SP_BIND_STR);
      SP_BIND(&b, spn_evt_compile_failed_t, error, "error", SP_BIND_STR);
    }
    schemas[SPN_EVENT_BUILD_SCRIPT_COMPILE_FAILED] = sp_bind_builder_end(&b);
  }

  // SPN_EVENT_TARGET_BUILD_PASSED
  {
    sp_bind_builder_t b = sp_bind_builder_begin(mem);
    SP_BIND_SCHEMA(&b) {
      SP_BIND(&b, spn_evt_target_passed_t, source_file, "source_file", SP_BIND_STR);
      SP_BIND(&b, spn_evt_target_passed_t, object_file, "object_file", SP_BIND_STR);
      SP_BIND(&b, spn_evt_target_passed_t, time, "time_ns", SP_BIND_U64);
    }
    schemas[SPN_EVENT_TARGET_BUILD_PASSED] = sp_bind_builder_end(&b);
  }

  // SPN_EVENT_TARGET_BUILD_FAILED
  {
    sp_bind_builder_t b = sp_bind_builder_begin(mem);
    SP_BIND_SCHEMA(&b) {
      SP_BIND(&b, spn_evt_target_failed_t, source_file, "source_file", SP_BIND_STR);
      SP_BIND(&b, spn_evt_target_failed_t, object_file, "object_file", SP_BIND_STR);
      SP_BIND(&b, spn_evt_target_failed_t, rc, "rc", SP_BIND_S32);
      SP_BIND(&b, spn_evt_target_failed_t, out, "out", SP_BIND_STR);
      SP_BIND(&b, spn_evt_target_failed_t, err, "err", SP_BIND_STR);
      SP_BIND(&b, spn_evt_target_failed_t, time, "time_ns", SP_BIND_U64);
    }
    schemas[SPN_EVENT_TARGET_BUILD_FAILED] = sp_bind_builder_end(&b);
  }

  // SPN_EVENT_TARGET_RUN
  {
    sp_bind_builder_t b = sp_bind_builder_begin(mem);
    SP_BIND_SCHEMA(&b) {
      SP_BIND(&b, spn_evt_run_t, name, "name", SP_BIND_STR);
      SP_BIND(&b, spn_evt_run_t, command, "command", SP_BIND_STR);
    }
    schemas[SPN_EVENT_TARGET_RUN] = sp_bind_builder_end(&b);
  }

  // SPN_EVENT_INIT_BUILD_GRAPH
  {
    sp_bind_builder_t b = sp_bind_builder_begin(mem);
    SP_BIND_SCHEMA(&b) {
      SP_BIND(&b, spn_evt_graph_init_t, profile, "profile", SP_BIND_STR);
      SP_BIND(&b, spn_evt_graph_init_t, force, "force", SP_BIND_BOOL);
      SP_BIND(&b, spn_evt_graph_init_t, packages, "packages", SP_BIND_U32);
    }
    schemas[SPN_EVENT_INIT_BUILD_GRAPH] = sp_bind_builder_end(&b);
  }

  // SPN_EVENT_PREPARE_BUILD_GRAPH_FAILED
  {
    sp_bind_builder_t b = sp_bind_builder_begin(mem);
    SP_BIND_SCHEMA(&b) {
      SP_BIND(&b, spn_err_build_graph_t, kind, "kind", SP_BIND_S32);
      SP_BIND(&b, spn_err_build_graph_t, file, "file", SP_BIND_STR);
      SP_BIND(&b, spn_err_build_graph_t, command_a, "command_a", SP_BIND_STR);
      SP_BIND(&b, spn_err_build_graph_t, command_b, "command_b", SP_BIND_STR);
    }
    schemas[SPN_EVENT_PREPARE_BUILD_GRAPH_FAILED] = sp_bind_builder_end(&b);
  }

  // SPN_EVENT_LINK_PASSED
  {
    sp_bind_builder_t b = sp_bind_builder_begin(mem);
    SP_BIND_SCHEMA(&b) {
      SP_BIND(&b, spn_evt_link_passed_t, output_path, "output_path", SP_BIND_STR);
      SP_BIND(&b, spn_evt_link_passed_t, time, "time_ns", SP_BIND_U64);
    }
    schemas[SPN_EVENT_LINK_PASSED] = sp_bind_builder_end(&b);
  }

  // SPN_EVENT_LINK_FAILED
  {
    sp_bind_builder_t b = sp_bind_builder_begin(mem);
    SP_BIND_SCHEMA(&b) {
      SP_BIND(&b, spn_evt_link_failed_t, exit_code, "exit_code", SP_BIND_S32);
      SP_BIND(&b, spn_evt_link_failed_t, out, "out", SP_BIND_STR);
      SP_BIND(&b, spn_evt_link_failed_t, err, "err", SP_BIND_STR);
    }
    schemas[SPN_EVENT_LINK_FAILED] = sp_bind_builder_end(&b);
  }

  // SPN_EVENT_BUILD_SCRIPT_CONFIGURE_OK
  {
    sp_bind_builder_t b = sp_bind_builder_begin(mem);
    SP_BIND_SCHEMA(&b) {
      SP_BIND(&b, spn_evt_configure_t, time, "time_ns", SP_BIND_U64);
    }
    schemas[SPN_EVENT_BUILD_SCRIPT_CONFIGURE_OK] = sp_bind_builder_end(&b);
  }

  // SPN_EVENT_RESOLVE_PACKAGE
  {
    sp_bind_builder_t b = sp_bind_builder_begin(mem);
    SP_BIND_SCHEMA(&b) {
      SP_BIND(&b, spn_evt_resolve_pkg_t, name, "name", SP_BIND_STR);
      SP_BIND(&b, spn_evt_resolve_pkg_t, version, "version", SP_BIND_STR);
      SP_BIND(&b, spn_evt_resolve_pkg_t, kind, "kind", SP_BIND_S32);
    }
    schemas[SPN_EVENT_RESOLVE_PACKAGE] = sp_bind_builder_end(&b);
  }

  // SPN_EVENT_ERR_OPTION
  {
    sp_bind_builder_t b = sp_bind_builder_begin(mem);
    SP_BIND_SCHEMA(&b) {
      SP_BIND(&b, spn_evt_option_t, pkg, "pkg", SP_BIND_STR);
      SP_BIND(&b, spn_evt_option_t, option, "option", SP_BIND_STR);
      SP_BIND(&b, spn_evt_option_t, value, "value", SP_BIND_STR);
      SP_BIND(&b, spn_evt_option_t, a.kind, "a_kind", SP_BIND_S32);
      SP_BIND(&b, spn_evt_option_t, a.name, "a", SP_BIND_STR);
      SP_BIND(&b, spn_evt_option_t, b.kind, "b_kind", SP_BIND_S32);
      SP_BIND(&b, spn_evt_option_t, b.name, "b", SP_BIND_STR);
    }
    schemas[SPN_EVENT_ERR_OPTION] = sp_bind_builder_end(&b);
  }

  // SPN_EVENT_RESOLVE_END
  {
    sp_bind_builder_t b = sp_bind_builder_begin(mem);
    SP_BIND_SCHEMA(&b) {
      SP_BIND(&b, spn_evt_resolve_end_t, num_resolved, "num_resolved", SP_BIND_U32);
      SP_BIND(&b, spn_evt_resolve_end_t, time, "time_ns", SP_BIND_U64);
    }
    schemas[SPN_EVENT_RESOLVE_END] = sp_bind_builder_end(&b);
  }

  // SPN_EVENT_API_CALL
  {
    sp_bind_builder_t b = sp_bind_builder_begin(mem);
    SP_BIND_SCHEMA(&b) {
      SP_BIND(&b, spn_evt_api_call_t, fn, "fn", SP_BIND_STR);
      SP_BIND(&b, spn_evt_api_call_t, args, "args", SP_BIND_STR);
    }
    schemas[SPN_EVENT_API_CALL] = sp_bind_builder_end(&b);
  }

  // SPN_EVENT_USER_LOG
  {
    sp_bind_builder_t b = sp_bind_builder_begin(mem);
    SP_BIND_SCHEMA(&b) {
      SP_BIND(&b, spn_evt_user_log_t, message, "message", SP_BIND_STR);
    }
    schemas[SPN_EVENT_USER_LOG] = sp_bind_builder_end(&b);
  }

  // SPN_EVENT_SYNC_START
  {
    sp_bind_builder_t b = sp_bind_builder_begin(mem);
    SP_BIND_SCHEMA(&b) {
      SP_BIND(&b, spn_evt_sync_start_t, num_packages, "num_packages", SP_BIND_U32);
      SP_BIND(&b, spn_evt_sync_start_t, num_index, "num_index", SP_BIND_U32);
      SP_BIND(&b, spn_evt_sync_start_t, num_file, "num_file", SP_BIND_U32);
    }
    schemas[SPN_EVENT_SYNC_START] = sp_bind_builder_end(&b);
  }

  // SPN_EVENT_SYNC_PACKAGE
  {
    sp_bind_builder_t b = sp_bind_builder_begin(mem);
    SP_BIND_SCHEMA(&b) {
      SP_BIND(&b, spn_evt_sync_pkg_t, name, "name", SP_BIND_STR);
      SP_BIND(&b, spn_evt_sync_pkg_t, url, "url", SP_BIND_STR);
      SP_BIND(&b, spn_evt_sync_pkg_t, source_path, "source_path", SP_BIND_STR);
      SP_BIND(&b, spn_evt_sync_pkg_t, time, "time_ns", SP_BIND_U64);
      SP_BIND(&b, spn_evt_sync_pkg_t, fetched, "fetched", SP_BIND_BOOL);
    }
    schemas[SPN_EVENT_SYNC_PACKAGE] = sp_bind_builder_end(&b);
  }

  // SPN_EVENT_SYNC_FAILED
  {
    sp_bind_builder_t b = sp_bind_builder_begin(mem);
    SP_BIND_SCHEMA(&b) {
      SP_BIND(&b, spn_evt_sync_failed_t, name, "name", SP_BIND_STR);
      SP_BIND(&b, spn_evt_sync_failed_t, url, "url", SP_BIND_STR);
      SP_BIND(&b, spn_evt_sync_failed_t, error, "error", SP_BIND_STR);
    }
    schemas[SPN_EVENT_SYNC_FAILED] = sp_bind_builder_end(&b);
  }

  // SPN_EVENT_SYNC_STALE
  {
    sp_bind_builder_t b = sp_bind_builder_begin(mem);
    SP_BIND_SCHEMA(&b) {
      SP_BIND(&b, spn_evt_sync_t, name, "name", SP_BIND_STR);
      SP_BIND(&b, spn_evt_sync_t, url, "url", SP_BIND_STR);
    }
    schemas[SPN_EVENT_SYNC_STALE] = sp_bind_builder_end(&b);
  }

  // SPN_EVENT_ERR_MANIFEST
  {
    sp_bind_builder_t b = sp_bind_builder_begin(mem);
    SP_BIND_SCHEMA(&b) {
      SP_BIND(&b, spn_evt_manifest_err_t, name, "name", SP_BIND_STR);
      SP_BIND(&b, spn_evt_manifest_err_t, path, "path", SP_BIND_STR);
      SP_BIND(&b, spn_evt_manifest_err_t, error, "error", SP_BIND_STR);
    }
    schemas[SPN_EVENT_ERR_MANIFEST] = sp_bind_builder_end(&b);
  }

  // SPN_EVENT_SYNC_END
  {
    sp_bind_builder_t b = sp_bind_builder_begin(mem);
    SP_BIND_SCHEMA(&b) {
      SP_BIND(&b, spn_evt_sync_end_t, num_synced, "num_synced", SP_BIND_U32);
      SP_BIND(&b, spn_evt_sync_end_t, time, "time_ns", SP_BIND_U64);
    }
    schemas[SPN_EVENT_SYNC_END] = sp_bind_builder_end(&b);
  }

  // SPN_EVENT_CIRCULAR_DEP
  {
    sp_bind_builder_t b = sp_bind_builder_begin(mem);
    SP_BIND_SCHEMA(&b) {
      SP_BIND_OBJECT(&b, spn_evt_circular_t, id, "id") {
        SP_BIND(&b, spn_pkg_name_t, namespace, "namespace", SP_BIND_STR);
        SP_BIND(&b, spn_pkg_name_t, name, "name", SP_BIND_STR);
      }
    }
    schemas[SPN_EVENT_ERR_CIRCULAR_DEP] = sp_bind_builder_end(&b);
  }

  // SPN_EVENT_ERR_UNKNOWN_PKG
  {
    sp_bind_builder_t b = sp_bind_builder_begin(mem);
    SP_BIND_SCHEMA(&b) {
      SP_BIND_OBJECT(&b, spn_evt_unknown_t, request, "request") {
        SP_BIND(&b, spn_requested_dep_t, qualified, "qualified", SP_BIND_STR);
      }
    }
    schemas[SPN_EVENT_ERR_UNKNOWN_PKG] = sp_bind_builder_end(&b);
  }

  // SPN_EVENT_ERR_UNIT_CYCLE
  {
    sp_bind_builder_t b = sp_bind_builder_begin(mem);
    SP_BIND_SCHEMA(&b) {
      SP_BIND_OBJECT(&b, spn_evt_unit_cycle_t, id, "id") {
        SP_BIND(&b, spn_pkg_name_t, namespace, "namespace", SP_BIND_STR);
        SP_BIND(&b, spn_pkg_name_t, name, "name", SP_BIND_STR);
      }
      SP_BIND_OBJECT(&b, spn_evt_unit_cycle_t, version, "version") {
        SP_BIND(&b, spn_semver_t, major, "major", SP_BIND_U32);
        SP_BIND(&b, spn_semver_t, minor, "minor", SP_BIND_U32);
        SP_BIND(&b, spn_semver_t, patch, "patch", SP_BIND_U32);
      }
    }
    schemas[SPN_EVENT_ERR_UNIT_CYCLE] = sp_bind_builder_end(&b);
  }

  // SPN_EVENT_ERR_DYNAMIC_DUPLICATE
  {
    sp_bind_builder_t b = sp_bind_builder_begin(mem);
    SP_BIND_SCHEMA(&b) {
      SP_BIND_OBJECT(&b, spn_evt_dynamic_dup_t, id, "id") {
        SP_BIND(&b, spn_pkg_name_t, namespace, "namespace", SP_BIND_STR);
        SP_BIND(&b, spn_pkg_name_t, name, "name", SP_BIND_STR);
      }
      SP_BIND_OBJECT(&b, spn_evt_dynamic_dup_t, low, "low") {
        SP_BIND(&b, spn_semver_t, major, "major", SP_BIND_U32);
        SP_BIND(&b, spn_semver_t, minor, "minor", SP_BIND_U32);
        SP_BIND(&b, spn_semver_t, patch, "patch", SP_BIND_U32);
      }
      SP_BIND_OBJECT(&b, spn_evt_dynamic_dup_t, high, "high") {
        SP_BIND(&b, spn_semver_t, major, "major", SP_BIND_U32);
        SP_BIND(&b, spn_semver_t, minor, "minor", SP_BIND_U32);
        SP_BIND(&b, spn_semver_t, patch, "patch", SP_BIND_U32);
      }
    }
    schemas[SPN_EVENT_ERR_DYNAMIC_DUPLICATE] = sp_bind_builder_end(&b);
  }

  // SPN_EVENT_ERR_RESOLUTION_TOO_COMPLEX
  {
    sp_bind_builder_t b = sp_bind_builder_begin(mem);
    SP_BIND_SCHEMA(&b) {
      SP_BIND_OBJECT(&b, spn_evt_too_complex_t, id, "id") {
        SP_BIND(&b, spn_pkg_name_t, namespace, "namespace", SP_BIND_STR);
        SP_BIND(&b, spn_pkg_name_t, name, "name", SP_BIND_STR);
      }
    }
    schemas[SPN_EVENT_ERR_RESOLUTION_TOO_COMPLEX] = sp_bind_builder_end(&b);
  }

  // SPN_EVENT_ERR_UNSATISFIABLE_VERSION
  {
    sp_bind_builder_t b = sp_bind_builder_begin(mem);
    SP_BIND_SCHEMA(&b) {
      SP_BIND_OBJECT(&b, spn_evt_unsatisfiable_t, request, "request") {
        SP_BIND(&b, spn_requested_dep_t, qualified, "qualified", SP_BIND_STR);
      }
      SP_BIND(&b, spn_evt_unsatisfiable_t, requester, "requester", SP_BIND_STR);
      SP_BIND(&b, spn_evt_unsatisfiable_t, conflict, "conflict", SP_BIND_BOOL);
      SP_BIND_OBJECT(&b, spn_evt_unsatisfiable_t, selected, "selected") {
        SP_BIND(&b, spn_semver_t, major, "major", SP_BIND_U32);
        SP_BIND(&b, spn_semver_t, minor, "minor", SP_BIND_U32);
        SP_BIND(&b, spn_semver_t, patch, "patch", SP_BIND_U32);
      }
    }
    schemas[SPN_EVENT_ERR_UNSATISFIABLE_VERSION] = sp_bind_builder_end(&b);
  }

  // SPN_EVENT_EMBED_START
  {
    sp_bind_builder_t b = sp_bind_builder_begin(mem);
    SP_BIND_SCHEMA(&b) {
      SP_BIND(&b, spn_evt_embed_start_t, num_files, "num_files", SP_BIND_U32);
    }
    schemas[SPN_EVENT_EMBED_START] = sp_bind_builder_end(&b);
  }

  // SPN_EVENT_EMBED_PASSED
  {
    sp_bind_builder_t b = sp_bind_builder_begin(mem);
    SP_BIND_SCHEMA(&b) {
      SP_BIND(&b, spn_evt_embed_passed_t, object_path, "object_path", SP_BIND_STR);
      SP_BIND(&b, spn_evt_embed_passed_t, header_path, "header_path", SP_BIND_STR);
      SP_BIND(&b, spn_evt_embed_passed_t, time, "time_ns", SP_BIND_U64);
    }
    schemas[SPN_EVENT_EMBED_PASSED] = sp_bind_builder_end(&b);
  }

  // SPN_EVENT_EMBED_FAILED
  {
    sp_bind_builder_t b = sp_bind_builder_begin(mem);
    SP_BIND_SCHEMA(&b) {
      SP_BIND(&b, spn_evt_embed_failed_t, path, "path", SP_BIND_STR);
      SP_BIND(&b, spn_evt_embed_failed_t, error, "error", SP_BIND_STR);
    }
    schemas[SPN_EVENT_EMBED_FAILED] = sp_bind_builder_end(&b);
  }

  // SPN_EVENT_DIRTY_SUMMARY
  {
    sp_bind_builder_t b = sp_bind_builder_begin(mem);
    SP_BIND_SCHEMA(&b) {
      SP_BIND(&b, spn_evt_dirty_summary_t, total_commands, "total_commands", SP_BIND_U32);
      SP_BIND(&b, spn_evt_dirty_summary_t, dirty_commands, "dirty_commands", SP_BIND_U32);
      SP_BIND(&b, spn_evt_dirty_summary_t, total_files, "total_files", SP_BIND_U32);
      SP_BIND(&b, spn_evt_dirty_summary_t, dirty_files, "dirty_files", SP_BIND_U32);
      SP_BIND(&b, spn_evt_dirty_summary_t, forced, "forced", SP_BIND_BOOL);
    }
    schemas[SPN_EVENT_DIRTY_SUMMARY] = sp_bind_builder_end(&b);
  }

  // SPN_EVENT_BUILD_FAILED
  {
    sp_bind_builder_t b = sp_bind_builder_begin(mem);
    SP_BIND_SCHEMA(&b) {
      SP_BIND(&b, spn_evt_build_failed_t, profile, "profile", SP_BIND_STR);
      SP_BIND(&b, spn_evt_build_failed_t, time, "time_ns", SP_BIND_U64);
      SP_BIND(&b, spn_evt_build_failed_t, num_errors, "num_errors", SP_BIND_U32);
      SP_BIND(&b, spn_evt_build_failed_t, first_error, "first_error", SP_BIND_STR);
    }
    schemas[SPN_EVENT_BUILD_FAILED] = sp_bind_builder_end(&b);
  }

  {
    sp_bind_builder_t b = sp_bind_builder_begin(mem);
    SP_BIND_SCHEMA(&b) {
      SP_BIND(&b, spn_evt_build_cancelled_t, profile, "profile", SP_BIND_STR);
      SP_BIND(&b, spn_evt_build_cancelled_t, time, "time_ns", SP_BIND_U64);
      SP_BIND(&b, spn_evt_build_cancelled_t, num_pending, "num_pending", SP_BIND_U32);
    }
    schemas[SPN_EVENT_BUILD_CANCELLED] = sp_bind_builder_end(&b);
  }

  // SPN_EVENT_BUILD_SUMMARY
  {
    sp_bind_builder_t b = sp_bind_builder_begin(mem);
    SP_BIND_SCHEMA(&b) {
      SP_BIND(&b, spn_evt_build_summary_t, success, "success", SP_BIND_BOOL);
      SP_BIND(&b, spn_evt_build_summary_t, num_dirty, "num_dirty", SP_BIND_U32);
      SP_BIND(&b, spn_evt_build_summary_t, total_commands, "total_commands", SP_BIND_U32);
      SP_BIND(&b, spn_evt_build_summary_t, time, "time_ns", SP_BIND_U64);
      SP_BIND(&b, spn_evt_build_summary_t, profile, "profile", SP_BIND_STR);
    }
    schemas[SPN_EVENT_BUILD_SUMMARY] = sp_bind_builder_end(&b);
  }

  // SPN_EVENT_BUILD_SCRIPT_PACKAGE_OK
  {
    sp_bind_builder_t b = sp_bind_builder_begin(mem);
    SP_BIND_SCHEMA(&b) {
      SP_BIND(&b, spn_evt_package_ok_t, time, "time_ns", SP_BIND_U64);
    }
    schemas[SPN_EVENT_BUILD_SCRIPT_PACKAGE_OK] = sp_bind_builder_end(&b);
  }
}

// ============================================================================
// JSON serializer — walks a bind schema + data pointer
// ============================================================================

void spn_json_write_str(sp_io_writer_t* out, sp_str_t str) {
  sp_io_write_c8(out, '"');
  for (u32 i = 0; i < str.len; i++) {
    c8 c = str.data[i];
    switch (c) {
      case '"':  sp_io_write_cstr(out, "\\\"", SP_NULLPTR); break;
      case '\\': sp_io_write_cstr(out, "\\\\", SP_NULLPTR); break;
      case '\n': sp_io_write_cstr(out, "\\n", SP_NULLPTR);  break;
      case '\r': sp_io_write_cstr(out, "\\r", SP_NULLPTR);  break;
      case '\t': sp_io_write_cstr(out, "\\t", SP_NULLPTR);  break;
      default: {
        if ((u8)c < 0x20) {
          const c8* hex = "0123456789abcdef";
          sp_io_write_cstr(out, "\\u00", SP_NULLPTR);
          sp_io_write_c8(out, hex[((u8)c >> 4) & 0xf]);
          sp_io_write_c8(out, hex[(u8)c & 0xf]);
        } else {
          sp_io_write_c8(out, c);
        }
        break;
      }
    }
  }
  sp_io_write_c8(out, '"');
}

static void json_write_value(sp_io_writer_t* out, sp_bind_field_t* field, void* base);

static void json_write_object(sp_io_writer_t* out, sp_bind_t* schema, void* data) {
  sp_io_write_c8(out, '{');
  sp_da_for(schema->as.object.fields, i) {
    sp_bind_field_t* f = &schema->as.object.fields[i];
    if (i > 0) sp_io_write_cstr(out, ", ", SP_NULLPTR);
    spn_json_write_str(out, sp_str_view(f->key));
    sp_io_write_cstr(out, ": ", SP_NULLPTR);
    json_write_value(out, f, data);
  }
  sp_io_write_c8(out, '}');
}

static void json_write_value(sp_io_writer_t* out, sp_bind_field_t* field, void* base) {
  void* ptr = sp_bind_field_ptr(field, base);

  switch (field->kind) {
    case SP_BIND_STR: {
      spn_json_write_str(out, *(sp_str_t*)ptr);
      break;
    }
    case SP_BIND_BOOL: {
      sp_io_write_cstr(out, *(bool*)ptr ? "true" : "false", SP_NULLPTR);
      break;
    }
    case SP_BIND_S32: {
      sp_fmt_io(out, "{}", sp_fmt_int(*(s32*)ptr));
      break;
    }
    case SP_BIND_U32: {
      sp_fmt_io(out, "{}", sp_fmt_uint(*(u32*)ptr));
      break;
    }
    case SP_BIND_U64: {
      sp_fmt_io(out, "{}", sp_fmt_uint(*(u64*)ptr));
      break;
    }
    case SP_BIND_S64: {
      sp_fmt_io(out, "{}", sp_fmt_int(*(s64*)ptr));
      break;
    }
    case SP_BIND_F64: {
      sp_fmt_io(out, "{}", sp_fmt_float(*(f64*)ptr));
      break;
    }
    case SP_BIND_OBJECT: {
      json_write_object(out, field, ptr);
      break;
    }
    case SP_BIND_NONE:
    case SP_BIND_ARRAY:
    case SP_BIND_MAP: {
      sp_io_write_cstr(out, "null", SP_NULLPTR);
      break;
    }
  }
}

// ============================================================================
// Public API
// ============================================================================

void spn_event_log_init(sp_mem_t mem) {
  build_schemas(mem);
}

void spn_event_log_jsonl(sp_io_writer_t* out, spn_build_event_t* event) {
  sp_bind_t* schema = schemas[event->kind];

  sp_tm_epoch_t now = sp_tm_now_epoch();

  const spn_event_info_t* info = &spn_event_info[event->kind];
  sp_io_write_cstr(out, "{\"event\": \"", SP_NULLPTR);
  sp_io_write_cstr(out, info->name ? info->name : "unknown", SP_NULLPTR);
  sp_io_write_cstr(out, "\"", SP_NULLPTR);

  s32 level = info->level;
  sp_fmt_io(out, ", \"level\": {}", sp_fmt_int(level));
  sp_io_write_cstr(out, ", \"level_name\": \"", SP_NULLPTR);
  sp_io_write_cstr(out, level_name(level), SP_NULLPTR);
  sp_io_write_cstr(out, "\"", SP_NULLPTR);

  sp_fmt_io(out, ", \"ts_s\": {}, \"ts_ns\": {}", sp_fmt_uint(now.s), sp_fmt_uint(now.ns));

  sp_fmt_io(out, ", \"tid\": {}", sp_fmt_uint(event->thread_id));

  if (event->pkg) {
    sp_io_write_cstr(out, ", \"pkg\": ", SP_NULLPTR);
    spn_json_write_str(out, event->pkg->name);
  }

  switch (event->kind) {
    case SPN_EVENT_TARGET_BUILD_PASSED:
    case SPN_EVENT_TARGET_BUILD_FAILED:
    case SPN_EVENT_LINK_START:
    case SPN_EVENT_LINK_PASSED:
    case SPN_EVENT_LINK_FAILED: {
      if (sp_str_valid(event->target.name)) {
        sp_io_write_cstr(out, ", \"target\": ", SP_NULLPTR);
        spn_json_write_str(out, event->target.name);
      }
      break;
    }
    default: break;
  }

  if (schema) {
    void* data = spn_event_payload(event);
    if (data) {
      sp_io_write_cstr(out, ", \"data\": ", SP_NULLPTR);
      json_write_object(out, schema, data);
    }
  }

  sp_io_write_cstr(out, "}\n", SP_NULLPTR);
}
