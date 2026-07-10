#include "app/types.h"
#include "ctx/types.h"

#include "event/event.h"
#include "index/cache.h"
#include "index/types.h"
#include "pkg/id.h"
#include "pkg/types.h"
#include "semver/compare.h"
#include "semver/convert.h"
#include "sp/atomic_file.h"
#include "task/task.h"
#include "toml/edit.h"

typedef struct {
  sp_str_t path [4];
  u32 num_segments;
} spn_add_site_t;

static spn_add_site_t spn_add_find_site(spn_toml_edit_t* edit, sp_mem_t mem, sp_str_t table, sp_str_t name, sp_str_t qualified) {
  sp_str_t deps [2] = { sp_str_lit("deps"), table };
  sp_da(sp_str_t) keys = spn_toml_edit_keys(edit, mem, deps, 2);

  sp_da_for(keys, it) {
    if (!sp_str_equal(spn_pkg_canonicalize_name(keys[it]), qualified)) {
      continue;
    }

    spn_add_site_t site = {
      .path = { sp_str_lit("deps"), table, keys[it], sp_str_lit("version") },
      .num_segments = 4,
    };

    spn_toml_edit_entry_t* entry = spn_toml_edit_find(edit, site.path, 3);
    if (entry && entry->kind != SPN_TOML_EDIT_VALUE_TABLE) {
      site.num_segments = 3;
    }
    return site;
  }

  return (spn_add_site_t) {
    .path = { sp_str_lit("deps"), table, name },
    .num_segments = 3,
  };
}

spn_task_step_t spn_task_add(spn_app_t* app) {
  spn_add_request_t* req = &app->request.add;

  spn_index_cache_t cache = sp_zero;
  spn_index_cache_init(&cache, spn.heap, spn.intern, &spn.indexes);

  spn_index_pkg_t* pkg = spn_index_cache_get_package(&cache, req->name);
  if (!pkg || sp_da_empty(pkg->releases)) {
    return spn_task_fail(SPN_ERR_PKG_UNKNOWN, .pkg = { .name = spn_pkg_name_to_qualified(req->name) });
  }

  spn_index_release_t* release = SP_NULLPTR;
  sp_da_rfor(pkg->releases, it) {
    spn_index_release_t* candidate = &pkg->releases[it];
    if (candidate->yanked) {
      continue;
    }
    if (!spn_semver_in_range(candidate->version, req->range)) {
      continue;
    }
    release = candidate;
    break;
  }

  if (!release) {
    return spn_task_fail(SPN_ERR_PKG_NO_MATCH, .pkg = {
      .name = spn_pkg_name_to_qualified(req->name),
      .requested = req->requested,
    });
  }

  sp_str_t version = req->requested;
  if (sp_str_empty(version)) {
    version = spn_semver_to_str(spn.heap, release->version);
  }

  spn_task_step_t result = spn_task_done();

  sp_mem_arena_marker_t s = sp_mem_begin_scratch();

  sp_str_t source = sp_zero;
  if (sp_io_read_file(s.mem, spn.paths.manifest, &source) != SP_OK) {
    result = spn_task_fail(SPN_ERR_FS_READ, .fs = { .path = spn.paths.manifest });
    goto cleanup;
  }

  spn_toml_edit_t edit = sp_zero;
  if (spn_toml_edit_init(&edit, s.mem, source)) {
    result = spn_task_fail(SPN_ERR_MANIFEST_PARSE, .manifest_parse = { .path = spn.paths.manifest });
    goto cleanup;
  }

  const c8* table = SP_NULLPTR;
  switch (req->dep) {
    case SPN_ADD_DEP_TEST:  table = "test";    break;
    case SPN_ADD_DEP_BUILD: table = "build";   break;
    default:                table = "package"; break;
  }

  spn_add_site_t site = spn_add_find_site(&edit, s.mem, sp_cstr_as_str(table), req->key, spn_pkg_name_to_qualified(req->name));
  if (spn_toml_edit_set_str(&edit, site.path, site.num_segments, version)) {
    result = spn_task_fail(SPN_ERR_MANIFEST_EDIT, .manifest_parse = { .path = spn.paths.manifest });
    goto cleanup;
  }

  sp_str_t updated = spn_toml_edit_render(&edit, s.mem);
  if (sp_fs_write_atomic(spn.paths.manifest, updated) != SP_OK) {
    result = spn_task_fail(SPN_ERR_FS_WRITE, .fs = { .path = spn.paths.manifest });
    goto cleanup;
  }

  spn_event_buffer_push(spn.events, (spn_build_event_t) {
    .kind = SPN_EVENT_ADDED,
    .added = {
      .name = sp_str_copy(spn.heap, site.path[2]),
      .version = version,
    },
  });

cleanup:
  sp_mem_end_scratch(s);
  return result;
}
