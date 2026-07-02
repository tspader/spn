#include "sp.h"
#include "sp/macro.h"
#include "git.h"

spn_err_t spn_git_clone(sp_str_t url, sp_str_t path) {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  sp_ps_output_t result = sp_ps_run(scratch.mem, (sp_ps_config_t) {
    .command = SP_LIT("git"),
    .args = {
      SP_LIT("clone"), SP_LIT("--quiet"),
      url,
      path
    },
  });
  sp_mem_end_scratch(scratch);

  if (result.status.exit_code) return SPN_ERROR;
  if (!sp_fs_is_dir(path)) return SPN_ERROR;

  return SPN_OK;
}

spn_err_t spn_git_pull(sp_str_t repo) {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  sp_ps_output_t result = sp_ps_run(scratch.mem, (sp_ps_config_t) {
    .command = SP_LIT("git"),
    .args = {
      SP_LIT("-C"), repo,
      SP_LIT("pull"), SP_LIT("--ff-only"), SP_LIT("--quiet")
    },
  });
  sp_mem_end_scratch(scratch);

  if (result.status.exit_code) return SPN_ERROR;
  return SPN_OK;
}

spn_err_t spn_git_fetch(sp_str_t repo) {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  sp_ps_output_t result = sp_ps_run(scratch.mem, (sp_ps_config_t) {
    .command = SP_LIT("git"),
    .args = {
      SP_LIT("-C"), repo,
      SP_LIT("fetch"), SP_LIT("--quiet")
    },
  });
  sp_mem_end_scratch(scratch);

  if (result.status.exit_code) return SPN_ERROR;
  return SPN_OK;
}

u32 spn_git_num_updates(sp_str_t repo, sp_str_t from, sp_str_t to) {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  sp_ps_output_t result = sp_ps_run(scratch.mem, (sp_ps_config_t) {
    .command = SP_LIT("git"),
    .args = {
      SP_LIT("-C"), repo,
      SP_LIT("rev-list"), sp_fmt(scratch.mem, "{}..{}", sp_fmt_str(from), sp_fmt_str(to)).value,
      SP_LIT("--count")
    },
  });
  SP_ASSERT_FMT(!result.status.exit_code, "Failed to get commit delta for {.cyan}", SP_FMT_STR(repo));

  u32 count = sp_parse_u32(sp_str_trim_right(result.out));
  sp_mem_end_scratch(scratch);
  return count;
}

spn_err_t spn_git_get_remote_url(sp_mem_t mem, sp_str_t repo, sp_str_t* url) {
  sp_ps_output_t result = sp_ps_run(mem, (sp_ps_config_t) {
    .command = SP_LIT("git"),
    .args = {
      SP_LIT("-C"), repo,
      SP_LIT("remote"), SP_LIT("get-url"), SP_LIT("origin")
    },
  });

  if (result.status.exit_code) return SPN_ERROR;
  *url = sp_str_trim_right(result.out);
  return SPN_OK;
}

spn_err_t spn_git_get_commit(sp_mem_t mem, sp_str_t repo, sp_str_t id, sp_str_t* sha) {
  sp_ps_output_t result = sp_ps_run(mem, (sp_ps_config_t) {
    .command = SP_LIT("git"),
    .args = {
      SP_LIT("-C"), repo,
      SP_LIT("rev-parse"),
      SP_LIT("--short=10"),
      id
    }
  });

  if (result.status.exit_code) return SPN_ERROR;
  *sha = sp_str_trim_right(result.out);
  return SPN_OK;
}

sp_str_t spn_git_get_commit_message(sp_mem_t mem, sp_str_t repo, sp_str_t id) {
  sp_ps_output_t result = sp_ps_run(mem, (sp_ps_config_t) {
    .command = SP_LIT("git"),
    .args = {
      SP_LIT("-C"), repo,
      SP_LIT("log"),
      SP_LIT("--format=%B"),
      SP_LIT("-n"),
      SP_LIT("1"),
      id
    }
  });
  SP_ASSERT_FMT(!result.status.exit_code, "Failed to log {.yellow}:{.cyan}", SP_FMT_STR(repo), SP_FMT_STR(id));

  return sp_str_trim_right(result.out);
}

spn_err_t spn_git_get_root(sp_mem_t mem, sp_str_t cwd, sp_str_t* root) {
  sp_ps_output_t result = sp_ps_run(mem, (sp_ps_config_t) {
    .command = SP_LIT("git"),
    .args = {
      SP_LIT("-C"), cwd,
      SP_LIT("rev-parse"), SP_LIT("--show-toplevel")
    },
  });

  if (result.status.exit_code) return SPN_ERROR;
  *root = sp_str_trim_right(result.out);
  return SPN_OK;
}

spn_err_t spn_git_checkout(sp_str_t repo, sp_str_t id) {
  if (sp_str_empty(id)) return SPN_ERROR;
  if (!sp_fs_exists(repo)) return SPN_ERROR;

  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  sp_ps_output_t result = sp_ps_run(scratch.mem, (sp_ps_config_t) {
    .command = SP_LIT("git"),
    .args = {
      SP_LIT("-C"), repo,
      SP_LIT("checkout"),
      SP_LIT("--quiet"),
      id
    }
  });
  sp_mem_end_scratch(scratch);

  if (result.status.exit_code) return SPN_ERROR;
  return SPN_OK;
}
