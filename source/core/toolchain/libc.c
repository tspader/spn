#include "toolchain/libc.h"

#include "ctx/types.h"
#include "error/error.h"
#include "hash/digest/digest.h"
#include "paths/paths.h"

typedef struct {
  spn_path_t include;
  spn_path_t sys_include;
  spn_path_t crt;
  spn_path_t msvc_lib;
  spn_path_t kernel32_lib;
} libc_t;

static libc_t libc_layout(sp_mem_t mem, const spn_sdk_t* sdk) {
  switch (sdk->kind) {
    case SPN_SDK_MACOS: {
      spn_path_t include = spn_path_join(mem, sdk->root, sp_str_lit("usr/include"));
      return (libc_t) { .include = include, .sys_include = include };
    }
    case SPN_SDK_MSVC: {
      return (libc_t) {
        .include = sdk->msvc.include.ucrt,
        .sys_include = sdk->msvc.include.vc,
        .crt = sdk->msvc.lib.ucrt,
        .msvc_lib = sdk->msvc.lib.vc,
        .kernel32_lib = sdk->msvc.lib.um,
      };
    }
    case SPN_SDK_NONE:
    case SPN_SDK_SYSROOT: {
      sp_unreachable_case();
    }
  }
  sp_unreachable_return(sp_zero_struct(libc_t));
}

static void render_key(sp_io_writer_t* io, const spn_path_roots_t* roots, sp_mem_t mem, const c8* key, spn_path_t path) {
  sp_str_t value = spn_path_empty(path) ? sp_str_lit("") : spn_path_str(roots, mem, path);
  sp_fmt_io(io, "{}={}\n", sp_fmt_cstr(key), sp_fmt_str(value));
}

void spn_sdk_render_libc(sp_io_writer_t* io, const spn_path_roots_t* roots, const spn_sdk_t* sdk) {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  libc_t libc = libc_layout(scratch.mem, sdk);
  render_key(io, roots, scratch.mem, "include_dir", libc.include);
  render_key(io, roots, scratch.mem, "sys_include_dir", libc.sys_include);
  render_key(io, roots, scratch.mem, "crt_dir", libc.crt);
  render_key(io, roots, scratch.mem, "msvc_lib_dir", libc.msvc_lib);
  render_key(io, roots, scratch.mem, "kernel32_lib_dir", libc.kernel32_lib);
  render_key(io, roots, scratch.mem, "gcc_dir", sp_zero_struct(spn_path_t));
  sp_mem_end_scratch(scratch);
}

static sp_str_t libc_content(sp_mem_t mem, const spn_path_roots_t* roots, const spn_sdk_t* sdk) {
  sp_io_dyn_mem_writer_t w;
  sp_io_dyn_mem_writer_init(mem, &w);
  spn_sdk_render_libc(&w.base, roots, sdk);
  return sp_io_dyn_mem_writer_take_str(&w);
}

spn_path_t spn_sdk_libc_path(sp_mem_t mem, const spn_path_roots_t* roots, const spn_sdk_t* sdk) {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch_for(mem);
  sp_str_t content = libc_content(scratch.mem, roots, sdk);
  u8 digest [32] = sp_zero;
  spn_digest(SPN_DIGEST_BLAKE3, content.data, content.len, digest);
  sp_str_t name = sp_fmt(scratch.mem, "libc/{}.txt", sp_fmt_str(spn_digest_hex(scratch.mem, digest))).value;
  spn_path_t path = spn_path_join(mem, spn_path_from_root(SPN_PATH_ROOT_CACHE), name);
  sp_mem_end_scratch(scratch);
  return path;
}

spn_err_t spn_sdk_libc_write(sp_mem_t mem, const spn_path_roots_t* roots, const spn_sdk_t* sdk) {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch_for(mem);
  sp_str_t path = spn_path_str(roots, scratch.mem, spn_sdk_libc_path(scratch.mem, roots, sdk));
  spn_err_t err = SPN_OK;
  if (!sp_fs_is_file(path)) {
    sp_fs_create_dir(sp_fs_parent_path(path));
    if (sp_fs_create_file_str(path, libc_content(scratch.mem, roots, sdk))) {
      err = spn_err_emit(&spn, (spn_err_union_t) { .kind = SPN_ERR_FS_WRITE, .fs = { .path = sp_str_copy(mem, path) } });
    }
  }
  sp_mem_end_scratch(scratch);
  return err;
}
