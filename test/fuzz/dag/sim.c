#include "fuzz.h"

static fz_sim_t* fz_sim;

#define fz_sys_todo(name) sp_fatal("fz_sim: {} is not implemented", sp_fmt_cstr(name))
#define fz_sys_unexpected(name) sp_fatal("fz_sim: unexpected syscall {}", sp_fmt_cstr(name))

static void fz_sys_init(void) {
  fz_sys_unexpected("init");
}

static s64 fz_sys_read(sp_sys_fd_t fd, void* buf, u64 count) {
  fz_sys_todo("read");
  return -1;
}

static s64 fz_sys_write(sp_sys_fd_t fd, const void* buf, u64 count) {
  if (fd == sp_sys_stdout || fd == sp_sys_stderr) {
    return sp_sys_vtable_platform.write(fd, buf, count);
  }
  fz_sys_todo("write");
  return -1;
}

static s64 fz_sys_pread(sp_sys_fd_t fd, void* buf, u64 count, u64 offset) {
  fz_sys_todo("pread");
  return -1;
}

static s64 fz_sys_pwrite(sp_sys_fd_t fd, const void* buf, u64 count, u64 offset) {
  fz_sys_todo("pwrite");
  return -1;
}

static sp_sys_fd_t fz_sys_get_root(s32 it) {
  if (it != 0) {
    fz_sys_unexpected("get_root");
  }
  return FZ_SIM_ROOT;
}

static s64 fz_sys_get_exe_path(c8* buf, u64 size) {
  fz_sys_unexpected("get_exe_path");
  return -1;
}

static s64 fz_sys_get_cwd_path(c8* buf, u64 size) {
  fz_sys_unexpected("get_cwd_path");
  return -1;
}

static s64 fz_sys_get_storage_path(c8* buf, u64 size) {
  fz_sys_unexpected("get_storage_path");
  return -1;
}

static s64 fz_sys_get_config_path(c8* buf, u64 size) {
  fz_sys_unexpected("get_config_path");
  return -1;
}

static sp_sys_fd_t fz_sys_open(sp_sys_fd_t fd, const c8* path, u32 len, s32 flags, s32 mode) {
  fz_sys_todo("open");
  return SP_SYS_INVALID_FD;
}

static s32 fz_sys_close(sp_sys_fd_t fd) {
  fz_sys_todo("close");
  return -1;
}

static s32 fz_sys_pipe(sp_sys_fd_t* read_end, sp_sys_fd_t* write_end) {
  fz_sys_unexpected("pipe");
  return -1;
}

static s32 fz_sys_mkdir(sp_sys_fd_t fd, const c8* path, u32 len, s32 mode) {
  fz_sys_todo("mkdir");
  return -1;
}

static s32 fz_sys_rmdir(sp_sys_fd_t fd, const c8* path, u32 len) {
  fz_sys_todo("rmdir");
  return -1;
}

static s32 fz_sys_unlink(sp_sys_fd_t fd, const c8* path, u32 len) {
  fz_sys_todo("unlink");
  return -1;
}

static s32 fz_sys_rename(sp_sys_fd_t from_fd, const c8* from, u32 from_len, sp_sys_fd_t to_fd, const c8* to, u32 to_len) {
  fz_sys_todo("rename");
  return -1;
}

static s32 fz_sys_link(sp_sys_fd_t from_fd, const c8* existing, u32 existing_len, sp_sys_fd_t to_fd, const c8* alias, u32 alias_len) {
  fz_sys_todo("link");
  return -1;
}

static s32 fz_sys_symlink(const c8* existing, u32 existing_len, sp_sys_fd_t to_fd, const c8* alias, u32 alias_len) {
  fz_sys_unexpected("symlink");
  return -1;
}

static s32 fz_sys_get_path_metadata(sp_sys_fd_t fd, const c8* path, u32 len, sp_sys_file_meta_t* st) {
  fz_sys_todo("get_path_metadata");
  return -1;
}

static s32 fz_sys_get_link_metadata(sp_sys_fd_t fd, const c8* path, u32 len, sp_sys_file_meta_t* st) {
  fz_sys_todo("get_link_metadata");
  return -1;
}

static s32 fz_sys_get_file_metadata(sp_sys_fd_t fd, sp_sys_file_meta_t* st) {
  fz_sys_todo("get_file_metadata");
  return -1;
}

static s32 fz_sys_chmod(sp_sys_fd_t fd, const c8* path, u32 len, const sp_sys_file_meta_t* st) {
  fz_sys_todo("chmod");
  return -1;
}

