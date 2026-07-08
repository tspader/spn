#include "sp.h"
#include "sp/macro.h"
#include "event/log.h"

#include "log/types.h"
#include "sp/bind.h"

static s32 event_level(spn_build_event_kind_t kind) {
  switch (kind) {
    case SPN_EVENT_ERR:
    case SPN_EVENT_ERR_CIRCULAR_DEP:
    case SPN_EVENT_ERR_UNKNOWN_PKG:
    case SPN_EVENT_ERR_UNSATISFIABLE_VERSION:
    case SPN_EVENT_ERR_RESOLUTION_TOO_COMPLEX:
    case SPN_EVENT_ERR_MANIFEST:
    case SPN_EVENT_BUILD_SCRIPT_COMPILE_FAILED:
    case SPN_EVENT_BUILD_SCRIPT_CRASHED:
    case SPN_EVENT_TARGET_BUILD_FAILED:
    case SPN_EVENT_SYNC_FAILED:
    case SPN_EVENT_LINK_FAILED:
    case SPN_EVENT_EMBED_FAILED:
    case SPN_EVENT_PREPARE_BUILD_GRAPH_FAILED:
    case SPN_EVENT_BUILD_FAILED: {
      return SPN_LOG_LEVEL_ERROR;
    }
    case SPN_EVENT_BUILD_SCRIPT_CONFIGURE_OK:
    case SPN_EVENT_BUILD_SCRIPT_USER_FN:
    case SPN_EVENT_API_CALL: {
      return SPN_LOG_LEVEL_DEBUG;
    }
    default: {
      return SPN_LOG_LEVEL_INFO;
    }
  }
}

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
      SP_BIND(&b, spn_evt_link_failed_t, linker, "linker", SP_BIND_STR);
      SP_BIND(&b, spn_evt_link_failed_t, args, "args", SP_BIND_STR);
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
        SP_BIND(&b, spn_requested_pkg_t, qualified, "qualified", SP_BIND_STR);
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
        SP_BIND(&b, spn_requested_pkg_t, qualified, "qualified", SP_BIND_STR);
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
// Variant pointer — switch on tag, return pointer into the union
// ============================================================================

static void* event_variant_ptr(spn_build_event_t* event) {
  switch (event->kind) {
    case SPN_EVENT_SYNC:                        return &event->sync;
    case SPN_EVENT_BUILD_SCRIPT_COMPILE:        return &event->script_compile;
    case SPN_EVENT_BUILD_SCRIPT_CRASHED:        return &event->crashed;
    case SPN_EVENT_BUILD_SCRIPT_PACKAGE:        return &event->script_package;
    case SPN_EVENT_BUILD_SCRIPT_CONFIGURE_OK:   return &event->configure;
    case SPN_EVENT_BUILD_SCRIPT_COMPILE_FAILED: return &event->compile_failed;
    case SPN_EVENT_TARGET_BUILD_PASSED:         return &event->target.passed;
    case SPN_EVENT_TARGET_BUILD_FAILED:         return &event->target.failed;
    case SPN_EVENT_TARGET_RUN:                  return &event->run;
    case SPN_EVENT_INIT_BUILD_GRAPH:            return &event->graph_init;
    case SPN_EVENT_PREPARE_BUILD_GRAPH_FAILED:  return &event->err.build_graph;
    case SPN_EVENT_LINK_START:                  return &event->target.link_start;
    case SPN_EVENT_LINK_PASSED:                 return &event->target.link_passed;
    case SPN_EVENT_LINK_FAILED:                 return &event->target.link_failed;
    case SPN_EVENT_RESOLVE_PACKAGE:             return &event->resolve_pkg;
    case SPN_EVENT_RESOLVE_END:                 return &event->resolve_end;
    case SPN_EVENT_USER_LOG:                    return &event->user_log;
    case SPN_EVENT_SYNC_START:                  return &event->sync_start;
    case SPN_EVENT_SYNC_PACKAGE:                return &event->sync_pkg;
    case SPN_EVENT_SYNC_FAILED:                 return &event->sync_failed;
    case SPN_EVENT_SYNC_STALE:                  return &event->sync;
    case SPN_EVENT_ERR_MANIFEST:                return &event->manifest_err;
    case SPN_EVENT_SYNC_END:                    return &event->sync_end;
    case SPN_EVENT_API_CALL:                    return &event->api_call;
    case SPN_EVENT_ERR_CIRCULAR_DEP:            return &event->circular;
    case SPN_EVENT_ERR_UNKNOWN_PKG:             return &event->unknown;
    case SPN_EVENT_ERR_UNIT_CYCLE:              return &event->unit_cycle;
    case SPN_EVENT_ERR_UNSATISFIABLE_VERSION:   return &event->unsatisfiable;
    case SPN_EVENT_ERR_DYNAMIC_DUPLICATE:       return &event->dynamic_dup;
    case SPN_EVENT_ERR_RESOLUTION_TOO_COMPLEX:  return &event->too_complex;
    case SPN_EVENT_EMBED_START:                 return &event->embed_start;
    case SPN_EVENT_EMBED_PASSED:                return &event->embed_passed;
    case SPN_EVENT_EMBED_FAILED:                return &event->embed_failed;
    case SPN_EVENT_DIRTY_SUMMARY:               return &event->dirty_summary;
    case SPN_EVENT_BUILD_FAILED:                return &event->build_failed;
    case SPN_EVENT_BUILD_SUMMARY:               return &event->build_summary;
    case SPN_EVENT_BUILD_SCRIPT_PACKAGE_OK:     return &event->package_ok;
    default:                                    return SP_NULLPTR;
  }
}

