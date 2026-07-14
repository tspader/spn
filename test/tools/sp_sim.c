#include "sp_sim.h"

#define SP_SIM_ROOT ((sp_sys_fd_t)-4097)
#define SP_SIM_FD_BASE ((sp_sys_fd_t)4096)

struct sp_sim_inode {
  u64 id;
  sp_fs_kind_t kind;
  u64 nlink;
  sp_sys_timespec_t mtime;
  sp_da(u8) bytes;
};

struct sp_sim_fd {
  sp_sim_inode_t* node;
  sp_str_t path;
  u64 offset;
  s32 flags;
  bool open;
};

static sp_sim_t* sp_sim_active;

#define sp_sim_unexpected(name) sp_fatal("sp_sim: unexpected syscall {}", sp_fmt_cstr(name))

static sp_sim_t* sp_sim_syscall(void) {
  SP_ASSERT(sp_sim_active);
  sp_sim_active->syscalls++;
  return sp_sim_active;
}

static sp_sim_t* sp_sim_syscall_at(sp_sys_fd_t fd) {
  SP_ASSERT(fd == SP_SIM_ROOT);
  return sp_sim_syscall();
}

static void sp_sim_stamp(sp_sim_inode_t* node) {
  sp_sim_t* sim = sp_sim_active;
  sim->clock.tv_nsec += 1000000;
  if (sim->clock.tv_nsec >= 1000000000) {
    sim->clock.tv_sec += 1;
    sim->clock.tv_nsec -= 1000000000;
  }
  node->mtime = sim->clock;
}

static sp_str_t sp_sim_norm(const c8* path, u32 len, c8* buf) {
  u64 out = 1;
  buf[0] = '/';

  u32 i = 0;
  while (i < len) {
    while (i < len && path[i] == '/') i++;
    u32 start = i;
    while (i < len && path[i] != '/') i++;
    u32 n = i - start;

    if (!n) continue;
    if (n == 1 && path[start] == '.') continue;
    if (n == 2 && path[start] == '.' && path[start + 1] == '.') {
      while (out > 1 && buf[out - 1] != '/') out--;
      if (out > 1) out--;
      continue;
    }

    SP_ASSERT(out + 1 + n < SP_PATH_MAX);
    if (out > 1) buf[out++] = '/';
    sp_sys_memcpy(buf + out, path + start, n);
    out += n;
  }

  return sp_str(buf, out);
}

static sp_str_t sp_sim_parent(sp_str_t path) {
  s32 i = sp_str_find_c8_reverse(path, '/');
  return i <= 0 ? sp_str_lit("/") : sp_str_prefix(path, i);
}

static bool sp_sim_is_within(sp_str_t path, sp_str_t dir) {
  if (path.len <= dir.len || !sp_str_starts_with(path, dir)) return false;
  return dir.len == 1 || path.data[dir.len] == '/';
}

static sp_str_t sp_sim_child_name(sp_str_t path, sp_str_t dir) {
  u64 start = dir.len == 1 ? 1 : dir.len + 1;
  sp_str_t name = sp_str(path.data + start, path.len - start);
  return sp_str_find_c8(name, '/') == SP_STR_NO_MATCH ? name : sp_str_lit("");
}

static sp_sim_inode_t* sp_sim_find(sp_str_t path) {
  sp_sim_inode_t** node = sp_ht_getp(sp_sim_active->nodes, path);
  return node ? *node : SP_NULLPTR;
}

static sp_sim_inode_t* sp_sim_inode(sp_fs_kind_t kind) {
  sp_sim_t* sim = sp_sim_active;
  sp_sim_inode_t* node = sp_mem_allocator_alloc_type(sim->mem, sp_sim_inode_t);
  *node = (sp_sim_inode_t) {
    .id = sim->ids++,
    .kind = kind,
    .nlink = 1,
    .bytes = sp_da_new(sim->mem, u8),
  };
  sp_sim_stamp(node);
  return node;
}

static void sp_sim_insert(sp_str_t path, sp_sim_inode_t* node) {
  sp_sim_t* sim = sp_sim_active;
  sp_ht_insert(sim->nodes, sp_str_copy(sim->mem, path), node);
}

static void sp_sim_log(sp_str_t path) {
  sp_sim_t* sim = sp_sim_active;
  sp_da_push(sim->events, ((sp_sim_event_t) {
    .path = sp_str_copy(sim->mem, path),
    .sys = sim->syscalls,
  }));
}

