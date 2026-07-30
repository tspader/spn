#ifndef SPN_FIXTURE_H
#define SPN_FIXTURE_H

#include "sp.h"
#include "sp/macro.h"

sp_str_t test_repo_root(sp_mem_t mem);
sp_str_t test_repo_path(sp_mem_t mem, sp_str_t rel);

static inline sp_str_t test_read_file(sp_mem_t mem, sp_str_t path) {
  sp_str_t content = sp_zero;
  sp_io_read_file(mem, path, &content);
  return content;
}

sp_ps_output_t git_repo_run(sp_str_t repo, const sp_str_t* args, u32 count);
#define git_repo_git(repo, ...) git_repo_run(repo, (sp_str_t[]) { __VA_ARGS__ }, sp_carr_len(((sp_str_t[]) { __VA_ARGS__ })))
void     git_repo_init(sp_str_t repo);
void     git_repo_stage_all(sp_str_t repo);
void     git_repo_commit(sp_str_t repo, sp_str_t message);
void     git_repo_commit_from_dir(sp_str_t source, sp_str_t repo, sp_str_t message);
sp_str_t git_repo_head(sp_str_t repo);

typedef struct {
  const c8* path;
  const c8* content;
} git_repo_file_t;

typedef struct {
  const c8* message;
  git_repo_file_t files [8];
} git_repo_commit_t;

typedef struct {
  const c8* file;
  const c8* content;
} git_repo_expect_file_t;

typedef struct {
  u32 num_commits;
  git_repo_expect_file_t commits [8][8];
  const c8* missing [8][8];
} git_repo_expect_t;

typedef struct {
  const c8* name;
  git_repo_commit_t commits [8];
  git_repo_expect_t expect;
} git_repo_fixture_t;

typedef struct {
  sp_str_t path;
  sp_str_t commits [8];
  u32 commit_count;
} git_repo_result_t;

git_repo_result_t git_repo_build_at(sp_str_t dir, const c8* name, git_repo_fixture_t* fixture);
sp_str_t          git_repo_file_at(sp_str_t repo, sp_str_t commit, sp_str_t path);
bool              git_repo_has_file(sp_str_t repo, sp_str_t commit, sp_str_t path);

#endif
