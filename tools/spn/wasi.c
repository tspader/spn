#include "sp.h"
#include <wasi/api.h>

typedef struct {
  __wasi_fd_t fd;
  u32 len;
  c8 prefix [SP_PATH_MAX];
} preopen_t;

typedef struct {
  preopen_t entries [16];
  u32 count;
  bool scanned;
} preopens_t;

static preopens_t preopens;

static void scan_preopens() {
  if (preopens.scanned) return;
  preopens.scanned = true;

  for (__wasi_fd_t fd = 3; preopens.count < sp_carr_len(preopens.entries); fd++) {
    __wasi_prestat_t prestat;
    __wasi_errno_t err = __wasi_fd_prestat_get(fd, &prestat);
    if (err == __WASI_ERRNO_BADF) {
      break;
    }
    if (err || prestat.tag != __WASI_PREOPENTYPE_DIR) {
      continue;
    }

    preopen_t* preopen = &preopens.entries[preopens.count];
    u32 len = prestat.u.dir.pr_name_len;
    if (len >= sizeof(preopen->prefix)) {
      continue;
    }
    if (__wasi_fd_prestat_dir_name(fd, (uint8_t*)preopen->prefix, len)) {
      continue;
    }
    while (len && preopen->prefix[len - 1] == '/') {
      len--;
    }
    preopen->fd = fd;
    preopen->len = len;
    preopens.count++;
  }
}

static bool resolve(sp_sys_fd_t fd, const c8* path, u32 len, c8 buf [SP_PATH_MAX], __wasi_fd_t* at) {
  __wasi_fd_t found_at = (__wasi_fd_t)fd;
  u32 skip = 0;

  if (len && path[0] == '/') {
    scan_preopens();
    bool found = false;
    sp_for(it, preopens.count) {
      preopen_t* preopen = &preopens.entries[it];
      if ((found && preopen->len < skip) || len < preopen->len) {
        continue;
      }
      if (sp_sys_memcmp(path, preopen->prefix, preopen->len)) {
        continue;
      }
      if (len > preopen->len && path[preopen->len] != '/') {
        continue;
      }
      found = true;
      found_at = preopen->fd;
      skip = preopen->len;
    }
    if (!found) {
      return false;
    }
    while (skip < len && path[skip] == '/') {
      skip++;
    }
  }

  if (skip == len) {
    buf[0] = '.';
    buf[1] = 0;
  }
  else {
    sp_cstr_copy_to_n(path + skip, len - skip, buf, SP_PATH_MAX);
  }
  *at = found_at;
  return true;
}

static sp_err_t err_from_wasi(__wasi_errno_t e) {
  switch (e) {
    case __WASI_ERRNO_SUCCESS:      return SP_OK;
    case __WASI_ERRNO_NOENT:        return SP_ERR_SYS_NOT_FOUND;
    case __WASI_ERRNO_PERM:
    case __WASI_ERRNO_ACCES:
    case __WASI_ERRNO_NOTCAPABLE:   return SP_ERR_SYS_ACCESS_DENIED;
    case __WASI_ERRNO_EXIST:        return SP_ERR_SYS_EXISTS;
    case __WASI_ERRNO_ISDIR:        return SP_ERR_SYS_IS_DIR;
    case __WASI_ERRNO_NOTDIR:       return SP_ERR_SYS_NOT_DIR;
    case __WASI_ERRNO_BADF:         return SP_ERR_SYS_BAD_FD;
    case __WASI_ERRNO_BUSY:
    case __WASI_ERRNO_TXTBSY:       return SP_ERR_SYS_BUSY;
    case __WASI_ERRNO_INVAL:        return SP_ERR_SYS_INVALID;
    case __WASI_ERRNO_NOSPC:
    case __WASI_ERRNO_DQUOT:        return SP_ERR_SYS_NO_SPACE;
    case __WASI_ERRNO_NOMEM:        return SP_ERR_SYS_NO_MEMORY;
    case __WASI_ERRNO_MFILE:
    case __WASI_ERRNO_NFILE:        return SP_ERR_SYS_TOO_MANY_FILES;
    case __WASI_ERRNO_NAMETOOLONG:  return SP_ERR_SYS_NAME_TOO_LONG;
    case __WASI_ERRNO_ROFS:         return SP_ERR_SYS_READ_ONLY_FS;
    case __WASI_ERRNO_AGAIN:        return SP_ERR_SYS_WOULD_BLOCK;
    case __WASI_ERRNO_PIPE:         return SP_ERR_SYS_BROKEN_PIPE;
    case __WASI_ERRNO_NOSYS:
    case __WASI_ERRNO_NOTSUP:       return SP_ERR_SYS_UNSUPPORTED;
    case __WASI_ERRNO_TIMEDOUT:     return SP_ERR_SYS_TIMED_OUT;
    default:                        return SP_ERR_OS;
  }
}

