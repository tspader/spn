#include "event/log.h"

#include "log/types.h"
#include "sp/bind.h"

static s32 event_level(spn_build_event_kind_t kind) {
  switch (kind) {
    case SPN_EVENT_ERR:
    case SPN_EVENT_ERR_CIRCULAR_DEP:
    case SPN_EVENT_ERR_UNKNOWN_PKG:
    case SPN_EVENT_ERR_UNSATISFIABLE_VERSION:
    case SPN_EVENT_BUILD_SCRIPT_COMPILE_FAILED:
    case SPN_EVENT_BUILD_SCRIPT_FAILED:
    case SPN_EVENT_BUILD_SCRIPT_CRASHED:
    case SPN_EVENT_DEP_BUILD_FAILED:
    case SPN_EVENT_TARGET_BUILD_FAILED:
    case SPN_EVENT_TCC_ERROR:
    case SPN_EVENT_TEST_FAILED:
    case SPN_EVENT_SYNC_FAILED:
    case SPN_EVENT_LINK_FAILED:
    case SPN_EVENT_EMBED_FAILED:
    case SPN_EVENT_PREPARE_BUILD_GRAPH_FAILED:
    case SPN_EVENT_BUILD_FAILED: {
      return SPN_LOG_LEVEL_ERROR;
    }
    case SPN_EVENT_ADD_TARGET:
    case SPN_EVENT_ADD_SOURCE:
    case SPN_EVENT_BUILD_SCRIPT_CONFIGURE_OK:
    case SPN_EVENT_BUILD_SCRIPT_USER_FN:
    case SPN_EVENT_DEBUG:
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

static void build_schemas(void) {
  // SPN_EVENT_SYNC
  {
    sp_bind_builder_t b = sp_bind_builder_begin();
    SP_BIND_SCHEMA(&b) {
      SP_BIND(&b, spn_evt_sync_t, url, "url", SP_BIND_STR);
    }
    schemas[SPN_EVENT_SYNC] = sp_bind_builder_end(&b);
  }

  // SPN_EVENT_CHECKOUT
  {
    sp_bind_builder_t b = sp_bind_builder_begin();
    SP_BIND_SCHEMA(&b) {
      SP_BIND(&b, spn_evt_checkout_t, commit, "commit", SP_BIND_STR);
      SP_BIND(&b, spn_evt_checkout_t, message, "message", SP_BIND_STR);
      SP_BIND_OBJECT(&b, spn_evt_checkout_t, version, "version") {
        SP_BIND(&b, spn_semver_t, major, "major", SP_BIND_U32);
        SP_BIND(&b, spn_semver_t, minor, "minor", SP_BIND_U32);
        SP_BIND(&b, spn_semver_t, patch, "patch", SP_BIND_U32);
      }
    }
    schemas[SPN_EVENT_CHECKOUT] = sp_bind_builder_end(&b);
  }

  // SPN_EVENT_RESOLVE
  {
    sp_bind_builder_t b = sp_bind_builder_begin();
    SP_BIND_SCHEMA(&b) {
      SP_BIND(&b, spn_evt_resolve_t, strategy, "strategy", SP_BIND_S32);
    }
    schemas[SPN_EVENT_RESOLVE] = sp_bind_builder_end(&b);
  }

  // SPN_EVENT_CLEAN
  {
    sp_bind_builder_t b = sp_bind_builder_begin();
    SP_BIND_SCHEMA(&b) {
      SP_BIND(&b, spn_evt_clean_t, path, "path", SP_BIND_STR);
    }
    schemas[SPN_EVENT_CLEAN] = sp_bind_builder_end(&b);
  }

  // SPN_EVENT_GENERATE
  {
    sp_bind_builder_t b = sp_bind_builder_begin();
    SP_BIND_SCHEMA(&b) {
      SP_BIND(&b, spn_evt_generate_t, path, "path", SP_BIND_STR);
    }
    schemas[SPN_EVENT_GENERATE] = sp_bind_builder_end(&b);
  }

  // SPN_EVENT_BUILD_SCRIPT_PACKAGE
  {
    sp_bind_builder_t b = sp_bind_builder_begin();
    SP_BIND_SCHEMA(&b) {
      SP_BIND(&b, spn_evt_script_package_t, time, "time_ns", SP_BIND_U64);
    }
    schemas[SPN_EVENT_BUILD_SCRIPT_PACKAGE] = sp_bind_builder_end(&b);
  }

  // SPN_EVENT_BUILD_SCRIPT_CRASHED
  {
    sp_bind_builder_t b = sp_bind_builder_begin();
    SP_BIND_SCHEMA(&b) {
      SP_BIND(&b, spn_evt_script_crashed_t, path, "script_path", SP_BIND_STR);
    }
    schemas[SPN_EVENT_BUILD_SCRIPT_CRASHED] = sp_bind_builder_end(&b);
  }

  // SPN_EVENT_BUILD_SCRIPT_COMPILE
  {
    sp_bind_builder_t b = sp_bind_builder_begin();
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
    sp_bind_builder_t b = sp_bind_builder_begin();
    SP_BIND_SCHEMA(&b) {
      SP_BIND(&b, spn_evt_compile_failed_t, script_path, "script_path", SP_BIND_STR);
      SP_BIND(&b, spn_evt_compile_failed_t, error, "error", SP_BIND_STR);
    }
    schemas[SPN_EVENT_BUILD_SCRIPT_COMPILE_FAILED] = sp_bind_builder_end(&b);
  }

  // SPN_EVENT_DEBUG
  {
    sp_bind_builder_t b = sp_bind_builder_begin();
    SP_BIND_SCHEMA(&b) {
      SP_BIND(&b, spn_evt_debug_t, message, "message", SP_BIND_STR);
    }
    schemas[SPN_EVENT_DEBUG] = sp_bind_builder_end(&b);
  }

  // SPN_EVENT_TARGET_BUILD (the compile event that carries args)
  {
    sp_bind_builder_t b = sp_bind_builder_begin();
    SP_BIND_SCHEMA(&b) {
      SP_BIND(&b, spn_evt_target_build_t, source_file, "source_file", SP_BIND_STR);
      SP_BIND(&b, spn_evt_target_build_t, object_file, "object_file", SP_BIND_STR);
      SP_BIND(&b, spn_evt_target_build_t, compiler, "compiler", SP_BIND_STR);
      SP_BIND(&b, spn_evt_target_build_t, args, "args", SP_BIND_STR);
    }
    sp_bind_t* s = sp_bind_builder_end(&b);
    schemas[SPN_EVENT_TARGET_BUILD] = s;
  }

  // SPN_EVENT_TARGET_BUILD_PASSED
  {
    sp_bind_builder_t b = sp_bind_builder_begin();
    SP_BIND_SCHEMA(&b) {
      SP_BIND(&b, spn_evt_target_passed_t, source_file, "source_file", SP_BIND_STR);
      SP_BIND(&b, spn_evt_target_passed_t, object_file, "object_file", SP_BIND_STR);
      SP_BIND(&b, spn_evt_target_passed_t, time, "time_ns", SP_BIND_U64);
    }
    schemas[SPN_EVENT_TARGET_BUILD_PASSED] = sp_bind_builder_end(&b);
  }

  // SPN_EVENT_TARGET_BUILD_FAILED
  {
    sp_bind_builder_t b = sp_bind_builder_begin();
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

  // SPN_EVENT_DEP_BUILD_FAILED
  {
    sp_bind_builder_t b = sp_bind_builder_begin();
    SP_BIND_SCHEMA(&b) {
      SP_BIND(&b, spn_evt_dep_failed_t, out, "out", SP_BIND_STR);
      SP_BIND(&b, spn_evt_dep_failed_t, err, "err", SP_BIND_STR);
    }
    schemas[SPN_EVENT_DEP_BUILD_FAILED] = sp_bind_builder_end(&b);
  }

  // SPN_EVENT_DEP_BUILD_PASSED
  {
    sp_bind_builder_t b = sp_bind_builder_begin();
    SP_BIND_SCHEMA(&b) {
      SP_BIND(&b, spn_evt_dep_passed_t, time, "time_ns", SP_BIND_U64);
    }
    schemas[SPN_EVENT_DEP_BUILD_PASSED] = sp_bind_builder_end(&b);
  }

  // SPN_EVENT_ADD_TARGET (debug: target name + kind)
  {
    sp_bind_builder_t b = sp_bind_builder_begin();
    SP_BIND_SCHEMA(&b) {
      SP_BIND(&b, spn_evt_target_add_debug_t, kind, "kind", SP_BIND_S32);
    }
    schemas[SPN_EVENT_ADD_TARGET] = sp_bind_builder_end(&b);
  }

  // SPN_EVENT_ADD_SOURCE
  {
    sp_bind_builder_t b = sp_bind_builder_begin();
    SP_BIND_SCHEMA(&b) {
      SP_BIND(&b, spn_evt_target_source_t, source, "source", SP_BIND_STR);
    }
    schemas[SPN_EVENT_ADD_SOURCE] = sp_bind_builder_end(&b);
  }

  // SPN_EVENT_INIT_BUILD_GRAPH
  {
    sp_bind_builder_t b = sp_bind_builder_begin();
    SP_BIND_SCHEMA(&b) {
      SP_BIND(&b, spn_evt_graph_init_t, profile, "profile", SP_BIND_STR);
      SP_BIND(&b, spn_evt_graph_init_t, force, "force", SP_BIND_BOOL);
      SP_BIND(&b, spn_evt_graph_init_t, packages, "packages", SP_BIND_U32);
    }
    schemas[SPN_EVENT_INIT_BUILD_GRAPH] = sp_bind_builder_end(&b);
  }

  // SPN_EVENT_PREPARE_BUILD_GRAPH_FAILED
  {
    sp_bind_builder_t b = sp_bind_builder_begin();
    SP_BIND_SCHEMA(&b) {
      SP_BIND(&b, spn_err_build_graph_t, kind, "kind", SP_BIND_S32);
      SP_BIND(&b, spn_err_build_graph_t, file, "file", SP_BIND_STR);
      SP_BIND(&b, spn_err_build_graph_t, command_a, "command_a", SP_BIND_STR);
      SP_BIND(&b, spn_err_build_graph_t, command_b, "command_b", SP_BIND_STR);
    }
    schemas[SPN_EVENT_PREPARE_BUILD_GRAPH_FAILED] = sp_bind_builder_end(&b);
  }

  // SPN_EVENT_LINK_START
  {
    sp_bind_builder_t b = sp_bind_builder_begin();
    SP_BIND_SCHEMA(&b) {
      SP_BIND(&b, spn_evt_link_start_t, kind, "kind", SP_BIND_S32);
      SP_BIND(&b, spn_evt_link_start_t, num_objects, "num_objects", SP_BIND_U32);
      SP_BIND(&b, spn_evt_link_start_t, output_path, "output_path", SP_BIND_STR);
      SP_BIND(&b, spn_evt_link_start_t, linker, "linker", SP_BIND_STR);
      SP_BIND(&b, spn_evt_link_start_t, args, "args", SP_BIND_STR);
      SP_BIND(&b, spn_evt_link_start_t, has_embeds, "has_embeds", SP_BIND_BOOL);
    }
    schemas[SPN_EVENT_LINK_START] = sp_bind_builder_end(&b);
  }

  // SPN_EVENT_LINK_PASSED
  {
    sp_bind_builder_t b = sp_bind_builder_begin();
    SP_BIND_SCHEMA(&b) {
      SP_BIND(&b, spn_evt_link_passed_t, output_path, "output_path", SP_BIND_STR);
      SP_BIND(&b, spn_evt_link_passed_t, time, "time_ns", SP_BIND_U64);
    }
    schemas[SPN_EVENT_LINK_PASSED] = sp_bind_builder_end(&b);
  }

  // SPN_EVENT_LINK_FAILED
  {
    sp_bind_builder_t b = sp_bind_builder_begin();
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
    sp_bind_builder_t b = sp_bind_builder_begin();
    SP_BIND_SCHEMA(&b) {
      SP_BIND(&b, spn_evt_configure_t, time, "time_ns", SP_BIND_U64);
    }
    schemas[SPN_EVENT_BUILD_SCRIPT_CONFIGURE_OK] = sp_bind_builder_end(&b);
  }

  // SPN_EVENT_CLI_ENTRY
  {
    sp_bind_builder_t b = sp_bind_builder_begin();
    SP_BIND_SCHEMA(&b) {
      SP_BIND(&b, spn_evt_cli_entry_t, command, "command", SP_BIND_STR);
      SP_BIND(&b, spn_evt_cli_entry_t, profile, "profile", SP_BIND_STR);
      SP_BIND(&b, spn_evt_cli_entry_t, target, "target", SP_BIND_STR);
      SP_BIND(&b, spn_evt_cli_entry_t, force, "force", SP_BIND_BOOL);
      SP_BIND(&b, spn_evt_cli_entry_t, cwd, "cwd", SP_BIND_STR);
      SP_BIND(&b, spn_evt_cli_entry_t, manifest, "manifest", SP_BIND_STR);
    }
    schemas[SPN_EVENT_CLI_ENTRY] = sp_bind_builder_end(&b);
  }

  // SPN_EVENT_RESOLVE_START
  {
    sp_bind_builder_t b = sp_bind_builder_begin();
    SP_BIND_SCHEMA(&b) {
      SP_BIND(&b, spn_evt_resolve_start_t, strategy, "strategy", SP_BIND_S32);
      SP_BIND(&b, spn_evt_resolve_start_t, num_deps, "num_deps", SP_BIND_U32);
    }
    schemas[SPN_EVENT_RESOLVE_START] = sp_bind_builder_end(&b);
  }

  // SPN_EVENT_RESOLVE_PACKAGE
  {
    sp_bind_builder_t b = sp_bind_builder_begin();
    SP_BIND_SCHEMA(&b) {
      SP_BIND(&b, spn_evt_resolve_pkg_t, name, "name", SP_BIND_STR);
      SP_BIND(&b, spn_evt_resolve_pkg_t, version, "version", SP_BIND_STR);
      SP_BIND(&b, spn_evt_resolve_pkg_t, kind, "kind", SP_BIND_S32);
    }
    schemas[SPN_EVENT_RESOLVE_PACKAGE] = sp_bind_builder_end(&b);
  }

  // SPN_EVENT_RESOLVE_END
  {
    sp_bind_builder_t b = sp_bind_builder_begin();
    SP_BIND_SCHEMA(&b) {
      SP_BIND(&b, spn_evt_resolve_end_t, num_resolved, "num_resolved", SP_BIND_U32);
      SP_BIND(&b, spn_evt_resolve_end_t, time, "time_ns", SP_BIND_U64);
    }
    schemas[SPN_EVENT_RESOLVE_END] = sp_bind_builder_end(&b);
  }

  // SPN_EVENT_API_CALL
  {
    sp_bind_builder_t b = sp_bind_builder_begin();
    SP_BIND_SCHEMA(&b) {
      SP_BIND(&b, spn_evt_api_call_t, fn, "fn", SP_BIND_STR);
      SP_BIND(&b, spn_evt_api_call_t, args, "args", SP_BIND_STR);
    }
    schemas[SPN_EVENT_API_CALL] = sp_bind_builder_end(&b);
  }

  // SPN_EVENT_USER_LOG
  {
    sp_bind_builder_t b = sp_bind_builder_begin();
    SP_BIND_SCHEMA(&b) {
      SP_BIND(&b, spn_evt_user_log_t, message, "message", SP_BIND_STR);
    }
    schemas[SPN_EVENT_USER_LOG] = sp_bind_builder_end(&b);
  }

  // SPN_EVENT_SYNC_START
  {
    sp_bind_builder_t b = sp_bind_builder_begin();
    SP_BIND_SCHEMA(&b) {
      SP_BIND(&b, spn_evt_sync_start_t, num_packages, "num_packages", SP_BIND_U32);
      SP_BIND(&b, spn_evt_sync_start_t, num_index, "num_index", SP_BIND_U32);
      SP_BIND(&b, spn_evt_sync_start_t, num_file, "num_file", SP_BIND_U32);
    }
    schemas[SPN_EVENT_SYNC_START] = sp_bind_builder_end(&b);
  }

  // SPN_EVENT_SYNC_PACKAGE
  {
    sp_bind_builder_t b = sp_bind_builder_begin();
    SP_BIND_SCHEMA(&b) {
      SP_BIND(&b, spn_evt_sync_pkg_t, name, "name", SP_BIND_STR);
      SP_BIND(&b, spn_evt_sync_pkg_t, kind, "kind", SP_BIND_S32);
      SP_BIND(&b, spn_evt_sync_pkg_t, url, "url", SP_BIND_STR);
      SP_BIND(&b, spn_evt_sync_pkg_t, source_path, "source_path", SP_BIND_STR);
      SP_BIND(&b, spn_evt_sync_pkg_t, time, "time_ns", SP_BIND_U64);
    }
    schemas[SPN_EVENT_SYNC_PACKAGE] = sp_bind_builder_end(&b);
  }

  // SPN_EVENT_SYNC_FAILED
  {
    sp_bind_builder_t b = sp_bind_builder_begin();
    SP_BIND_SCHEMA(&b) {
      SP_BIND(&b, spn_evt_sync_failed_t, name, "name", SP_BIND_STR);
      SP_BIND(&b, spn_evt_sync_failed_t, url, "url", SP_BIND_STR);
      SP_BIND(&b, spn_evt_sync_failed_t, error, "error", SP_BIND_STR);
    }
    schemas[SPN_EVENT_SYNC_FAILED] = sp_bind_builder_end(&b);
  }

  // SPN_EVENT_SYNC_END
  {
    sp_bind_builder_t b = sp_bind_builder_begin();
    SP_BIND_SCHEMA(&b) {
      SP_BIND(&b, spn_evt_sync_end_t, num_synced, "num_synced", SP_BIND_U32);
      SP_BIND(&b, spn_evt_sync_end_t, time, "time_ns", SP_BIND_U64);
    }
    schemas[SPN_EVENT_SYNC_END] = sp_bind_builder_end(&b);
  }

  // SPN_EVENT_CIRCULAR_DEP
  {
    sp_bind_builder_t b = sp_bind_builder_begin();
    SP_BIND_SCHEMA(&b) {
      SP_BIND_OBJECT(&b, spn_evt_circular_t, id, "id") {
        SP_BIND(&b, spn_pkg_id_t, namespace, "namespace", SP_BIND_STR);
        SP_BIND(&b, spn_pkg_id_t, name, "name", SP_BIND_STR);
      }
    }
    schemas[SPN_EVENT_ERR_CIRCULAR_DEP] = sp_bind_builder_end(&b);
  }

  // SPN_EVENT_ERR_UNKNOWN_PKG
  {
    sp_bind_builder_t b = sp_bind_builder_begin();
    SP_BIND_SCHEMA(&b) {
      SP_BIND_OBJECT(&b, spn_evt_unknown_t, request, "request") {
        SP_BIND(&b, spn_requested_pkg_t, qualified, "qualified", SP_BIND_STR);
      }
    }
    schemas[SPN_EVENT_ERR_UNKNOWN_PKG] = sp_bind_builder_end(&b);
  }

  // SPN_EVENT_EMBED_START
  {
    sp_bind_builder_t b = sp_bind_builder_begin();
    SP_BIND_SCHEMA(&b) {
      SP_BIND(&b, spn_evt_embed_start_t, num_files, "num_files", SP_BIND_U32);
    }
    schemas[SPN_EVENT_EMBED_START] = sp_bind_builder_end(&b);
  }

  // SPN_EVENT_EMBED_PASSED
  {
    sp_bind_builder_t b = sp_bind_builder_begin();
    SP_BIND_SCHEMA(&b) {
      SP_BIND(&b, spn_evt_embed_passed_t, object_path, "object_path", SP_BIND_STR);
      SP_BIND(&b, spn_evt_embed_passed_t, header_path, "header_path", SP_BIND_STR);
      SP_BIND(&b, spn_evt_embed_passed_t, time, "time_ns", SP_BIND_U64);
    }
    schemas[SPN_EVENT_EMBED_PASSED] = sp_bind_builder_end(&b);
  }

  // SPN_EVENT_EMBED_FAILED
  {
    sp_bind_builder_t b = sp_bind_builder_begin();
    SP_BIND_SCHEMA(&b) {
      SP_BIND(&b, spn_evt_embed_failed_t, path, "path", SP_BIND_STR);
      SP_BIND(&b, spn_evt_embed_failed_t, error, "error", SP_BIND_STR);
    }
    schemas[SPN_EVENT_EMBED_FAILED] = sp_bind_builder_end(&b);
  }

  // SPN_EVENT_DIRTY_SUMMARY
  {
    sp_bind_builder_t b = sp_bind_builder_begin();
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
    sp_bind_builder_t b = sp_bind_builder_begin();
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
    sp_bind_builder_t b = sp_bind_builder_begin();
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
    sp_bind_builder_t b = sp_bind_builder_begin();
    SP_BIND_SCHEMA(&b) {
      SP_BIND(&b, spn_evt_package_ok_t, time, "time_ns", SP_BIND_U64);
    }
    schemas[SPN_EVENT_BUILD_SCRIPT_PACKAGE_OK] = sp_bind_builder_end(&b);
  }
}

// ============================================================================
// JSON serializer — walks a bind schema + data pointer
// ============================================================================

static void json_write_str(sp_str_builder_t* b, sp_str_t str) {
  sp_str_builder_append_c8(b, '"');
  for (u32 i = 0; i < str.len; i++) {
    c8 c = str.data[i];
    switch (c) {
      case '"':  sp_str_builder_append_cstr(b, "\\\""); break;
      case '\\': sp_str_builder_append_cstr(b, "\\\\"); break;
      case '\n': sp_str_builder_append_cstr(b, "\\n");  break;
      case '\r': sp_str_builder_append_cstr(b, "\\r");  break;
      case '\t': sp_str_builder_append_cstr(b, "\\t");  break;
      default:   sp_str_builder_append_c8(b, c);        break;
    }
  }
  sp_str_builder_append_c8(b, '"');
}

static void json_write_value(sp_str_builder_t* b, sp_bind_field_t* field, void* base);

static void json_write_object(sp_str_builder_t* b, sp_bind_t* schema, void* data) {
  sp_str_builder_append_c8(b, '{');
  sp_da_for(schema->as.object.fields, i) {
    sp_bind_field_t* f = &schema->as.object.fields[i];
    if (i > 0) sp_str_builder_append_cstr(b, ", ");
    json_write_str(b, sp_str_from_cstr(spn_allocator, f->key));
    sp_str_builder_append_cstr(b, ": ");
    json_write_value(b, f, data);
  }
  sp_str_builder_append_c8(b, '}');
}

static void json_write_value(sp_str_builder_t* b, sp_bind_field_t* field, void* base) {
  void* ptr = sp_bind_field_ptr(field, base);

  switch (field->kind) {
    case SP_BIND_STR: {
      json_write_str(b, *(sp_str_t*)ptr);
      break;
    }
    case SP_BIND_BOOL: {
      sp_str_builder_append_cstr(b, *(bool*)ptr ? "true" : "false");
      break;
    }
    case SP_BIND_S32: {
      sp_str_builder_append(b, sp_format("{}", SP_FMT_S32(*(s32*)ptr)));
      break;
    }
    case SP_BIND_U32: {
      sp_str_builder_append(b, sp_format("{}", SP_FMT_U32(*(u32*)ptr)));
      break;
    }
    case SP_BIND_U64: {
      sp_str_builder_append(b, sp_format("{}", SP_FMT_U64(*(u64*)ptr)));
      break;
    }
    case SP_BIND_S64: {
      sp_str_builder_append(b, sp_format("{}", SP_FMT_S64(*(s64*)ptr)));
      break;
    }
    case SP_BIND_F64: {
      sp_str_builder_append(b, sp_format("{}", SP_FMT_F64(*(f64*)ptr)));
      break;
    }
    case SP_BIND_OBJECT: {
      json_write_object(b, field, ptr);
      break;
    }
    case SP_BIND_NONE:
    case SP_BIND_ARRAY:
    case SP_BIND_MAP: {
      sp_str_builder_append_cstr(b, "null");
      break;
    }
  }
}

// ============================================================================
// Variant pointer — switch on tag, return pointer into the union
// ============================================================================

// @spader
// Why is every single one of these not the same
static void* event_variant_ptr(spn_build_event_t* event) {
  switch (event->kind) {
    case SPN_EVENT_SYNC:                        return &event->sync;
    case SPN_EVENT_CHECKOUT:                    return &event->checkout;
    case SPN_EVENT_RESOLVE:                     return &event->resolve;
    case SPN_EVENT_CLEAN:                       return &event->clean;
    case SPN_EVENT_GENERATE:                    return &event->generate;
    case SPN_EVENT_BUILD_SCRIPT_COMPILE:        return &event->script_compile;
    case SPN_EVENT_BUILD_SCRIPT_CRASHED:        return &event->crashed;
    case SPN_EVENT_BUILD_SCRIPT_PACKAGE:        return &event->script_package;
    case SPN_EVENT_BUILD_SCRIPT_CONFIGURE_OK:   return &event->configure;
    case SPN_EVENT_BUILD_SCRIPT_COMPILE_FAILED: return &event->compile_failed;
    case SPN_EVENT_DEBUG:                       return &event->debug;
    case SPN_EVENT_TARGET_BUILD:                return &event->target.build;
    case SPN_EVENT_TARGET_BUILD_PASSED:         return &event->target.passed;
    case SPN_EVENT_TARGET_BUILD_FAILED:         return &event->target.failed;
    case SPN_EVENT_DEP_BUILD_PASSED:            return &event->dep.passed;
    case SPN_EVENT_DEP_BUILD_FAILED:            return &event->dep.failed;
    case SPN_EVENT_ADD_TARGET:                  return &event->target.add_debug;
    case SPN_EVENT_ADD_SOURCE:                  return &event->target.source;
    case SPN_EVENT_INIT_BUILD_GRAPH:            return &event->graph_init;
    case SPN_EVENT_PREPARE_BUILD_GRAPH_FAILED:  return &event->err.build_graph;
    case SPN_EVENT_LINK_START:                  return &event->target.link_start;
    case SPN_EVENT_LINK_PASSED:                 return &event->target.link_passed;
    case SPN_EVENT_LINK_FAILED:                 return &event->target.link_failed;
    case SPN_EVENT_CLI_ENTRY:                   return &event->cli_entry;
    case SPN_EVENT_RESOLVE_START:               return &event->resolve_start;
    case SPN_EVENT_RESOLVE_PACKAGE:             return &event->resolve_pkg;
    case SPN_EVENT_RESOLVE_END:                 return &event->resolve_end;
    case SPN_EVENT_API_CALL:                    return &event->api_call;
    case SPN_EVENT_USER_LOG:                    return &event->user_log;
    case SPN_EVENT_SYNC_START:                  return &event->sync_start;
    case SPN_EVENT_SYNC_PACKAGE:                return &event->sync_pkg;
    case SPN_EVENT_SYNC_FAILED:                 return &event->sync_failed;
    case SPN_EVENT_SYNC_END:                    return &event->sync_end;
    case SPN_EVENT_ERR_CIRCULAR_DEP:            return &event->circular;
    case SPN_EVENT_ERR_UNKNOWN_PKG:             return &event->unknown;
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
  [SPN_EVENT_FETCH]                         = "fetch",
  [SPN_EVENT_ERR]                           = "err",
  [SPN_EVENT_ERR_CIRCULAR_DEP]              = "err_circular_dep",
  [SPN_EVENT_ERR_UNKNOWN_PKG]               = "err_unknown_pkg",
  [SPN_EVENT_ERR_UNSATISFIABLE_VERSION]     = "err_unsatisfiable_version",
  [SPN_EVENT_RESOLVE]                       = "resolve",
  [SPN_EVENT_SYNC]                          = "sync",
  [SPN_EVENT_CHECKOUT]                      = "checkout",
  [SPN_EVENT_BUILD_SCRIPT_COMPILE]          = "script_compile",
  [SPN_EVENT_BUILD_SCRIPT_COMPILE_FAILED]   = "script_compile_failed",
  [SPN_EVENT_BUILD_SCRIPT_CONFIGURE]        = "configure",
  [SPN_EVENT_BUILD_SCRIPT_CONFIGURE_OK]     = "configure_ok",
  [SPN_EVENT_BUILD_SCRIPT_BUILD]            = "script_build",
  [SPN_EVENT_BUILD_SCRIPT_PACKAGE]          = "script_package",
  [SPN_EVENT_BUILD_SCRIPT_FAILED]           = "script_failed",
  [SPN_EVENT_BUILD_SCRIPT_CRASHED]          = "script_crashed",
  [SPN_EVENT_BUILD_SCRIPT_USER_FN]          = "script_user_fn",
  [SPN_EVENT_DEP_BUILD]                     = "dep_build",
  [SPN_EVENT_DEP_BUILD_PASSED]              = "dep_build_passed",
  [SPN_EVENT_DEP_BUILD_FAILED]              = "dep_build_failed",
  [SPN_EVENT_TARGET_BUILD]                  = "target_build",
  [SPN_EVENT_TARGET_BUILD_PASSED]           = "target_build_passed",
  [SPN_EVENT_TARGET_BUILD_FAILED]           = "target_build_failed",
  [SPN_EVENT_BUILD_PASSED]                  = "build_passed",
  [SPN_EVENT_TCC_ERROR]                     = "tcc_error",
  [SPN_EVENT_TEST_RUN]                      = "test_run",
  [SPN_EVENT_TEST_PASSED]                   = "test_passed",
  [SPN_EVENT_TESTS_PASSED]                  = "tests_passed",
  [SPN_EVENT_TEST_FAILED]                   = "test_failed",
  [SPN_EVENT_CLEAN]                         = "clean",
  [SPN_EVENT_GENERATE]                      = "generate",
  [SPN_EVENT_ADD_TARGET]                    = "add_target",
  [SPN_EVENT_DEBUG]                         = "debug",
  [SPN_EVENT_ADD_SOURCE]                    = "add_source",
  [SPN_EVENT_INIT_BUILD_GRAPH]              = "init_build_graph",
  [SPN_EVENT_PREPARE_BUILD_GRAPH_FAILED]    = "prepare_build_graph_failed",
  [SPN_EVENT_LINK_START]                    = "link_start",
  [SPN_EVENT_LINK_PASSED]                   = "link_passed",
  [SPN_EVENT_LINK_FAILED]                   = "link_failed",
  [SPN_EVENT_CLI_ENTRY]                     = "cli_entry",
  [SPN_EVENT_RESOLVE_START]                 = "resolve_start",
  [SPN_EVENT_RESOLVE_PACKAGE]               = "resolve_package",
  [SPN_EVENT_RESOLVE_END]                   = "resolve_end",
  [SPN_EVENT_SYNC_START]                    = "sync_start",
  [SPN_EVENT_SYNC_PACKAGE]                  = "sync_package",
  [SPN_EVENT_SYNC_FAILED]                   = "sync_failed",
  [SPN_EVENT_SYNC_END]                      = "sync_end",
  [SPN_EVENT_API_CALL]                      = "api_call",
  [SPN_EVENT_USER_LOG]                      = "user_log",
  [SPN_EVENT_EMBED_START]                   = "embed_start",
  [SPN_EVENT_EMBED_PASSED]                  = "embed_passed",
  [SPN_EVENT_EMBED_FAILED]                  = "embed_failed",
  [SPN_EVENT_DIRTY_SUMMARY]                 = "dirty_summary",
  [SPN_EVENT_BUILD_FAILED]                  = "build_failed",
  [SPN_EVENT_BUILD_SUMMARY]                 = "build_summary",
  [SPN_EVENT_BUILD_SCRIPT_PACKAGE_OK]       = "package_ok",
};

// ============================================================================
// Public API
// ============================================================================

void spn_event_log_init(void) {
  build_schemas();
}

void spn_event_log_jsonl(sp_io_writer_t* out, spn_build_event_t* event) {
  sp_bind_t* schema = schemas[event->kind];

  sp_str_builder_t b = SP_ZERO_INITIALIZE();

  sp_tm_epoch_t now = sp_tm_now_epoch();

  const c8* name = event_names[event->kind];
  sp_str_builder_append_cstr(&b, "{\"event\": \"");
  sp_str_builder_append_cstr(&b, name ? name : "unknown");
  sp_str_builder_append_cstr(&b, "\"");

  // level
  s32 level = event_level(event->kind);
  sp_str_builder_append(    &b, sp_format(", \"level\": {}", SP_FMT_S32(level)));
  sp_str_builder_append_cstr(&b, ", \"level_name\": \"");
  sp_str_builder_append_cstr(&b, level_name(level));
  sp_str_builder_append_cstr(&b, "\"");

  // timestamp: separate s and ns for full precision
  sp_str_builder_append(    &b, sp_format(", \"ts_s\": {}, \"ts_ns\": {}",
    SP_FMT_U64(now.s), SP_FMT_U32(now.ns)));

  // thread id
  sp_str_builder_append(    &b, sp_format(", \"tid\": {}", SP_FMT_U64(event->thread_id)));

  // package name
  if (event->pkg) {
    sp_str_builder_append_cstr(&b, ", \"pkg\": ");
    json_write_str(&b, event->pkg->name);
  }

  // target name
  switch (event->kind) {
    case SPN_EVENT_TARGET_BUILD:
    case SPN_EVENT_TARGET_BUILD_PASSED:
    case SPN_EVENT_TARGET_BUILD_FAILED:
    case SPN_EVENT_ADD_TARGET:
    case SPN_EVENT_ADD_SOURCE:
    case SPN_EVENT_LINK_START:
    case SPN_EVENT_LINK_PASSED:
    case SPN_EVENT_LINK_FAILED: {
      if (sp_str_valid(event->target.name)) {
        sp_str_builder_append_cstr(&b, ", \"target\": ");
        json_write_str(&b, event->target.name);
      }
      break;
    }
    default: break;
  }

  // structured data (if this event kind has a schema)
  if (schema) {
    void* data = event_variant_ptr(event);
    if (data) {
      sp_str_builder_append_cstr(&b, ", \"data\": ");
      json_write_object(&b, schema, data);
    }
  }

  sp_str_builder_append_cstr(&b, "}\n");
  sp_io_write_str(out, sp_str_builder_to_str(&b), SP_NULLPTR);
}

void spn_event_log_build(sp_io_writer_t* out, spn_build_event_t* event) {
  sp_str_t args = SP_ZERO_INITIALIZE();
  sp_str_t transcript = SP_ZERO_INITIALIZE();

  switch (event->kind) {
    case SPN_EVENT_TARGET_BUILD_PASSED: args = event->target.passed.args;      transcript = event->target.passed.out;      break;
    case SPN_EVENT_TARGET_BUILD_FAILED: args = event->target.failed.args;      transcript = event->target.failed.out;      break;
    case SPN_EVENT_LINK_PASSED:         args = event->target.link_passed.args; transcript = event->target.link_passed.out; break;
    case SPN_EVENT_LINK_FAILED:         args = event->target.link_failed.args; transcript = event->target.link_failed.out; break;
    default: return;
  }

  if (sp_str_empty(transcript)) return;

  sp_str_builder_t b = SP_ZERO_INITIALIZE();
  sp_str_builder_append(&b, args);
  sp_str_builder_append_c8(&b, '\n');
  sp_str_builder_append(&b, transcript);
  sp_str_builder_append_c8(&b, '\n');
  sp_io_write_str(out, sp_str_builder_to_str(&b), SP_NULLPTR);
}
