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
  sp_da(sp_fs_entry_t) entries = sp_fs_collect_recursive(source);
  sp_dyn_array_for(entries, it) {
    sp_fs_entry_t* entry = &entries[it];
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

  git_repo_init(repo);
  git_repo_commit_from_dir(source, repo, sp_str_lit("fixture"));
}

void git_repo_init(sp_str_t repo) {
  sp_fs_create_dir(sp_fs_parent_path(repo));
  sp_fs_create_dir(repo);

  git_repo_run(repo, sp_str_lit("init"), sp_str_lit("--quiet"), SP_ZERO_STRUCT(sp_str_t), SP_ZERO_STRUCT(sp_str_t), SP_ZERO_STRUCT(sp_str_t));
  git_repo_run(repo, sp_str_lit("config"), sp_str_lit("user.name"), sp_str_lit("spn-test"), SP_ZERO_STRUCT(sp_str_t), SP_ZERO_STRUCT(sp_str_t));
  git_repo_run(repo, sp_str_lit("config"), sp_str_lit("user.email"), sp_str_lit("spn-test@local"), SP_ZERO_STRUCT(sp_str_t), SP_ZERO_STRUCT(sp_str_t));
}

void git_repo_stage_all(sp_str_t repo) {
  sp_ps_output_t add = sp_ps_run((sp_ps_config_t) {
    .command = sp_str_lit("git"),
    .args = {
      sp_str_lit("-C"), repo,
      sp_str_lit("add"), sp_str_lit("."),
    },
  });
  sp_assert(!add.status.exit_code);
}

void git_repo_commit(sp_str_t repo, sp_str_t message) {
  sp_ps_output_t add = sp_ps_run((sp_ps_config_t) {
    .command = sp_str_lit("git"),
    .args = {
      sp_str_lit("-C"), repo,
      sp_str_lit("commit"), sp_str_lit("-m"), message,
      sp_str_lit("--quiet"),
      sp_str_lit("--allow-empty"),
    },
  });
  sp_assert(!add.status.exit_code);
}

void git_repo_commit_from_dir(sp_str_t source, sp_str_t repo, sp_str_t message) {
  SP_ASSERT(sp_fs_exists(source));
  SP_ASSERT(sp_fs_is_dir(source));
  SP_ASSERT(sp_fs_exists(repo));
  SP_ASSERT(sp_fs_is_dir(repo));

  git_repo_run(repo, sp_str_lit("rm"), sp_str_lit("-r"), sp_str_lit("--quiet"), sp_str_lit("--ignore-unmatch"), sp_str_lit("."));

  git_repo_copy_dir(source, repo);
  git_repo_run(repo, sp_str_lit("add"), sp_str_lit("."), SP_ZERO_STRUCT(sp_str_t), SP_ZERO_STRUCT(sp_str_t), SP_ZERO_STRUCT(sp_str_t));
  git_repo_run(repo, sp_str_lit("commit"), sp_str_lit("-m"), message, sp_str_lit("--quiet"), sp_str_lit("--allow-empty"));
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

static void git_repo_write_file(sp_str_t repo, const c8* path, const c8* content) {
  sp_str_t full = sp_fs_join_path(repo, sp_str_view(path));
  sp_str_t parent = sp_fs_parent_path(full);
  if (!sp_str_empty(parent)) {
    sp_fs_create_dir(parent);
  }

  sp_io_writer_t io = sp_io_writer_from_file(full, SP_IO_WRITE_MODE_OVERWRITE);
  SP_ASSERT(io.file.fd != 0);

  sp_str_t str = sp_str_view(content);
  if (!sp_str_empty(str)) {
    sp_io_write_str(&io, str);
  }

  sp_io_writer_close(&io);
}

git_repo_result_t git_repo_build(tmpfs_t* fs, const c8* name, git_repo_fixture_t* fixture) {
  git_repo_result_t result = SP_ZERO_INITIALIZE();
  result.path = tmpfs_get(fs, sp_str_view(name));

  git_repo_init(result.path);

  sp_carr_for(fixture->commits, c) {
    git_repo_commit_t* commit = &fixture->commits[c];
    if (!commit->message) break;

    // clear working tree
    git_repo_run(result.path,
      sp_str_lit("rm"), sp_str_lit("-r"), sp_str_lit("--quiet"),
      sp_str_lit("--ignore-unmatch"), sp_str_lit("."));

    // write files
    sp_carr_for(commit->files, f) {
      git_repo_file_t* file = &commit->files[f];
      if (!file->path) break;

      git_repo_write_file(result.path, file->path, file->content ? file->content : "");
    }

    // stage and commit
    git_repo_stage_all(result.path);
    git_repo_commit(result.path, sp_str_view(commit->message));

    result.commits[c] = git_repo_head(result.path);
    result.commit_count++;
  }

  return result;
}

sp_str_t git_repo_file_at(sp_str_t repo, sp_str_t commit, sp_str_t path) {
  sp_str_t spec = sp_format("{}:{}", SP_FMT_STR(commit), SP_FMT_STR(path));

  sp_ps_output_t output = sp_ps_run((sp_ps_config_t) {
    .command = sp_str_lit("git"),
    .args = {
      sp_str_lit("-C"), repo,
      sp_str_lit("show"), spec,
    },
  });

  SP_ASSERT(output.status.exit_code == 0);
  return output.out;
}