static sp_sim_fd_t* sp_sim_fd(sp_sys_fd_t fd) {
  sp_sim_t* sim = sp_sim_active;
  u64 idx = (u64)(fd - SP_SIM_FD_BASE);
  if (fd < SP_SIM_FD_BASE || idx >= sp_da_size(sim->fds) || !sim->fds[idx].open) {
    sp_fatal("sp_sim: bad fd {}", sp_fmt_uint((u64)fd));
  }
  return &sim->fds[idx];
}

static void sp_sim_meta(sp_sim_inode_t* node, sp_sys_file_meta_t* st) {
  *st = (sp_sys_file_meta_t) {
    .kind = node->kind,
    .size = (s64)sp_da_size(node->bytes),
    .atime = node->mtime,
    .mtime = node->mtime,
    .btime = node->mtime,
    .id = node->id,
    .device = 1,
    .nlink = node->nlink,
  };
}

static void sp_sim_file_reserve(sp_sim_inode_t* node, u64 size) {
  u64 old = sp_da_size(node->bytes);
  if (size <= old) return;
  sp_da_reserve(node->bytes, size);
  sp_da_head(node->bytes)->size = size;
  sp_sys_memset(node->bytes + old, 0, size - old);
}

static s64 sp_sim_read_at(sp_sim_inode_t* node, void* buf, u64 count, u64 offset) {
  if (node->kind != SP_FS_KIND_FILE) return -1;
  u64 len = sp_da_size(node->bytes);
  if (offset >= len) return 0;
  u64 n = sp_min(count, len - offset);
  sp_sys_memcpy(buf, node->bytes + offset, n);
  return (s64)n;
}

static s64 sp_sim_write_at(sp_sim_inode_t* node, const void* buf, u64 count, u64 offset) {
  if (node->kind != SP_FS_KIND_FILE) return -1;
  sp_sim_file_reserve(node, offset + count);
  sp_sys_memcpy(node->bytes + offset, buf, count);
  sp_sim_stamp(node);
  return (s64)count;
}

static void sp_sim_sys_init(void) {
  sp_sim_unexpected("init");
}

static s64 sp_sim_sys_read(sp_sys_fd_t fd, void* buf, u64 count) {
  sp_sim_syscall();
  sp_sim_fd_t* entry = sp_sim_fd(fd);
  s64 n = sp_sim_read_at(entry->node, buf, count, entry->offset);
  if (n > 0) {
    entry->offset += (u64)n;
  }
  return n;
}

static s64 sp_sim_sys_write(sp_sys_fd_t fd, const void* buf, u64 count) {
  if (fd == sp_sys_stdout || fd == sp_sys_stderr) {
    return sp_sys_vtable_platform.write(fd, buf, count);
  }
  sp_sim_syscall();
  sp_sim_fd_t* entry = sp_sim_fd(fd);
  if (entry->flags & SP_O_APPEND) {
    entry->offset = sp_da_size(entry->node->bytes);
  }
  s64 n = sp_sim_write_at(entry->node, buf, count, entry->offset);
  if (n > 0) {
    entry->offset += (u64)n;
    sp_sim_log(entry->path);
  }
  return n;
}

static s64 sp_sim_sys_pread(sp_sys_fd_t fd, void* buf, u64 count, u64 offset) {
  sp_sim_syscall();
  return sp_sim_read_at(sp_sim_fd(fd)->node, buf, count, offset);
}

static s64 sp_sim_sys_pwrite(sp_sys_fd_t fd, const void* buf, u64 count, u64 offset) {
  sp_sim_syscall();
  sp_sim_fd_t* entry = sp_sim_fd(fd);
  s64 n = sp_sim_write_at(entry->node, buf, count, offset);
  if (n > 0) {
    sp_sim_log(entry->path);
  }
  return n;
}

static sp_sys_fd_t sp_sim_sys_get_root(s32 it) {
  if (it != 0) {
    sp_sim_unexpected("get_root");
  }
  return SP_SIM_ROOT;
}

static s64 sp_sim_sys_get_exe_path(c8* buf, u64 size) {
  sp_sim_unexpected("get_exe_path");
  return -1;
}

static s64 sp_sim_sys_get_cwd_path(c8* buf, u64 size) {
  sp_sim_unexpected("get_cwd_path");
  return -1;
}

