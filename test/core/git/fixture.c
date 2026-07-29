#include "git.h"

static const git_repo_fixture_t tests [] = {
  {
    .name = "multiple_commits",
    .commits = {
      {
        .message = "initial",
        .files = {
          { "readme.txt", "hello" },
        },
      },
      {
        .message = "add lib",
        .files = {
          { "readme.txt", "hello" },
          { "lib.c", "int x = 1;" },
        },
      },
      {
        .message = "update lib",
        .files = {
          { "readme.txt", "updated" },
          { "lib.c", "int x = 2;" },
        },
      },
    },
    .expect = {
      .num_commits = 3,
      .commits = {
        {
          { .file = "readme.txt", .content = "hello" },
        },
        {
          { .file = "readme.txt", .content = "hello" },
          { .file = "lib.c", .content = "int x = 1;" },
        },
        {
          { .file = "readme.txt", .content = "updated" },
          { .file = "lib.c", .content = "int x = 2;" },
        },
      },
    },
  },
  {
    .name = "file_content_at_commit",
    .commits = {
      {
        .message = "v1",
        .files = {
          { "data.txt", "version-one" },
        },
      },
      {
        .message = "v2",
        .files = {
          { "data.txt", "version-two" },
          { "extra.txt", "bonus" },
        },
      },
    },
    .expect = {
      .num_commits = 2,
      .commits = {
        {
          { .file = "data.txt", .content = "version-one" },
        },
        {
          { .file = "data.txt", .content = "version-two" },
          { .file = "extra.txt", .content = "bonus" },
        },
      },
    },
  },
  {
    .name = "subdirectory_files",
    .commits = {
      {
        .message = "nested",
        .files = {
          { "src/main.c", "int main() {}" },
          { "include/lib.h", "#pragma once" },
        },
      },
    },
    .expect = {
      .num_commits = 1,
      .commits = {
        {
          { .file = "src/main.c", .content = "int main() {}" },
          { .file = "include/lib.h", .content = "#pragma once" },
        },
      },
    },
  },
  {
    .name = "commit_replaces_tree",
    .commits = {
      {
        .message = "has old file",
        .files = {
          { "old.txt", "present" },
          { "keep.txt", "same" },
        },
      },
      {
        .message = "old file removed",
        .files = {
          { "keep.txt", "same" },
          { "new.txt", "added" },
        },
      },
    },
    .expect = {
      .num_commits = 2,
      .commits = {
        {
          { .file = "old.txt", .content = "present" },
          { .file = "keep.txt", .content = "same" },
        },
        {
          { .file = "keep.txt", .content = "same" },
          { .file = "new.txt", .content = "added" },
        },
      },
      .missing = {
        { 0 },
        { "old.txt" },
      },
    },
  },
};

sp_test_each(git_fixture, build, git_repo_fixture_t, tests) {
  sp_mem_t mem = sp_test_arena(t);

  // build repo from fixture
  git_repo_result_t repo = git_repo_build_at(sp_test_dir(t), it->name, it);

  // assert commit count
  sp_must_eq(t, repo.commit_count, it->expect.num_commits);

  // all SHAs non-empty and distinct
  sp_for(c, repo.commit_count) {
    sp_must(t, !sp_str_empty(repo.commits[c]));
    sp_for(j, c) {
      sp_must(t, !sp_str_equal(repo.commits[c], repo.commits[j]));
    }
  }

  sp_for(c, repo.commit_count) {
    // verify expected file content per commit
    sp_carr_for(it->expect.commits[c], f) {
      git_repo_expect_file_t* expect = &it->expect.commits[c][f];
      if (!expect->file) {
        break;
      }

      sp_str_t actual = git_repo_file_at(repo.path, repo.commits[c], sp_str_view(expect->file));
      sp_expect_str_eq_c(t, actual, expect->content);
    }

    // verify expected missing files per commit
    sp_carr_for(it->expect.missing[c], f) {
      const c8* path = it->expect.missing[c][f];
      if (!path) {
        break;
      }

      sp_str_t spec = sp_fmt(mem, "{}:{}", sp_fmt_str(repo.commits[c]), sp_fmt_cstr(path)).value;
      sp_ps_output_t check = sp_ps_run(mem, (sp_ps_config_t) {
        .command = sp_str_lit("git"),
        .args = { sp_str_lit("-C"), repo.path, sp_str_lit("show"), spec },
      });
      sp_expect_ne(t, check.status.exit_code, 0);
    }
  }

  return SP_OK;
}
