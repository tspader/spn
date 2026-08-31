#ifndef SPN_TEST_PATH_ROOTS_H
#define SPN_TEST_PATH_ROOTS_H

// Builds a spn_path_roots_t from named directories. This is shared fixture
// code, not a paths-suite detail: the dag tests build roots too, so it lives
// beside the other test tools rather than inside one suite's directory.

#include "sp.h"
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

#endif
