typedef struct {
  const c8* event;
  const c8* key;
  const c8* value;
  bool absent;
} opt_event_t;

typedef struct {
  const c8* profile;
  const c8* manifest;
  struct {
    s32 rc;
    struct { const c8* name; s32 rc; } bin;
    const c8* contains [4];
    opt_event_t events [2];
  } expect;
} opt_build_t;

typedef struct {
  const c8* project;
  const c8* copy [4];
  opt_build_t builds [3];
} opt_test_t;

static void run_opt_test(s32* utest_result, fixture_t* fixture, opt_test_t test) {
  (void)utest_result;
  (void)fixture;
  (void)test;
}

SPN_TEST_SUITE(when)

// Every build fact resolves from the profile: os/arch/mode gate defines that
// must be present, and a windows-gated define must be absent. abi is covered
// in not_form, where the assertion holds under both gnu and musl.
UTEST_F(when, facts) {
  tmpfs_init_named(&uf->fixture.fs, "when_facts");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/when/facts",
    .builds = {
      { .expect = { .bin = { .name = "main" } } },
    },
  });
}

// { not = v }: abi != msvc holds on the host, os != linux does not
UTEST_F(when, not_form) {
  tmpfs_init_named(&uf->fixture.fs, "when_not_form");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/when/not_form",
    .builds = {
      { .expect = { .bin = { .name = "main" } } },
    },
  });
}

// Multiple keys AND: { os = "linux", mode = "release" } is false in debug
// even though os matches (an OR evaluator returns 20 on the first build),
// true in release
UTEST_F(when, and_form) {
  tmpfs_init_named(&uf->fixture.fs, "when_and_form");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/when/and_form",
    .builds = {
      { .expect = { .bin = { .name = "main", .rc = 10 } } },
      { .profile = "release", .expect = { .bin = { .name = "main", .rc = 20 } } },
    },
  });
}

// Gated source entries: both impl files define impl_value, so failing to
// exclude the windows one is a duplicate-symbol link error and failing to
// include the linux one is an undefined reference
UTEST_F(when, source) {
  tmpfs_init_named(&uf->fixture.fs, "when_source");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/when/source",
    .copy = { "impl_linux.c", "impl_windows.c" },
    .builds = {
      { .expect = { .bin = { .name = "main" } } },
    },
  });
}

// Gated flags entries: the msvc-gated /DFLAG_WINDOWS is not a valid gcc
// argument, so failing to exclude it breaks the compile; the linux-gated
// define proves inclusion
UTEST_F(when, flags) {
  tmpfs_init_named(&uf->fixture.fs, "when_flags");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/when/flags",
    .builds = {
      { .expect = { .bin = { .name = "main" } } },
    },
  });
}

// Gated entries in the package system_deps list: ws2_32 does not exist on
// linux, so failing to exclude the false arm breaks the link. The true arm
// is c++ rather than m because musl folds libm into libc — main references
// std::terminate by mangled name, which only resolves if the matching entry
// survives filtering.
UTEST_F(when, system_deps) {
  tmpfs_init_named(&uf->fixture.fs, "when_system_deps");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/when/system_deps",
    .builds = {
      { .expect = { .bin = { .name = "main" } } },
    },
  });
}

// Gated dependency edges: wsock has no package in the index, so the build
// succeeds only if the false edge is pruned before resolution ever looks at
// it; plat proves the true edge resolves and links
UTEST_F(when, dep_edge) {
  tmpfs_init_named(&uf->fixture.fs, "when_dep_edge");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/when/dep_edge",
    .builds = {
      { .expect = { .bin = { .name = "main" } } },
    },
  });
}

SPN_TEST_SUITE(options)

// A declared bool defaults on and its define reaches the dep's own TUs; no
// consumer says anything
UTEST_F(options, declare_default) {
  tmpfs_init_named(&uf->fixture.fs, "options_declare_default");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/options/declare_default",
    .builds = {
      { .expect = { .bin = { .name = "main" } } },
    },
  });
}

// [config.codec] overrides the dep's default (rc 10 proves the root won, not
// the default), and swapping the root manifest flips the built behavior —
// option values participate in staleness, a stale store returns 10 twice
UTEST_F(options, root_set) {
  tmpfs_init_named(&uf->fixture.fs, "options_root_set");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/options/root_set",
    .copy = { "spn.on.toml" },
    .builds = {
      { .expect = { .bin = { .name = "main", .rc = 10 } } },
      { .manifest = "spn.on.toml", .expect = { .bin = { .name = "main", .rc = 20 } } },
    },
  });
}

