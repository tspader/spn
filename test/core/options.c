#define SP_IMPLEMENTATION
#include "sp.h"

#define UTEST_IMPLEMENTATION
#include "utest.h"

#include "test.h"
#include "spn.h"
#include "event/event.h"
#include "event/types.h"
#include "pkg/options.h"
#include "resolve/types.h"
#include "when/when.h"

UTEST_MAIN();

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
  clause_lit_t when [2];
  value_lit_t value;
} default_lit_t;

typedef struct {
  const c8* name;
  spn_option_type_t type;
  bool additive;
  const c8* values [4];
  default_lit_t defaults [3];
} option_decl_t;

typedef struct {
  const c8* consumer;
  clause_lit_t options [3];
} request_t;

typedef struct {
  const c8* name;
  value_lit_t value;
  bool is_default;
} expect_option_t;

typedef struct {
  spn_err_t err;
  spn_option_err_t option_err;
  const c8* option;
  expect_option_t options [4];
} merge_expect_t;

typedef struct {
  bool is_root;
  option_decl_t decls [4];
  clause_lit_t config [3];
  bool defaults_declined;
  clause_lit_t profile_options [3];
  spn_when_facts_t facts;
  request_t requests [4];
  merge_expect_t expect;
} merge_test_t;

static spn_option_value_t make_value(value_lit_t lit) {
  if (lit.str) {
    return spn_option_value_str(sp_str_view(lit.str));
  }
  if (lit.is_bool) {
    return spn_option_value_bool(lit.b);
  }
  return spn_option_value_none();
}

static spn_when_t make_when(sp_mem_t mem, const clause_lit_t* clauses, u64 count) {
  spn_when_t when = { .clauses = sp_da_new(mem, spn_when_clause_t) };
  for (u64 it = 0; it < count; it++) {
    if (!clauses[it].key) break;
    sp_da_push(when.clauses, ((spn_when_clause_t) {
      .key = sp_str_view(clauses[it].key),
      .negated = clauses[it].negated,
      .value = make_value((value_lit_t) { .str = clauses[it].str, .b = clauses[it].b, .is_bool = clauses[it].is_bool }),
    }));
  }
  return when;
}

void run_merge_test(s32* utest_result, merge_test_t t) {
  sp_mem_t mem = sp_mem_os_new();

  spn_resolved_pkg_t pkg = {
    .name = sp_str_lit("p"),
    .source = t.is_root ? SPN_PKG_SOURCE_ROOT : SPN_PKG_SOURCE_INDEX,
  };
  sp_carr_for(t.decls, it) {
    option_decl_t* decl = &t.decls[it];
    if (!decl->name) break;

    spn_option_info_t option = {
      .name = sp_str_view(decl->name),
      .type = decl->type,
      .additive = decl->additive,
      .values = sp_da_new(mem, sp_str_t),
      .defaults = sp_da_new(mem, spn_option_default_t),
    };
    sp_carr_for(decl->values, vt) {
      if (!decl->values[vt]) break;
      sp_da_push(option.values, sp_str_view(decl->values[vt]));
    }
    sp_carr_for(decl->defaults, dt) {
      default_lit_t* arm = &decl->defaults[dt];
      if (make_value(arm->value).kind == SPN_OPTION_VALUE_NONE) break;
      sp_da_push(option.defaults, ((spn_option_default_t) {
        .when = make_when(mem, arm->when, SP_CARR_LEN(arm->when)),
        .value = make_value(arm->value),
      }));
    }
    sp_str_om_insert(pkg.options, option.name, option);
  }

  spn_profile_info_t profile = {
    .os = t.facts.os,
    .arch = t.facts.arch,
    .abi = t.facts.abi,
    .mode = t.facts.mode,
    .opt = t.facts.opt,
    .sanitizers = t.facts.sanitizers,
    .options = make_when(mem, t.profile_options, SP_CARR_LEN(t.profile_options)),
  };

  sp_da(spn_pkg_config_entry_t) config = sp_da_new(mem, spn_pkg_config_entry_t);
  if (t.config[0].key || t.defaults_declined) {
    sp_da_push(config, ((spn_pkg_config_entry_t) {
      .key = sp_str_lit("p"),
      .value = {
        .options = make_when(mem, t.config, SP_CARR_LEN(t.config)),
        .defaults_declined = t.defaults_declined,
      },
    }));
  }

  spn_option_requests_t requests = sp_da_new(mem, spn_option_request_t);
  sp_carr_for(t.requests, it) {
    if (!t.requests[it].consumer) break;
    spn_when_t* options = (spn_when_t*)sp_alloc(mem, sizeof(spn_when_t));
    *options = make_when(mem, t.requests[it].options, SP_CARR_LEN(t.requests[it].options));
    sp_da_push(requests, ((spn_option_request_t) {
      .consumer = sp_str_view(t.requests[it].consumer),
      .options = options,
    }));
  }

  spn_event_buffer_t* events = spn_event_buffer_new(mem);
  spn_resolved_options_t resolved = sp_zero;
  spn_err_t err = spn_pkg_options_merge(mem, &pkg, &profile, config, requests, events, &resolved);
  EXPECT_EQ(err, t.expect.err);

  if (t.expect.err) {
    sp_da(spn_build_event_t) drained = spn_event_buffer_drain(mem, events);
    ASSERT_EQ(sp_da_size(drained), (u64)1);
    ASSERT_EQ(drained[0].kind, SPN_EVENT_ERR_OPTION);
    EXPECT_EQ(drained[0].option.err, t.expect.option_err);
    EXPECT_TRUE(sp_str_equal_cstr(drained[0].option.option, t.expect.option));
    return;
  }

  sp_carr_for(t.expect.options, it) {
    expect_option_t* expected = &t.expect.options[it];
    if (!expected->name) break;

    bool found = false;
    sp_da_for(resolved, rt) {
      if (!sp_str_equal_cstr(resolved[rt].name, expected->name)) continue;
      utest_kv("option", sp_str_view(expected->name));
      EXPECT_TRUE(spn_option_value_equal(resolved[rt].value, make_value(expected->value)));
      utest_kv("option", sp_str_view(expected->name));
      EXPECT_EQ(resolved[rt].is_default, expected->is_default);
      found = true;
      break;
    }
    utest_kv("option", sp_str_view(expected->name));
    EXPECT_TRUE(found);
  }
}

