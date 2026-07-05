#include "sp/macro.h"
#include "sp.h"

#include "error/types.h"
#include "index/types.h"

#include "external/git.h"
#include "external/mz.h"
#include "index/index.h"
#include "index/json.h"
#include "semver/compare.h"
#include "sp/io.h"

void spn_index_init(spn_index_info_t* index, sp_mem_t mem) {
  index->arena = sp_mem_arena_new(mem);
  mz_ctx_init(&index->json.ctx, sp_mem_arena_as_allocator(index->arena));
  index->json.schema = spn_index_build_schema(&index->json.ctx);
}

void spn_index_deinit(spn_index_info_t* index) {
  index->json.schema = SP_NULLPTR;
  index->json.ctx = SP_ZERO_STRUCT(mz_ctx_t);

  if (index->arena) {
    sp_mem_arena_destroy(index->arena);
    index->arena = SP_NULLPTR;
  }
}

sp_str_t spn_index_get_package_path(sp_mem_t mem, spn_index_info_t* index, spn_pkg_name_t id) {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch_for(mem);
  sp_str_t file = sp_fmt(scratch.mem, "{}.jsonl", sp_fmt_str(id.name)).value;
  sp_str_t relative = sp_fs_join_path(scratch.mem, id.namespace, file);
  sp_str_t path = sp_fs_join_path(mem, index->location, relative);
  sp_mem_end_scratch(scratch);
  return path;
}

spn_err_t spn_index_sync(spn_index_info_t* index) {
  switch (index->protocol) {
    case SPN_INDEX_PROTOCOL_GIT: {
      bool pinned = !sp_str_empty(index->rev);

      if (sp_fs_exists(index->location)) {
        if (pinned) {
          spn_try(spn_git_checkout(index->location, index->rev));
          return SPN_OK;
        }

        sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
        sp_str_t head = sp_fs_join_path(scratch.mem, index->location, sp_str_lit(".git/FETCH_HEAD"));
        sp_tm_epoch_t mod_time = sp_fs_get_mod_time(head);
        sp_mem_end_scratch(scratch);

        sp_tm_epoch_t now = sp_tm_now_epoch();
        if (mod_time.s + 600 <= now.s) {
          spn_try(spn_git_pull(index->location));
        }
        return SPN_OK;
      }

      spn_try(spn_git_clone(index->url, index->location));
      if (pinned) {
        spn_try(spn_git_checkout(index->location, index->rev));
      }
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

spn_index_pkg_t* spn_index_get_package(spn_index_info_t* index, spn_pkg_name_t id) {
  spn_index_pkg_t* package = SP_NULLPTR;

  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();

  sp_str_t path = spn_index_get_package_path(scratch.mem, index, id);
  if (!sp_fs_exists(path)) {
    goto cleanup;
  }

  sp_str_t blob = sp_zero;
  sp_io_read_file(scratch.mem, path, &blob);
  if (sp_str_empty(blob)) {
    goto cleanup;
  }

  package = sp_alloc_type(sp_mem_arena_as_allocator(index->arena), spn_index_pkg_t);
  *package = SP_ZERO_STRUCT(spn_index_pkg_t);
  if (spn_index_parse_pkg(&index->json.ctx, index->json.schema, id, blob, package) != SPN_OK) {
    package = SP_NULLPTR;
    goto cleanup;
  }

cleanup:
  sp_mem_end_scratch(scratch);
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

  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();

  sp_str_t path = spn_index_get_package_path(scratch.mem, index, rel->id);
  sp_str_t parent = sp_fs_join_path(scratch.mem, index->location, rel->id.namespace);
  if (!sp_fs_exists(parent)) {
    sp_fs_create_dir(parent);
  }

  sp_str_t json = spn_index_rel_to_json(scratch.mem, rel);

  sp_sys_fd_t fd = sp_sys_open_s(sp_sys_get_root(0), path, SP_O_WRONLY | SP_O_CREAT | SP_O_APPEND | SP_O_BINARY, 0644);
  sp_io_file_writer_t io;
  sp_io_file_writer_from_fd(&io, fd, SP_IO_CLOSE_MODE_AUTO);
  sp_io_write_line(&io.base, json);
  sp_io_file_writer_close(&io);

  sp_mem_end_scratch(scratch);

  return SPN_OK;
}

spn_index_rel_t* spn_index_get_release(spn_index_info_t* index, spn_pkg_name_t id, spn_semver_t version) {
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