// ============================================================================
// Machine-readable event names (unique per kind, unlike TUI display names)
// ============================================================================

static const c8* event_names[SPN_EVENT_COUNT] = {
  [SPN_EVENT_ERR]                           = "err",
  [SPN_EVENT_ERR_CIRCULAR_DEP]              = "err_circular_dep",
  [SPN_EVENT_ERR_UNKNOWN_PKG]               = "err_unknown_pkg",
  [SPN_EVENT_ERR_UNSATISFIABLE_VERSION]     = "err_unsatisfiable_version",
  [SPN_EVENT_ERR_MANIFEST]                  = "err_manifest",
  [SPN_EVENT_ERR_UNIT_CYCLE]                = "err_unit_cycle",
  [SPN_EVENT_ERR_DYNAMIC_DUPLICATE]         = "err_dynamic_duplicate",
  [SPN_EVENT_ERR_RESOLUTION_TOO_COMPLEX]    = "err_resolution_too_complex",
  [SPN_EVENT_RESOLVE_START]                 = "resolve_start",
  [SPN_EVENT_RESOLVE_PACKAGE]               = "resolve_package",
  [SPN_EVENT_RESOLVE_END]                   = "resolve_end",
  [SPN_EVENT_SYNC]                          = "sync",
  [SPN_EVENT_SYNC_START]                    = "sync_start",
  [SPN_EVENT_SYNC_PACKAGE]                  = "sync_package",
  [SPN_EVENT_SYNC_FAILED]                   = "sync_failed",
  [SPN_EVENT_SYNC_STALE]                    = "sync_stale",
  [SPN_EVENT_SYNC_END]                      = "sync_end",
  [SPN_EVENT_BUILD_SCRIPT_COMPILE]          = "script_compile",
  [SPN_EVENT_BUILD_SCRIPT_COMPILE_FAILED]   = "script_compile_failed",
  [SPN_EVENT_BUILD_SCRIPT_CONFIGURE]        = "configure",
  [SPN_EVENT_BUILD_SCRIPT_CONFIGURE_OK]     = "configure_ok",
  [SPN_EVENT_BUILD_SCRIPT_PACKAGE]          = "script_package",
  [SPN_EVENT_BUILD_SCRIPT_PACKAGE_OK]       = "package_ok",
  [SPN_EVENT_BUILD_SCRIPT_CRASHED]          = "script_crashed",
  [SPN_EVENT_BUILD_SCRIPT_USER_FN]          = "script_user_fn",
  [SPN_EVENT_COMPILE_START]                 = "compile_start",
  [SPN_EVENT_TARGET_BUILD_PASSED]           = "target_build_passed",
  [SPN_EVENT_TARGET_BUILD_FAILED]           = "target_build_failed",
  [SPN_EVENT_TARGET_RUN]                    = "target_run",
  [SPN_EVENT_LINK_START]                    = "link_start",
  [SPN_EVENT_LINK_PASSED]                   = "link_passed",
  [SPN_EVENT_LINK_FAILED]                   = "link_failed",
  [SPN_EVENT_EMBED_START]                   = "embed_start",
  [SPN_EVENT_EMBED_PASSED]                  = "embed_passed",
  [SPN_EVENT_EMBED_FAILED]                  = "embed_failed",
  [SPN_EVENT_INIT_BUILD_GRAPH]              = "init_build_graph",
  [SPN_EVENT_PREPARE_BUILD_GRAPH_FAILED]    = "prepare_build_graph_failed",
  [SPN_EVENT_DIRTY_SUMMARY]                 = "dirty_summary",
  [SPN_EVENT_BUILD_PASSED]                  = "build_passed",
  [SPN_EVENT_BUILD_FAILED]                  = "build_failed",
  [SPN_EVENT_BUILD_SUMMARY]                 = "build_summary",
  [SPN_EVENT_API_CALL]                      = "api_call",
  [SPN_EVENT_USER_LOG]                      = "user_log",
  [SPN_EVENT_ADDED]                         = "added",
};