static s64 sp_sim_sys_get_storage_path(c8* buf, u64 size) {
  sp_sim_unexpected("get_storage_path");
  return -1;
}

static s64 sp_sim_sys_get_config_path(c8* buf, u64 size) {
  sp_sim_unexpected("get_config_path");
  return -1;
}

static sp_sys_fd_t sp_sim_sys_open(sp_sys_fd_t fd, const c8* path, u32 len, s32 flags, s32 mode) {
  sp_sim_t* sim = sp_sim_syscall_at(fd);

  c8 buf [SP_PATH_MAX];
  sp_str_t norm = sp_sim_norm(path, len, buf);
  sp_sim_inode_t* node = sp_sim_find(norm);

  if (node && (flags & SP_O_CREAT) && (flags & SP_O_EXCL)) {
    return SP_SYS_INVALID_FD;
  }

  if (!node) {
    if (!(flags & SP_O_CREAT)) {
      return SP_SYS_INVALID_FD;
    }
    sp_sim_inode_t* parent = sp_sim_find(sp_sim_parent(norm));
    if (!parent || parent->kind != SP_FS_KIND_DIR) {
      return SP_SYS_INVALID_FD;
    }
    node = sp_sim_inode(SP_FS_KIND_FILE);
    sp_sim_insert(norm, node);
    sp_sim_log(norm);
  }

  if (node->kind == SP_FS_KIND_DIR && (flags & (SP_O_WRONLY | SP_O_RDWR))) {
    return SP_SYS_INVALID_FD;
  }

  if ((flags & SP_O_TRUNC) && node->kind == SP_FS_KIND_FILE && sp_da_size(node->bytes)) {
    sp_da_clear(node->bytes);
    sp_sim_stamp(node);
    sp_sim_log(norm);
  }

  sp_da_push(sim->fds, ((sp_sim_fd_t) {
    .node = node,
    .path = sp_str_copy(sim->mem, norm),
    .offset = 0,
    .flags = flags,
    .open = true,
  }));
  return SP_SIM_FD_BASE + (sp_sys_fd_t)(sp_da_size(sim->fds) - 1);
}

static s32 sp_sim_sys_close(sp_sys_fd_t fd) {
  sp_sim_syscall();
  sp_sim_fd(fd)->open = false;
  return 0;
}

static s32 sp_sim_sys_pipe(sp_sys_fd_t* read_end, sp_sys_fd_t* write_end) {
  sp_sim_unexpected("pipe");
  return -1;
}

static s32 sp_sim_sys_mkdir(sp_sys_fd_t fd, const c8* path, u32 len, s32 mode) {
  sp_sim_syscall_at(fd);

  c8 buf [SP_PATH_MAX];
  sp_str_t norm = sp_sim_norm(path, len, buf);
  if (sp_sim_find(norm)) {
    return -1;
  }

  sp_sim_inode_t* parent = sp_sim_find(sp_sim_parent(norm));
  if (!parent || parent->kind != SP_FS_KIND_DIR) {
    return -1;
  }

  sp_sim_insert(norm, sp_sim_inode(SP_FS_KIND_DIR));
  return 0;
}

static s32 sp_sim_sys_rmdir(sp_sys_fd_t fd, const c8* path, u32 len) {
  sp_sim_t* sim = sp_sim_syscall_at(fd);

  c8 buf [SP_PATH_MAX];
  sp_str_t norm = sp_sim_norm(path, len, buf);
  sp_sim_inode_t* node = sp_sim_find(norm);
  if (!node || node->kind != SP_FS_KIND_DIR || norm.len == 1) {
    return -1;
  }

  sp_ht_for_kv(sim->nodes, it) {
    if (sp_sim_is_within(*it.key, norm)) {
      return -1;
    }
  }

  sp_ht_erase(sim->nodes, norm);
  node->nlink--;
  return 0;
}

static s32 sp_sim_sys_unlink(sp_sys_fd_t fd, const c8* path, u32 len) {
  sp_sim_t* sim = sp_sim_syscall_at(fd);

  c8 buf [SP_PATH_MAX];
  sp_str_t norm = sp_sim_norm(path, len, buf);
  sp_sim_inode_t* node = sp_sim_find(norm);
  if (!node || node->kind == SP_FS_KIND_DIR) {
    return -1;
  }

  sp_ht_erase(sim->nodes, norm);
  node->nlink--;
  return 0;
}

