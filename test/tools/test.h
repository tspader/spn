#ifndef SPN_TEST_H
#define SPN_TEST_H
#include "sp.h"
#include "utest.h"

#define ut (*utest_fixture)
#define uf utest_fixture
#define ur (*utest_result)
#define UTEST_RESULT(r) s32* utest_result = (r)
#define r(str) str "\n"
#define q(str) #str
#define k(key) q(key) ": "
#define kv(key, val) q(key) ": " #val

#define ts(section) "[" q(section) "]\n"
#define tk(key) q(key) " = "
#define tkv(key, val) tk(key) #val
#define tt(key, val) tk(key) #val




// TMPFS
typedef struct {
  sp_str_t root;
} tmpfs_t;

void     tmpfs_init(tmpfs_t* fs);
void     tmpfs_init_named(tmpfs_t* fs, const c8* test);
void     tmpfs_set_top_level(sp_str_t root);
sp_str_t tmpfs_get(tmpfs_t* fs, sp_str_t name);
void     tmpfs_create(tmpfs_t* fs, sp_str_t path, sp_str_t content);
sp_str_t tmpfs_touch(tmpfs_t* fs, sp_str_t path);
void     tmpfs_deinit(tmpfs_t* fs);


// CONTEXT
typedef struct {
  tmpfs_t fs;
  sp_mem_arena_t* arena;
} ctx_t;

typedef struct {
  sp_str_t repo;
  struct {
    sp_str_t dir;
    sp_str_t fixtures;
  } test;
} ctx_paths_t;

ctx_t* ctx_get();
void   ctx_init(ctx_t* ctx);
void   ctx_deinit(ctx_t* ctx);
ctx_paths_t ctx_get_paths(ctx_t* ctx);


// GIT
void     git_repo_create_from_dir(sp_str_t source, sp_str_t repo);
void     git_repo_init(sp_str_t repo);
void     git_repo_commit_from_dir(sp_str_t source, sp_str_t repo, sp_str_t message);
sp_str_t git_repo_head(sp_str_t repo);
void     git_repo_stage_all(sp_str_t repo);
void     git_repo_commit(sp_str_t repo, sp_str_t message);

// GIT FIXTURE BUILDER
//
// Declarative helpers for creating git repos with known file content at known
// commits. Each commit replaces the entire working tree.
//
// Usage:
//   git_repo_result_t repo = git_repo_build(&fs, "my-repo", &(git_repo_fixture_t) {
//     .commits = {
//       { .message = "initial",  .files = { { "lib.h", "#pragma once" } } },
//       { .message = "add impl", .files = { { "lib.h", "#pragma once" }, { "lib.c", "int x;" } } },
//     },
//   });
//
// Each commit replaces the entire working tree (like git_repo_commit_from_dir).
// repo.commits[i] holds the short SHA for commit i.
// repo.path is the absolute path to the created repo.

typedef struct {
  const c8* path;
  const c8* content;
} git_repo_file_t;

typedef struct {
  const c8* message;
  git_repo_file_t files[8];
} git_repo_commit_t;

typedef struct {
  const c8* file;
  const c8* content;
} git_repo_expect_file_t;

typedef struct {
  u32 num_commits;
  git_repo_expect_file_t commits[8][8];
  const c8* missing[8][8];
} git_repo_expect_t;

typedef struct {
  const c8* name;
  git_repo_commit_t commits[8];
  git_repo_expect_t expect;
} git_repo_fixture_t;

typedef struct {
  sp_str_t path;
  sp_str_t commits[8];
  u32 commit_count;
} git_repo_result_t;

git_repo_result_t git_repo_build(tmpfs_t* fs, const c8* name, git_repo_fixture_t* fixture);
sp_str_t          git_repo_file_at(sp_str_t repo, sp_str_t commit, sp_str_t path);

bool str_equal(sp_str_t a, sp_str_t b);

// UTEST
#define SP_TEST_REPORT(fmt, ...) \
  do { \
    sp_str_t formatted = sp_format_str(fmt, ##__VA_ARGS__); \
    UTEST_PRINTF("%s\n", sp_str_to_cstr(formatted)); \
  } while (0)

#define SP_TEST_STREQ(a, b, is_assert) \
  UTEST_SURPRESS_WARNING_BEGIN do { \
    if (!str_equal((a), (b))) { \
      const c8* __file = __FILE__; \
      const u32 __line = __LINE__; \
      sp_str_builder_t __builder = SP_ZERO_INITIALIZE(); \
      sp_str_builder_append_fmt_str(&__builder, SP_LIT("{}:{} Failure:"), SP_FMT_CSTR(__file), SP_FMT_U32(__line)); \
      sp_str_builder_new_line(&__builder); \
      sp_str_builder_indent(&__builder); \
      sp_str_builder_append_fmt_str(&__builder, SP_LIT("{} != {}"), SP_FMT_QUOTED_STR((a)), SP_FMT_QUOTED_STR((b))); \
      SP_TEST_REPORT(sp_str_builder_to_str(&__builder)); \
      *utest_result = UTEST_TEST_FAILURE; \
 \
      if (is_assert) { \
        return; \
      } \
    } \
  } while (0) \
  UTEST_SURPRESS_WARNING_END

#define SP_EXPECT_STR_EQ_CSTR(a, b) SP_TEST_STREQ((a), SP_CSTR(b), false)
#define SP_EXPECT_STR_EQ(a, b) SP_TEST_STREQ((a), (b), false)
#define SP_EXPECT_ERR(err) EXPECT_EQ(sp_err_get(), err)


#endif
