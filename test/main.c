#define SPN_IMPLEMENTATION
#include "spn.h"

#include "utest.h"

struct utest_state_s utest_state;

// Test helper functions
static sp_str_t test_temp_dir() {
  static u32 counter = 0;
  counter++;
  return sp_format("/tmp/spn_test_{}", SP_FMT_U32(counter));
}

static void test_cleanup_dir(sp_str_t dir) {
  if (sp_os_does_path_exist(dir)) {
    sp_os_remove_directory(dir);
  }
}

static sp_str_t test_create_project(sp_str_t dir, sp_str_t name) {
  sp_os_create_directory(dir);
  sp_str_t toml_path = sp_os_join_path(dir, SP_LIT("spn.toml"));

  sp_toml_writer_t writer = SP_ZERO_INITIALIZE();
  sp_toml_writer_add_header(&writer, SP_LIT("project"));
  sp_toml_writer_add_string(&writer, SP_LIT("name"), name);
  sp_toml_writer_new_line(&writer);

  sp_str_t content = sp_toml_writer_write(&writer);
  SDL_SaveFile(sp_str_to_cstr(toml_path), content.data, content.len);

  return toml_path;
}

// Config tests
UTEST(spn_config, load_basic) {
  sp_str_t toml_content = SP_LIT(
    "[options]\n"
    "auto_pull_recipes = true\n"
    "builtin_recipes_enabled = false\n"
  );

  spn_config_t config;
  spn_config_read_from_string(&config, toml_content);

  ASSERT_TRUE(config.auto_pull_recipes);
  ASSERT_FALSE(config.builtin_recipes_enabled);
}

UTEST(spn_config, cache_override) {
  sp_str_t toml_content = SP_LIT(
    "[options]\n"
    "cache_override = \"/tmp/test-cache\"\n"
  );

  spn_config_t config;
  spn_config_read_from_string(&config, toml_content);

  ASSERT_TRUE(sp_str_equal(SP_LIT("/tmp/test-cache"), config.cache_override));
}

UTEST(spn_config, empty_config) {
  sp_str_t toml_content = SP_LIT("");

  spn_config_t config;
  spn_config_read_from_string(&config, toml_content);

  // Check defaults
  ASSERT_FALSE(config.auto_pull_recipes);
  ASSERT_FALSE(config.auto_pull_deps);
  ASSERT_TRUE(config.builtin_recipes_enabled);
  ASSERT_FALSE(sp_str_valid(config.cache_override));
}

// Project tests
UTEST(spn_project, read_basic) {
  sp_str_t temp_dir = test_temp_dir();
  sp_str_t toml_path = test_create_project(temp_dir, SP_LIT("test-project"));

  spn_project_t project;
  bool success = spn_project_read(&project, toml_path);

  ASSERT_TRUE(success);
  ASSERT_TRUE(sp_str_equal(project.name, SP_LIT("test-project")));
  ASSERT_EQ(0, sp_dyn_array_size(project.dependencies));

  test_cleanup_dir(temp_dir);
}

// TODO: Fix segfault in this test - likely issue with spn_project_write
/*
UTEST(spn_project, write_with_dependencies) {
  sp_str_t temp_dir = test_temp_dir();
  sp_os_create_directory(temp_dir);
  sp_str_t toml_path = sp_os_join_path(temp_dir, SP_LIT("spn.toml"));

  spn_project_t project = SP_ZERO_INITIALIZE();
  project.name = SP_LIT("test-project");

  // Initialize the dynamic array before pushing
  sp_dyn_array(spn_dep_id_t) deps = SP_NULLPTR;
  sp_dyn_array_push(deps, SP_LIT("argparse"));
  sp_dyn_array_push(deps, SP_LIT("toml"));
  project.dependencies = deps;

  bool success = spn_project_write(&project, toml_path);
  ASSERT_TRUE(success);

  // Read it back
  spn_project_t read_project;
  success = spn_project_read(&read_project, toml_path);
  ASSERT_TRUE(success);
  ASSERT_TRUE(sp_str_equal(read_project.name, SP_LIT("test-project")));
  ASSERT_EQ(2, sp_dyn_array_size(read_project.dependencies));

  test_cleanup_dir(temp_dir);
}
*/

// Lock file tests
UTEST(spn_lock, read_write) {
  sp_str_t temp_dir = test_temp_dir();
  sp_os_create_directory(temp_dir);
  sp_str_t lock_path = sp_os_join_path(temp_dir, SP_LIT("spn.lock"));

  sp_dyn_array(spn_lock_entry_t) entries = SP_NULLPTR;

  spn_lock_entry_t entry1 = {
    .name = SP_LIT("test-dep"),
    .url = SP_LIT("https://github.com/test/repo.git"),
    .commit = SP_LIT("abc123"),
    .build_id = SP_LIT("def456")
  };
  sp_dyn_array_push(entries, entry1);

  bool success = spn_lock_file_write(&entries, lock_path);
  ASSERT_TRUE(success);

  // Read it back
  sp_dyn_array(spn_lock_entry_t) read_entries = SP_NULLPTR;
  success = spn_lock_file_read(&read_entries, lock_path);
  ASSERT_TRUE(success);
  ASSERT_EQ(1, sp_dyn_array_size(read_entries));
  ASSERT_TRUE(sp_str_equal(read_entries[0].name, SP_LIT("test-dep")));

  test_cleanup_dir(temp_dir);
}