static s32 sp_sim_sys_rename(sp_sys_fd_t from_fd, const c8* from, u32 from_len, sp_sys_fd_t to_fd, const c8* to, u32 to_len) {
  SP_ASSERT(to_fd == SP_SIM_ROOT);
  sp_sim_t* sim = sp_sim_syscall_at(from_fd);

  c8 from_buf [SP_PATH_MAX];
  c8 to_buf [SP_PATH_MAX];
  sp_str_t from_norm = sp_sim_norm(from, from_len, from_buf);
  sp_str_t to_norm = sp_sim_norm(to, to_len, to_buf);

  sp_sim_inode_t* src = sp_sim_find(from_norm);
  if (!src) {
    return -1;
  }
  if (sp_str_equal(from_norm, to_norm)) {
    return 0;
  }

  if (src->kind == SP_FS_KIND_DIR) {
    if (sp_sim_find(to_norm) || sp_sim_is_within(to_norm, from_norm)) {
      return -1;
    }
    sp_sim_inode_t* parent = sp_sim_find(sp_sim_parent(to_norm));
    if (!parent || parent->kind != SP_FS_KIND_DIR) {
      return -1;
    }

    sp_da(sp_str_t) moved = sp_da_new(sim->mem, sp_str_t);
    sp_ht_for_kv(sim->nodes, it) {
      if (sp_str_equal(*it.key, from_norm) || sp_sim_is_within(*it.key, from_norm)) {
        sp_da_push(moved, *it.key);
      }
    }
    sp_da_for(moved, it) {
      sp_str_t old_key = moved[it];
      sp_sim_inode_t* node = sp_sim_find(old_key);
      sp_str_t suffix = sp_str(old_key.data + from_norm.len, old_key.len - from_norm.len);
      sp_str_t new_key = sp_fmt(sim->mem, "{}{}", sp_fmt_str(to_norm), sp_fmt_str(suffix)).value;
      sp_ht_erase(sim->nodes, old_key);
      sp_sim_insert(new_key, node);
    }
    return 0;
  }

  sp_sim_inode_t* dst = sp_sim_find(to_norm);
  if (dst == src) {
    return 0;
  }
  if (dst) {
    if (dst->kind == SP_FS_KIND_DIR) {
      return -1;
    }
    sp_ht_erase(sim->nodes, to_norm);
    dst->nlink--;
  }
  else {
    sp_sim_inode_t* parent = sp_sim_find(sp_sim_parent(to_norm));
    if (!parent || parent->kind != SP_FS_KIND_DIR) {
      return -1;
    }
  }

  sp_ht_erase(sim->nodes, from_norm);
  sp_sim_insert(to_norm, src);
  sp_sim_log(to_norm);
  return 0;
}

static s32 sp_sim_sys_link(sp_sys_fd_t from_fd, const c8* existing, u32 existing_len, sp_sys_fd_t to_fd, const c8* alias, u32 alias_len) {
  SP_ASSERT(to_fd == SP_SIM_ROOT);
  sp_sim_syscall_at(from_fd);

  c8 existing_buf [SP_PATH_MAX];
  c8 alias_buf [SP_PATH_MAX];
  sp_str_t existing_norm = sp_sim_norm(existing, existing_len, existing_buf);
  sp_str_t alias_norm = sp_sim_norm(alias, alias_len, alias_buf);

  sp_sim_inode_t* src = sp_sim_find(existing_norm);
  if (!src || src->kind != SP_FS_KIND_FILE) {
    return -1;
  }
  if (sp_sim_find(alias_norm)) {
    return -1;
  }

  sp_sim_inode_t* parent = sp_sim_find(sp_sim_parent(alias_norm));
  if (!parent || parent->kind != SP_FS_KIND_DIR) {
    return -1;
  }

  sp_sim_insert(alias_norm, src);
  src->nlink++;
  sp_sim_log(alias_norm);
  return 0;
}

static s32 sp_sim_sys_symlink(const c8* existing, u32 existing_len, sp_sys_fd_t to_fd, const c8* alias, u32 alias_len) {
  sp_sim_unexpected("symlink");
  return -1;
}

static s32 sp_sim_sys_get_path_metadata(sp_sys_fd_t fd, const c8* path, u32 len, sp_sys_file_meta_t* st) {
  sp_sim_syscall_at(fd);

  c8 buf [SP_PATH_MAX];
  sp_sim_inode_t* node = sp_sim_find(sp_sim_norm(path, len, buf));
  if (!node) {
    return -1;
  }
  sp_sim_meta(node, st);
  return 0;
}

