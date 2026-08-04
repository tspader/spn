#include "options.h"

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
  const c8* name;
  const c8* config;
  bool stale_loaded;
  bool direct_dep;
  bool gated_dep;
  u32 resolves;
  session_expect_t expect;
} session_test_t;

sp_test_suite(options_session, .serial = true);

static spn_pkg_id_t make_id(sp_intern_t* intern, const c8* qualified) {
  return (spn_pkg_id_t) {
    .qualified = sp_intern_get_or_insert(intern, sp_cstr_as_str(qualified)),
  };
}

static const session_test_t tests [] = {
  {
    .name = "late_gate_resolve_cap",
    .gated_dep = true,
    .resolves = 4,
    .expect = {
      .err = SPN_ERR_OPTION,
      .event = true,
      .option_err = SPN_OPTION_ERR_LATE_GATE,
      .pkg = "test",
      .setter = SPN_OPTION_SETTER_CONSUMER,
      .setter_name = "spum",
      .resolves = 4,
    },
  },
  {
    .name = "unknown_config_package",
    .config = "spum",
    .expect = {
      .err = SPN_ERR_OPTION,
      .event = true,
      .option_err = SPN_OPTION_ERR_UNKNOWN_PKG,
      .pkg = "spum",
    },
  },
  {
    .name = "stale_loaded_unresolved_config_package",
    .config = "spum",
    .stale_loaded = true,
    .expect = {
      .err = SPN_ERR_OPTION,
      .event = true,
      .option_err = SPN_OPTION_ERR_UNKNOWN_PKG,
      .pkg = "spum",
    },
  },
  {
    .name = "absent_direct_dependency_config_package",
    .config = "spum",
    .direct_dep = true,
  },
};

sp_test_each(options_session, apply, session_test_t, tests, .setup = spn_test_ctx_setup) {
  sp_mem_t mem = sp_test_arena(t);
  sp_intern_t* intern = spn.intern;

  spn_pkg_info_t root = {
    .name = sp_str_lit("test"),
    .qualified = sp_str_lit("core/test"),
    .deps = sp_da_new(mem, spn_requested_dep_t),
    .config = sp_da_new(mem, spn_pkg_config_entry_t),
  };
  if (it->config) {
    sp_da_push(root.config, ((spn_pkg_config_entry_t) {
      .key = sp_cstr_as_str(it->config),
    }));
  }
  if (it->direct_dep || it->gated_dep) {
    spn_requested_dep_t dep = {
      .qualified = sp_str_lit("core/spum"),
      .kind = SPN_DEP_KIND_PACKAGE,
    };
    if (it->gated_dep) {
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
    .ctx = &spn,
    .mem = mem,
    .pkg = &root,
    .gates = { .resolves = it->resolves },
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
  if (it->stale_loaded) {
    spn_pkg_id_t stale_id = make_id(intern, "core/spum");
    sp_ht_insert(session.packages, stale_id, ((spn_loaded_pkg_t) {
      .source = SPN_PKG_SOURCE_INDEX,
      .info = &stale,
    }));
  }

  spn_err_t err = spn_session_apply_options(&session).kind;
  sp_expect_eq(t, err, it->expect.err);
  sp_expect_eq(t, session.gates.resolves, it->expect.resolves);
  sp_expect_eq(t, session.gates.reresolve, it->expect.reresolve);

  sp_da(spn_build_event_t) events = spn_event_buffer_drain(mem, spn.events);
  sp_must_eq(t, sp_da_size(events), it->expect.event ? 1 : 0);
  if (!it->expect.event) {
    return SP_OK;
  }

  spn_build_event_t* event = &events[0];
  sp_expect_eq(t, event->kind, SPN_EVENT_ERR_OPTION);
  sp_expect_eq(t, event->option.err, it->expect.option_err);
  sp_expect(t, sp_str_equal_cstr(event->option.pkg, it->expect.pkg));
  sp_expect_eq(t, event->option.a.kind, it->expect.setter);
  if (it->expect.setter_name) {
    sp_expect(t, sp_str_equal_cstr(event->option.a.name, it->expect.setter_name));
  }

  return SP_OK;
}
