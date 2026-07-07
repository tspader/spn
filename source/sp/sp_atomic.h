#ifndef SP_FS_ATOMIC_H
#define SP_FS_ATOMIC_H

#include "sp.h"

typedef enum {
  SP_FS_ATOMIC_REPLACE,
  SP_FS_ATOMIC_EXCLUSIVE,
} sp_fs_atomic_mode_t;

typedef struct {
  sp_sys_fd_t         dir;
  sp_str_t            path;
  sp_str_t            temp;
  c8                  temp_buf[SP_PATH_MAX];
  sp_io_file_writer_t writer;
} sp_fs_atomic_t;

SP_API sp_err_t        sp_fs_atomic_open(sp_fs_atomic_t* af, sp_str_t path);
SP_API sp_err_t        sp_fs_atomic_open_at(sp_fs_atomic_t* af, sp_sys_fd_t dir, sp_str_t path);
SP_API sp_io_writer_t* sp_fs_atomic_writer(sp_fs_atomic_t* af);
SP_API sp_err_t        sp_fs_atomic_commit(sp_fs_atomic_t* af, sp_fs_atomic_mode_t mode);
SP_API sp_err_t        sp_fs_atomic_abort(sp_fs_atomic_t* af);
SP_API sp_err_t        sp_fs_write_atomic(sp_str_t path, sp_str_t str);
SP_API sp_err_t        sp_fs_write_atomic_slice(sp_str_t path, sp_mem_slice_t slice);
SP_API sp_err_t        sp_fs_write_atomic_cstr(sp_str_t path, const c8* str);

#endif // SP_FS_ATOMIC_H

#if defined(SP_IMPLEMENTATION) && !defined(SP_FS_ATOMIC_IMPLEMENTATION)
  #define SP_FS_ATOMIC_IMPLEMENTATION
#endif

#ifdef SP_FS_ATOMIC_IMPLEMENTATION

static sp_atomic_s32_t sp_fs_atomic_sequence;

static void sp_fs_atomic_temp_name(sp_fs_atomic_t* af) {
  sp_tm_epoch_t now = sp_tm_now_epoch();
  u64 stamp = (now.s << 20) ^ (u64)now.ns;
  u64 sequence = (u64)(u32)sp_atomic_s32_add(&sp_fs_atomic_sequence, 1);
  sp_str_t parent = sp_fs_parent_path(af->path);
  sp_str_t name = sp_fs_get_name(af->path);
  if (sp_str_empty(parent)) {
    af->temp = sp_fmt_buf(af->temp_buf, SP_PATH_MAX, ".{}.{}.{}.tmp", sp_fmt_str(name), sp_fmt_uint(stamp), sp_fmt_uint(sequence)).value;
  } else {
    af->temp = sp_fmt_buf(af->temp_buf, SP_PATH_MAX, "{}/.{}.{}.{}.tmp", sp_fmt_str(parent), sp_fmt_str(name), sp_fmt_uint(stamp), sp_fmt_uint(sequence)).value;
  }
}

static void sp_fs_atomic_make_parents(sp_sys_fd_t dir, sp_str_t path) {
  sp_for(i, path.len) {
    if (i > 0 && sp_str_at(path, (s32)i) == '/') {
      sp_sys_mkdir_s(dir, sp_str_prefix(path, (s32)i), 0755);
    }
  }
}

sp_err_t sp_fs_atomic_open_at(sp_fs_atomic_t* af, sp_sys_fd_t dir, sp_str_t path) {
  *af = sp_zero_s(sp_fs_atomic_t);
  af->dir = dir;
  af->path = path;
  sp_fs_atomic_temp_name(af);
  sp_fs_atomic_make_parents(dir, af->temp);

  sp_sys_fd_t fd = sp_sys_open_s(dir, af->temp, SP_O_CREAT | SP_O_EXCL | SP_O_WRONLY | SP_O_BINARY, 0644);
  if (fd == SP_SYS_INVALID_FD) {
    *af = sp_zero_s(sp_fs_atomic_t);
    return SP_ERR_OS;
  }

  sp_err_t err = sp_io_file_writer_from_fd(&af->writer, fd, SP_IO_CLOSE_MODE_AUTO);
  if (err) {
    sp_sys_unlink_s(dir, af->temp);
    *af = sp_zero_s(sp_fs_atomic_t);
  }
  return err;
}

sp_err_t sp_fs_atomic_open(sp_fs_atomic_t* af, sp_str_t path) {
  return sp_fs_atomic_open_at(af, sp_sys_get_root(0), path);
}

sp_io_writer_t* sp_fs_atomic_writer(sp_fs_atomic_t* af) {
  return &af->writer.base;
}

sp_err_t sp_fs_atomic_commit(sp_fs_atomic_t* af, sp_fs_atomic_mode_t mode) {
  sp_err_t err = sp_io_flush(&af->writer.base);
  if (err) {
    sp_io_file_writer_close(&af->writer);
    goto fail;
  }

  err = sp_io_file_writer_close(&af->writer);
  if (err) goto fail;

  switch (mode) {
    case SP_FS_ATOMIC_REPLACE:
      if (sp_sys_rename_s(af->dir, af->temp, af->dir, af->path)) { err = SP_ERR_OS; goto fail; }
      break;
    case SP_FS_ATOMIC_EXCLUSIVE:
      if (sp_sys_link_s(af->dir, af->temp, af->dir, af->path)) { err = SP_ERR_OS; goto fail; }
      sp_sys_unlink_s(af->dir, af->temp);
      break;
  }

  af->temp = sp_zero_s(sp_str_t);
  return SP_OK;

fail:
  sp_sys_unlink_s(af->dir, af->temp);
  af->temp = sp_zero_s(sp_str_t);
  return err;
}

sp_err_t sp_fs_atomic_abort(sp_fs_atomic_t* af) {
  if (sp_str_empty(af->temp)) return SP_OK;
  sp_io_file_writer_close(&af->writer);
  s32 rc = sp_sys_unlink_s(af->dir, af->temp);
  af->temp = sp_zero_s(sp_str_t);
  return rc ? SP_ERR_OS : SP_OK;
}

sp_err_t sp_fs_write_atomic_slice(sp_str_t path, sp_mem_slice_t slice) {
  sp_fs_atomic_t af = sp_zero;
  sp_try(sp_fs_atomic_open(&af, path));

  sp_err_t err = sp_io_write_all(sp_fs_atomic_writer(&af), slice.data, slice.len, SP_NULLPTR);
  if (err) {
    sp_fs_atomic_abort(&af);
    return err;
  }

  return sp_fs_atomic_commit(&af, SP_FS_ATOMIC_REPLACE);
}

sp_err_t sp_fs_write_atomic(sp_str_t path, sp_str_t str) {
  sp_fs_atomic_t af = sp_zero;
  sp_try(sp_fs_atomic_open(&af, path));

  sp_err_t err = sp_io_write_str(sp_fs_atomic_writer(&af), str, SP_NULLPTR);
  if (err) {
    sp_fs_atomic_abort(&af);
    return err;
  }

  return sp_fs_atomic_commit(&af, SP_FS_ATOMIC_REPLACE);
}

sp_err_t sp_fs_write_atomic_cstr(sp_str_t path, const c8* str) {
  return sp_fs_write_atomic(path, sp_str_view(str));
}

#endif // SP_FS_ATOMIC_IMPLEMENTATION
