#include "common.h"

SPN_TEST_SUITE(worlds)

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

UTEST_F(worlds, bool_demand_on_gated_edge) {
  tmpfs_init_named(&uf->fixture.fs, "worlds_bool_demand_on_gated");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/worlds/bool_demand_on_gated",
    .builds = {
      { .expect = { .bin = { .name = "main" } } },
    },
  });
}
