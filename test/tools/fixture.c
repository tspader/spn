#include "fixture.h"

sp_str_t test_repo_root(sp_mem_t mem) {
  sp_str_t path = sp_fs_get_exe_path(mem);
  while (true) {
    sp_assert(!sp_str_empty(path));
    if (sp_fs_exists(sp_fs_join_path(mem, path, strl("spn.toml")))) {
      return path;
    }
    path = sp_fs_parent_path(path);
  }
}

sp_str_t test_repo_path(sp_mem_t mem, sp_str_t rel) {
  return sp_fs_join_path(mem, test_repo_root(mem), rel);
}

sp_ps_output_t git_repo_run(sp_str_t repo, const sp_str_t* args, u32 count) {
  sp_ps_config_t config = {
    .command = sp_str_lit("git"),
    .args = { sp_str_lit("-C"), repo },
  };
  SP_ASSERT(count <= SP_PS_MAX_ARGS - 2);
  sp_for(it, count) {
    config.args[2 + it] = args[it];
  }

  sp_ps_output_t output = sp_ps_run(sp_mem_os_new(), config);
  SP_ASSERT(output.status.exit_code == 0);
  return output;
}

static void git_repo_copy_dir(sp_str_t source, sp_str_t repo) {
  sp_mem_t mem = sp_mem_os_new();
  sp_da(sp_fs_entry_t) entries = sp_zero;
  sp_fs_collect_recursive(mem, source, &entries);
  sp_da_for(entries, it) {
    sp_fs_entry_t* entry = &entries[it];
    if (sp_fs_is_dir(entry->path)) {
      continue;
    }

    sp_str_t relative = sp_str_strip_left(entry->path, source);
    relative = sp_str_strip_left(relative, sp_str_lit("/"));
    sp_str_t target = sp_fs_join_path(mem, repo, relative);
    sp_fs_create_dir(sp_fs_parent_path(target));
    sp_fs_copy_file(entry->path, target);
  }
}

void git_repo_init(sp_str_t repo) {
  sp_fs_create_dir(sp_fs_parent_path(repo));
  sp_fs_create_dir(repo);

  git_repo_git(repo, sp_str_lit("init"), sp_str_lit("--quiet"));
  git_repo_git(repo, sp_str_lit("config"), sp_str_lit("user.name"), sp_str_lit("spn-test"));
  git_repo_git(repo, sp_str_lit("config"), sp_str_lit("user.email"), sp_str_lit("spn-test@local"));
}

void git_repo_stage_all(sp_str_t repo) {
  git_repo_git(repo, sp_str_lit("add"), sp_str_lit("."));
}

void git_repo_commit(sp_str_t repo, sp_str_t message) {
  git_repo_git(repo, sp_str_lit("commit"), sp_str_lit("-m"), message, sp_str_lit("--quiet"), sp_str_lit("--allow-empty"));
}

void git_repo_commit_from_dir(sp_str_t source, sp_str_t repo, sp_str_t message) {
  SP_ASSERT(sp_fs_exists(source));
  SP_ASSERT(sp_fs_is_dir(source));
  SP_ASSERT(sp_fs_exists(repo));
  SP_ASSERT(sp_fs_is_dir(repo));

  git_repo_git(repo, sp_str_lit("rm"), sp_str_lit("-r"), sp_str_lit("--quiet"), sp_str_lit("--ignore-unmatch"), sp_str_lit("."));

  git_repo_copy_dir(source, repo);
  git_repo_stage_all(repo);
  git_repo_commit(repo, message);
}

sp_str_t git_repo_head(sp_str_t repo) {
  sp_ps_output_t output = git_repo_git(repo, sp_str_lit("rev-parse"), sp_str_lit("--short=12"), sp_str_lit("HEAD"));
  return sp_str_trim_right(output.out);
}

static void git_repo_write_file(sp_str_t repo, const c8* path, const c8* content) {
  sp_str_t full = sp_fs_join_path(sp_mem_os_new(), repo, sp_str_view(path));
  sp_fs_create_dir(sp_fs_parent_path(full));
  sp_fs_create_file_str(full, content ? sp_str_view(content) : sp_str_lit(""));
}

git_repo_result_t git_repo_build_at(sp_str_t dir, const c8* name, git_repo_fixture_t* fixture) {
  git_repo_result_t result = sp_zero;
  result.path = sp_fs_join_path(sp_mem_os_new(), dir, sp_str_view(name));

  git_repo_init(result.path);

  sp_carr_for(fixture->commits, c) {
    git_repo_commit_t* commit = &fixture->commits[c];
    if (!commit->message) break;

    git_repo_git(result.path,
      sp_str_lit("rm"), sp_str_lit("-r"), sp_str_lit("--quiet"),
      sp_str_lit("--ignore-unmatch"), sp_str_lit("."));

    sp_carr_for(commit->files, f) {
      git_repo_file_t* file = &commit->files[f];
      if (!file->path) break;

      git_repo_write_file(result.path, file->path, file->content);
    }

    git_repo_stage_all(result.path);
    git_repo_commit(result.path, sp_str_view(commit->message));

    result.commits[c] = git_repo_head(result.path);
    result.commit_count++;
  }

  return result;
}

sp_str_t git_repo_file_at(sp_str_t repo, sp_str_t commit, sp_str_t path) {
  sp_str_t spec = sp_fmt(sp_mem_os_new(), "{}:{}", sp_fmt_str(commit), sp_fmt_str(path)).value;
  sp_ps_output_t output = git_repo_git(repo, sp_str_lit("show"), spec);
  return output.out;
}

bool git_repo_has_file(sp_str_t repo, sp_str_t commit, sp_str_t path) {
  sp_str_t spec = sp_fmt(sp_mem_os_new(), "{}:{}", sp_fmt_str(commit), sp_fmt_str(path)).value;
  sp_ps_output_t output = sp_ps_run(sp_mem_os_new(), (sp_ps_config_t) {
    .command = sp_str_lit("git"),
    .args = { sp_str_lit("-C"), repo, sp_str_lit("cat-file"), sp_str_lit("-e"), spec },
  });
  return output.status.exit_code == 0;
}