static s32 sp_sim_sys_get_file_metadata(sp_sys_fd_t fd, sp_sys_file_meta_t* st) {
  sp_sim_syscall();
  sp_sim_meta(sp_sim_fd(fd)->node, st);
  return 0;
}

static s32 sp_sim_sys_chmod(sp_sys_fd_t fd, const c8* path, u32 len, const sp_sys_file_meta_t* st) {
  sp_sim_syscall_at(fd);

  c8 buf [SP_PATH_MAX];
  return sp_sim_find(sp_sim_norm(path, len, buf)) ? 0 : -1;
}

static s32 sp_sim_sys_clock_gettime(s32 clockid, sp_sys_timespec_t* ts) {
  sp_sim_syscall();
  *ts = sp_sim_active->clock;
  return 0;
}

static s32 sp_sim_sys_nanosleep(const sp_sys_timespec_t* req, sp_sys_timespec_t* rem) {
  sp_sim_unexpected("nanosleep");
  return -1;
}

static s64 sp_sim_sys_canonicalize_path(const c8* path, u32 len, c8* buf, u64 size) {
  sp_sim_syscall();

  c8 norm_buf [SP_PATH_MAX];
  sp_str_t norm = sp_sim_norm(path, len, norm_buf);
  if (!sp_sim_find(norm) || norm.len > size) {
    return -1;
  }
  sp_sys_memcpy(buf, norm.data, norm.len);
  return (s64)norm.len;
}

static s32 sp_sim_sys_fd_ready(sp_sys_fd_t fd, u8* ready) {
  sp_sim_unexpected("fd_ready");
  return -1;
}

static s32 sp_sim_sys_fd_wait(sp_sys_fd_t fd) {
  sp_sim_unexpected("fd_wait");
  return -1;
}

static s32 sp_sim_sys_fds_wait(const sp_sys_fd_t* fds, u8* ready, u64 nfds) {
  sp_sim_unexpected("fds_wait");
  return -1;
}

static s32 sp_sim_sys_socket_open(sp_sys_socket_t* out) {
  sp_sim_unexpected("socket_open");
  return -1;
}

static s32 sp_sim_sys_socket_bind(sp_sys_socket_t socket, sp_sys_ipv4_t addr) {
  sp_sim_unexpected("socket_bind");
  return -1;
}

static s32 sp_sim_sys_socket_listen(sp_sys_socket_t socket, u32 backlog) {
  sp_sim_unexpected("socket_listen");
  return -1;
}

static s32 sp_sim_sys_socket_connect(sp_sys_socket_t socket, sp_sys_ipv4_t addr) {
  sp_sim_unexpected("socket_connect");
  return -1;
}

static s32 sp_sim_sys_socket_error(sp_sys_socket_t socket) {
  sp_sim_unexpected("socket_error");
  return -1;
}

static s32 sp_sim_sys_socket_accept(sp_sys_socket_t listener, sp_sys_socket_t* out) {
  sp_sim_unexpected("socket_accept");
  return -1;
}

static s32 sp_sim_sys_socket_close(sp_sys_socket_t socket) {
  sp_sim_unexpected("socket_close");
  return -1;
}

static s64 sp_sim_sys_socket_recv(sp_sys_socket_t socket, void* buf, u64 count) {
  sp_sim_unexpected("socket_recv");
  return -1;
}

static s64 sp_sim_sys_socket_send(sp_sys_socket_t socket, const void* buf, u64 count) {
  sp_sim_unexpected("socket_send");
  return -1;
}

static s32 sp_sim_sys_socket_wait(sp_sys_socket_t socket, bool readable, u32 timeout_ms) {
  sp_sim_unexpected("socket_wait");
  return -1;
}

static s32 sp_sim_sys_socket_set_nonblocking(sp_sys_socket_t socket) {
  sp_sim_unexpected("socket_set_nonblocking");
  return -1;
}

static s32 sp_sim_sys_socket_reuse_addr(sp_sys_socket_t socket) {
  sp_sim_unexpected("socket_reuse_addr");
  return -1;
}

static s32 sp_sim_sys_socket_local_port(sp_sys_socket_t socket, u16* out) {
  sp_sim_unexpected("socket_local_port");
  return -1;
}

