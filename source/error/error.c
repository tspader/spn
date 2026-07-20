#include "error/error.h"

#include "ctx/types.h"
#include "event/event.h"

spn_err_union_t spn_err_emit(spn_err_union_t err) {
  if (err.kind && !err.reported) {
    spn_event_buffer_push(spn.events, (spn_build_event_t) {
      .kind = SPN_EVENT_ERR,
      .err = err,
    });
    err.reported = true;
  }
  return err;
}

const c8* spn_err_to_str(spn_err_t err) {
  switch (err) {
    case SPN_OK:                               return "ok";
    case SPN_ERROR:                            return "error";
    case SPN_ERR_MANIFEST_PARSE:               return "manifest_parse";
    case SPN_ERR_MANIFEST_FIELD:               return "manifest_field";
    case SPN_ERR_MANIFEST_ISSUES:              return "manifest_issues";
    case SPN_ERR_NO_MANIFEST:                  return "no_manifest";
    case SPN_ERR_NOT_GIT_REPO:                 return "not_git_repo";
    case SPN_ERR_GIT:                          return "git";
    case SPN_ERR_VERSION_EXISTS:               return "version_exists";
    case SPN_ERR_BUILD_GRAPH:                  return "build_graph";
    case SPN_ERR_TOOLCHAIN_UNKNOWN:            return "toolchain_unknown";
    case SPN_ERR_TOOLCHAIN_TARGET:             return "toolchain_target";
    case SPN_ERR_TOOLCHAIN_HOST:               return "toolchain_host";
    case SPN_ERR_TOOLCHAIN_FETCH:              return "toolchain_fetch";
    case SPN_ERR_TOOLCHAIN_NO_SHA:             return "toolchain_no_sha";
    case SPN_ERR_TOOLCHAIN_SHA:                return "toolchain_sha";
    case SPN_ERR_TOOLCHAIN_EXTRACT:            return "toolchain_extract";
    case SPN_ERR_TOOLCHAIN_NO_CXX:             return "toolchain_no_cxx";
    case SPN_ERR_TOML_MISSING:                 return "toml_missing";
    case SPN_ERR_TOML_TYPE:                    return "toml_type";
    case SPN_ERR_TOML_PARSE:                   return "toml_parse";
    case SPN_CODEGEN_ERR_EXPECTED_BOOL:        return "codegen_expected_bool";
    case SPN_CODEGEN_ERR_EXPECTED_STR:         return "codegen_expected_str";
    case SPN_CODEGEN_ERR_EXPECTED_INT:         return "codegen_expected_int";
    case SPN_CODEGEN_ERR_EXPECTED_OBJECT:      return "codegen_expected_object";
    case SPN_CODEGEN_ERR_MISSING_KEY:          return "codegen_missing_key";
    case SPN_CODEGEN_ERR_DUPLICATE_KEY:        return "codegen_duplicate_key";
    case SPN_CODEGEN_ERR_UNKNOWN_KEY:          return "codegen_unknown_key";
    case SPN_CODEGEN_ERR_PARSE:                return "codegen_parse";
    case SPN_CODEGEN_ERR_FILE_MISSING:         return "codegen_file_missing";
    case SPN_CODEGEN_ERR_INVALID:              return "codegen_invalid";
    case SPN_CODEGEN_ERR_ROOT_ONLY:            return "codegen_root_only";
    case SPN_ERR_WASM_INIT_FAILED:             return "wasm_init_failed";
    case SPN_ERR_WASM_REGISTER_FAILED:         return "wasm_register_failed";
    case SPN_ERR_WASM_MODULE_LOAD_FAILED:      return "wasm_module_load_failed";
    case SPN_ERR_WASM_MODULE_INSTANCE_FAILED:  return "wasm_module_instance_failed";
    case SPN_ERR_WASM_CTX_FAILED:              return "wasm_ctx_failed";
    case SPN_ERR_WASM_MODULE_CALL_FAILED:      return "wasm_module_call_failed";
    case SPN_ERR_WASM_READ_FAILED:             return "wasm_read_failed";
    case SPN_ERR_WASM_THREAD_ENV_FAILED:       return "wasm_thread_env_failed";
    case SPN_ERR_WASM_SCRIPT_ERROR:            return "wasm_script_error";
    case SPN_ERR_WASM_NO_SCRIPT:               return "wasm_no_script";
    case SPN_ERR_WASM_EXPORT_NOT_FOUND:        return "wasm_export_not_found";
    case SPN_ERR_PROFILE_INVALID:              return "profile_invalid";
    case SPN_ERR_PROFILE_UNDEFINED:            return "profile_undefined";
    case SPN_ERR_FLAG_INVALID:                 return "flag_invalid";
    case SPN_ERR_SANITIZER_UNSUPPORTED:        return "sanitizer_unsupported";
    case SPN_ERR_SANITIZER_STATIC:             return "sanitizer_static";
    case SPN_ERR_COMPILER_FEATURE_UNSUPPORTED: return "compiler_feature_unsupported";
    case SPN_ERR_TOC_MAGIC:                    return "toc_magic";
    case SPN_ERR_TOC_TRUNCATED:                return "toc_truncated";
    case SPN_ERR_TOC_MISSING:                  return "toc_missing";
    case SPN_ERR_FS_REMOVE:                    return "fs_remove";
    case SPN_ERR_FS_READ:                      return "fs_read";
    case SPN_ERR_FS_WRITE:                     return "fs_write";
    case SPN_ERR_INDEX_UNKNOWN:                return "index_unknown";
    case SPN_ERR_INDEX_SYNC:                   return "index_sync";
    case SPN_ERR_INDEX_PINNED:                 return "index_pinned";
    case SPN_ERR_INDEX_PUBLISH_PROTOCOL:       return "index_publish_protocol";
    case SPN_ERR_PUBLISH_PUSH:                 return "publish_push";
    case SPN_ERR_PUBLISH_DIRTY:                return "publish_dirty";
    case SPN_ERR_PUBLISH_UNPUSHED:             return "publish_unpushed";
    case SPN_ERR_PKG_UNKNOWN:                  return "pkg_unknown";
    case SPN_ERR_PKG_NO_MATCH:                 return "pkg_no_match";
    case SPN_ERR_MANIFEST_EDIT:                return "manifest_edit";
    case SPN_ERR_DAG_GLOB:                     return "dag_glob";
    case SPN_ERR_DAG_OUTPUT_NAME:              return "dag_output_name";
    case SPN_ERR_DAG_DUPLICATE_OUTPUT:         return "dag_duplicate_output";
    case SPN_ERR_DAG_STAT:                     return "dag_stat";
    case SPN_ERR_DAG_MISSING_INPUT:            return "dag_missing_input";
    case SPN_ERR_DAG_MISSING_OUTPUT:           return "dag_missing_output";
    case SPN_ERR_DAG_SCRATCH:                  return "dag_scratch";
    case SPN_ERR_DAG_ACTION:                   return "dag_action";
    case SPN_ERR_DAG_STORE_READ:               return "dag_store_read";
    case SPN_ERR_DAG_STORE_WRITE:              return "dag_store_write";
    case SPN_ERR_DAG_STORE_MISSING:            return "dag_store_missing";
    case SPN_ERR_DAG_TREE:                     return "dag_tree";
    case SPN_ERR_DAG_STALLED:                  return "dag_stalled";
    case SPN_ERR_DEP_MANIFEST:                 return "dep_manifest";
    case SPN_ERR_DEP_CYCLE:                    return "dep_cycle";
    case SPN_ERR_UNIT_CYCLE:                   return "unit_cycle";
    case SPN_ERR_DYNAMIC_DUPLICATE:            return "dynamic_duplicate";
    case SPN_ERR_RESOLVE_TOO_COMPLEX:          return "resolve_too_complex";
    case SPN_ERR_OPTION:                       return "option";
  }
  return "unknown";
}
