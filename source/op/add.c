#include "ctx/ctx.h"
#include "ctx/types.h"

#include "error/error.h"
#include "event/event.h"
#include "index/cache.h"
#include "index/types.h"
#include "op/op.h"
#include "spn/host.h"
#include "pkg/id.h"
#include "pkg/types.h"
#include "project/types.h"
#include "semver/compare.h"
#include "semver/convert.h"
#include "semver/parser.h"
#include "sp/atomic_file.h"
#include "toml/edit.h"
#include "toml/issue.h"

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

static spn_err_t index_err(spn_ctx_t* ctx, spn_pkg_name_t name, spn_err_t kind, spn_index_diag_t* diag) {
  switch (kind) {
    case SPN_ERR_MANIFEST_ISSUES: {
      return spn_err_emit(ctx, (spn_err_union_t) {
        .kind = kind,
        .manifest = { .name = spn_pkg_name_to_qualified(name), .path = diag->path, .issues = spn_codegen_issues_to_err(ctx->heap, diag->issues) },
      });
    }
    case SPN_ERR_INDEX_CORRUPT: {
      return spn_err_emit(ctx, (spn_err_union_t) {
        .kind = kind,
        .index_corrupt = { .name = spn_pkg_name_to_qualified(name), .path = diag->path },
      });
    }
    case SPN_ERR_INDEX_PATH_DEP: {
      return spn_err_emit(ctx, (spn_err_union_t) {
        .kind = kind,
        .pkg = { .name = spn_pkg_name_to_qualified(name), .requested = diag->dep },
      });
    }
    default: {
      return spn_err_emit(ctx, (spn_err_union_t) { .kind = kind });
    }
  }
}

static spn_err_t add(spn_ctx_t* ctx, spn_add_request_t request, spn_semver_range_t range) {
  spn_try(spn_ctx_require_project(ctx));

  spn_pkg_name_t name = spn_pkg_name_from_qualified(request.name);

  spn_index_cache_t cache = sp_zero;
  spn_index_cache_init(&cache, ctx->heap, ctx->intern, &ctx->indexes);

  spn_index_pkg_t* pkg = SP_NULLPTR;
  spn_index_diag_t diag = sp_zero;
  spn_err_t got = spn_index_cache_get_package(&cache, name, &pkg, &diag);
  if (got) {
    return index_err(ctx, name, got, &diag);
  }
  if (!pkg || sp_da_empty(pkg->releases)) {
    return spn_err_emit(ctx, (spn_err_union_t) { .kind = SPN_ERR_PKG_UNKNOWN, .unknown = { .qualified = spn_pkg_name_to_qualified(name) } });
  }

  spn_index_release_t* release = SP_NULLPTR;
  sp_da_rfor(pkg->releases, it) {
    spn_index_release_t* candidate = &pkg->releases[it];
    if (candidate->yanked) {
      continue;
    }
    if (!spn_semver_in_range(candidate->version, range)) {
      continue;
    }
    release = candidate;
    break;
  }

  if (!release) {
    return spn_err_emit(ctx, (spn_err_union_t) { .kind = SPN_ERR_PKG_NO_MATCH, .unsatisfiable = {
      .qualified = spn_pkg_name_to_qualified(name),
      .range = spn_semver_range_to_str(ctx->heap, range),
    }});
  }

  sp_str_t version = request.version;
  if (sp_str_empty(version)) {
    version = spn_semver_to_str(ctx->heap, release->version);
  }

  spn_err_t result = SPN_OK;

  sp_mem_arena_marker_t s = sp_mem_begin_scratch();

  sp_str_t source = sp_zero;
  sp_str_t manifest = ctx->project->paths.manifest;
  if (sp_io_read_file(s.mem, manifest, &source) != SP_OK) {
    result = spn_err_emit(ctx, (spn_err_union_t) { .kind = SPN_ERR_FS_READ, .fs = { .path = manifest } });
    goto cleanup;
  }

  spn_toml_edit_t edit = sp_zero;
  if (spn_toml_edit_init(&edit, s.mem, source)) {
    result = spn_err_emit(ctx, (spn_err_union_t) { .kind = SPN_ERR_MANIFEST_PARSE, .manifest_parse = { .path = manifest } });
    goto cleanup;
  }

  const c8* table = SP_NULLPTR;
  switch (request.kind) {
    case SPN_DEP_KIND_TEST:  table = "test";    break;
    case SPN_DEP_KIND_BUILD: table = "build";   break;
    default:                 table = "package"; break;
  }

  site_t site = find_site(&edit, s.mem, sp_cstr_as_str(table), request.name, spn_pkg_name_to_qualified(name));
  if (spn_toml_edit_set_str(&edit, site.path, site.num_segments, version)) {
    result = spn_err_emit(ctx, (spn_err_union_t) { .kind = SPN_ERR_MANIFEST_EDIT, .manifest_parse = { .path = manifest } });
    goto cleanup;
  }

  sp_str_t updated = spn_toml_edit_render(&edit, s.mem);
  if (sp_fs_write_atomic(manifest, updated) != SP_OK) {
    result = spn_err_emit(ctx, (spn_err_union_t) { .kind = SPN_ERR_FS_WRITE, .fs = { .path = manifest } });
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

spn_err_t spn_op_add(spn_op_t* op) {
  spn_ctx_t* ctx = op->ctx;
  spn_add_request_t request = op->request.add;

  spn_semver_range_t range = spn_semver_any();
  if (!sp_str_empty(request.version) && spn_semver_parse_range(request.version, &range)) {
    return spn_err_emit(ctx, (spn_err_union_t) {
      .kind = SPN_ERR_VERSION_INVALID,
      .version_invalid = { .requested = request.version },
    });
  }

  return add(ctx, request, range);
}

spn_op_t* spn_add_dependency(spn_ctx_t* ctx, spn_add_request_t request) {
  spn_op_t* op = spn_op_new(ctx, SP_NULLPTR, SPN_OP_ADD);
  op->request.add = (spn_add_request_t) {
    .name = sp_str_copy(ctx->heap, request.name),
    .version = sp_str_copy(ctx->heap, request.version),
    .kind = request.kind,
  };
  spn_op_submit(op);
  return op;
}
