#ifndef SPN_WASI_H
#define SPN_WASI_H

#include "sp.h"

typedef struct spn_wasi spn_wasi_t;

typedef enum {
  SPN_WASI_ESUCCESS = 0,
  SPN_WASI_EACCES = 2,
  SPN_WASI_EBADF = 8,
  SPN_WASI_EBADMSG = 9,
  SPN_WASI_EEXIST = 20,
  SPN_WASI_EFAULT = 21,
  SPN_WASI_EINVAL = 28,
  SPN_WASI_EIO = 29,
  SPN_WASI_EISDIR = 31,
  SPN_WASI_ELOOP = 32,
  SPN_WASI_ENOENT = 44,
  SPN_WASI_ENOSYS = 52,
  SPN_WASI_ENOTDIR = 54,
  SPN_WASI_ENOTEMPTY = 55,
  SPN_WASI_EOVERFLOW = 61,
  SPN_WASI_EPERM = 63,
  SPN_WASI_ENOTCAPABLE = 76,
} spn_wasi_errno_t;

typedef enum {
  SPN_WASI_FILETYPE_UNKNOWN = 0,
  SPN_WASI_FILETYPE_BLOCK_DEVICE = 1,
  SPN_WASI_FILETYPE_CHARACTER_DEVICE = 2,
  SPN_WASI_FILETYPE_DIRECTORY = 3,
  SPN_WASI_FILETYPE_REGULAR_FILE = 4,
  SPN_WASI_FILETYPE_SOCKET_DGRAM = 5,
  SPN_WASI_FILETYPE_SOCKET_STREAM = 6,
  SPN_WASI_FILETYPE_SYMBOLIC_LINK = 7,
} spn_wasi_filetype_t;

typedef struct {
  u64 dev;
  u64 ino;
  u8 filetype;
  u64 nlink;
  u64 size;
  u64 atim;
  u64 mtim;
  u64 ctim;
} spn_wasi_filestat_t;

typedef void (*spn_wasi_resolve_hook_t)(void* user, sp_str_t op, sp_str_t host_abs, bool allowed);

spn_wasi_t* spn_wasi_new(sp_mem_t mem);
void        spn_wasi_add_preopen(spn_wasi_t* w, sp_str_t guest_path, sp_str_t host_dir);
void        spn_wasi_set_resolve_hook(spn_wasi_t* w, spn_wasi_resolve_hook_t hook, void* user);
void        spn_wasi_free(spn_wasi_t* w);

spn_wasi_errno_t spn_wasi_path_open(spn_wasi_t* w, s32 dirfd, sp_str_t path, u32 oflags, u32 fdflags, u64 rights_base, u64 rights_inheriting, s32* out_fd);
spn_wasi_errno_t spn_wasi_fd_read(spn_wasi_t* w, s32 fd, void* buf, u64 len, u64* out_read);
spn_wasi_errno_t spn_wasi_fd_pread(spn_wasi_t* w, s32 fd, void* buf, u64 len, u64 offset, u64* out_read);
spn_wasi_errno_t spn_wasi_fd_write(spn_wasi_t* w, s32 fd, const void* buf, u64 len, u64* out_written);
spn_wasi_errno_t spn_wasi_fd_pwrite(spn_wasi_t* w, s32 fd, const void* buf, u64 len, u64 offset, u64* out_written);
spn_wasi_errno_t spn_wasi_fd_seek(spn_wasi_t* w, s32 fd, s64 offset, u32 whence, u64* out_pos);
spn_wasi_errno_t spn_wasi_fd_tell(spn_wasi_t* w, s32 fd, u64* out_pos);
spn_wasi_errno_t spn_wasi_fd_close(spn_wasi_t* w, s32 fd);
spn_wasi_errno_t spn_wasi_fd_renumber(spn_wasi_t* w, s32 from, s32 to);
spn_wasi_errno_t spn_wasi_fd_readdir(spn_wasi_t* w, s32 fd, void* buf, u64 len, u64 cookie, u64* out_used);
spn_wasi_errno_t spn_wasi_fd_filestat_get(spn_wasi_t* w, s32 fd, spn_wasi_filestat_t* out);
spn_wasi_errno_t spn_wasi_fd_prestat_get(spn_wasi_t* w, s32 fd, u32* out_name_len);
spn_wasi_errno_t spn_wasi_fd_prestat_dir_name(spn_wasi_t* w, s32 fd, c8* buf, u64 len);
spn_wasi_errno_t spn_wasi_path_filestat_get(spn_wasi_t* w, s32 dirfd, u32 lookup_flags, sp_str_t path, spn_wasi_filestat_t* out);
spn_wasi_errno_t spn_wasi_path_create_directory(spn_wasi_t* w, s32 dirfd, sp_str_t path);
spn_wasi_errno_t spn_wasi_path_remove_directory(spn_wasi_t* w, s32 dirfd, sp_str_t path);
spn_wasi_errno_t spn_wasi_path_unlink_file(spn_wasi_t* w, s32 dirfd, sp_str_t path);
spn_wasi_errno_t spn_wasi_path_rename(spn_wasi_t* w, s32 old_dirfd, sp_str_t old_path, s32 new_dirfd, sp_str_t new_path);
spn_wasi_errno_t spn_wasi_path_symlink(spn_wasi_t* w, sp_str_t target, s32 dirfd, sp_str_t link_path);
spn_wasi_errno_t spn_wasi_path_readlink(spn_wasi_t* w, s32 dirfd, sp_str_t path, c8* buf, u64 len, u64* out_used);