static sp_fs_kind_t kind_from_wasi(__wasi_filetype_t filetype) {
  switch (filetype) {
    case __WASI_FILETYPE_REGULAR_FILE:  return SP_FS_KIND_FILE;
    case __WASI_FILETYPE_DIRECTORY:     return SP_FS_KIND_DIR;
    case __WASI_FILETYPE_SYMBOLIC_LINK: return SP_FS_KIND_SYMLINK;
    default:                            return SP_FS_KIND_NONE;
  }
}

static void meta_from_wasi(const __wasi_filestat_t* src, sp_sys_file_meta_t* meta) {
  *meta = sp_zero_s(sp_sys_file_meta_t);
  meta->kind = kind_from_wasi(src->filetype);
  meta->size = (s64)src->size;
  meta->atime = (sp_sys_timespec_t) { .tv_sec = (s64)(src->atim / SP_TM_S_TO_NS), .tv_nsec = (s64)(src->atim % SP_TM_S_TO_NS) };
  meta->mtime = (sp_sys_timespec_t) { .tv_sec = (s64)(src->mtim / SP_TM_S_TO_NS), .tv_nsec = (s64)(src->mtim % SP_TM_S_TO_NS) };
  meta->id = src->ino;
  meta->device = src->dev;
  meta->nlink = src->nlink;
}

static sp_err_t wasi_open(sp_sys_fd_t fd, const c8* path, u32 len, sp_sys_open_mode_t mode, u32 flags, sp_sys_fd_t* out) {
  *out = SP_SYS_INVALID_FD;
  c8 buf [SP_PATH_MAX] = sp_zero;
  __wasi_fd_t at;
  if (!resolve(fd, path, len, buf, &at)) return SP_ERR_SYS_NOT_FOUND;

  __wasi_oflags_t oflags = 0;
  if (flags & (SP_SYS_OPEN_CREATE | SP_SYS_OPEN_EXCLUSIVE)) oflags |= __WASI_OFLAGS_CREAT;
  if (flags & SP_SYS_OPEN_EXCLUSIVE) oflags |= __WASI_OFLAGS_EXCL;
  if (flags & SP_SYS_OPEN_TRUNCATE)  oflags |= __WASI_OFLAGS_TRUNC;

  __wasi_rights_t rights =
    __WASI_RIGHTS_FD_READ | __WASI_RIGHTS_FD_SEEK | __WASI_RIGHTS_FD_TELL |
    __WASI_RIGHTS_FD_ADVISE | __WASI_RIGHTS_FD_FILESTAT_GET;
  if (mode != SP_SYS_OPEN_MODE_RO || (flags & (SP_SYS_OPEN_CREATE | SP_SYS_OPEN_TRUNCATE | SP_SYS_OPEN_APPEND))) {
    rights |=
      __WASI_RIGHTS_FD_WRITE | __WASI_RIGHTS_FD_DATASYNC | __WASI_RIGHTS_FD_SYNC |
      __WASI_RIGHTS_FD_ALLOCATE | __WASI_RIGHTS_FD_FILESTAT_SET_SIZE | __WASI_RIGHTS_FD_FILESTAT_SET_TIMES;
  }

  __wasi_fdflags_t fdflags = 0;
  if (flags & SP_SYS_OPEN_APPEND) fdflags |= __WASI_FDFLAGS_APPEND;

  __wasi_fd_t result = (__wasi_fd_t)-1;
  __wasi_errno_t err = __wasi_path_open(at, __WASI_LOOKUPFLAGS_SYMLINK_FOLLOW, buf, oflags, rights, rights, fdflags, &result);
  if (err) return err_from_wasi(err);
  *out = (sp_sys_fd_t)result;
  return SP_OK;
}

static sp_err_t wasi_open_dir(sp_sys_fd_t fd, const c8* path, u32 len, sp_sys_fd_t* out) {
  *out = SP_SYS_INVALID_FD;
  c8 buf [SP_PATH_MAX] = sp_zero;
  __wasi_fd_t at;
  if (!resolve(fd, path, len, buf, &at)) return SP_ERR_SYS_NOT_FOUND;

  __wasi_rights_t rights =
    __WASI_RIGHTS_FD_READ | __WASI_RIGHTS_FD_READDIR | __WASI_RIGHTS_FD_FILESTAT_GET |
    __WASI_RIGHTS_PATH_OPEN | __WASI_RIGHTS_PATH_FILESTAT_GET;

  __wasi_fd_t result = (__wasi_fd_t)-1;
  __wasi_errno_t err = __wasi_path_open(at, __WASI_LOOKUPFLAGS_SYMLINK_FOLLOW, buf, __WASI_OFLAGS_DIRECTORY, rights, rights, 0, &result);
  if (err) return err_from_wasi(err);
  *out = (sp_sys_fd_t)result;
  return SP_OK;
}

