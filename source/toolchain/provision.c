#include "toolchain/provision.h"

#include "toolchain/sha256.h"

spn_err_t spn_fetch_curl(sp_str_t url, sp_str_t dest, void* user_data) {
  sp_mem_t mem = sp_mem_os_new();
  sp_ps_output_t result = sp_ps_run(mem, (sp_ps_config_t) {
    .command = sp_str_lit("curl"),
    .args = {
      sp_str_lit("-fSL"),
      sp_str_lit("-o"), dest,
      url,
    }
  });
  return result.status.exit_code ? SPN_ERROR : SPN_OK;
}

sp_str_t spn_toolchain_store_path(spn_toolchain_store_t* store, spn_artifact_t artifact) {
  return sp_fs_join_path(store->mem, store->dir, artifact.sha256);
}

sp_str_t spn_artifact_resolve_url(sp_mem_t mem, spn_artifact_t artifact, sp_str_t mirror) {
  if (sp_str_empty(mirror)) return artifact.url;

  while (mirror.len && mirror.data[mirror.len - 1] == '/') {
    mirror.len--;
  }

  sp_str_t name = sp_fs_get_name(artifact.url);
  return sp_fmt(mem, "{}/{}", sp_fmt_str(mirror), sp_fmt_str(name)).value;
}

SP_PRIVATE spn_err_t spn_toolchain_provision_fetch(spn_toolchain_store_t* store, spn_artifact_t artifact, sp_str_t dest, sp_str_t* url) {
  if (!sp_str_empty(store->mirror)) {
    *url = spn_artifact_resolve_url(store->mem, artifact, store->mirror);
    if (store->fetch(*url, dest, store->fetch_user_data) == SPN_OK) return SPN_OK;
  }

  *url = artifact.url;
  return store->fetch(*url, dest, store->fetch_user_data);
}

spn_toolchain_provision_err_t spn_toolchain_provision(spn_toolchain_store_t* store, spn_toolchain_t* toolchain, sp_str_t* root) {
  spn_toolchain_provision_err_t ok = { .status = SPN_TOOLCHAIN_PROVISION_OK };

  *root = sp_str_lit("");
  if (sp_opt_is_null(toolchain->artifact)) return ok;

  spn_artifact_t artifact = sp_opt_get(toolchain->artifact);
  sp_str_t dest = spn_toolchain_store_path(store, artifact);
  *root = dest;

  if (sp_fs_is_dir(dest)) return ok;

  sp_str_t tarball = sp_fmt(store->mem, "{}.download", sp_fmt_str(dest)).value;
  sp_str_t work = sp_fmt(store->mem, "{}.tmp", sp_fmt_str(dest)).value;

  sp_str_t url = sp_zero;
  if (spn_toolchain_provision_fetch(store, artifact, tarball, &url)) {
    sp_fs_remove_file(tarball);
    return (spn_toolchain_provision_err_t) {
      .status = SPN_TOOLCHAIN_PROVISION_ERR_FETCH,
      .url = url,
    };
  }

  sp_str_t actual = sp_zero;
  if (spn_sha256_file(store->mem, tarball, &actual) || !sp_str_equal(actual, artifact.sha256)) {
    sp_fs_remove_file(tarball);
    return (spn_toolchain_provision_err_t) {
      .status = SPN_TOOLCHAIN_PROVISION_ERR_SHA,
      .url = url,
      .expected = artifact.sha256,
      .actual = actual,
    };
  }

  sp_fs_remove_dir(work);
  sp_fs_create_dir(work);

  sp_ps_output_t extract = sp_ps_run(store->mem, (sp_ps_config_t) {
    .command = sp_str_lit("tar"),
    .args = {
      sp_str_lit("xf"), tarball,
      sp_str_lit("--strip-components=1"),
      sp_str_lit("-C"), work,
    }
  });

  sp_fs_remove_file(tarball);

  if (extract.status.exit_code) {
    sp_fs_remove_dir(work);
    return (spn_toolchain_provision_err_t) {
      .status = SPN_TOOLCHAIN_PROVISION_ERR_EXTRACT,
      .url = url,
    };
  }

  if (sp_sys_rename_s(SP_AT_FDCWD, work, SP_AT_FDCWD, dest)) {
    sp_fs_remove_dir(work);
    if (!sp_fs_is_dir(dest)) {
      return (spn_toolchain_provision_err_t) {
        .status = SPN_TOOLCHAIN_PROVISION_ERR_EXTRACT,
        .url = url,
      };
    }
  }

  return ok;
}
