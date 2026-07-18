#include "common.h"
#include "intern/intern.h"
#include "pkg/id.h"
#include "session/session.h"
#include "session/types.h"

typedef struct {
  spn_err_t err;
  bool event;
  spn_option_err_t option_err;
  const c8* pkg;
  spn_option_setter_kind_t setter;
  const c8* setter_name;
  u32 resolves;
  bool reresolve;
} session_expect_t;

typedef struct {
  const c8* config;
  bool stale_loaded;
  bool direct_dep;
  bool gated_dep;
  u32 resolves;
  session_expect_t expect;
} session_test_t;

spn_pkg_name_t spn_pkg_name_from_qualified(sp_str_t qualified) {
  sp_str_pair_t pair = sp_str_cleave_c8(qualified, '/');
  return (spn_pkg_name_t) {
    .namespace = pair.first,
    .name = sp_str_empty(pair.second) ? pair.first : pair.second,
  };
}

static spn_pkg_id_t make_id(sp_intern_t* intern, const c8* qualified) {
  return (spn_pkg_id_t) {
    .qualified = sp_intern_get_or_insert(intern, sp_cstr_as_str(qualified)),
  };
}

static void run_session_test(s32* utest_result, session_test_t test) {
  sp_mem_t mem = sp_mem_os_new();
  sp_intern_t* intern = sp_intern_new(mem);

  spn_pkg_info_t root = {
    .name = sp_str_lit("test"),
    .qualified = sp_str_lit("core/test"),
    .deps = sp_da_new(mem, spn_requested_dep_t),
    .config = sp_da_new(mem, spn_pkg_config_entry_t),
  };
  if (test.config) {
    sp_da_push(root.config, ((spn_pkg_config_entry_t) {
      .key = sp_cstr_as_str(test.config),
    }));
  }
  if (test.direct_dep || test.gated_dep) {
    spn_requested_dep_t dep = {
      .qualified = sp_str_lit("core/spum"),
      .kind = SPN_DEP_KIND_PACKAGE,
    };
    if (test.gated_dep) {
      dep.when.clauses = sp_da_new(mem, spn_when_clause_t);
      sp_da_push(dep.when.clauses, ((spn_when_clause_t) {
        .key = sp_str_lit("os"),
        .value = spn_option_value_str(sp_str_lit("linux")),
      }));
    }
    sp_da_push(root.deps, dep);
  }

  spn_pkg_id_t root_id = make_id(intern, "core/test");
  spn_resolved_pkg_t root_node = {
    .id = root_id,
    .name = root.name,
    .source = SPN_PKG_SOURCE_ROOT,
    .edges = sp_da_new(mem, spn_resolved_dep_t),
  };
  spn_loaded_pkg_t root_loaded = {
    .source = SPN_PKG_SOURCE_ROOT,
    .info = &root,
  };

  spn_session_t session = {
    .mem = mem,
    .intern = intern,
    .pkg = &root,
    .events = spn_event_buffer_new(mem),
    .gates = { .resolves = test.resolves },
    .profile = { .os = SPN_OS_LINUX },
  };
  sp_ht_init(mem, session.resolve);
  sp_ht_init(mem, session.packages);
  sp_ht_insert(session.resolve, root_id, root_node);
  sp_ht_insert(session.packages, root_id, root_loaded);

  spn_pkg_info_t stale = {
    .name = sp_str_lit("spum"),
    .qualified = sp_str_lit("core/spum"),
    .deps = sp_da_new(mem, spn_requested_dep_t),
  };
  if (test.stale_loaded) {
    spn_pkg_id_t stale_id = make_id(intern, "core/spum");
    sp_ht_insert(session.packages, stale_id, ((spn_loaded_pkg_t) {
      .source = SPN_PKG_SOURCE_INDEX,
      .info = &stale,
    }));
  }

  spn_err_t err = spn_session_apply_options(&session);
  EXPECT_EQ(err, test.expect.err);
  EXPECT_EQ(session.gates.resolves, test.expect.resolves);
  EXPECT_EQ(session.gates.reresolve, test.expect.reresolve);

  sp_da(spn_build_event_t) events = spn_event_buffer_drain(mem, session.events);
  EXPECT_EQ(sp_da_size(events), test.expect.event ? 1 : 0);
  if (!test.expect.event || sp_da_empty(events)) {
    return;
  }

  spn_build_event_t* event = &events[0];
  EXPECT_EQ(event->kind, SPN_EVENT_ERR_OPTION);
  EXPECT_EQ(event->option.err, test.expect.option_err);
  EXPECT_TRUE(sp_str_equal_cstr(event->option.pkg, test.expect.pkg));
  EXPECT_EQ(event->option.a.kind, test.expect.setter);
  if (test.expect.setter_name) {
    EXPECT_TRUE(sp_str_equal_cstr(event->option.a.name, test.expect.setter_name));
  }
}

UTEST(options_session, late_gate_resolve_cap) {
  run_session_test(utest_result, (session_test_t) {
    .gated_dep = true,
    .resolves = 4,
    .expect = {
      .err = SPN_ERROR,
      .event = true,
      .option_err = SPN_OPTION_ERR_LATE_GATE,
      .pkg = "test",
      .setter = SPN_OPTION_SETTER_CONSUMER,
      .setter_name = "spum",
      .resolves = 4,
    },
  });
}

UTEST(options_session, unknown_config_package) {
  run_session_test(utest_result, (session_test_t) {
    .config = "spum",
    .expect = {
      .err = SPN_ERROR,
      .event = true,
      .option_err = SPN_OPTION_ERR_UNKNOWN_PKG,
      .pkg = "spum",
    },
  });
}

UTEST(options_session, stale_loaded_unresolved_config_package) {
  run_session_test(utest_result, (session_test_t) {
    .config = "spum",
    .stale_loaded = true,
    .expect = {
      .err = SPN_ERROR,
      .event = true,
      .option_err = SPN_OPTION_ERR_UNKNOWN_PKG,
      .pkg = "spum",
    },
  });
}

UTEST(options_session, absent_direct_dependency_config_package) {
  run_session_test(utest_result, (session_test_t) {
    .config = "spum",
    .direct_dep = true,
  });
}