static sp_err_t wasi_mkdir(sp_sys_fd_t fd, const c8* path, u32 len, s32 mode) {
  c8 buf [SP_PATH_MAX] = sp_zero;
  __wasi_fd_t at;
  if (!resolve(fd, path, len, buf, &at)) return SP_ERR_SYS_NOT_FOUND;
  return err_from_wasi(__wasi_path_create_directory(at, buf));
}

static sp_err_t wasi_rmdir(sp_sys_fd_t fd, const c8* path, u32 len) {
  c8 buf [SP_PATH_MAX] = sp_zero;
  __wasi_fd_t at;
  if (!resolve(fd, path, len, buf, &at)) return SP_ERR_SYS_NOT_FOUND;
  return err_from_wasi(__wasi_path_remove_directory(at, buf));
}

static sp_err_t wasi_unlink(sp_sys_fd_t fd, const c8* path, u32 len) {
  c8 buf [SP_PATH_MAX] = sp_zero;
  __wasi_fd_t at;
  if (!resolve(fd, path, len, buf, &at)) return SP_ERR_SYS_NOT_FOUND;
  return err_from_wasi(__wasi_path_unlink_file(at, buf));
}

static sp_err_t wasi_rename(sp_sys_fd_t from_fd, const c8* from, u32 from_len, sp_sys_fd_t to_fd, const c8* to, u32 to_len) {
  c8 from_buf [SP_PATH_MAX] = sp_zero;
  c8 to_buf [SP_PATH_MAX] = sp_zero;
  __wasi_fd_t from_at;
  __wasi_fd_t to_at;
  if (!resolve(from_fd, from, from_len, from_buf, &from_at)) return SP_ERR_SYS_NOT_FOUND;
  if (!resolve(to_fd, to, to_len, to_buf, &to_at)) return SP_ERR_SYS_NOT_FOUND;
  return err_from_wasi(__wasi_path_rename(from_at, from_buf, to_at, to_buf));
}

static sp_err_t wasi_link(sp_sys_fd_t from_fd, const c8* existing, u32 existing_len, sp_sys_fd_t to_fd, const c8* alias, u32 alias_len) {
  c8 existing_buf [SP_PATH_MAX] = sp_zero;
  c8 alias_buf [SP_PATH_MAX] = sp_zero;
  __wasi_fd_t existing_at;
  __wasi_fd_t alias_at;
  if (!resolve(from_fd, existing, existing_len, existing_buf, &existing_at)) return SP_ERR_SYS_NOT_FOUND;
  if (!resolve(to_fd, alias, alias_len, alias_buf, &alias_at)) return SP_ERR_SYS_NOT_FOUND;
  return err_from_wasi(__wasi_path_link(existing_at, __WASI_LOOKUPFLAGS_SYMLINK_FOLLOW, existing_buf, alias_at, alias_buf));
}

static sp_err_t wasi_symlink(const c8* existing, u32 existing_len, sp_sys_fd_t to_fd, const c8* alias, u32 alias_len) {
  c8 existing_buf [SP_PATH_MAX] = sp_zero;
  c8 alias_buf [SP_PATH_MAX] = sp_zero;
  __wasi_fd_t alias_at;
  sp_cstr_copy_to_n(existing, existing_len, existing_buf, SP_PATH_MAX);
  if (!resolve(to_fd, alias, alias_len, alias_buf, &alias_at)) return SP_ERR_SYS_NOT_FOUND;
  return err_from_wasi(__wasi_path_symlink(existing_buf, alias_at, alias_buf));
}

static sp_err_t wasi_get_path_metadata(sp_sys_fd_t fd, const c8* path, u32 len, sp_sys_file_meta_t* st) {
  c8 buf [SP_PATH_MAX] = sp_zero;
  __wasi_fd_t at;
  if (!resolve(fd, path, len, buf, &at)) return SP_ERR_SYS_NOT_FOUND;

  __wasi_filestat_t native;
  __wasi_errno_t err = __wasi_path_filestat_get(at, __WASI_LOOKUPFLAGS_SYMLINK_FOLLOW, buf, &native);
  if (err) return err_from_wasi(err);
  meta_from_wasi(&native, st);
  return SP_OK;
}

