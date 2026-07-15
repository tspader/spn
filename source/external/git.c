#include "sp.h"
#include "sp/macro.h"
#include "git.h"

spn_err_t spn_git_clone(sp_str_t url, sp_str_t path) {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  sp_ps_output_t result = sp_ps_run(scratch.mem, (sp_ps_config_t) {
    .command = SP_LIT("git"),
    .args = {
      SP_LIT("clone"), SP_LIT("--quiet"),
      // Index content must be byte-identical to what was committed no matter
      // what the machine's autocrlf is
      SP_LIT("-c"), SP_LIT("core.autocrlf=false"),
      url,
      path
    },
  });
  sp_mem_end_scratch(scratch);

  if (result.status.exit_code) return SPN_ERROR;
  if (!sp_fs_is_dir(path)) return SPN_ERROR;

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

spn_err_t spn_git_get_commit_full(sp_mem_t mem, sp_str_t repo, sp_str_t id, sp_str_t* sha) {
  sp_ps_output_t result = sp_ps_run(mem, (sp_ps_config_t) {
    .command = SP_LIT("git"),
    .args = {
      SP_LIT("-C"), repo,
      SP_LIT("rev-parse"),
      id
    }
  });

  if (result.status.exit_code) return SPN_ERROR;
  *sha = sp_str_trim_right(result.out);
  return SPN_OK;
}

spn_err_t spn_git_default_branch(sp_mem_t mem, sp_str_t repo, sp_str_t* branch) {
  sp_ps_output_t result = sp_ps_run(mem, (sp_ps_config_t) {
    .command = SP_LIT("git"),
    .args = {
      SP_LIT("-C"), repo,
      SP_LIT("symbolic-ref"), SP_LIT("--short"),
      SP_LIT("refs/remotes/origin/HEAD")
    },
  });

  if (result.status.exit_code) return SPN_ERROR;
  sp_str_t name = sp_str_trim_right(result.out);
  *branch = sp_str_strip_left(name, sp_str_lit("origin/"));
  if (sp_str_empty(*branch)) return SPN_ERROR;
  return SPN_OK;
}

bool spn_git_is_repo_root(sp_str_t repo) {
  if (sp_str_empty(repo) || !sp_fs_is_dir(repo)) return false;

  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  sp_str_t root = sp_zero;
  bool ok = !spn_git_get_root(scratch.mem, repo, &root) &&
    sp_str_equal(sp_fs_canonicalize_path(scratch.mem, root), sp_fs_canonicalize_path(scratch.mem, repo));
  sp_mem_end_scratch(scratch);
  return ok;
}

spn_err_t spn_git_checkout_branch(sp_str_t repo, sp_str_t branch) {
  if (!spn_git_is_repo_root(repo)) return SPN_ERROR;
  if (sp_str_empty(branch)) return SPN_ERROR;

  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  sp_ps_output_t result = sp_ps_run(scratch.mem, (sp_ps_config_t) {
    .command = SP_LIT("git"),
    .args = {
      SP_LIT("-C"), repo,
      SP_LIT("checkout"), SP_LIT("--quiet"), SP_LIT("--force"),
      SP_LIT("-B"), branch,
      sp_fmt(scratch.mem, "origin/{}", sp_fmt_str(branch)).value
    },
  });
  sp_mem_end_scratch(scratch);

  if (result.status.exit_code) return SPN_ERROR;
  return SPN_OK;
}

spn_err_t spn_git_current_branch(sp_mem_t mem, sp_str_t repo, sp_str_t* branch) {
  sp_ps_output_t result = sp_ps_run(mem, (sp_ps_config_t) {
    .command = SP_LIT("git"),
    .args = {
      SP_LIT("-C"), repo,
      SP_LIT("symbolic-ref"), SP_LIT("--short"), SP_LIT("HEAD")
    },
  });

  if (result.status.exit_code) return SPN_ERROR;
  *branch = sp_str_trim_right(result.out);
  return SPN_OK;
}

bool spn_git_has_remote_branches(sp_str_t repo) {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  sp_ps_output_t result = sp_ps_run(scratch.mem, (sp_ps_config_t) {
    .command = SP_LIT("git"),
    .args = {
      SP_LIT("-C"), repo,
      SP_LIT("branch"), SP_LIT("-r")
    },
  });
  bool any = !result.status.exit_code && !sp_str_empty(sp_str_trim_right(result.out));
  sp_mem_end_scratch(scratch);
  return any;
}

spn_err_t spn_git_clean(sp_str_t repo) {
  if (!spn_git_is_repo_root(repo)) return SPN_ERROR;

  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  sp_ps_output_t result = sp_ps_run(scratch.mem, (sp_ps_config_t) {
    .command = SP_LIT("git"),
    .args = {
      SP_LIT("-C"), repo,
      SP_LIT("clean"), SP_LIT("-fdq")
    },
  });
  sp_mem_end_scratch(scratch);

  if (result.status.exit_code) return SPN_ERROR;
  return SPN_OK;
}

spn_err_t spn_git_add(sp_str_t repo, sp_str_t path) {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  sp_ps_output_t result = sp_ps_run(scratch.mem, (sp_ps_config_t) {
    .command = SP_LIT("git"),
    .args = {
      SP_LIT("-C"), repo,
      SP_LIT("add"),
      path
    },
  });
  sp_mem_end_scratch(scratch);

  if (result.status.exit_code) return SPN_ERROR;
  return SPN_OK;
}

spn_err_t spn_git_commit(sp_str_t repo, sp_str_t message) {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  sp_ps_output_t result = sp_ps_run(scratch.mem, (sp_ps_config_t) {
    .command = SP_LIT("git"),
    .args = {
      SP_LIT("-C"), repo,
      SP_LIT("-c"), SP_LIT("user.name=spn"),
      SP_LIT("-c"), SP_LIT("user.email=spn"),
      SP_LIT("-c"), SP_LIT("commit.gpgsign=false"),
      SP_LIT("commit"), SP_LIT("--quiet"), SP_LIT("--no-verify"),
      SP_LIT("-m"), message
    },
  });
  sp_mem_end_scratch(scratch);

  if (result.status.exit_code) return SPN_ERROR;
  return SPN_OK;
}

spn_err_t spn_git_push(sp_mem_t mem, sp_str_t repo, sp_str_t url, sp_str_t refspec, sp_str_t* output) {
  sp_ps_output_t result = sp_ps_run(mem, (sp_ps_config_t) {
    .command = SP_LIT("git"),
    .args = {
      SP_LIT("-C"), repo,
      SP_LIT("push"), SP_LIT("--quiet"),
      url,
      refspec
    },
  });

  if (result.status.exit_code) {
    if (output) {
      *output = sp_str_trim_right(sp_str_empty(result.err) ? result.out : result.err);
    }
    return SPN_ERROR;
  }
  return SPN_OK;
}

bool spn_git_is_dirty(sp_str_t repo, sp_str_t dir) {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  sp_ps_output_t result = sp_ps_run(scratch.mem, (sp_ps_config_t) {
    .command = SP_LIT("git"),
    .args = {
      SP_LIT("-C"), repo,
      SP_LIT("status"), SP_LIT("--porcelain"),
      SP_LIT("--"), dir
    },
  });
  bool dirty = result.status.exit_code || !sp_str_empty(sp_str_trim_right(result.out));
  sp_mem_end_scratch(scratch);
  return dirty;
}

bool spn_git_rev_on_remote(sp_str_t repo, sp_str_t rev) {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  sp_ps_output_t result = sp_ps_run(scratch.mem, (sp_ps_config_t) {
    .command = SP_LIT("git"),
    .args = {
      SP_LIT("-C"), repo,
      SP_LIT("branch"), SP_LIT("-r"), SP_LIT("--contains"),
      rev
    },
  });
  bool contained = !result.status.exit_code && !sp_str_empty(sp_str_trim_right(result.out));
  sp_mem_end_scratch(scratch);
  return contained;
}

spn_err_t spn_git_checkout(sp_str_t repo, sp_str_t id) {
  if (sp_str_empty(id)) return SPN_ERROR;
  if (!spn_git_is_repo_root(repo)) return SPN_ERROR;

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