static s64 sp_sim_sys_lseek(sp_sys_fd_t fd, s64 offset, s32 whence) {
  sp_sim_syscall();
  sp_sim_fd_t* entry = sp_sim_fd(fd);

  s64 base = 0;
  switch (whence) {
    case SP_SEEK_SET: base = 0; break;
    case SP_SEEK_CUR: base = (s64)entry->offset; break;
    case SP_SEEK_END: base = (s64)sp_da_size(entry->node->bytes); break;
    default: return -1;
  }

  s64 next = base + offset;
  if (next < 0) {
    return -1;
  }
  entry->offset = (u64)next;
  return next;
}

static s32 sp_sim_sys_chdir(const c8* path, u32 len) {
  sp_sim_unexpected("chdir");
  return -1;
}

static s32 sp_sim_sys_fs_it_open(sp_sys_fd_t fd, sp_sys_fs_it_t* it, const c8* path, u32 path_len, void* buf, u64 cap) {
  sp_sim_t* sim = sp_sim_syscall_at(fd);

  c8 norm_buf [SP_PATH_MAX];
  sp_str_t norm = sp_sim_norm(path, path_len, norm_buf);
  sp_sim_inode_t* node = sp_sim_find(norm);
  if (!node || node->kind != SP_FS_KIND_DIR) {
    return -1;
  }

  u8* data = (u8*)buf;
  u64 out = 0;
  sp_ht_for_kv(sim->nodes, kv) {
    if (!sp_sim_is_within(*kv.key, norm)) continue;
    sp_str_t name = sp_sim_child_name(*kv.key, norm);
    if (sp_str_empty(name)) continue;

    if (out + 5 + name.len > cap) {
      sp_fatal("sp_sim: fs_it buffer overflow listing {}", sp_fmt_str(norm));
    }
    u32 name_len = (u32)name.len;
    sp_sys_memcpy(data + out, &name_len, sizeof(name_len));
    data[out + 4] = (u8)(*kv.val)->kind;
    sp_sys_memcpy(data + out + 5, name.data, name.len);
    out += 5 + name.len;
  }

  *it = (sp_sys_fs_it_t) {
    .handle = 0,
    .buf = { .data = data, .len = out, .capacity = cap },
    .cursor = 0,
  };
  return 0;
}

static s32 sp_sim_sys_fs_it_next(sp_sys_fs_it_t* it, sp_sys_fs_entry_t* out) {
  if (it->cursor >= it->buf.len) {
    return -1;
  }

  u32 name_len = 0;
  sp_sys_memcpy(&name_len, it->buf.data + it->cursor, sizeof(name_len));
  out->kind = (sp_fs_kind_t)it->buf.data[it->cursor + 4];
  out->name = (const c8*)(it->buf.data + it->cursor + 5);
  out->len = name_len;
  it->cursor += 5 + name_len;
  return 0;
}

static void sp_sim_sys_fs_it_close(sp_sys_fs_it_t* it) {
}