// First-match default lists: one option's when-arm matches the host and one
// falls through to the bare arm; opposite orderings catch an evaluator that
// blindly takes the first or the last entry
UTEST_F(options, default_when) {
  tmpfs_init_named(&uf->fixture.fs, "options_default_when");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/options/default_when",
    .builds = {
      { .expect = { .bin = { .name = "main" } } },
    },
  });
}

// Options join the when key domain: the enum value gates which backend
// source compiles inside the dep. Both backends define render_backend, so a
// wrong or double selection fails the link, and the root picks the
// non-default so the default can't mask a no-op
UTEST_F(options, enum_gate) {
  tmpfs_init_named(&uf->fixture.fs, "options_enum_gate");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/options/enum_gate",
    .builds = {
      { .expect = { .bin = { .name = "main" } } },
    },
  });
}

// Setting an option the dep never declared is a load-time error naming it
UTEST_F(options, undeclared) {
  tmpfs_init_named(&uf->fixture.fs, "options_undeclared");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/options/undeclared",
    .builds = {
      { .expect = { .rc = 1, .contains = { "nosuch" } } },
    },
  });
}

// Setting an enum outside its declared values is a load-time error naming
// the value
UTEST_F(options, bad_value) {
  tmpfs_init_named(&uf->fixture.fs, "options_bad_value");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/options/bad_value",
    .builds = {
      { .expect = { .rc = 1, .contains = { "dx12" } } },
    },
  });
}

// define + public = true: the option's define lands in the consumer's own
// compile, not just the dep's. The general propagation machinery is pinned
// in reexport; this pins the option -> public define wiring specifically.
UTEST_F(options, public_define) {
  tmpfs_init_named(&uf->fixture.fs, "options_public_define");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/options/public_define",
    .builds = {
      { .expect = { .bin = { .name = "main" } } },
    },
  });
}

// The root's own option gates its dep edge and a profile flips it: default
// build never resolves tracer (rc 10, no resolve event), -p tracing resolves
// and links it (rc 20). Version 1.0.0 identifies tracer; the root is 0.1.0.
UTEST_F(options, gates_dep) {
  tmpfs_init_named(&uf->fixture.fs, "options_gates_dep");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/options/gates_dep",
    .builds = {
      {
        .expect = {
          .bin = { .name = "main", .rc = 10 },
          .events = {
            { .event = "resolve_package", .key = "version", .value = "1.0.0", .absent = true },
          },
        },
      },
      {
        .profile = "tracing",
        .expect = {
          .bin = { .name = "main", .rc = 20 },
          .events = {
            { .event = "resolve_package", .key = "version", .value = "1.0.0" },
          },
        },
      },
    },
  });
}

// A consumer edge requests a value with no root config in play: the request
// alone flips the dep off its default
UTEST_F(options, edge_request) {
  tmpfs_init_named(&uf->fixture.fs, "options_edge_request");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/options/edge_request",
    .builds = {
      { .expect = { .bin = { .name = "main" } } },
    },
  });
}

// additive = true: liba requests audio = true and libb requests audio =
// false, which a strict agree-or-error merge rejects; the union resolves
// true. libb's video rides along, so caps == 3 through both consumers.
UTEST_F(options, additive) {
  tmpfs_init_named(&uf->fixture.fs, "options_additive");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/options/additive",
    .builds = {
      { .expect = { .bin = { .name = "main" } } },
    },
  });
}

// Non-additive disagreement with no tiebreaker: liba wants gl, libb wants
// vk, resolve fails naming the option and both requesting sides
UTEST_F(options, conflict) {
  tmpfs_init_named(&uf->fixture.fs, "options_conflict");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/options/conflict",
    .builds = {
      { .expect = { .rc = 1, .contains = { "backend", "liba", "libb" } } },
    },
  });
}

// The identical topology settles when the root sets the value: liba's gl
// request loses to the root, both consumers observe vk
UTEST_F(options, tiebreak) {
  tmpfs_init_named(&uf->fixture.fs, "options_tiebreak");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/options/tiebreak",
    .builds = {
      { .expect = { .bin = { .name = "main" } } },
    },
  });
}

// Root contradicting an edge's { not = ... } request is an error naming both
// sides, not a silent root win: the root is the tiebreaker between edges,
// never an override of them
UTEST_F(options, root_veto) {
  tmpfs_init_named(&uf->fixture.fs, "options_root_veto");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/options/root_veto",
    .builds = {
      { .expect = { .rc = 1, .contains = { "ssl", "liba" } } },
    },
  });
}

// defaults = false declines the dep's defaults wholesale; only the edge's
// ogg request survives, so caps == 2 rather than 3
UTEST_F(options, defaults_false) {
  tmpfs_init_named(&uf->fixture.fs, "options_defaults_false");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/options/defaults_false",
    .builds = {
      { .expect = { .bin = { .name = "main" } } },
    },
  });
}
