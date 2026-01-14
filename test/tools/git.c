#include "test.h"

void git_repo_run(sp_str_t repo, sp_str_t a, sp_str_t b, sp_str_t c, sp_str_t d, sp_str_t e) {
  sp_ps_output_t output = sp_ps_run((sp_ps_config_t) {
    .command = sp_str_lit("git"),
    .args = {
      sp_str_lit("-C"), repo,
      a, b, c, d, e,
    },
  });

  SP_ASSERT(output.status.exit_code == 0);
}

void git_repo_copy_dir(sp_str_t source, sp_str_t repo) {
  sp_da(sp_os_dir_ent_t) entries = sp_fs_collect_recursive(source);
  sp_dyn_array_for(entries, it) {
    sp_os_dir_ent_t* entry = &entries[it];
    if (sp_fs_is_dir(entry->file_path)) {
      continue;
    }

    sp_str_t relative = sp_str_strip_left(entry->file_path, source);
    relative = sp_str_strip_left(relative, sp_str_lit("/"));
    sp_str_t target = sp_fs_join_path(repo, relative);
    sp_fs_create_dir(sp_fs_parent_path(target));
    sp_fs_copy_file(entry->file_path, target);
  }
}

void git_repo_create_from_dir(sp_str_t source, sp_str_t repo) {
  SP_ASSERT(sp_fs_exists(source));
  SP_ASSERT(sp_fs_is_dir(source));

  sp_fs_create_dir(sp_fs_parent_path(repo));
  sp_fs_create_dir(repo);
  git_repo_copy_dir(source, repo);

  git_repo_run(repo, sp_str_lit("init"), sp_str_lit("--quiet"), SP_ZERO_STRUCT(sp_str_t), SP_ZERO_STRUCT(sp_str_t), SP_ZERO_STRUCT(sp_str_t));
  git_repo_run(repo, sp_str_lit("config"), sp_str_lit("user.name"), sp_str_lit("spn-test"), SP_ZERO_STRUCT(sp_str_t), SP_ZERO_STRUCT(sp_str_t));
  git_repo_run(repo, sp_str_lit("config"), sp_str_lit("user.email"), sp_str_lit("spn-test@local"), SP_ZERO_STRUCT(sp_str_t), SP_ZERO_STRUCT(sp_str_t));
  git_repo_run(repo, sp_str_lit("add"), sp_str_lit("."), SP_ZERO_STRUCT(sp_str_t), SP_ZERO_STRUCT(sp_str_t), SP_ZERO_STRUCT(sp_str_t));
  git_repo_run(repo, sp_str_lit("commit"), sp_str_lit("-m"), sp_str_lit("fixture"), sp_str_lit("--quiet"), SP_ZERO_STRUCT(sp_str_t));
}

sp_str_t git_repo_head(sp_str_t repo) {
  sp_ps_output_t output = sp_ps_run((sp_ps_config_t) {
    .command = sp_str_lit("git"),
    .args = {
      sp_str_lit("-C"), repo,
      sp_str_lit("rev-parse"),
      sp_str_lit("--short=12"),
      sp_str_lit("HEAD"),
    },
  });

  SP_ASSERT(output.status.exit_code == 0);
  return sp_str_trim_right(output.out);
}
