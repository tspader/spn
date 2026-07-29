#pragma once

#include "spn_test.h"

#include "toml/edit.h"

#define TOML_TEST_MAX_SETS 4
#define TOML_TEST_MAX_PATH 4

sp_err_t toml_read_source(sp_test_t* t, const c8* manifest, const c8* toml, sp_str_t* source);
u32      toml_collect_path(const c8* (*segments)[TOML_TEST_MAX_PATH], sp_str_t* path);
