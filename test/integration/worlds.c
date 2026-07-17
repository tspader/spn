#include "common.h"

// The review contract for worlds.md cut 0 (.llm/doc/resolve/worlds.md): every
// test is skipped until its cut lands, then un-skipped verbatim. Invariant
// numbers refer to that document.

SPN_TEST_SUITE(worlds)

// I0.1: type = "string" is rejected at load naming the option
UTEST_F(worlds, cut0_string_option_rejected) {
  UTEST_SKIP("worlds cut 0");
  tmpfs_init_named(&uf->fixture.fs, "worlds_string_rejected");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/worlds/string_rejected",
    .builds = {
      { .expect = { .rc = 1, .contains = { "kram" } } },
    },
  });
}

// I0.5: a when referencing a key that is neither a fact nor a declared
// option of the consuming package is a manifest error, not a silently-FALSE
// clause
UTEST_F(worlds, cut0_undeclared_when_key) {
  UTEST_SKIP("worlds cut 0");
  tmpfs_init_named(&uf->fixture.fs, "worlds_undeclared_key");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/worlds/undeclared_key",
    .builds = {
      { .expect = { .rc = 1, .contains = { "kram" } } },
    },
  });
}

// I0.5: clause values accept lists — membership on the positive form,
// complement on the not form; wrong inclusion is a duplicate symbol, wrong
// exclusion an undefined reference
UTEST_F(worlds, cut0_when_list_membership) {
  UTEST_SKIP("worlds cut 0");
  tmpfs_init_named(&uf->fixture.fs, "worlds_when_list");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/worlds/when_list",
    .builds = {
      { .expect = { .bin = { .name = "main" } } },
    },
  });
}

// I0.5: every listed value must live in the key's declared domain
UTEST_F(worlds, cut0_when_list_outside_domain) {
  UTEST_SKIP("worlds cut 0");
  tmpfs_init_named(&uf->fixture.fs, "worlds_when_list_domain");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/worlds/when_list_domain",
    .builds = {
      { .expect = { .rc = 1, .contains = { "qux" } } },
    },
  });
}

// I0.6: a default arm referencing an additive bool is a manifest error
// naming the arm's option and the bool
UTEST_F(worlds, cut0_default_arm_references_bool) {
  UTEST_SKIP("worlds cut 0");
  tmpfs_init_named(&uf->fixture.fs, "worlds_default_bool_arm");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/worlds/default_bool_arm",
    .builds = {
      { .expect = { .rc = 1, .contains = { "spum", "kram" } } },
    },
  });
}

// I0.6: a cycle in same-package default-arm references is a manifest error
// naming the options on the cycle
UTEST_F(worlds, cut0_default_arm_cycle) {
  UTEST_SKIP("worlds cut 0");
  tmpfs_init_named(&uf->fixture.fs, "worlds_default_cycle");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/worlds/default_cycle",
    .builds = {
      { .expect = { .rc = 1, .contains = { "spum", "kram" } } },
    },
  });
}

// I0.7: x = false in a dep edge's options is a load error naming edge and
// bool — edges may only demand
UTEST_F(worlds, cut0_edge_prohibition_rejected) {
  UTEST_SKIP("worlds cut 0");
  tmpfs_init_named(&uf->fixture.fs, "worlds_edge_prohibition");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/worlds/edge_prohibition",
    .builds = {
      { .expect = { .rc = 1, .contains = { "spum", "kram" } } },
    },
  });
}

// I0.8: an edge carrying enum options entries must have a when free of
// additive-bool clauses; violation names edge, bool, and enum
UTEST_F(worlds, cut0_enum_constraint_under_bool_gate) {
  UTEST_SKIP("worlds cut 0");
  tmpfs_init_named(&uf->fixture.fs, "worlds_enum_under_bool");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/worlds/enum_under_bool",
    .builds = {
      { .expect = { .rc = 1, .contains = { "spum", "kram", "grum" } } },
    },
  });
}

// I0.5a: edge constraint values accept gated arms — the forwarding idiom.
// Root config picks vk on the forwarder, whose arm forwards vk to the dep.
UTEST_F(worlds, cut0_constraint_arms_forward) {
  UTEST_SKIP("worlds cut 0");
  tmpfs_init_named(&uf->fixture.fs, "worlds_constraint_arms");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/worlds/constraint_arms",
    .builds = {
      { .expect = { .bin = { .name = "main" } } },
    },
  });
}

