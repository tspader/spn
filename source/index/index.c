#include "sp.h"

#include "error/types.h"
#include "index/types.h"

#include "external/git.h"
#include "external/mz.h"
#include "index/index.h"
#include "index/json.h"
#include "semver/compare.h"
#include "sp/io.h"

void spn_index_init(spn_index_info_t* index) {
  index->arena = sp_mem_arena_new(spn_allocator);
  mz_ctx_init_ex(&index->json.ctx, sp_mem_arena_as_allocator(index->arena));

  sp_context_push_arena(index->arena); {
    index->json.schema = spn_index_build_schema(&index->json.ctx);

    sp_context_pop();
  }
}

void spn_index_deinit(spn_index_info_t* index) {
  index->json.schema = SP_NULLPTR;
  index->json.ctx = SP_ZERO_STRUCT(mz_ctx_t);

  if (index->arena) {
    sp_mem_arena_destroy(index->arena);
    index->arena = SP_NULLPTR;
  }
}

sp_str_t spn_index_get_package_path(spn_index_info_t* index, spn_pkg_id_t id) {
  sp_str_t relative = sp_fs_join_path(spn_allocator, id.namespace, sp_format("{}.jsonl", SP_FMT_STR(id.name)));
  return sp_fs_join_path(spn_allocator, index->location, relative);
}

spn_err_t spn_index_sync(spn_index_info_t* index) {
  switch (index->protocol) {
    case SPN_INDEX_PROTOCOL_GIT: {
      if (sp_fs_exists(index->location)) {
        sp_str_t head = sp_fs_join_path(spn_allocator, index->location, sp_str_lit(".git/FETCH_HEAD"));
        sp_tm_epoch_t mod_time = sp_fs_get_mod_time(head);
        sp_tm_epoch_t now = sp_tm_now_epoch();
        if (mod_time.s + 600 <= now.s) {
          spn_try(spn_git_pull(index->location));
        }
        return SPN_OK;
      }

      spn_try(spn_git_clone(index->url, index->location));
      return SPN_OK;
    }
    case SPN_INDEX_PROTOCOL_HTTP: {
      return SPN_ERROR;
    }
    case SPN_INDEX_PROTOCOL_FILESYSTEM: {
      return SPN_OK;
    }
  }
  return SPN_ERROR;
}

spn_index_pkg_t* spn_index_get_package(spn_index_info_t* index, spn_pkg_id_t id) {
  spn_index_pkg_t* package = SP_NULLPTR;

  sp_context_push_arena(index->arena);

  sp_str_t path = spn_index_get_package_path(index, id);
  if (!sp_fs_exists(path)) {
    goto cleanup;
  }

  sp_str_t blob = sp_zero; sp_io_read_file(spn_allocator, path, &blob);
  if (sp_str_empty(blob)) {
    goto cleanup;
  }

  package = sp_alloc_type(spn_allocator, spn_index_pkg_t);
  if (spn_index_parse_pkg(&index->json.ctx, index->json.schema, id, blob, package) != SPN_OK) {
    package = SP_NULLPTR;
    goto cleanup;
  }

cleanup:
  sp_context_pop();
  return package;
}

spn_err_t spn_index_publish(spn_index_info_t* index, spn_index_rel_t* rel) {
  spn_index_pkg_t* existing = spn_index_get_package(index, rel->id);
  if (existing) {
    sp_da_for(existing->releases, it) {
      if (spn_semver_eq(existing->releases[it].version, rel->version)) {
        return SPN_ERR_VERSION_EXISTS;
      }
    }
  }

  sp_str_t path = spn_index_get_package_path(index, rel->id);
  sp_str_t parent = sp_fs_join_path(spn_allocator, index->location, rel->id.namespace);
  if (!sp_fs_exists(parent)) {
    sp_fs_create_dir(parent);
  }

  sp_str_t json = spn_index_rel_to_json(rel);
  sp_io_writer_t* io = sp_io_writer_from_file(path, SP_IO_WRITE_MODE_APPEND);
  sp_io_write_line(io, json);
  sp_io_writer_close(io);

  return SPN_OK;
}

spn_index_rel_t* spn_index_get_release(spn_index_info_t* index, spn_pkg_id_t id, spn_semver_t version) {
  spn_index_pkg_t* package = spn_index_get_package(index, id);
  if (!package) return SP_NULLPTR;

  sp_da_for(package->releases, it) {
    spn_index_rel_t* release = &package->releases[it];
    if (spn_semver_eq(release->version, version)) {
      return release;
    }
  }

  return SP_NULLPTR;
}
