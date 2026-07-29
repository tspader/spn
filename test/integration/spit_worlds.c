sp_test(worlds, cut0_string_option_rejected) {
  return sp_test_skip(t, "worlds cut 0");
}

sp_test(worlds, cut0_undeclared_when_key) {
  return sp_test_skip(t, "worlds cut 0");
}

sp_test(worlds, cut0_when_list_membership) {
  return sp_test_skip(t, "worlds cut 0");
}

sp_test(worlds, cut0_when_list_outside_domain) {
  return sp_test_skip(t, "worlds cut 0");
}

sp_test(worlds, cut0_default_arm_references_bool) {
  return sp_test_skip(t, "worlds cut 0");
}

sp_test(worlds, cut0_default_arm_cycle) {
  return sp_test_skip(t, "worlds cut 0");
}

sp_test(worlds, cut0_edge_prohibition_rejected) {
  return sp_test_skip(t, "worlds cut 0");
}

sp_test(worlds, cut0_enum_constraint_under_bool_gate) {
  return sp_test_skip(t, "worlds cut 0");
}

sp_test(worlds, cut0_constraint_arms_forward) {
  return sp_test_skip(t, "worlds cut 0");
}

sp_test(worlds, cut0_peer_undemanded_inert) {
  return sp_test_skip(t, "worlds cut 0");
}

sp_test(worlds, cut0_peer_demanded_in_range) {
  return sp_test_skip(t, "worlds cut 0");
}

sp_test(worlds, cut0_peer_demanded_out_of_range) {
  return sp_test_skip(t, "worlds cut 0");
}

sp_test(worlds, cut0_peer_demanded_absent) {
  return sp_test_skip(t, "worlds cut 0");
}

sp_test(worlds, cut0_peer_private_rejected) {
  return sp_test_skip(t, "worlds cut 0");
}

sp_test(worlds, cut0_when_list_gates_dep) {
  return sp_test_skip(t, "worlds cut 0");
}

sp_test(worlds, cut0_undeclared_key_gated_list) {
  return sp_test_skip(t, "worlds cut 0");
}

sp_test(worlds, cut0_nonadditive_bool_rejected) {
  return sp_test_skip(t, "worlds cut 0");
}

sp_test(worlds, cut0_negated_bool_gate_rejected) {
  return sp_test_skip(t, "worlds cut 0");
}

sp_test(worlds, bool_demand_on_gated_edge) {
  return run_opt_test(t, (opt_test_t) {
    .project = "test/integration/fixtures/worlds/bool_demand_on_gated",
    .builds = {
      { .expect = { .bin = { .name = "main" } } },
    },
  });
}
