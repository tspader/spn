#include "harness.h"

sp_test(options, when) {
  return run_opt_test(t, (opt_test_t) {
    .project = "test/integration/fixtures/options/when",
    .copy = { "src/*", "packages/*" },
    .builds = {
      { .present = true },
      { .alternate = true },
    },
  });
}

sp_test(options, public_define) {
  return run_opt_test(t, (opt_test_t) {
    .project = "test/integration/fixtures/options/public_define",
    .builds = { { .present = true } },
  });
}

sp_test(options, gates_dep) {
  return run_opt_test(t, (opt_test_t) {
    .project = "test/integration/fixtures/options/gates_dep",
    .builds = {
      { .present = true },
      { .profile = "tracing" },
    },
  });
}

sp_test(options, additive) {
  return run_opt_test(t, (opt_test_t) {
    .project = "test/integration/fixtures/options/additive",
    .copy = { "main.off.c", "spn.off.toml" },
    .builds = {
      { .present = true },
      { .manifest = "spn.off.toml" },
    },
  });
}

sp_test(options, edge_gates_dep) {
  return run_opt_test(t, (opt_test_t) {
    .project = "test/integration/fixtures/options/edge_gates_dep",
    .copy = { "vendor/*" },
    .builds = { { .present = true } },
  });
}

sp_test(options, index_gated_dep) {
  return run_opt_test(t, (opt_test_t) {
    .project = "test/integration/fixtures/options/index_gated_dep",
    .builds = { { .present = true } },
  });
}

sp_test(options, index_eager_gate) {
  return run_opt_test(t, (opt_test_t) {
    .project = "test/integration/fixtures/options/index_eager_gate",
    .builds = { { .present = true } },
  });
}

sp_test(options, edge_gates_build_dep) {
  return run_opt_test(t, (opt_test_t) {
    .project = "test/integration/fixtures/options/edge_gates_build_dep",
    .copy = { "vendor/*" },
    .builds = { { .present = true } },
  });
}

sp_test(options, dep_rebuild) {
  return run_opt_test(t, (opt_test_t) {
    .project = "test/integration/fixtures/options/dep_rebuild",
    .copy = { "main.on.c", "spn.on.toml", "spn.off.toml" },
    .builds = {
      { .present = true },
      { .manifest = "spn.on.toml" },
      { .manifest = "spn.off.toml" },
    },
  });
}

sp_test(options, fact_identity) {
  return run_opt_test(t, (opt_test_t) {
    .project = "test/integration/fixtures/options/fact_identity",
    .builds = {
      { .present = true },
      { .profile = "release" },
      { .present = true },
    },
  });
}

sp_test(options, default_identity) {
  return run_opt_test(t, (opt_test_t) {
    .project = "test/integration/fixtures/options/default_identity",
    .copy = { "main.on.c", "spn.on.toml" },
    .builds = {
      { .present = true },
      { .manifest = "spn.on.toml" },
    },
  });
}

sp_test(options, private_versions) {
  return run_opt_test(t, (opt_test_t) {
    .project = "test/integration/fixtures/options/private_versions",
    .when.msvc_todo = true,
    .builds = { { .present = true } },
  });
}
