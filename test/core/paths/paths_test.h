#pragma once

#include "spn_test.h"

#include "paths/paths.h"

typedef struct {
  const c8* project;
  const c8* store;
  const c8* build;
  const c8* checkout;
  const c8* toolchain;
  const c8* index;
  const c8* runtime;
  const c8* cache;
} paths_test_roots_t;

const spn_path_roots_t* paths_test_roots_build(paths_test_roots_t spec, spn_path_roots_t* storage);
