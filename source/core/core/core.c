#include "core/core.h"
#include "sp/io.h"

static bool file_matches(sp_str_t path, sp_mem_slice_t content) {
  sp_mem_arena_marker_t s = sp_mem_begin_scratch();
  bool matches = false;

  sp_sys_file_meta_t meta = sp_zero;
  if (!sp_sys_get_path_metadata_s(sp_sys_get_root(0), path, &meta) && (u64)meta.size == content.len) {
    sp_mem_slice_t existing = sp_zero;
    if (!sp_io_read_file_slice(s.mem, path, &existing)) {
      matches = existing.len == content.len && sp_mem_is_equal(existing.data, content.data, content.len);
    }
  }

  sp_mem_end_scratch(s);
  return matches;
}

spn_err_t spn_fs_update_file(sp_str_t from, sp_str_t to) {
  sp_mem_arena_marker_t s = sp_mem_begin_scratch();
  spn_err_t err = SPN_OK;

  sp_mem_slice_t content = sp_zero;
  if (sp_io_read_file_slice(s.mem, from, &content)) {
    err = SPN_ERROR;
  }
  else {
    if (sp_fs_is_dir(to)) {
      to = sp_fs_join_path(s.mem, to, sp_fs_get_name(from));
    }
    if (!file_matches(to, content) && sp_fs_copy(from, to)) {
      err = SPN_ERROR;
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

sp_str_t spn_tree_root(spn_tree_roots_t roots, spn_tree_t tree) {
  return tree == SPN_TREE_MANIFEST ? roots.recipe : roots.source;
}

sp_str_t spn_tree_path_resolve(sp_mem_t mem, spn_tree_roots_t roots, spn_tree_path_t entry) {
  if (sp_fs_is_absolute(entry.path)) {
    return entry.path;
  }
  return sp_fs_join_path(mem, spn_tree_root(roots, entry.tree), entry.path);
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