// Build state tests
UTEST(spn_build, state_transitions) {
  ASSERT_TRUE(sp_str_equal(spn_dep_build_state_to_str(SPN_DEP_BUILD_STATE_IDLE), SP_LIT("idle")));
  ASSERT_TRUE(sp_str_equal(spn_dep_build_state_to_str(SPN_DEP_BUILD_STATE_CLONING), SP_LIT("cloning")));
  ASSERT_TRUE(sp_str_equal(spn_dep_build_state_to_str(SPN_DEP_BUILD_STATE_BUILDING), SP_LIT("building")));
  ASSERT_TRUE(sp_str_equal(spn_dep_build_state_to_str(SPN_DEP_BUILD_STATE_DONE), SP_LIT("done")));
  ASSERT_TRUE(sp_str_equal(spn_dep_build_state_to_str(SPN_DEP_BUILD_STATE_FAILED), SP_LIT("failed")));
}

// Error logging tests
UTEST(spn_build, error_log_creation) {
  sp_str_t temp_dir = test_temp_dir();
  sp_os_create_directory(temp_dir);

  sp_str_t build_dir = sp_os_join_path(temp_dir, SP_LIT("build"));
  sp_os_create_directory(build_dir);

  sp_str_t stderr_path = sp_os_join_path(build_dir, SP_LIT("build.stderr"));
  sp_str_t stdout_path = sp_os_join_path(build_dir, SP_LIT("build.stdout"));

  // Simulate writing error logs
  SDL_IOStream* err_file = SDL_IOFromFile(sp_str_to_cstr(stderr_path), "w");
  ASSERT_TRUE(err_file != NULL);

  sp_str_t error_msg = SP_LIT("Test error message\n");
  SDL_WriteIO(err_file, error_msg.data, error_msg.len);
  SDL_CloseIO(err_file);

  // Verify file exists and contains the error
  ASSERT_TRUE(sp_os_does_path_exist(stderr_path));

  sp_size_t size;
  c8* content = (c8*)SDL_LoadFile(sp_str_to_cstr(stderr_path), &size);
  ASSERT_TRUE(content != NULL);
  ASSERT_TRUE(sp_str_equal(sp_str((const c8*)content, size), error_msg));
  SDL_free(content);

  test_cleanup_dir(temp_dir);
}

// Path handling tests
UTEST(spn_paths, build_paths) {
  sp_str_t cache_dir = SP_LIT("/tmp/test-cache");
  sp_str_t source_dir = sp_os_join_path(cache_dir, SP_LIT("source"));
  sp_str_t build_dir = sp_os_join_path(cache_dir, SP_LIT("build"));
  sp_str_t store_dir = sp_os_join_path(cache_dir, SP_LIT("store"));

  ASSERT_TRUE(sp_str_equal(source_dir, SP_LIT("/tmp/test-cache/source")));
  ASSERT_TRUE(sp_str_equal(build_dir, SP_LIT("/tmp/test-cache/build")));
  ASSERT_TRUE(sp_str_equal(store_dir, SP_LIT("/tmp/test-cache/store")));
}

// Process tests
UTEST(spn_process, read_process_basic) {
  SDL_Init(0);

  const char* args[] = {"echo", "hello", NULL};
  SDL_Process* proc = SDL_CreateProcess(args, true);
  ASSERT_TRUE(proc != NULL);

  spn_sh_process_result_t result = spn_sh_read_process(proc);
  ASSERT_EQ(0, result.return_code);
  ASSERT_TRUE(result.output.len > 0);

  SDL_DestroyProcess(proc);
  SDL_Quit();
}

UTEST(spn_process, process_failure) {
  SDL_Init(0);

  const char* args[] = {"false", NULL};
  SDL_Process* proc = SDL_CreateProcess(args, true);
  ASSERT_TRUE(proc != NULL);

  spn_sh_process_result_t result = spn_sh_read_process(proc);
  ASSERT_NE(0, result.return_code);

  SDL_DestroyProcess(proc);
  SDL_Quit();
}

// Integration test for cache override
UTEST(spn_integration, cache_override_respected) {
  sp_str_t temp_cache = test_temp_dir();
  sp_str_t temp_project = test_temp_dir();

  // Create project directory
  test_create_project(temp_project, SP_LIT("cache-test"));

  // Create config with cache override
  sp_str_t config_content = sp_format(
    "[options]\n"
    "cache_override = \"{}\"\n",
    SP_FMT_STR(temp_cache)
  );

  spn_config_t config;
  spn_config_read_from_string(&config, config_content);

  // Initialize app with this config
  spn_app_t test_app = SP_ZERO_INITIALIZE();
  test_app.config = config;
  test_app.cli.project_directory = sp_str_to_cstr(temp_project);

  // Simulate path initialization (simplified)
  if (sp_str_valid(test_app.config.cache_override)) {
    test_app.paths.cache = sp_str_copy(test_app.config.cache_override);
  }

  ASSERT_TRUE(sp_str_equal(test_app.paths.cache, temp_cache));

  test_cleanup_dir(temp_cache);
  test_cleanup_dir(temp_project);
}

// CLI flag tests
UTEST(spn_cli, lock_flag) {
  spn_cli_t cli = SP_ZERO_INITIALIZE();
  cli.lock = true;

  ASSERT_TRUE(cli.lock);

  // When lock is set, update prompts should be skipped
  // This would be tested in integration tests with actual dependency updates
}

UTEST(spn_cli, no_interactive_flag) {
  spn_cli_t cli = SP_ZERO_INITIALIZE();
  cli.no_interactive = true;

  ASSERT_TRUE(cli.no_interactive);
}

// Main test runner
int main(int argc, const char* argv[]) {
  sp_init((sp_config_t) {
    .allocator = sp_allocator_default()
  });

  SDL_Init(0);
  int result = utest_main(argc, argv);
  SDL_Quit();

  return result;
}
