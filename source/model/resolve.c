#include "sp.h"
#include "sp/macro.h"
#include "ctx/types.h"
#include "intern/types.h"
#include "project/types.h"
#include "resolve/types.h"

#include "error/error.h"
#include "event/event.h"
#include "index/cache.h"
#include "op/types.h"
#include "intern/intern.h"
#include "pkg/id.h"
#include "pkg/load.h"
#include "pkg/pkg.h"
#include "resolve/resolve.h"
#include "semver/convert.h"
#include "session/session.h"
#include "unit/types.h"

static sp_str_t patch_dir(sp_mem_t mem, spn_index_release_t* release) {
  if (sp_str_empty(spn.paths.patches)) {
    return sp_str_lit("");
  }

  sp_str_t dir = sp_fs_join_path(mem, spn.paths.patches, release->id.name);
  sp_str_t manifest = sp_fs_join_path(mem, dir, release->paths.manifest);
  if (!sp_fs_exists(manifest)) {
    return sp_str_lit("");
  }

  return dir;
}

static spn_err_t apply_patch_overrides(spn_session_t* session, spn_resolve_query_t* query) {
  spn_err_t result = SPN_OK;
  sp_ht_for_kv(query->result, it) {
    spn_resolved_pkg_t* pkg = it.val;
    if (pkg->source != SPN_PKG_SOURCE_INDEX) {
      continue;
    }

    sp_str_t patch = patch_dir(spn.mem, pkg->origin.release);
    if (sp_str_empty(patch)) {
      continue;
    }

    sp_str_t manifest = sp_fs_join_path(spn.mem, patch, pkg->origin.paths.manifest);
    sp_str_t name = sp_intern_str_from_id(session->ctx->intern, pkg->id.qualified);
    spn_pkg_info_t* info = sp_alloc_type(spn.mem, spn_pkg_info_t);
    spn_codegen_issues_t issues = sp_zero;
    spn_err_t loaded = spn_pkg_load(spn.mem, session->ctx->intern, manifest, SPN_MANIFEST_DEP, info, &issues);
    if (loaded == SPN_ERR_NO_MANIFEST) {
      result = spn_err_emit(session->ctx, (spn_err_union_t) {
        .kind = SPN_ERR_NO_MANIFEST,
        .no_manifest = { .path = manifest },
      });
      continue;
    }
    if (loaded) {
      result = spn_err_emit(session->ctx, (spn_err_union_t) {
        .kind = SPN_ERR_MANIFEST_ISSUES,
        .manifest = { .name = name, .path = manifest, .issues = issues },
      });
      continue;
    }

    pkg->origin.recipe = (spn_pkg_root_t) { .kind = SPN_PKG_ROOT_LOCAL, .local = patch };
    pkg->origin.source = spn_pkg_upstream(info);
    pkg->origin.info = info;
    pkg->name = info->name;
    pkg->options = info->options;
  }
  return result;
}

static void add_root(spn_session_t* session, spn_resolve_query_t* query) {
  spn_resolve_query_add(query, (spn_requested_dep_t) {
    .qualified = session->pkg->qualified,
    .source = SPN_PKG_SOURCE_ROOT,
  });
}

static void emit_resolved(sp_mem_t mem, spn_resolve_query_t* query) {
  sp_ht_for_kv(query->result, it) {
    spn_event_buffer_push(spn.events, (spn_build_event_t) {
      .kind = SPN_EVENT_RESOLVE_PACKAGE,
      .resolve_pkg = {
        .name = spn_intern_str(it.val->id.qualified),
        .version = spn_semver_to_str(mem, it.val->id.version),
      }
    });
  }

  spn_event_buffer_push(spn.events, (spn_build_event_t) {
    .kind = SPN_EVENT_RESOLVE_END,
    .resolve_end = {
      .num_resolved = sp_ht_size(query->result),
      .time = query->time,
    }
  });
}

spn_err_t resolve(spn_op_t* op) {
  spn_session_t* session = op->session;
  spn_event_buffer_push(spn.events, (spn_build_event_t) {
    .kind = SPN_EVENT_RESOLVE_START,
    .pkg = session->pkg,
  });

  sp_ht_insert(session->registry, spn_pkg_id(session->ctx->intern, session->pkg->qualified), ((spn_registry_pkg_t) {
    .source = SPN_PKG_SOURCE_ROOT,
    .info = session->pkg,
    .manifest = session->project->paths.manifest,
  }));

  spn_index_cache_t index = sp_zero;
  spn_index_cache_init(&index, session->mem, session->ctx->intern, &spn.indexes);

  spn_resolver_t resolver = sp_zero;
  spn_resolver_init(&resolver, spn.mem, session->ctx->intern, &index, &session->registry, session->profile, session->pkg->config, 0);
  resolver.seeds = session->gates.seeds;

  spn_resolve_query_t query = sp_zero_initialize();
  spn_resolve_query_init(session->mem, &query);
  add_root(session, &query);

  if (spn_resolve_from_solver(&resolver, &query)) {
    sp_da_for(query.errors, it) {
      spn_err_emit(session->ctx, query.errors[it]);
    }
    return query.errors[0].kind;
  }

  spn_try(apply_patch_overrides(session, &query));
  session->resolve = query.result;

  emit_resolved(session->mem, &query);

  return SPN_OK;
}
