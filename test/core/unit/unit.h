#pragma once

#include "spn_test.h"

#include "ctx/types.h"
#include "external/wasm/types.h"
#include "intern/intern.h"
#include "session/session.h"
#include "session/types.h"
#include "unit/types.h"
#include "unit/unit.h"

#define UNIT_TEST_MAX_PKGS 8
#define UNIT_TEST_MAX_DEPS 8
#define UNIT_TEST_MAX_LIBS 4
#define UNIT_TEST_MAX_STRS 8

#define STATIC_ONLY ((spn_linkage_set_t) { .static_lib = true })
#define SHARED_ONLY ((spn_linkage_set_t) { .shared = true })
#define OBJECT_ONLY ((spn_linkage_set_t) { .object = true })
#define SOURCE_ONLY ((spn_linkage_set_t) { .source = true })

typedef struct {
  const c8* to;
  spn_dep_kind_t kind;
  bool private;
} unit_dep_t;

typedef struct {
  const c8* name;
  spn_linkage_set_t linkages;
  bool no_link;
  const c8* source [UNIT_TEST_MAX_STRS];
  const c8* deps [UNIT_TEST_MAX_STRS];
  const c8* frameworks [UNIT_TEST_MAX_STRS];
  spn_os_version_t min_os;
} unit_lib_t;

typedef struct {
  const c8* name;
  bool scripts;
  unit_dep_t deps [UNIT_TEST_MAX_DEPS];
  unit_lib_t libs [UNIT_TEST_MAX_LIBS];
  const c8* system_deps [UNIT_TEST_MAX_STRS];
  const c8* frameworks [UNIT_TEST_MAX_STRS];
  spn_os_version_t min_os;
} unit_pkg_t;

typedef struct {
  spn_os_t os; // zero selects linux
  const c8* sysroot;
  unit_pkg_t pkgs [UNIT_TEST_MAX_PKGS]; // pkgs[0] is the root package
} unit_graph_test_t;

spn_session_t* build_session(sp_mem_t mem, unit_graph_test_t* g);
spn_pkg_id_t find_pkg_id(spn_session_t* s, unit_graph_test_t* g, const c8* name);
sp_da(sp_str_t) test_str_list(sp_mem_t mem, const c8* const* items, u32 max);
sp_da(spn_tree_path_t) test_tree_path_list(sp_mem_t mem, const c8* const* items, u32 max);