#endif

#ifdef SPN_WASI_IMPLEMENTATION
#ifndef SPN_WASI_IMPLEMENTATION_ONCE
#define SPN_WASI_IMPLEMENTATION_ONCE

typedef struct {
  sp_str_t guest;
  sp_str_t host;
} spn_wasi_preopen_t;

struct spn_wasi {
  sp_mem_t mem;
  sp_da(spn_wasi_preopen_t) preopens;
  s32 next_fd;
  spn_wasi_resolve_hook_t hook;
  void* hook_user;
};

spn_wasi_t* spn_wasi_new(sp_mem_t mem) {
  spn_wasi_t* w = sp_alloc_type(mem, spn_wasi_t);
  w->mem = mem;
  w->next_fd = 3;
  sp_da_init(mem, w->preopens);
  return w;
}

void spn_wasi_add_preopen(spn_wasi_t* w, sp_str_t guest_path, sp_str_t host_dir) {
  sp_da_push(w->preopens, ((spn_wasi_preopen_t) {
    .guest = sp_str_copy(w->mem, guest_path),
    .host = sp_str_copy(w->mem, host_dir)
  }));
}

void spn_wasi_set_resolve_hook(spn_wasi_t* w, spn_wasi_resolve_hook_t hook, void* user) {
  w->hook = hook;
  w->hook_user = user;
}

void spn_wasi_free(spn_wasi_t* w) {
  sp_free(w->mem, w, sizeof(spn_wasi_t));
}

spn_wasi_errno_t spn_wasi_path_open(spn_wasi_t* w, s32 dirfd, sp_str_t path, u32 oflags, u32 fdflags, u64 rights_base, u64 rights_inheriting, s32* out_fd) {
  (void)dirfd; (void)path; (void)oflags; (void)fdflags; (void)rights_base; (void)rights_inheriting;
  *out_fd = w->next_fd++;
  return SPN_WASI_ESUCCESS;
}

spn_wasi_errno_t spn_wasi_fd_read(spn_wasi_t* w, s32 fd, void* buf, u64 len, u64* out_read) {
  (void)w; (void)fd; (void)buf; (void)len;
  *out_read = 0;
  return SPN_WASI_ENOSYS;
}

spn_wasi_errno_t spn_wasi_fd_pread(spn_wasi_t* w, s32 fd, void* buf, u64 len, u64 offset, u64* out_read) {
  (void)w; (void)fd; (void)buf; (void)len; (void)offset;
  *out_read = 0;
  return SPN_WASI_ENOSYS;
}

spn_wasi_errno_t spn_wasi_fd_write(spn_wasi_t* w, s32 fd, const void* buf, u64 len, u64* out_written) {
  (void)w; (void)fd; (void)buf; (void)len;
  *out_written = 0;
  return SPN_WASI_ENOSYS;
}

spn_wasi_errno_t spn_wasi_fd_pwrite(spn_wasi_t* w, s32 fd, const void* buf, u64 len, u64 offset, u64* out_written) {
  (void)w; (void)fd; (void)buf; (void)len; (void)offset;
  *out_written = 0;
  return SPN_WASI_ENOSYS;
}

