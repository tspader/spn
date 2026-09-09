#define SP_IMPLEMENTATION
#include "sp.h"
#include "sp/sp_cli.h"

// Pins the sha256 of every hosted artifact in a [[toolchain]] file. Each
// artifact line is `<host> = { url = "...", sha256 = "..." }` under a
// [toolchain.host] table; the tarball is fetched, verified against the
// vendor's signature, hashed, and the digest written back in place.

typedef struct {
  const c8* manifest;
  const c8* mirror;
} args_t;

#define ZIG_PUBLIC_KEY "RWSGOq2NVecA2UPNdBUZykf1CCb147pkmdtYxgb3Ti+JO/wCYvhbAb/U"

#define try(expr) do { sp_cli_result_t _e = (expr); if (_e) return _e; } while (0)

typedef struct {
  sp_str_t host;
  sp_str_t url;
  sp_str_t sha256;
} artifact_line_t;

static sp_str_t quoted_after(sp_str_t line, const c8* key) {
  s32 at = sp_str_find(line, sp_cstr_as_str(key));
  if (at < 0) {
    return sp_str_lit("");
  }
  sp_str_t rest = sp_str_suffix(line, line.len - (u32)at - (u32)sp_cstr_len(key));
  s32 end = sp_str_find_c8(rest, '"');
  return end < 0 ? sp_str_lit("") : sp_str_prefix(rest, (u32)end);
}

static bool parse_artifact(sp_str_t line, artifact_line_t* out) {
  sp_str_t trimmed = sp_str_trim(line);
  s32 eq = sp_str_find(trimmed, sp_str_lit(" = {"));
  if (eq < 0) {
    return false;
  }
  out->host = sp_str_prefix(trimmed, (u32)eq);
  out->url = quoted_after(trimmed, "url = \"");
  out->sha256 = quoted_after(trimmed, "sha256 = \"");
  return !sp_str_empty(out->url);
}

static sp_str_t with_sha256(sp_mem_t mem, sp_str_t line, sp_str_t hash) {
  s32 at = sp_str_find(line, sp_str_lit("sha256 = \""));
  sp_str_t head = sp_str_prefix(line, (u32)at + (u32)sp_cstr_len("sha256 = \""));
  sp_str_t rest = sp_str_suffix(line, line.len - head.len);
  s32 end = sp_str_find_c8(rest, '"');
  return sp_fmt(mem, "{}{}{}", sp_fmt_str(head), sp_fmt_str(hash), sp_fmt_str(sp_str_suffix(rest, rest.len - (u32)end))).value;
}

sp_cli_result_t fetch(sp_cli_t* cli, sp_mem_t mem, sp_str_t url, sp_str_t out) {
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
    return sp_cli_set_error(cli, sp_fmt(mem, "download failed: {.cyan}", sp_fmt_str(url)).value);
  }
  return SP_CLI_OK;
}

sp_cli_result_t pin(sp_cli_t* cli, sp_mem_t mem, sp_str_t work, sp_str_t mirror, artifact_line_t artifact, sp_str_t* hash) {
  sp_str_t filename = sp_fs_get_name(artifact.url);

  sp_str_t tarball = sp_fs_join_path(mem, work, filename);
  sp_str_t sig = sp_fmt(mem, "{}.minisig", sp_fmt_str(tarball)).value;

  sp_str_t src = sp_str_empty(mirror) ? artifact.url : sp_fmt(mem, "{}/{}", sp_fmt_str(mirror), sp_fmt_str(filename)).value;
  sp_str_t src_sig = sp_fmt(mem, "{}.minisig", sp_fmt_str(src)).value;

  sp_log("{.cyan}: fetching {}", sp_fmt_str(artifact.host), sp_fmt_str(filename));
  try(fetch(cli, mem, src, tarball));
  try(fetch(cli, mem, src_sig, sig));

  sp_ps_output_t verify = sp_ps_run_c(mem, (sp_ps_config_cstr_t) {
    .command = "minisign",
    .args = {
      "-V",
      "-m", sp_str_to_cstr(mem, tarball),
      "-P", ZIG_PUBLIC_KEY
    }
  });
  if (verify.status.exit_code) {
    return sp_cli_set_error(cli, sp_fmt(mem, "minisign verification failed for {} (is minisign installed?)", sp_fmt_str(filename)).value);
  }

  sp_str_t sigtext = sp_zero;
  sp_io_read_file(mem, sig, &sigtext);
  if (!sp_str_contains(sigtext, filename)) {
    return sp_cli_set_error(cli, sp_fmt(mem, "trusted comment does not reference {} (possible downgrade)", sp_fmt_str(filename)).value);
  }

  sp_ps_output_t digest = sp_ps_run(mem, (sp_ps_config_t) {
    .command = sp_str_lit("sha256sum"),
    .args = { tarball },
  });
  if (digest.status.exit_code) {
    return sp_cli_set_error(cli, sp_fmt(mem, "sha256sum failed for {}", sp_fmt_str(filename)).value);
  }
  *hash = sp_str_sub(sp_str_trim(digest.out), 0, 64);
  sp_log("{.cyan}: {.green}", sp_fmt_str(artifact.host), sp_fmt_str(*hash));
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
    return sp_cli_set_error(cli, sp_fmt(mem, "failed to read {.cyan}", sp_fmt_str(manifest_path)).value);
  }

  sp_da(sp_str_t) lines = sp_str_split_c8(mem, content, '\n');
  bool hosts = false;
  sp_da_for(lines, it) {
    sp_str_t trimmed = sp_str_trim(lines[it]);
    if (sp_str_starts_with(trimmed, sp_str_lit("name = \""))) {
      sp_log("{.yellow}", sp_fmt_str(quoted_after(trimmed, "name = \"")));
    }
    if (sp_str_starts_with(trimmed, sp_str_lit("["))) {
      hosts = sp_str_equal_cstr(trimmed, "[toolchain.host]");
      continue;
    }
    artifact_line_t artifact = sp_zero;
    if (!hosts || !parse_artifact(lines[it], &artifact)) {
      continue;
    }
    sp_str_t hash = sp_zero;
    try(pin(cli, mem, work, mirror, artifact, &hash));
    lines[it] = with_sha256(mem, lines[it], hash);
  }

  sp_str_t out = sp_str_join_n(mem, lines, (u32)sp_da_size(lines), sp_str_lit("\n"));
  if (sp_fs_create_file_str(manifest_path, out)) {
    return sp_cli_set_error(cli, sp_fmt(mem, "failed to write {.cyan}", sp_fmt_str(manifest_path)).value);
  }

  sp_log("wrote {.cyan}", sp_fmt_str(manifest_path));
  return SP_CLI_OK;
}

s32 main(s32 num_args, const c8** args) {
  args_t parsed = sp_zero;

  sp_cli_cmd_t root = {
    .name = "toolchain",
    .summary = "Verify toolchain tarballs (minisign) and pin their sha256 into a [[toolchain]] file",
    .opts = {
      {
        .name = "mirror",
        .kind = SP_CLI_OPT_CSTR,
        .summary = "fetch tarballs from this mirror base instead of the canonical url",
        .placeholder = "URL",
        .ptr = &parsed.mirror
      },
    },
    .args = {
      {
        .name = "manifest",
        .arity = SP_CLI_ARG_REQUIRED,
        .summary = "path to toolchains.toml",
        .ptr = &parsed.manifest
      },
    },
    .handler = run,
  };

  return sp_cli_main((sp_cli_desc_t) {
    .root = &root,
    .num_args = num_args,
    .args = args,
    .user_data = &parsed,
  });
}
