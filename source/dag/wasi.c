#include "dag/wasi.h"

#define SPN_WASI_OP_PATH_OPEN 0
#define SPN_WASI_OP_PATH_FILESTAT_GET 1
#define SPN_WASI_OP_FD_READDIR 2
#define SPN_WASI_OP_FD_CLOSE 3
#define SPN_WASI_OP_FD_RENUMBER 4

#define SPN_WASI_ERRNO_NOENT 44

#define SPN_WASI_OFLAGS_CREAT 0x1
#define SPN_WASI_OFLAGS_DIRECTORY 0x2
#define SPN_WASI_OFLAGS_TRUNC 0x8
#define SPN_WASI_RIGHTS_FD_WRITE 0x40

#define SPN_WASI_PREOPEN_BASE_FD 3

extern void (*spn_wasi_hook)(wasm_exec_env_t exec_env, s32 op, u32 fd,
                             const c8* path, u32 path_len, u16 error,
                             u32 new_fd, u32 oflags, u64 rights);

typedef struct {
  sp_str_t guest;
  sp_str_t host;
} spn_dag_wasi_dir_t;

struct spn_dag_wasi_t {
  sp_mem_t mem;
  sp_da(spn_dag_wasi_dir_t) mounts;
  sp_ht(u32, sp_str_t) dirs;
  sp_str_ht(u8) writes;
  sp_mem_t obs_mem;
  sp_da(spn_dag_obs_t)* obs;
};

static sp_str_t wasi_guest_path(spn_dag_wasi_t* w, sp_mem_t mem, u32 fd, const c8* path, u32 path_len) {
  sp_str_t* parent = sp_ht_getp(w->dirs, fd);
  if (!parent) {
    return sp_str_lit("");
  }
  return sp_fmt(mem, "{}/{}", sp_fmt_str(*parent), sp_fmt_str(sp_str(path, path_len))).value;
}

static sp_str_t wasi_host_path(spn_dag_wasi_t* w, sp_mem_t mem, sp_str_t guest) {
  sp_da_for(w->mounts, it) {
    sp_str_t prefix = w->mounts[it].guest;
    if (!sp_str_starts_with(guest, prefix)) {
      continue;
    }
    if (guest.len == prefix.len) {
      return w->mounts[it].host;
    }
    if (guest.data[prefix.len] != '/') {
      continue;
    }
    sp_str_t rest = sp_str_sub(guest, prefix.len, guest.len - prefix.len);
    sp_str_t joined = sp_fmt(mem, "{}{}", sp_fmt_str(w->mounts[it].host), sp_fmt_str(rest)).value;
    return sp_fs_normalize_path(mem, joined);
  }
  return sp_str_lit("");
}

static void wasi_track_dir(spn_dag_wasi_t* w, u32 fd, sp_str_t guest) {
  sp_ht_erase(w->dirs, fd);
  sp_ht_insert(w->dirs, fd, sp_str_copy(w->mem, guest));
}

static void wasi_push_obs(spn_dag_wasi_t* w, spn_dag_obs_kind_t kind, sp_str_t host) {
  if (!w->obs || sp_str_empty(host)) {
    return;
  }
  sp_da_push(*w->obs, ((spn_dag_obs_t) {
    .kind = kind,
    .path = sp_str_copy(w->obs_mem, host)
  }));
}

static void wasi_on_open(spn_dag_wasi_t* w, u32 fd, const c8* path, u32 path_len, u16 error, u32 new_fd, u32 oflags, u64 rights) {
  if (error && error != SPN_WASI_ERRNO_NOENT) {
    return;
  }

  sp_mem_arena_marker_t s = sp_mem_begin_scratch();
  sp_str_t guest = wasi_guest_path(w, s.mem, fd, path, path_len);
  sp_str_t host = wasi_host_path(w, s.mem, guest);
  if (sp_str_empty(host)) {
    sp_mem_end_scratch(s);
    return;
  }

  if (error == SPN_WASI_ERRNO_NOENT) {
    wasi_push_obs(w, SPN_DAG_OBS_ABSENT, host);
    sp_mem_end_scratch(s);
    return;
  }

  wasi_track_dir(w, new_fd, guest);

  bool write = (oflags & (SPN_WASI_OFLAGS_CREAT | SPN_WASI_OFLAGS_TRUNC)) || (rights & SPN_WASI_RIGHTS_FD_WRITE);
  if (write) {
    if (!sp_str_ht_get(w->writes, host)) {
      sp_str_ht_insert(w->writes, sp_str_copy(w->mem, host), (u8)true);
    }
  }
  else if (!(oflags & SPN_WASI_OFLAGS_DIRECTORY) && !sp_str_ht_get(w->writes, host)) {
    wasi_push_obs(w, SPN_DAG_OBS_FILE, host);
  }

  sp_mem_end_scratch(s);
}

