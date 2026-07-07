#include "toolchain/provision.h"

#include "toolchain/sha256.h"

spn_err_t spn_fetch_curl(sp_str_t url, sp_str_t dest, void* user_data) {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  sp_ps_output_t result = sp_ps_run(scratch.mem, (sp_ps_config_t) {
    .command = sp_str_lit("curl"),
    .args = {
      sp_str_lit("-fsSL"),
      sp_str_lit("-o"), dest,
      url,
    },
    .io = {
      .err = { .mode = SP_PS_IO_MODE_NULL },
    },
  });
  sp_mem_end_scratch(scratch);
  return result.status.exit_code ? SPN_ERROR : SPN_OK;
}

sp_str_t spn_toolchain_store_path(spn_toolchain_store_t* store, spn_artifact_t artifact) {
  return sp_fs_join_path(store->mem, store->dir, artifact.sha256);
}

sp_str_t spn_artifact_resolve_url(sp_mem_t mem, spn_artifact_t artifact, sp_str_t mirror) {
  if (sp_str_empty(mirror)) return artifact.url;

  sp_str_t name = sp_fs_get_name(artifact.url);
  if (sp_str_empty(name)) return artifact.url;

  while (mirror.len && mirror.data[mirror.len - 1] == '/') {
    mirror.len--;
  }

  return sp_fmt(mem, "{}/{}", sp_fmt_str(mirror), sp_fmt_str(name)).value;
}

SP_PRIVATE spn_err_t spn_toolchain_provision_fetch(spn_toolchain_store_t* store, spn_artifact_t artifact, sp_str_t dest, sp_str_t* url) {
  if (!sp_str_empty(store->mirror)) {
    *url = spn_artifact_resolve_url(store->mem, artifact, store->mirror);
    if (!sp_str_equal(*url, artifact.url)) {
      if (store->fetch(*url, dest, store->fetch_user_data) == SPN_OK) return SPN_OK;
    }
  }

  *url = artifact.url;
  return store->fetch(*url, dest, store->fetch_user_data);
}

SP_PRIVATE u64 spn_toolchain_provision_stamp(void) {
  static sp_atomic_s32_t sequence;
  sp_tm_epoch_t now = sp_tm_now_epoch();
  u64 stamp = ((u64)now.s << 20) ^ (u64)now.ns;
  return stamp ^ ((u64)(u32)sp_atomic_s32_add(&sequence, 1) << 48);
}

spn_err_union_t spn_toolchain_provision(spn_toolchain_store_t* store, spn_toolchain_t* toolchain, sp_str_t* root) {
  *root = sp_str_lit("");
  if (sp_opt_is_null(toolchain->artifact)) return spn_result(SPN_OK);

  spn_artifact_t artifact = sp_opt_get(toolchain->artifact);
  if (sp_str_empty(artifact.sha256)) {
    return (spn_err_union_t) {
      .kind = SPN_ERR_TOOLCHAIN_NO_SHA,
      .artifact = {
        .name = toolchain->name,
        .url = artifact.url,
      },
    };
  }

  sp_str_t dest = spn_toolchain_store_path(store, artifact);
  *root = dest;

  if (sp_fs_is_dir(dest)) return spn_result(SPN_OK);
  sp_fs_create_dir(store->dir);

  u64 stamp = spn_toolchain_provision_stamp();
  sp_str_t tarball = sp_fmt(store->mem, "{}.{}.download", sp_fmt_str(dest), sp_fmt_uint(stamp)).value;
  sp_str_t work = sp_fmt(store->mem, "{}.{}.tmp", sp_fmt_str(dest), sp_fmt_uint(stamp)).value;

  sp_str_t url = sp_zero;
  if (spn_toolchain_provision_fetch(store, artifact, tarball, &url)) {
    sp_fs_remove_file(tarball);
    return (spn_err_union_t) {
      .kind = SPN_ERR_TOOLCHAIN_FETCH,
      .artifact = {
        .name = toolchain->name,
        .url = url,
      },
    };
  }

  sp_str_t actual = sp_zero;
  if (spn_sha256_file(store->mem, tarball, &actual) || !sp_str_equal(actual, artifact.sha256)) {
    sp_fs_remove_file(tarball);
    return (spn_err_union_t) {
      .kind = SPN_ERR_TOOLCHAIN_SHA,
      .artifact = {
        .name = toolchain->name,
        .url = url,
        .expected = artifact.sha256,
        .actual = actual,
      },
    };
  }

  sp_fs_create_dir(work);

  sp_ps_output_t extract = sp_ps_run(store->mem, (sp_ps_config_t) {
    .command = sp_str_lit("tar"),
    .args = {
      sp_str_lit("xf"), tarball,
      sp_str_lit("--strip-components=1"),
      sp_str_lit("-C"), work,
    },
    .io = {
      .err = { .mode = SP_PS_IO_MODE_NULL },
    },
  });

  sp_fs_remove_file(tarball);

  bool empty = sp_da_empty(sp_fs_collect(store->mem, work));
  if (extract.status.exit_code || empty) {
    sp_fs_remove_dir(work);
    return (spn_err_union_t) {
      .kind = SPN_ERR_TOOLCHAIN_EXTRACT,
      .artifact = {
        .name = toolchain->name,
        .url = url,
      },
    };
  }

  sp_sys_fd_t cwd = sp_sys_get_root(0);
  if (sp_sys_rename_s(cwd, work, cwd, dest)) {
    sp_fs_remove_dir(work);
    if (!sp_fs_is_dir(dest)) {
      return (spn_err_union_t) {
        .kind = SPN_ERR_TOOLCHAIN_EXTRACT,
        .artifact = {
          .name = toolchain->name,
          .url = url,
        },
      };
    }
  }

  return spn_result(SPN_OK);
}