spn_wasi_errno_t spn_wasi_fd_seek(spn_wasi_t* w, s32 fd, s64 offset, u32 whence, u64* out_pos) {
  (void)w; (void)fd; (void)offset; (void)whence;
  *out_pos = 0;
  return SPN_WASI_ENOSYS;
}

spn_wasi_errno_t spn_wasi_fd_tell(spn_wasi_t* w, s32 fd, u64* out_pos) {
  (void)w; (void)fd;
  *out_pos = 0;
  return SPN_WASI_ENOSYS;
}

spn_wasi_errno_t spn_wasi_fd_close(spn_wasi_t* w, s32 fd) {
  (void)w; (void)fd;
  return SPN_WASI_ESUCCESS;
}

spn_wasi_errno_t spn_wasi_fd_renumber(spn_wasi_t* w, s32 from, s32 to) {
  (void)w; (void)from; (void)to;
  return SPN_WASI_ESUCCESS;
}

spn_wasi_errno_t spn_wasi_fd_readdir(spn_wasi_t* w, s32 fd, void* buf, u64 len, u64 cookie, u64* out_used) {
  (void)w; (void)fd; (void)buf; (void)len; (void)cookie;
  *out_used = 0;
  return SPN_WASI_ENOSYS;
}

spn_wasi_errno_t spn_wasi_fd_filestat_get(spn_wasi_t* w, s32 fd, spn_wasi_filestat_t* out) {
  (void)w; (void)fd;
  *out = (spn_wasi_filestat_t) sp_zero;
  return SPN_WASI_ENOSYS;
}

spn_wasi_errno_t spn_wasi_fd_prestat_get(spn_wasi_t* w, s32 fd, u32* out_name_len) {
  (void)w; (void)fd;
  *out_name_len = 0;
  return SPN_WASI_ENOSYS;
}

spn_wasi_errno_t spn_wasi_fd_prestat_dir_name(spn_wasi_t* w, s32 fd, c8* buf, u64 len) {
  (void)w; (void)fd; (void)buf; (void)len;
  return SPN_WASI_ENOSYS;
}

spn_wasi_errno_t spn_wasi_path_filestat_get(spn_wasi_t* w, s32 dirfd, u32 lookup_flags, sp_str_t path, spn_wasi_filestat_t* out) {
  (void)w; (void)dirfd; (void)lookup_flags; (void)path;
  *out = (spn_wasi_filestat_t) sp_zero;
  return SPN_WASI_ENOSYS;
}

spn_wasi_errno_t spn_wasi_path_create_directory(spn_wasi_t* w, s32 dirfd, sp_str_t path) {
  (void)w; (void)dirfd; (void)path;
  return SPN_WASI_ENOSYS;
}

spn_wasi_errno_t spn_wasi_path_remove_directory(spn_wasi_t* w, s32 dirfd, sp_str_t path) {
  (void)w; (void)dirfd; (void)path;
  return SPN_WASI_ENOSYS;
}

spn_wasi_errno_t spn_wasi_path_unlink_file(spn_wasi_t* w, s32 dirfd, sp_str_t path) {
  (void)w; (void)dirfd; (void)path;
  return SPN_WASI_ENOSYS;
}

spn_wasi_errno_t spn_wasi_path_rename(spn_wasi_t* w, s32 old_dirfd, sp_str_t old_path, s32 new_dirfd, sp_str_t new_path) {
  (void)w; (void)old_dirfd; (void)old_path; (void)new_dirfd; (void)new_path;
  return SPN_WASI_ENOSYS;
}

spn_wasi_errno_t spn_wasi_path_symlink(spn_wasi_t* w, sp_str_t target, s32 dirfd, sp_str_t link_path) {
  (void)w; (void)target; (void)dirfd; (void)link_path;
  return SPN_WASI_ENOSYS;
}

spn_wasi_errno_t spn_wasi_path_readlink(spn_wasi_t* w, s32 dirfd, sp_str_t path, c8* buf, u64 len, u64* out_used) {
  (void)w; (void)dirfd; (void)path; (void)buf; (void)len;
  *out_used = 0;
  return SPN_WASI_ENOSYS;
}

#endif
#endif