static s32 fz_sys_clock_gettime(s32 clockid, sp_sys_timespec_t* ts) {
  fz_sys_todo("clock_gettime");
  return -1;
}

static s32 fz_sys_nanosleep(const sp_sys_timespec_t* req, sp_sys_timespec_t* rem) {
  fz_sys_unexpected("nanosleep");
  return -1;
}

static s64 fz_sys_canonicalize_path(const c8* path, u32 len, c8* buf, u64 size) {
  fz_sys_unexpected("canonicalize_path");
  return -1;
}

static s32 fz_sys_fd_ready(sp_sys_fd_t fd, u8* ready) {
  fz_sys_unexpected("fd_ready");
  return -1;
}

static s32 fz_sys_fd_wait(sp_sys_fd_t fd) {
  fz_sys_unexpected("fd_wait");
  return -1;
}

static s32 fz_sys_fds_wait(const sp_sys_fd_t* fds, u8* ready, u64 nfds) {
  fz_sys_unexpected("fds_wait");
  return -1;
}

static s32 fz_sys_socket_open(sp_sys_socket_t* out) {
  fz_sys_unexpected("socket_open");
  return -1;
}

static s32 fz_sys_socket_bind(sp_sys_socket_t socket, sp_sys_ipv4_t addr) {
  fz_sys_unexpected("socket_bind");
  return -1;
}

static s32 fz_sys_socket_listen(sp_sys_socket_t socket, u32 backlog) {
  fz_sys_unexpected("socket_listen");
  return -1;
}

static s32 fz_sys_socket_connect(sp_sys_socket_t socket, sp_sys_ipv4_t addr) {
  fz_sys_unexpected("socket_connect");
  return -1;
}

static s32 fz_sys_socket_error(sp_sys_socket_t socket) {
  fz_sys_unexpected("socket_error");
  return -1;
}

static s32 fz_sys_socket_accept(sp_sys_socket_t listener, sp_sys_socket_t* out) {
  fz_sys_unexpected("socket_accept");
  return -1;
}

static s32 fz_sys_socket_close(sp_sys_socket_t socket) {
  fz_sys_unexpected("socket_close");
  return -1;
}

static s64 fz_sys_socket_recv(sp_sys_socket_t socket, void* buf, u64 count) {
  fz_sys_unexpected("socket_recv");
  return -1;
}

static s64 fz_sys_socket_send(sp_sys_socket_t socket, const void* buf, u64 count) {
  fz_sys_unexpected("socket_send");
  return -1;
}

static s32 fz_sys_socket_wait(sp_sys_socket_t socket, bool readable, u32 timeout_ms) {
  fz_sys_unexpected("socket_wait");
  return -1;
}

static s32 fz_sys_socket_set_nonblocking(sp_sys_socket_t socket) {
  fz_sys_unexpected("socket_set_nonblocking");
  return -1;
}

static s32 fz_sys_socket_reuse_addr(sp_sys_socket_t socket) {
  fz_sys_unexpected("socket_reuse_addr");
  return -1;
}

static s32 fz_sys_socket_local_port(sp_sys_socket_t socket, u16* out) {
  fz_sys_unexpected("socket_local_port");
  return -1;
}

static void* fz_sys_alloc(u64 size) {
  return sp_sys_vtable_platform.alloc(size);
}

static void fz_sys_free(void* ptr, u64 size) {
  sp_sys_vtable_platform.free(ptr, size);
}

static void* fz_sys_memcpy(void* dest, const void* src, u64 n) {
  return sp_sys_vtable_platform.memcpy(dest, src, n);
}

static void* fz_sys_memmove(void* dest, const void* src, u64 n) {
  return sp_sys_vtable_platform.memmove(dest, src, n);
}

static void* fz_sys_memset(void* dest, u8 fill, u64 n) {
  return sp_sys_vtable_platform.memset(dest, fill, n);
}

static s32 fz_sys_memcmp(const void* a, const void* b, u64 n) {
  return sp_sys_vtable_platform.memcmp(a, b, n);
}

static void fz_sys_assert(bool cond) {
  (sp_sys_vtable_platform.assert)(cond);
}

static void fz_sys_exit(s32 code) {
  sp_sys_vtable_platform.exit(code);
}

static void fz_sys_env(const c8** env, u32* len) {
  sp_sys_vtable_platform.env(env, len);
}

static s64 fz_sys_lseek(sp_sys_fd_t fd, s64 offset, s32 whence) {
  fz_sys_todo("lseek");
  return -1;
}

