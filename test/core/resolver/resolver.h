#pragma once

#include "spn_test.h"

#include "spn.h"
#include "event/types.h"
#include "index/types.h"
#include "intern/types.h"
#include "resolve/types.h"
#include "semver/types.h"
#include "when/types.h"

/////////////////
// DESCRIPTOR //
////////////////
typedef struct {
  const c8* str;
  bool b;
  bool is_bool;
} value_lit_t;

typedef struct {
  const c8* key;
  const c8* str;
  bool b;
  bool is_bool;
  bool negated;
} clause_lit_t;

typedef struct {
  const c8* namespace;
  const c8* name;
  const c8* version;
  spn_index_dep_kind_t kind;
  bool private;
  clause_lit_t when[2];
} index_dep_t;

typedef struct {
  const c8* name;
  spn_linkage_t linkages[4];
} index_target_t;

typedef struct {
  const c8* name;
  spn_option_type_t type;
  const c8* values[4];
  value_lit_t fallback;
} index_option_t;

typedef struct {
  spn_semver_t version;
  bool yanked;
  index_dep_t deps[4];
  index_target_t targets[2];
  index_option_t options[2];
} index_rel_t;

typedef struct {
  const c8* namespace;
  const c8* name;
  index_rel_t releases[8];
} index_pkg_t;

typedef struct {
  const c8* name;
  const c8* version;
  spn_dep_kind_t kind;
  bool private;
  clause_lit_t when[2];
} manifest_dep_t;

typedef struct {
  manifest_dep_t package[8];
} manifest_deps_t;

// [config.$pkg] overrides for the root manifest
typedef struct {
  const c8* name;
  spn_linkage_t kind;
} config_kind_t;

// Payload of the expected unsatisfiable event: selected set expects a
// conflict against that version; selected unset expects a plain
// no-version-in-range failure
typedef struct {
  const c8* namespace;
  const c8* name;
  const c8* requester;
  const c8* selected;
} expect_unsat_t;

typedef struct {
  const c8* name;
  index_pkg_t index[8];
  struct {
    manifest_deps_t deps;
  } manifest;
  spn_linkage_t linkage;
  spn_when_facts_t facts;
  config_kind_t config[4];
  u64 budget;
  spn_err_t err;
  spn_build_event_kind_t event;
  expect_unsat_t unsat;
} fixture_t;

typedef struct {
  spn_resolve_query_t query;
  sp_intern_t* intern;
  sp_da(spn_build_event_t) events;
  spn_err_t err;
} resolve_result_t;

sp_err_t run_fixture(sp_test_t* t, const fixture_t* fixture);
