#include "ctx/types.h"
#include "session/types.h"

#include "enum/enum.h"
#include "event/event.h"
#include "log/log.h"
#include "toolchain/toolchain.h"
#include "unit/build.h"
#include "sp/io.h"
#include "sp/tm.h"

sp_str_t spn_cache_dir_kind_to_path(spn_pkg_dir_t kind) {
  switch (kind) {
    case SPN_DIR_PROJECT: {
      return spn.paths.project;
    }
    case SPN_DIR_CACHE: {
      return spn.paths.cache;
    }
    case SPN_DIR_STORE: {
      return spn.paths.store;
    }
    case SPN_DIR_SOURCE: {
      return spn.paths.source;
    }
    case SPN_DIR_WORK: {
      return spn.paths.cwd;
    }
    case SPN_DIR_NONE:
    case SPN_DIR_INCLUDE:
    case SPN_DIR_LIB:
    case SPN_DIR_VENDOR: {
      SP_UNREACHABLE_RETURN(sp_str_lit(""));
    }
  }

  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

sp_str_t spn_build_ctx_resolve_dir(const spn_build_ctx_t* ctx, spn_pkg_dir_t kind, sp_str_t sub) {
  return sp_fs_join_path(spn_build_ctx_get_dir(ctx, kind), sub);
}

sp_str_t spn_build_ctx_get_dir(const spn_build_ctx_t* ctx, spn_pkg_dir_t kind) {
  switch (kind) {
    case SPN_DIR_STORE: {
      return ctx->paths.store;
    }
    case SPN_DIR_INCLUDE: {
      return ctx->paths.include;
    }
    case SPN_DIR_LIB: {
      return ctx->paths.lib;
    }
    case SPN_DIR_VENDOR: {
      return ctx->paths.vendor;
    }
    case SPN_DIR_SOURCE: {
      return ctx->paths.source;
    }
    case SPN_DIR_WORK: {
      return ctx->paths.work;
    }
    case SPN_DIR_CACHE: {
      return ctx->paths.store;
    }
    case SPN_DIR_PROJECT: {
      return spn.paths.project;
    }
    case SPN_DIR_NONE: {
      return sp_str_lit("");
    }
  }

  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}
