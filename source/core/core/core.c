#include "core/core.h"
#include "io/io.h"
#include "sp/sp_glob.h"

static bool content_matches(sp_mem_t mem, sp_str_t path, sp_mem_slice_t content) {
  sp_mem_slice_t existing = sp_zero;
  if (sp_io_read_file_slice(mem, path, &existing)) {
    return false;
  }
  return existing.len == content.len && sp_mem_is_equal(existing.data, content.data, content.len);
}

static bool file_matches(sp_str_t path, sp_mem_slice_t content) {
  sp_sys_file_meta_t meta = sp_zero;
  if (sp_sys_get_path_metadata_s(sp_sys_get_root(0), path, &meta) || (u64)meta.size != content.len) {
    return false;
  }

  sp_mem_arena_marker_t s = sp_mem_begin_scratch();
  bool matches = content_matches(s.mem, path, content);
  sp_mem_end_scratch(s);
  return matches;
}

spn_err_t spn_fs_update_file(sp_str_t from, sp_str_t to) {
  sp_sys_file_meta_t source = sp_zero;
  if (sp_sys_get_path_metadata_s(sp_sys_get_root(0), from, &source)) {
    return SPN_ERROR;
  }

  sp_mem_arena_marker_t s = sp_mem_begin_scratch();

  sp_sys_file_meta_t dest = sp_zero;
  sp_sys_get_path_metadata_s(sp_sys_get_root(0), to, &dest);

  bool matches = false;
  if (dest.kind == SP_FS_KIND_FILE && dest.size == source.size) {
    sp_mem_slice_t content = sp_zero;
    matches = !sp_io_read_file_slice(s.mem, from, &content) && content_matches(s.mem, to, content);
  }

  spn_err_t err = SPN_OK;
  if (!matches && sp_fs_copy_file(from, to, SP_FS_ATOMIC_REPLACE)) {
    err = SPN_ERROR;
  }

  sp_mem_end_scratch(s);
  return err;
}

spn_err_t spn_fs_update_glob(sp_str_t from, sp_str_t to) {
  sp_mem_arena_marker_t s = sp_mem_begin_scratch();
  spn_err_t err = SPN_OK;

  sp_glob_set_t* glob = sp_glob_set_new(s.mem);
  sp_glob_set_add(glob, sp_str_to_cstr(s.mem, sp_fs_get_name(from)));
  sp_glob_set_build(glob);

  sp_da(sp_fs_entry_t) entries = sp_zero;
  if (sp_fs_collect(s.mem, sp_fs_parent_path(from), &entries) || sp_fs_create_dir(to)) {
    err = SPN_ERROR;
  }

  sp_da_for(entries, it) {
    sp_fs_entry_t* entry = &entries[it];
    if (err) {
      break;
    }
    if (!sp_glob_set_match(glob, entry->name)) {
      continue;
    }

    sp_str_t dest = sp_fs_join_path(s.mem, to, entry->name);
    sp_fs_kind_t kind = entry->kind == SP_FS_KIND_SYMLINK ? sp_fs_get_target_kind(entry->path) : entry->kind;
    switch (kind) {
      case SP_FS_KIND_FILE: {
        err = spn_fs_update_file(entry->path, dest);
        break;
      }
      case SP_FS_KIND_DIR: {
        err = sp_fs_copy_tree(entry->path, dest, SP_FS_ATOMIC_REPLACE) ? SPN_ERROR : SPN_OK;
        break;
      }
      case SP_FS_KIND_SYMLINK:
      case SP_FS_KIND_NONE: {
        err = SPN_ERROR;
        break;
      }
    }
  }

  sp_mem_end_scratch(s);
  return err;
}

spn_err_t spn_fs_update_file_str(sp_str_t path, sp_str_t content) {
  if (file_matches(path, sp_mem_slice((u8*)content.data, content.len))) {
    return SPN_OK;
  }

  sp_io_file_writer_t writer = sp_zero;
  if (sp_io_file_writer_from_path(&writer, path)) {
    return SPN_ERROR;
  }
  sp_err_t err = sp_io_write_all(&writer.base, content.data, content.len, SP_NULLPTR);
  sp_io_file_writer_close(&writer);
  return err ? SPN_ERROR : SPN_OK;
}

void spn_wake_ring(spn_wake_t* wake) {
  if (!wake->fn) {
    return;
  }
  if (sp_atomic_u32_cas(&wake->signaled, 0, 1, SP_ATOMIC_SEQ_CST)) {
    wake->fn(wake->data);
  }
}

void spn_wake_pulse(spn_wake_t* wake) {
  if (!wake->fn) {
    return;
  }
  sp_atomic_u32_store(&wake->signaled, 1, SP_ATOMIC_SEQ_CST);
  wake->fn(wake->data);
}

void spn_wake_rearm(spn_wake_t* wake) {
  sp_atomic_u32_store(&wake->signaled, 0, SP_ATOMIC_SEQ_CST);
}