UTEST(options_merge, bool_defaults_false) {
  run_merge_test(utest_result, (merge_test_t) {
    .decls = {
      { .name = "x", .type = SPN_OPTION_TYPE_BOOL },
    },
    .expect = {
      .options = {
        { .name = "x", .value = { .is_bool = true }, .is_default = true },
      },
    },
  });
}

// Additive requests union: any true request wins over any false one, and the
// result is attributed to the union, not a single consumer
UTEST(options_merge, additive_unions) {
  run_merge_test(utest_result, (merge_test_t) {
    .decls = {
      { .name = "x", .type = SPN_OPTION_TYPE_BOOL, .additive = true },
    },
    .requests = {
      { .consumer = "a", .options = { { .key = "x", .is_bool = true } } },
      { .consumer = "b", .options = { { .key = "x", .is_bool = true, .b = true } } },
    },
    .expect = {
      .options = {
        { .name = "x", .value = { .is_bool = true, .b = true } },
      },
    },
  });
}

// A root config entry beats a consumer request on a non-additive option
// (today's tiebreak rule; worlds cut 0 turns this into a conflict)
UTEST(options_merge, config_overrides_request) {
  run_merge_test(utest_result, (merge_test_t) {
    .decls = {
      { .name = "e", .type = SPN_OPTION_TYPE_ENUM, .values = { "gl", "vk" }, .defaults = { { .value = { .str = "gl" } } } },
    },
    .config = { { "e", "vk" } },
    .requests = {
      { .consumer = "a", .options = { { "e", "gl" } } },
    },
    .expect = {
      .options = {
        { .name = "e", .value = { .str = "vk" } },
      },
    },
  });
}

// The profile options table is the root package's setter
UTEST(options_merge, profile_sets_root) {
  run_merge_test(utest_result, (merge_test_t) {
    .is_root = true,
    .decls = {
      { .name = "e", .type = SPN_OPTION_TYPE_ENUM, .values = { "gl", "vk" }, .defaults = { { .value = { .str = "gl" } } } },
    },
    .profile_options = { { "e", "vk" } },
    .expect = {
      .options = {
        { .name = "e", .value = { .str = "vk" } },
      },
    },
  });
}

// Two consumers disagree on a non-additive option with no tiebreaker
UTEST(options_merge, request_conflict) {
  run_merge_test(utest_result, (merge_test_t) {
    .decls = {
      { .name = "e", .type = SPN_OPTION_TYPE_ENUM, .values = { "gl", "vk" } },
    },
    .requests = {
      { .consumer = "a", .options = { { "e", "gl" } } },
      { .consumer = "b", .options = { { "e", "vk" } } },
    },
    .expect = {
      .err = SPN_ERROR,
      .option_err = SPN_OPTION_ERR_CONFLICT,
      .option = "e",
    },
  });
}

// A { not = v } request vetoes the settled value
UTEST(options_merge, veto_contradicted) {
  run_merge_test(utest_result, (merge_test_t) {
    .decls = {
      { .name = "e", .type = SPN_OPTION_TYPE_ENUM, .values = { "gl", "vk" }, .defaults = { { .value = { .str = "vk" } } } },
    },
    .requests = {
      { .consumer = "a", .options = { { .key = "e", .str = "vk", .negated = true } } },
    },
    .expect = {
      .err = SPN_ERROR,
      .option_err = SPN_OPTION_ERR_VETO,
      .option = "e",
    },
  });
}

// A veto that the settled value already avoids is inert
UTEST(options_merge, veto_respected) {
  run_merge_test(utest_result, (merge_test_t) {
    .decls = {
      { .name = "e", .type = SPN_OPTION_TYPE_ENUM, .values = { "gl", "vk" }, .defaults = { { .value = { .str = "gl" } } } },
    },
    .requests = {
      { .consumer = "a", .options = { { .key = "e", .str = "vk", .negated = true } } },
    },
    .expect = {
      .options = {
        { .name = "e", .value = { .str = "gl" }, .is_default = true },
      },
    },
  });
}