static const sp_sys_vtable_t sp_sim_vtable = {
  .init                   = sp_sim_sys_init,
  .read                   = sp_sim_sys_read,
  .write                  = sp_sim_sys_write,
  .pread                  = sp_sim_sys_pread,
  .pwrite                 = sp_sim_sys_pwrite,
  .get_root               = sp_sim_sys_get_root,
  .get_exe_path           = sp_sim_sys_get_exe_path,
  .get_cwd_path           = sp_sim_sys_get_cwd_path,
  .get_storage_path       = sp_sim_sys_get_storage_path,
  .get_config_path        = sp_sim_sys_get_config_path,
  .open                   = sp_sim_sys_open,
  .close                  = sp_sim_sys_close,
  .pipe                   = sp_sim_sys_pipe,
  .mkdir                  = sp_sim_sys_mkdir,
  .rmdir                  = sp_sim_sys_rmdir,
  .unlink                 = sp_sim_sys_unlink,
  .rename                 = sp_sim_sys_rename,
  .link                   = sp_sim_sys_link,
  .symlink                = sp_sim_sys_symlink,
  .get_path_metadata      = sp_sim_sys_get_path_metadata,
  .get_link_metadata      = sp_sim_sys_get_path_metadata,
  .get_file_metadata      = sp_sim_sys_get_file_metadata,
  .chmod                  = sp_sim_sys_chmod,
  .clock_gettime          = sp_sim_sys_clock_gettime,
  .nanosleep              = sp_sim_sys_nanosleep,
  .canonicalize_path      = sp_sim_sys_canonicalize_path,
  .fd_ready               = sp_sim_sys_fd_ready,
  .fd_wait                = sp_sim_sys_fd_wait,
  .fds_wait               = sp_sim_sys_fds_wait,
  .socket_open            = sp_sim_sys_socket_open,
  .socket_bind            = sp_sim_sys_socket_bind,
  .socket_listen          = sp_sim_sys_socket_listen,
  .socket_connect         = sp_sim_sys_socket_connect,
  .socket_error           = sp_sim_sys_socket_error,
  .socket_accept          = sp_sim_sys_socket_accept,
  .socket_close           = sp_sim_sys_socket_close,
  .socket_recv            = sp_sim_sys_socket_recv,
  .socket_send            = sp_sim_sys_socket_send,
  .socket_wait            = sp_sim_sys_socket_wait,
  .socket_set_nonblocking = sp_sim_sys_socket_set_nonblocking,
  .socket_reuse_addr      = sp_sim_sys_socket_reuse_addr,
  .socket_local_port      = sp_sim_sys_socket_local_port,
  .alloc                  = sp_sys_alloc_p,
  .free                   = sp_sys_free_p,
  .memcpy                 = sp_sys_memcpy_p,
  .memmove                = sp_sys_memmove_p,
  .memset                 = sp_sys_memset_p,
  .memcmp                 = sp_sys_memcmp_p,
  .assert                 = sp_sys_assert_p,
  .exit                   = sp_sys_exit_p,
  .env                    = sp_sys_env_p,
  .lseek                  = sp_sim_sys_lseek,
  .chdir                  = sp_sim_sys_chdir,
  .fs_it_open             = sp_sim_sys_fs_it_open,
  .fs_it_next             = sp_sim_sys_fs_it_next,
  .fs_it_close            = sp_sim_sys_fs_it_close,
};

void sp_sim_init(sp_sim_t* sim, sp_mem_t mem) {
  *sim = (sp_sim_t) {
    .mem = mem,
    .fds = sp_da_new(mem, sp_sim_fd_t),
    .events = sp_da_new(mem, sp_sim_event_t),
    .clock = { .tv_sec = 1 },
    .ids = 1,
  };
  sp_str_ht_init(mem, sim->nodes);

  sp_sim_inode_t* root = sp_mem_allocator_alloc_type(mem, sp_sim_inode_t);
  *root = (sp_sim_inode_t) {
    .id = sim->ids++,
    .kind = SP_FS_KIND_DIR,
    .nlink = 1,
    .mtime = sim->clock,
    .bytes = sp_da_new(mem, u8),
  };
  sp_ht_insert(sim->nodes, sp_str_lit("/"), root);
}

bool sp_sim_touch(sp_sim_t* sim, sp_str_t path) {
  SP_ASSERT(sp_sim_active == sim);

  c8 buf [SP_PATH_MAX];
  sp_sim_inode_t* node = sp_sim_find(sp_sim_norm(path.data, (u32)path.len, buf));
  if (!node || node->kind != SP_FS_KIND_FILE) {
    return false;
  }
  sp_sim_stamp(node);
  return true;
}

bool sp_sim_stealth_write(sp_sim_t* sim, sp_str_t path, sp_str_t bytes) {
  SP_ASSERT(sp_sim_active == sim);

  c8 buf [SP_PATH_MAX];
  sp_sim_inode_t* node = sp_sim_find(sp_sim_norm(path.data, (u32)path.len, buf));
  if (!node || node->kind != SP_FS_KIND_FILE || sp_da_size(node->bytes) != bytes.len) {
    return false;
  }
  sp_sys_memcpy(node->bytes, bytes.data, bytes.len);
  return true;
}

void sp_sim_install(sp_sim_t* sim) {
  SP_ASSERT(!sp_sim_active);
  sim->prev = sp_sys_set_vtable(&sp_sim_vtable);
  sp_sim_active = sim;
}

void sp_sim_remove(sp_sim_t* sim) {
  SP_ASSERT(sp_sim_active == sim);
  sp_sys_set_vtable(sim->prev);
  sim->prev = SP_NULLPTR;
  sp_sim_active = SP_NULLPTR;
}