static s32 fz_sys_chdir(const c8* path, u32 len) {
  fz_sys_unexpected("chdir");
  return -1;
}

static s32 fz_sys_fs_it_open(sp_sys_fd_t fd, sp_sys_fs_it_t* it, const c8* path, u32 path_len, void* buf, u64 cap) {
  fz_sys_todo("fs_it_open");
  return -1;
}

static s32 fz_sys_fs_it_next(sp_sys_fs_it_t* it, sp_sys_fs_entry_t* out) {
  fz_sys_todo("fs_it_next");
  return -1;
}

static void fz_sys_fs_it_close(sp_sys_fs_it_t* it) {
  fz_sys_todo("fs_it_close");
}

static const sp_sys_vtable_t fz_sim_vtable = {
  .init                   = fz_sys_init,
  .read                   = fz_sys_read,
  .write                  = fz_sys_write,
  .pread                  = fz_sys_pread,
  .pwrite                 = fz_sys_pwrite,
  .get_root               = fz_sys_get_root,
  .get_exe_path           = fz_sys_get_exe_path,
  .get_cwd_path           = fz_sys_get_cwd_path,
  .get_storage_path       = fz_sys_get_storage_path,
  .get_config_path        = fz_sys_get_config_path,
  .open                   = fz_sys_open,
  .close                  = fz_sys_close,
  .pipe                   = fz_sys_pipe,
  .mkdir                  = fz_sys_mkdir,
  .rmdir                  = fz_sys_rmdir,
  .unlink                 = fz_sys_unlink,
  .rename                 = fz_sys_rename,
  .link                   = fz_sys_link,
  .symlink                = fz_sys_symlink,
  .get_path_metadata      = fz_sys_get_path_metadata,
  .get_link_metadata      = fz_sys_get_link_metadata,
  .get_file_metadata      = fz_sys_get_file_metadata,
  .chmod                  = fz_sys_chmod,
  .clock_gettime          = fz_sys_clock_gettime,
  .nanosleep              = fz_sys_nanosleep,
  .canonicalize_path      = fz_sys_canonicalize_path,
  .fd_ready               = fz_sys_fd_ready,
  .fd_wait                = fz_sys_fd_wait,
  .fds_wait               = fz_sys_fds_wait,
  .socket_open            = fz_sys_socket_open,
  .socket_bind            = fz_sys_socket_bind,
  .socket_listen          = fz_sys_socket_listen,
  .socket_connect         = fz_sys_socket_connect,
  .socket_error           = fz_sys_socket_error,
  .socket_accept          = fz_sys_socket_accept,
  .socket_close           = fz_sys_socket_close,
  .socket_recv            = fz_sys_socket_recv,
  .socket_send            = fz_sys_socket_send,
  .socket_wait            = fz_sys_socket_wait,
  .socket_set_nonblocking = fz_sys_socket_set_nonblocking,
  .socket_reuse_addr      = fz_sys_socket_reuse_addr,
  .socket_local_port      = fz_sys_socket_local_port,
  .alloc                  = fz_sys_alloc,
  .free                   = fz_sys_free,
  .memcpy                 = fz_sys_memcpy,
  .memmove                = fz_sys_memmove,
  .memset                 = fz_sys_memset,
  .memcmp                 = fz_sys_memcmp,
  .assert                 = fz_sys_assert,
  .exit                   = fz_sys_exit,
  .env                    = fz_sys_env,
  .lseek                  = fz_sys_lseek,
  .chdir                  = fz_sys_chdir,
  .fs_it_open             = fz_sys_fs_it_open,
  .fs_it_next             = fz_sys_fs_it_next,
  .fs_it_close            = fz_sys_fs_it_close,
};

void fz_sim_init(fz_sim_t* sim, sp_mem_t mem) {
  sim->mem = mem;
  sp_str_ht_init(mem, sim->nodes);
  sp_da_init(mem, sim->fds);
  sim->clock = (sp_sys_timespec_t) { .tv_sec = 1, .tv_nsec = 0 };
  sim->inodes = 1;
  sim->syscalls = 0;
  sim->prev = SP_NULLPTR;
}

void fz_sim_install(fz_sim_t* sim) {
  SP_ASSERT(!fz_sim);
  sim->prev = sp_sys_set_vtable(&fz_sim_vtable);
  fz_sim = sim;
}

void fz_sim_remove(fz_sim_t* sim) {
  SP_ASSERT(fz_sim == sim);
  sp_sys_set_vtable(sim->prev);
  sim->prev = SP_NULLPTR;
  fz_sim = SP_NULLPTR;
}
