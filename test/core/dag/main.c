#define SP_IMPLEMENTATION
#include "sp.h"

#define UTEST_IMPLEMENTATION
#include "utest.h"

#include "test.h"
#include "dag/dag.h"

#define DAG_TEST_MAX_INPUTS 4
#define DAG_TEST_MAX_OUTPUTS 4
#define DAG_TEST_MAX_OPS 8

#define EXPECT_STR(actual, cstr) EXPECT_TRUE(sp_str_equal((actual), sp_str_view(cstr)))

#include "digest.c"
#include "key.c"
#include "strong_key.c"
#include "store.c"
#include "file_cache.c"
#include "action_cache.c"
#include "discovery.c"
#include "cc.c"
#include "exec.c"
#include "discover_exec.c"
#include "run.c"

UTEST_MAIN();
