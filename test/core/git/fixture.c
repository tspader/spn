#include "sp.h"
#include "utest.h"
#include "test.h"


///////////
// STATE //
///////////
struct git_fixture {
  u32 dummy;
};

UTEST_F_SETUP(git_fixture) {
}

UTEST_F_TEARDOWN(git_fixture) {
}


///////////////
// EXECUTOR //
//////////////
static void run_fixture(s32* utest_result, git_repo_fixture_t fixture) {
  ctx_t* harness = ctx_get();
  sp_mem_t mem = sp_mem_arena_as_allocator(harness->arena);

  // build repo from fixture
  git_repo_result_t repo = git_repo_build(&harness->fs, fixture.name, &fixture);

  // assert commit count
  ASSERT_EQ(repo.commit_count, fixture.expect.num_commits);

  // all SHAs non-empty and distinct
  sp_for(it, repo.commit_count) {
    ASSERT_FALSE(sp_str_empty(repo.commits[it]));
    sp_for(j, it) {
      ASSERT_FALSE(sp_str_equal(repo.commits[it], repo.commits[j]));
    }
  }

  sp_for(c, repo.commit_count) {
    // verify expected file content per commit
    sp_carr_for(fixture.expect.commits[c], f) {
      git_repo_expect_file_t* expect = &fixture.expect.commits[c][f];
      if (!expect->file) {
        break;
      }

      sp_str_t actual = git_repo_file_at(repo.path, repo.commits[c], sp_str_view(expect->file));
      SP_EXPECT_STR_EQ_CSTR(actual, expect->content);
    }

    // verify expected missing files per commit
    sp_carr_for(fixture.expect.missing[c], f) {
      const c8* path = fixture.expect.missing[c][f];
      if (!path) {
        break;
      }

      sp_str_t spec = sp_fmt(mem, "{}:{}", sp_fmt_str(repo.commits[c]), sp_fmt_cstr(path)).value;
      sp_ps_output_t check = sp_ps_run(mem, (sp_ps_config_t) {
        .command = sp_str_lit("git"),
        .args = { sp_str_lit("-C"), repo.path, sp_str_lit("show"), spec },
      });
      EXPECT_NE(check.status.exit_code, 0);
    }
  }
}


////////////
// CASES //
///////////
UTEST_F(git_fixture, multiple_commits) {
  run_fixture(utest_result, (git_repo_fixture_t) {
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
  });
}

UTEST_F(git_fixture, file_content_at_commit) {
  run_fixture(utest_result, (git_repo_fixture_t) {
    .name = "file_content",
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
  });
}

UTEST_F(git_fixture, subdirectory_files) {
  run_fixture(utest_result, (git_repo_fixture_t) {
    .name = "subdir_files",
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
  });
}

UTEST_F(git_fixture, commit_replaces_tree) {
  run_fixture(utest_result, (git_repo_fixture_t) {
    .name = "replaces_tree",
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
  });
}
