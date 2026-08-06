#include "ctx/ctx.h"
#include "ctx/types.h"

#include "error/error.h"
#include "event/event.h"
#include "index/cache.h"
#include "index/types.h"
#include "op/op.h"
#include "pkg/id.h"
#include "pkg/types.h"
#include "project/types.h"
#include "semver/compare.h"
#include "semver/convert.h"
#include "sp/atomic_file.h"
#include "toml/edit.h"

typedef struct {
  sp_str_t path [4];
  u32 num_segments;
} site_t;

static site_t find_site(spn_toml_edit_t* edit, sp_mem_t mem, sp_str_t table, sp_str_t name, sp_str_t qualified) {
  sp_str_t deps [2] = { sp_str_lit("deps"), table };
  sp_da(sp_str_t) keys = spn_toml_edit_keys(edit, mem, deps, 2);

  sp_da_for(keys, it) {
    if (!sp_str_equal(spn_pkg_canonicalize_name(keys[it]), qualified)) {
      continue;
    }

    site_t site = {
      .path = { sp_str_lit("deps"), table, keys[it], sp_str_lit("version") },
      .num_segments = 4,
    };

    spn_toml_edit_entry_t* entry = spn_toml_edit_find(edit, site.path, 3);
    if (entry && entry->kind != SPN_TOML_EDIT_VALUE_TABLE) {
      site.num_segments = 3;
    }
    return site;
  }

  return (site_t) {
    .path = { sp_str_lit("deps"), table, name },
    .num_segments = 3,
  };
}

static spn_err_union_t add(spn_ctx_t* ctx, spn_add_request_t request) {
  spn_try_union(spn_ctx_require_project(ctx));

  spn_pkg_name_t name = spn_pkg_name_from_qualified(request.key);

  spn_index_cache_t cache = sp_zero;
  spn_index_cache_init(&cache, ctx->heap, ctx->intern, &ctx->indexes);

  spn_index_pkg_t* pkg = SP_NULLPTR;
  spn_try_union(spn_index_cache_get_package(&cache, name, &pkg));
  if (!pkg || sp_da_empty(pkg->releases)) {
    return (spn_err_union_t) { .kind = SPN_ERR_PKG_UNKNOWN, .unknown = { .request = { .qualified = spn_pkg_name_to_qualified(name) } } };
  }

  spn_index_release_t* release = SP_NULLPTR;
  sp_da_rfor(pkg->releases, it) {
    spn_index_release_t* candidate = &pkg->releases[it];
    if (candidate->yanked) {
      continue;
    }
    if (!spn_semver_in_range(candidate->version, request.range)) {
      continue;
    }
    release = candidate;
    break;
  }

  if (!release) {
    return (spn_err_union_t) { .kind = SPN_ERR_PKG_NO_MATCH, .unsatisfiable = {
      .request = {
        .qualified = spn_pkg_name_to_qualified(name),
        .source = SPN_PKG_SOURCE_INDEX,
        .index = { .range = request.range },
      },
    }};
  }

  sp_str_t version = request.requested;
  if (sp_str_empty(version)) {
    version = spn_semver_to_str(ctx->heap, release->version);
  }

  spn_err_union_t result = spn_result(SPN_OK);

  sp_mem_arena_marker_t s = sp_mem_begin_scratch();

  sp_str_t source = sp_zero;
  sp_str_t manifest = ctx->project->paths.manifest;
  if (sp_io_read_file(s.mem, manifest, &source) != SP_OK) {
    result = (spn_err_union_t) { .kind = SPN_ERR_FS_READ, .fs = { .path = manifest } };
    goto cleanup;
  }

  spn_toml_edit_t edit = sp_zero;
  if (spn_toml_edit_init(&edit, s.mem, source)) {
    result = (spn_err_union_t) { .kind = SPN_ERR_MANIFEST_PARSE, .manifest_parse = { .path = manifest } };
    goto cleanup;
  }

  const c8* table = SP_NULLPTR;
  switch (request.dep) {
    case SPN_ADD_DEP_TEST:  table = "test";    break;
    case SPN_ADD_DEP_BUILD: table = "build";   break;
    default:                table = "package"; break;
  }

  site_t site = find_site(&edit, s.mem, sp_cstr_as_str(table), request.key, spn_pkg_name_to_qualified(name));
  if (spn_toml_edit_set_str(&edit, site.path, site.num_segments, version)) {
    result = (spn_err_union_t) { .kind = SPN_ERR_MANIFEST_EDIT, .manifest_parse = { .path = manifest } };
    goto cleanup;
  }

  sp_str_t updated = spn_toml_edit_render(&edit, s.mem);
  if (sp_fs_write_atomic(manifest, updated) != SP_OK) {
    result = (spn_err_union_t) { .kind = SPN_ERR_FS_WRITE, .fs = { .path = manifest } };
    goto cleanup;
  }

  spn_event_buffer_push(ctx->events, (spn_build_event_t) {
    .kind = SPN_EVENT_ADDED,
    .added = {
      .name = sp_str_copy(ctx->heap, site.path[2]),
      .version = version,
    },
  });

cleanup:
  sp_mem_end_scratch(s);
  return result;
}

spn_err_t spn_op_add(spn_ctx_t* ctx, spn_add_request_t request) {
  spn_try(spn_op_sync_indexes(ctx, (spn_index_refresh_t) sp_zero));
  return spn_err_emit(ctx, add(ctx, request));
}
