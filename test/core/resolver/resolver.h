#pragma once

#include "sp/sp_test.h"

#include "spn.h"
#include "event/types.h"
#include "index/types.h"
#include "intern/types.h"
#include "resolve/types.h"
#include "semver/types.h"
#include "when/types.h"


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
  spn_linkage_t linkage;
  spn_when_facts_t facts;
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