// ============================================================================
// Public API
// ============================================================================

void spn_event_log_init(sp_mem_t mem) {
  build_schemas(mem);
}

void spn_event_log_jsonl(sp_io_writer_t* out, spn_build_event_t* event) {
  sp_bind_t* schema = schemas[event->kind];

  sp_tm_epoch_t now = sp_tm_now_epoch();

  const c8* name = event_names[event->kind];
  sp_io_write_cstr(out, "{\"event\": \"", SP_NULLPTR);
  sp_io_write_cstr(out, name ? name : "unknown", SP_NULLPTR);
  sp_io_write_cstr(out, "\"", SP_NULLPTR);

  s32 level = event_level(event->kind);
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
    void* data = event_variant_ptr(event);
    if (data) {
      sp_io_write_cstr(out, ", \"data\": ", SP_NULLPTR);
      json_write_object(out, schema, data);
    }
  }

  sp_io_write_cstr(out, "}\n", SP_NULLPTR);
}

void spn_event_log_build(sp_io_writer_t* out, spn_build_event_t* event) {
  spn_invocation_t* invocation = SP_NULLPTR;
  sp_str_t args = SP_ZERO_INITIALIZE();
  sp_str_t transcript = SP_ZERO_INITIALIZE();

  switch (event->kind) {
    case SPN_EVENT_TARGET_BUILD_PASSED: invocation = event->target.passed.invocation; transcript = event->target.passed.out;      break;
    case SPN_EVENT_TARGET_BUILD_FAILED: invocation = event->target.failed.invocation; transcript = event->target.failed.out;      break;
    case SPN_EVENT_LINK_PASSED:         args = event->target.link_passed.args;        transcript = event->target.link_passed.out; break;
    case SPN_EVENT_LINK_FAILED:         args = event->target.link_failed.args;        transcript = event->target.link_failed.out; break;
    default: return;
  }

  if (sp_str_empty(transcript)) return;

  if (invocation) {
    sp_io_write_str(out, invocation->program, SP_NULLPTR);
    sp_da_for(invocation->args, it) {
      sp_io_write_c8(out, ' ');
      sp_io_write_str(out, invocation->args[it], SP_NULLPTR);
    }
  }
  else {
    sp_io_write_str(out, args, SP_NULLPTR);
  }
  sp_io_write_c8(out, '\n');
  sp_io_write_str(out, transcript, SP_NULLPTR);
  sp_io_write_c8(out, '\n');
}