// Setting an undeclared option is an error naming it
UTEST(options_merge, undeclared) {
  run_merge_test(utest_result, (merge_test_t) {
    .decls = {
      { .name = "x", .type = SPN_OPTION_TYPE_BOOL },
    },
    .config = { { "nosuch", "vk" } },
    .expect = {
      .err = SPN_ERROR,
      .option_err = SPN_OPTION_ERR_UNDECLARED,
      .option = "nosuch",
    },
  });
}

// A value outside the enum's declared domain is an error, from config and
// from a consumer request alike
UTEST(options_merge, bad_value_config) {
  run_merge_test(utest_result, (merge_test_t) {
    .decls = {
      { .name = "e", .type = SPN_OPTION_TYPE_ENUM, .values = { "gl", "vk" } },
    },
    .config = { { "e", "dx12" } },
    .expect = {
      .err = SPN_ERROR,
      .option_err = SPN_OPTION_ERR_BAD_VALUE,
      .option = "e",
    },
  });
}

UTEST(options_merge, bad_value_request) {
  run_merge_test(utest_result, (merge_test_t) {
    .decls = {
      { .name = "e", .type = SPN_OPTION_TYPE_ENUM, .values = { "gl", "vk" } },
    },
    .requests = {
      { .consumer = "a", .options = { { "e", "dx12" } } },
    },
    .expect = {
      .err = SPN_ERROR,
      .option_err = SPN_OPTION_ERR_BAD_VALUE,
      .option = "e",
    },
  });
}

// An enum with no applicable default and no setter has no value
UTEST(options_merge, no_value) {
  run_merge_test(utest_result, (merge_test_t) {
    .decls = {
      { .name = "e", .type = SPN_OPTION_TYPE_ENUM, .values = { "gl", "vk" } },
    },
    .expect = {
      .err = SPN_ERROR,
      .option_err = SPN_OPTION_ERR_NO_VALUE,
      .option = "e",
    },
  });
}

// defaults = false declines the declared defaults wholesale; a request still
// lands, a bool with no request falls to false
UTEST(options_merge, defaults_declined) {
  run_merge_test(utest_result, (merge_test_t) {
    .decls = {
      { .name = "e", .type = SPN_OPTION_TYPE_ENUM, .values = { "gl", "vk" }, .defaults = { { .value = { .str = "vk" } } } },
      { .name = "x", .type = SPN_OPTION_TYPE_BOOL, .defaults = { { .value = { .is_bool = true, .b = true } } } },
    },
    .defaults_declined = true,
    .requests = {
      { .consumer = "a", .options = { { "e", "gl" } } },
    },
    .expect = {
      .options = {
        { .name = "e", .value = { .str = "gl" } },
        { .name = "x", .value = { .is_bool = true }, .is_default = true },
      },
    },
  });
}

// A default arm gated on a fact applies only when the fact holds
UTEST(options_merge, default_arm_facts) {
  run_merge_test(utest_result, (merge_test_t) {
    .facts = { .os = SPN_OS_LINUX },
    .decls = {
      {
        .name = "e",
        .type = SPN_OPTION_TYPE_ENUM,
        .values = { "gl", "vk", "off" },
        .defaults = {
          { .when = { { "os", "windows" } }, .value = { .str = "off" } },
          { .when = { { "os", "linux" } },   .value = { .str = "vk" } },
          { .value = { .str = "gl" } },
        },
      },
    },
    .expect = {
      .options = {
        { .name = "e", .value = { .str = "vk" }, .is_default = true },
      },
    },
  });
}

// A later option's default arm sees an earlier option's resolved value
UTEST(options_merge, default_arm_chains) {
  run_merge_test(utest_result, (merge_test_t) {
    .decls = {
      { .name = "x", .type = SPN_OPTION_TYPE_BOOL, .defaults = { { .value = { .is_bool = true, .b = true } } } },
      {
        .name = "e",
        .type = SPN_OPTION_TYPE_ENUM,
        .values = { "gl", "off" },
        .defaults = {
          { .when = { { .key = "x", .is_bool = true, .b = true } }, .value = { .str = "gl" } },
          { .value = { .str = "off" } },
        },
      },
    },
    .expect = {
      .options = {
        { .name = "e", .value = { .str = "gl" }, .is_default = true },
      },
    },
  });
}

// An explicit setter that lands on the default value still hashes as default
UTEST(options_merge, explicit_default_is_default) {
  run_merge_test(utest_result, (merge_test_t) {
    .decls = {
      { .name = "e", .type = SPN_OPTION_TYPE_ENUM, .values = { "gl", "vk" }, .defaults = { { .value = { .str = "gl" } } } },
    },
    .config = { { "e", "gl" } },
    .expect = {
      .options = {
        { .name = "e", .value = { .str = "gl" }, .is_default = true },
      },
    },
  });
}
