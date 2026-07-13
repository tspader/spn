#pragma once

#include "sp.h"
#include "sp/atomic_file.h"
#include "utest.h"
#include "test.h"
#include "dag/dag.h"

#define DAG_TEST_MAX_INPUTS 4
#define DAG_TEST_MAX_OUTPUTS 4
#define DAG_TEST_MAX_OPS 8

#define EXPECT_STR(actual, cstr) EXPECT_TRUE(sp_str_equal((actual), sp_str_view(cstr)))
