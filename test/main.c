#define SP_IMPLEMENTATION
#define SP_OS_BACKEND_SDL
#include "sp/sp.h"

#define ARGPARSE_IMPLEMENTATION
#include "argparse/argparse.h"

#include "toml/toml.h"

#define SPN_IMPLEMENTATION
#include "spn.h"

#include "utest/utest.h"

void spn_test_use_malloc() {
  static sp_allocator_malloc_t malloc_allocator = SP_ZERO_INITIALIZE();
  static sp_allocator_t allocator = SP_ZERO_INITIALIZE();

  sp_context = &sp_context_stack[0];
  *sp_context = SP_ZERO_STRUCT(sp_context_t);
  allocator = sp_allocator_malloc_init(&malloc_allocator);
  sp_context_push_allocator(&allocator);
}

UTEST(spn_config, load_basic) {
  spn_test_use_malloc();

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

UTEST(spn_config, load_with_overrides) {
  spn_test_use_malloc();

  sp_str_t toml_content = SP_LIT(
    "[options]\n"
    "cache_override = \"/tmp/test-cache\"\n"
    "additional_recipe_dirs = [\"/tmp/recipes1\", \"/tmp/recipes2\"]\n"
  );

  spn_config_t config;
  spn_config_read_from_string(&config, toml_content);

  ASSERT_TRUE(sp_str_equal(SP_LIT("/tmp/test-cache"), config.cache_override));
  ASSERT_EQ(2, sp_dyn_array_size(config.additional_recipe_dirs));
  ASSERT_TRUE(sp_str_equal(SP_LIT("/tmp/recipes1"), config.additional_recipe_dirs[0]));
  ASSERT_TRUE(sp_str_equal(SP_LIT("/tmp/recipes2"), config.additional_recipe_dirs[1]));
}

UTEST_MAIN();
