#define SP_IMPLEMENTATION
#include "sp.h"
#include "sp/sp_cli.h"
#include "yyjson.h"

typedef struct {
  const c8* manifest;
  const c8* mirror;
} args_t;

#define ZIG_PUBLIC_KEY "RWSGOq2NVecA2UPNdBUZykf1CCb147pkmdtYxgb3Ti+JO/wCYvhbAb/U"

#define try(expr) do { sp_cli_result_t _e = (expr); if (_e) return _e; } while (0)

sp_cli_result_t fetch(sp_mem_t mem, sp_str_t url, sp_str_t out) {
  if (sp_fs_exists(out)) return SP_CLI_OK;

  sp_ps_output_t r = sp_ps_run_c(mem, (sp_ps_config_cstr_t) {
    .command = "curl",
    .args = {
      "-fSL", "--retry", "3",
      "-o", sp_str_to_cstr(mem, out),
      sp_str_to_cstr(mem, url)
    }
  });
  if (r.status.exit_code) {
    sp_cli_log_error("download failed: {.cyan}", sp_fmt_str(url));
    return SP_CLI_ERR;
  }
  return SP_CLI_OK;
}

sp_cli_result_t pin(sp_mem_t mem, sp_str_t work, sp_str_t mirror, yyjson_mut_doc* doc, sp_str_t hostkey, yyjson_mut_val* dist) {
  sp_str_t url = sp_str_view(yyjson_mut_get_str(yyjson_mut_obj_get(dist, "url")));
  sp_str_t filename = sp_fs_get_name(url);

  sp_str_t tarball = sp_fs_join_path(mem, work, filename);
  sp_str_t sig = sp_fmt(mem, "{}.minisig", sp_fmt_str(tarball)).value;

  sp_str_t src = sp_str_empty(mirror) ? url : sp_fmt(mem, "{}/{}", sp_fmt_str(mirror), sp_fmt_str(filename)).value;
  sp_str_t src_sig = sp_fmt(mem, "{}.minisig", sp_fmt_str(src)).value;

  sp_log("{.cyan}: fetching {}", sp_fmt_str(hostkey), sp_fmt_str(filename));
  try(fetch(mem, src, tarball));
  try(fetch(mem, src_sig, sig));


  sp_ps_output_t verify = sp_ps_run_c(mem, (sp_ps_config_cstr_t) {
    .command = "minisign",
    .args = {
      "-V",
      "-m", sp_str_to_cstr(mem, tarball),
      "-P", ZIG_PUBLIC_KEY
    }
  });
  if (verify.status.exit_code) {
    sp_cli_log_error("minisign verification failed for {} (is minisign installed?)", sp_fmt_str(filename));
    return SP_CLI_ERR;
  }

  sp_str_t sigtext = sp_zero;
  sp_io_read_file(mem, sig, &sigtext);
  if (!sp_str_contains(sigtext, filename)) {
    sp_cli_log_error("trusted comment does not reference {} (possible downgrade)", sp_fmt_str(filename));
    return SP_CLI_ERR;
  }

  sp_ps_output_t digest = sp_ps_run(mem, (sp_ps_config_t) {
    .command = sp_str_lit("sha256sum"),
    .args = { tarball },
  });
  if (digest.status.exit_code) {
    sp_cli_log_error("sha256sum failed for {}", sp_fmt_str(filename));
    return SP_CLI_ERR;
  }
  sp_str_t hash = sp_str_sub(sp_str_trim(digest.out), 0, 64);
  const c8* hash_cstr = sp_str_to_cstr(mem, hash);

  yyjson_mut_val* sha = yyjson_mut_obj_get(dist, "sha256");
  if (sha) {
    yyjson_mut_set_strn(sha, hash_cstr, 64);
  } else {
    yyjson_mut_obj_add_strcpy(doc, dist, "sha256", hash_cstr);
  }

  sp_log("{.cyan}: {.fg green}", sp_fmt_str(hostkey), sp_fmt_str(hash));
  return SP_CLI_OK;
}

static sp_cli_result_t run(sp_cli_t* cli) {
  args_t* a = (args_t*)cli->user_data;
  sp_mem_t mem = sp_mem_os_new();

  sp_str_t manifest_path = sp_cstr_as_str(a->manifest);
  sp_str_t mirror = a->mirror ? sp_cstr_as_str(a->mirror) : sp_str_lit("");
  sp_str_t work = sp_str_lit(".cache/toolchain");
  sp_fs_create_dir(work);

  sp_str_t content = sp_zero;
  if (sp_io_read_file(mem, manifest_path, &content) != SP_OK) {
    sp_cli_log_error("failed to read {.cyan}", sp_fmt_str(manifest_path));
    return SP_CLI_ERR;
  }

  yyjson_doc* idoc = yyjson_read(content.data, content.len, 0);
  if (!idoc) {
    sp_cli_log_error("invalid JSON in {.cyan}", sp_fmt_str(manifest_path));
    return SP_CLI_ERR;
  }
  yyjson_mut_doc* doc = yyjson_doc_mut_copy(idoc, SP_NULLPTR);
  yyjson_mut_val* root = yyjson_mut_doc_get_root(doc);

  yyjson_mut_val* toolchains = yyjson_mut_obj_get(root, "toolchain");
  size_t ti, tn;
  yyjson_mut_val* tc;
  yyjson_mut_arr_foreach(toolchains, ti, tn, tc) {
    sp_str_t name = sp_str_view(yyjson_mut_get_str(yyjson_mut_obj_get(tc, "name")));
    sp_str_t version = sp_str_view(yyjson_mut_get_str(yyjson_mut_obj_get(tc, "version")));
    sp_log("{.yellow} {}", sp_fmt_str(name), sp_fmt_str(version));

    yyjson_mut_val* host = yyjson_mut_obj_get(tc, "host");
    size_t hi, hn;
    yyjson_mut_val* key;
    yyjson_mut_val* dist;
    yyjson_mut_obj_foreach(host, hi, hn, key, dist) {
      sp_str_t hostkey = sp_str_view(yyjson_mut_get_str(key));
      try(pin(mem, work, mirror, doc, hostkey, dist));
    }
  }

  if (!yyjson_mut_write_file(a->manifest, doc, YYJSON_WRITE_PRETTY_TWO_SPACES, SP_NULLPTR, SP_NULLPTR)) {
    sp_cli_log_error("failed to write {.cyan}", sp_fmt_str(manifest_path));
    return SP_CLI_ERR;
  }

  sp_log("wrote {.cyan}", sp_fmt_str(manifest_path));
  return SP_CLI_OK;
}

s32 main(s32 num_args, const c8** args) {
  args_t parsed = sp_zero;

  sp_cli_cmd_t root = {
    .name = "toolchain",
    .summary = "Verify toolchain tarballs (minisign) and pin their sha256 into a toolchains.json manifest",
    .opts = {
      {
        .name = "mirror",
        .kind = SP_CLI_OPT_STRING,
        .summary = "fetch tarballs from this mirror base instead of the canonical url",
        .placeholder = "URL",
        .ptr = &parsed.mirror
      },
    },
    .args = {
      {
        .name = "manifest",
        .kind = SP_CLI_ARG_REQUIRED,
        .summary = "path to toolchains.json",
        .ptr = &parsed.manifest
      },
    },
    .handler = run,
  };

  return sp_cli_main(&root, num_args, args, &parsed);
}