// I0.11: an undemanded peer imposes nothing — the out-of-range provider
// resolves, builds, and the consumer compiles without the peer
UTEST_F(worlds, cut0_peer_undemanded_inert) {
  UTEST_SKIP("worlds cut 0");
  tmpfs_init_named(&uf->fixture.fs, "worlds_peer_undemanded");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/worlds/peer_undemanded",
    .builds = {
      { .expect = { .bin = { .name = "main" } } },
    },
  });
}

// I0.11: a demanded peer with an in-range provider validates and compiles
// against the scope's instance
UTEST_F(worlds, cut0_peer_demanded_in_range) {
  UTEST_SKIP("worlds cut 0");
  tmpfs_init_named(&uf->fixture.fs, "worlds_peer_demanded");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/worlds/peer_demanded",
    .builds = {
      { .expect = { .bin = { .name = "main" } } },
    },
  });
}

// I0.11: demanded with the provider out of range is an error naming the
// edge, range, and provider version. The index also holds an in-range 1.5.0:
// a peer range that leaked into candidate selection would downgrade to it
// and build, so the error is proof the range reached no selection.
UTEST_F(worlds, cut0_peer_demanded_out_of_range) {
  UTEST_SKIP("worlds cut 0");
  tmpfs_init_named(&uf->fixture.fs, "worlds_peer_out_of_range");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/worlds/peer_out_of_range",
    .builds = {
      { .expect = { .rc = 1, .contains = { "spum", "2.0.0" } } },
    },
  });
}

// I0.11: demanded with no provider in scope is the absent-provider error
UTEST_F(worlds, cut0_peer_demanded_absent) {
  UTEST_SKIP("worlds cut 0");
  tmpfs_init_named(&uf->fixture.fs, "worlds_peer_absent");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/worlds/peer_absent",
    .builds = {
      { .expect = { .rc = 1, .contains = { "spum" } } },
    },
  });
}

// I0.11: peer + private is a load error — the provider is by definition not
// the consumer's own
UTEST_F(worlds, cut0_peer_private_rejected) {
  UTEST_SKIP("worlds cut 0");
  tmpfs_init_named(&uf->fixture.fs, "worlds_peer_private");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/worlds/peer_private",
    .builds = {
      { .expect = { .rc = 1, .contains = { "spum" } } },
    },
  });
}

// I0.5: enum-list clauses gate dep edges in both polarities — the membership
// edge resolves, the complement edge to a package that exists nowhere prunes
UTEST_F(worlds, cut0_when_list_gates_dep) {
  UTEST_SKIP("worlds cut 0");
  tmpfs_init_named(&uf->fixture.fs, "worlds_when_list_dep");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/worlds/when_list_dep",
    .builds = {
      { .expect = { .bin = { .name = "main" } } },
    },
  });
}

// I0.5: an undeclared key at a non-dep site (target gated list) is the same
// manifest error as on a dep gate
UTEST_F(worlds, cut0_undeclared_key_gated_list) {
  UTEST_SKIP("worlds cut 0");
  tmpfs_init_named(&uf->fixture.fs, "worlds_undeclared_gated_list");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/worlds/undeclared_gated_list",
    .builds = {
      { .expect = { .rc = 1, .contains = { "grum" } } },
    },
  });
}

// I0.1: non-additive bool forms are rejected at load like strings
UTEST_F(worlds, cut0_nonadditive_bool_rejected) {
  UTEST_SKIP("worlds cut 0");
  tmpfs_init_named(&uf->fixture.fs, "worlds_bool_nonadditive");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/worlds/bool_nonadditive",
    .builds = {
      { .expect = { .rc = 1, .contains = { "kram" } } },
    },
  });
}

// I0.5: a dep gate testing an additive bool negatively is a manifest error —
// the negation of a feature is spelled as an enum
UTEST_F(worlds, cut0_negated_bool_gate_rejected) {
  UTEST_SKIP("worlds cut 0");
  tmpfs_init_named(&uf->fixture.fs, "worlds_bool_negated_gate");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/worlds/bool_negated_gate",
    .builds = {
      { .expect = { .rc = 1, .contains = { "kram", "spum" } } },
    },
  });
}

// I0.8's legal half, live today and after every cut: a bool demand riding a
// bool-gated edge — chained gating settles in the prune
UTEST_F(worlds, bool_demand_on_gated_edge) {
  tmpfs_init_named(&uf->fixture.fs, "worlds_bool_demand_on_gated");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/worlds/bool_demand_on_gated",
    .builds = {
      { .expect = { .bin = { .name = "main" } } },
    },
  });
}
