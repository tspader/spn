#ifndef SPN_TEST_HARNESS_H
#define SPN_TEST_HARNESS_H

#include "sp.h"
#include "test.h"
#include "action.h"

typedef struct {
  tmpfs_t fs;
  struct {
    sp_str_t root;
    sp_str_t spn;
    sp_str_t storage;
    sp_str_t toolchain;
    sp_str_t config;
    sp_str_t index;
    sp_str_t include;
    sp_str_t patches;
  } paths;
} fixture_t;

void fixture_setup_paths(fixture_t* fixture);

sp_str_t shared_lib(const c8* name);
sp_str_t staged_lib(const c8* name);
sp_str_t test_lib(const c8* name);
sp_str_t exe(const c8* name);
sp_str_t test_exe(const c8* name);
sp_str_t target_exe(const c8* name, const c8* triple);
sp_str_t store_file(const c8* rest);
sp_str_t work_file(const c8* rest);
sp_str_t profile_exe(const c8* profile, const c8* name);
sp_str_t profile_store_file(const c8* profile, const c8* rest);
sp_str_t target_store_file(const c8* rest, const c8* triple);

#define SPN_TEST_SUITE(suite) \
  struct suite { \
    fixture_t fixture; \
  }; \
  UTEST_F_SETUP(suite) { \
    fixture_setup_paths(&uf->fixture); \
    ASSERT_TRUE(sp_fs_exists(uf->fixture.paths.spn)); \
  } \
  UTEST_F_TEARDOWN(suite) { \
  }

void expect_exists(s32* utest_result, tmpfs_t* fs, sp_str_t path, bool expected, const c8* file, u32 line);

#define SP_EXPECT_CONTAINS(haystack, needle)
#define SP_EXPECT_EXISTS(path) expect_exists(utest_result, SP_NULLPTR, path, true, __FILE__, __LINE__)
#define SP_EXPECT_EXISTS_TMPFS(fs, path) expect_exists(utest_result, fs, path, true, __FILE__, __LINE__)
#define SP_EXPECT_NOT_EXISTS_TMPFS(fs, path) expect_exists(utest_result, fs, path, false, __FILE__, __LINE__)

void copy_project_path(s32* utest_result, tmpfs_t* fs, sp_str_t project, sp_str_t relative);
void setup_fixture_index_from_remote(s32* utest_result, fixture_t* fixture, sp_str_t project);
void setup_fixture_source_repos(s32* utest_result, fixture_t* fixture, sp_str_t project);
void setup_fixture_envrc(tmpfs_t* fs, sp_str_t storage, sp_str_t toolchain, sp_str_t config);
void setup_fixture_config(tmpfs_t* fs, sp_str_t config_dir, sp_str_t index_dir, sp_str_t spn_dir);
void setup_e2e_config(tmpfs_t* fs, sp_str_t config_dir, sp_str_t spn_dir, sp_str_t index_url, sp_str_t index_rev);

void fixture_copy_project(s32* utest_result, fixture_t* fixture, sp_str_t project, const c8* const* copy);
void run_actions(s32* utest_result, fixture_t* fixture, const action_t* actions);
void run_test(s32* utest_result, fixture_t* fixture, test_t test);

#endif
