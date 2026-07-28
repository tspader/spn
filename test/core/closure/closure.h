#pragma once

#include "sp.h"
#include "sp/macro.h"

#include "session/types.h"
#include "unit/types.h"
#include "unit/unit.h"

#define CLOSURE_TEST_MAX_PKGS 8
#define CLOSURE_TEST_MAX_DEPS 8
#define CLOSURE_TEST_MAX_LIBS 4
#define CLOSURE_TEST_MAX_NAMES 8

typedef struct {
  const c8* name;
  spn_dep_kind_t kind;
  bool private;
} closure_dep_t;

typedef struct {
  const c8* name;
  spn_linkage_t kind;
  bool no_link;
  const c8* deps [CLOSURE_TEST_MAX_DEPS];
} closure_lib_t;

typedef struct {
  const c8* name;
  spn_linkage_t kind;
  closure_dep_t deps [CLOSURE_TEST_MAX_DEPS];
  closure_lib_t libs [CLOSURE_TEST_MAX_LIBS];
} closure_pkg_t;

typedef struct {
  const c8* root;
  spn_target_kind_t target;
  const c8* root_deps [CLOSURE_TEST_MAX_DEPS];
  closure_pkg_t pkgs [CLOSURE_TEST_MAX_PKGS];
} closure_graph_test_t;

typedef struct {
  sp_mem_t mem;
  spn_pkg_unit_t* pkgs [CLOSURE_TEST_MAX_PKGS];
  u32 count;
  spn_target_unit_t* root;
} closure_graph_t;

// Shared by suites whose expectation is an ordered list of targets by name
typedef struct {
  closure_graph_test_t graph;
  const c8* expect [CLOSURE_TEST_MAX_NAMES];
} target_list_test_t;

closure_graph_t build_graph(closure_graph_test_t* t);
spn_pkg_unit_t* find_pkg(spn_pkg_unit_t** pkgs, u32 count, const c8* name);
spn_target_unit_t* find_lib(spn_pkg_unit_t** pkgs, u32 count, const c8* name);
void expect_target_names(s32* utest_result, sp_da(spn_target_unit_t*) targets, const c8* const* expect, u32 max);