static void wasi_on_stat(spn_dag_wasi_t* w, u32 fd, const c8* path, u32 path_len, u16 error) {
  if (error && error != SPN_WASI_ERRNO_NOENT) {
    return;
  }

  sp_mem_arena_marker_t s = sp_mem_begin_scratch();
  sp_str_t guest = wasi_guest_path(w, s.mem, fd, path, path_len);
  sp_str_t host = wasi_host_path(w, s.mem, guest);
  if (!sp_str_empty(host) && !sp_str_ht_get(w->writes, host)) {
    wasi_push_obs(w, error ? SPN_DAG_OBS_ABSENT : SPN_DAG_OBS_FILE, host);
  }
  sp_mem_end_scratch(s);
}

static void wasi_on_readdir(spn_dag_wasi_t* w, u32 fd, u16 error) {
  if (error) {
    return;
  }

  sp_str_t* guest = sp_ht_getp(w->dirs, fd);
  if (!guest) {
    return;
  }

  sp_mem_arena_marker_t s = sp_mem_begin_scratch();
  wasi_push_obs(w, SPN_DAG_OBS_ENUMERATION, wasi_host_path(w, s.mem, *guest));
  sp_mem_end_scratch(s);
}

static void wasi_on_renumber(spn_dag_wasi_t* w, u32 from, u32 to, u16 error) {
  if (error) {
    return;
  }

  sp_str_t* guest = sp_ht_getp(w->dirs, from);
  if (guest) {
    wasi_track_dir(w, to, *guest);
    sp_ht_erase(w->dirs, from);
  }
  else {
    sp_ht_erase(w->dirs, to);
  }
}

static void wasi_hook(wasm_exec_env_t exec_env, s32 op, u32 fd, const c8* path, u32 path_len, u16 error, u32 new_fd, u32 oflags, u64 rights) {
  wasm_module_inst_t instance = wasm_runtime_get_module_inst(exec_env);
  spn_dag_wasi_t* w = (spn_dag_wasi_t*)wasm_runtime_get_custom_data(instance);
  if (!w) {
    return;
  }

  switch (op) {
    case SPN_WASI_OP_PATH_OPEN: {
      wasi_on_open(w, fd, path, path_len, error, new_fd, oflags, rights);
      break;
    }
    case SPN_WASI_OP_PATH_FILESTAT_GET: {
      wasi_on_stat(w, fd, path, path_len, error);
      break;
    }
    case SPN_WASI_OP_FD_READDIR: {
      wasi_on_readdir(w, fd, error);
      break;
    }
    case SPN_WASI_OP_FD_CLOSE: {
      if (!error) {
        sp_ht_erase(w->dirs, fd);
      }
      break;
    }
    case SPN_WASI_OP_FD_RENUMBER: {
      wasi_on_renumber(w, fd, new_fd, error);
      break;
    }
  }
}

spn_err_t spn_dag_wasi_install(void) {
  spn_wasi_hook = wasi_hook;
  return SPN_OK;
}

spn_dag_wasi_t* spn_dag_wasi_new(sp_mem_t mem, const spn_dag_wasi_mount_t* mounts, u32 count) {
  spn_dag_wasi_t* w = sp_alloc_type(mem, spn_dag_wasi_t);
  w->mem = mem;
  w->obs_mem = (sp_mem_t) sp_zero;
  w->obs = SP_NULLPTR;
  sp_da_init(mem, w->mounts);
  sp_ht_init(mem, w->dirs);
  sp_str_ht_init(mem, w->writes);

  sp_for(it, count) {
    sp_da_push(w->mounts, ((spn_dag_wasi_dir_t) {
      .guest = sp_str_from_cstr(mem, mounts[it].guest),
      .host = sp_str_copy(mem, mounts[it].host)
    }));
    sp_ht_insert(w->dirs, SPN_WASI_PREOPEN_BASE_FD + it, w->mounts[it].guest);
  }

  return w;
}

void spn_dag_wasi_bind(spn_dag_wasi_t* w, wasm_module_inst_t instance) {
  wasm_runtime_set_custom_data(instance, w);
}

void spn_dag_wasi_begin(spn_dag_wasi_t* w, sp_mem_t mem, sp_da(spn_dag_obs_t)* obs) {
  w->obs_mem = mem;
  w->obs = obs;
}

void spn_dag_wasi_end(spn_dag_wasi_t* w) {
  w->obs = SP_NULLPTR;
}