static sp_err_t wasi_get_link_metadata(sp_sys_fd_t fd, const c8* path, u32 len, sp_sys_file_meta_t* st) {
  c8 buf [SP_PATH_MAX] = sp_zero;
  __wasi_fd_t at;
  if (!resolve(fd, path, len, buf, &at)) return SP_ERR_SYS_NOT_FOUND;

  __wasi_filestat_t native;
  __wasi_errno_t err = __wasi_path_filestat_get(at, 0, buf, &native);
  if (err) return err_from_wasi(err);
  meta_from_wasi(&native, st);
  return SP_OK;
}

static sp_err_t wasi_get_file_metadata(sp_sys_fd_t fd, sp_sys_file_meta_t* st) {
  __wasi_filestat_t native;
  __wasi_errno_t err = __wasi_fd_filestat_get((__wasi_fd_t)fd, &native);
  if (err) return err_from_wasi(err);
  meta_from_wasi(&native, st);
  return SP_OK;
}

static sp_err_t wasi_dir_from_fd(sp_sys_fd_t fd, sp_sys_dir_t* dir) {
  *dir = sp_zero_s(sp_sys_dir_t);
  dir->handle = (s64)fd;
  return SP_OK;
}

static sp_err_t wasi_dir_read(sp_sys_dir_t* dir, sp_mem_buffer_t* buf) {
  buf->len = 0;
  __wasi_size_t used = 0;
  __wasi_errno_t err = __wasi_fd_readdir((__wasi_fd_t)dir->handle, buf->data, (__wasi_size_t)buf->capacity, dir->cookie, &used);
  if (err) {
    return err_from_wasi(err);
  }
  if (!used) {
    return SP_OK;
  }

  u64 end = 0;
  __wasi_dircookie_t next = dir->cookie;
  while (true) {
    __wasi_dirent_t d;
    if (end + sizeof(d) > used) {
      break;
    }
    sp_mem_copy(&d, buf->data + end, sizeof(d));
    if (end + sizeof(d) + d.d_namlen > used) {
      break;
    }
    end += sizeof(d) + d.d_namlen;
    next = d.d_next;
  }
  if (!end) {
    return SP_ERR_SYS_INVALID;
  }

  dir->cookie = next;
  buf->len = end;
  return SP_OK;
}

static sp_err_t wasi_dir_parse(sp_sys_dir_t* dir, sp_mem_buffer_t* buf, u64* cursor, sp_sys_dir_entry_t* entry) {
  *entry = sp_zero_s(sp_sys_dir_entry_t);

  __wasi_dirent_t d;
  sp_mem_copy(&d, buf->data + *cursor, sizeof(d));
  const c8* name = (const c8*)(buf->data + *cursor + sizeof(d));
  *cursor += sizeof(d) + d.d_namlen;

  entry->name = name;
  entry->len = d.d_namlen;
  entry->kind = kind_from_wasi(d.d_type);
  if (entry->kind == SP_FS_KIND_NONE) {
    c8 cstr [SP_PATH_MAX] = sp_zero;
    sp_cstr_copy_to_n(name, d.d_namlen, cstr, SP_PATH_MAX);
    __wasi_filestat_t st;
    if (!__wasi_path_filestat_get((__wasi_fd_t)dir->handle, 0, cstr, &st)) {
      entry->kind = kind_from_wasi(st.filetype);
    }
  }
  return SP_OK;
}

static sp_err_t wasi_dir_close(sp_sys_dir_t* dir) {
  return sp_sys_close((sp_sys_fd_t)dir->handle);
}

static sp_sys_vtable_t vtable;

__attribute__((constructor))
static void install() {
  vtable = *sp_rt.vt;
  vtable.open = wasi_open;
  vtable.open_dir = wasi_open_dir;
  vtable.mkdir = wasi_mkdir;
  vtable.rmdir = wasi_rmdir;
  vtable.unlink = wasi_unlink;
  vtable.rename = wasi_rename;
  vtable.link = wasi_link;
  vtable.symlink = wasi_symlink;
  vtable.get_path_metadata = wasi_get_path_metadata;
  vtable.get_link_metadata = wasi_get_link_metadata;
  vtable.get_file_metadata = wasi_get_file_metadata;
  vtable.dir_from_fd = wasi_dir_from_fd;
  vtable.dir_read = wasi_dir_read;
  vtable.dir_parse = wasi_dir_parse;
  vtable.dir_close = wasi_dir_close;
  sp_sys_set_vtable(&vtable);
}
