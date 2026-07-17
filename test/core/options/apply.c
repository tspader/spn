#include "common.h"
#include "forward/types.h"

typedef struct {
  const c8* key;
  const c8* value;
  bool negated;
} apply_clause_t;

typedef struct {
  const c8* value;
  bool plain;
  apply_clause_t when [2];
} apply_value_t;

typedef struct {
  apply_value_t values [4];
} apply_list_t;

typedef struct {
  const c8* lib_source [4];
  const c8* source [4];
  const c8* script_source [4];
  const c8* test_source [4];
  const c8* include [4];
  const c8* define [4];
  const c8* flags [4];
  const c8* sys_target [4];
  const c8* deps [4];
  const c8* sys [4];
} apply_expect_t;

typedef struct {
  spn_when_facts_t facts;
  bool reapply;
  spn_when_facts_t reapply_facts;
  apply_list_t lib_source;
  apply_list_t source;
  apply_list_t script_source;
  apply_list_t test_source;
  apply_list_t include;
  apply_list_t define;
  apply_list_t flags;
  apply_list_t sys_target;
  apply_list_t deps;
  apply_list_t sys;
  apply_expect_t expect;
} apply_test_t;

typedef struct {
  const c8* name;
  const c8* define;
  bool public;
} apply_option_t;

typedef struct {
  const c8* name;
  const c8* str;
  bool b;
  bool is_bool;
} apply_env_value_t;

typedef struct {
  const c8* define [4];
  const c8* public_define [4];
} apply_option_expect_t;

typedef struct {
  const c8* define [4];
  const c8* public_define [4];
  apply_option_t options [4];
  apply_env_value_t env [4];
  apply_option_expect_t expect;
} apply_option_test_t;

static spn_when_t make_apply_when(sp_mem_t mem, const apply_clause_t* clauses, u64 count) {
  spn_when_t when = { .clauses = sp_da_new(mem, spn_when_clause_t) };
  sp_for(it, count) {
    if (!clauses[it].key) {
      break;
    }
    sp_da_push(when.clauses, ((spn_when_clause_t) {
      .key = sp_cstr_as_str(clauses[it].key),
      .negated = clauses[it].negated,
      .value = spn_option_value_str(sp_cstr_as_str(clauses[it].value)),
    }));
  }
  return when;
}

static void make_list(
  sp_mem_t mem,
  apply_list_t test,
  sp_da(sp_str_t)* plain,
  spn_gated_list_t* gated
) {
  *plain = sp_da_new(mem, sp_str_t);
  *gated = sp_da_new(mem, spn_gated_str_t);

  sp_carr_for(test.values, it) {
    apply_value_t* value = &test.values[it];
    if (!value->value) {
      break;
    }
    if (value->plain) {
      sp_da_push(*plain, sp_cstr_as_str(value->value));
      continue;
    }
    sp_da_push(*gated, ((spn_gated_str_t) {
      .value = sp_cstr_as_str(value->value),
      .when = make_apply_when(mem, value->when, SP_CARR_LEN(value->when)),
    }));
  }
}

static void expect_list(s32* utest_result, sp_da(sp_str_t) actual, const c8** expected) {
  sp_for(it, 4) {
    if (!expected[it]) {
      EXPECT_EQ(sp_da_size(actual), it);
      return;
    }
    utest_kv("value", sp_cstr_as_str(expected[it]));
    EXPECT_TRUE(it < sp_da_size(actual));
    if (it >= sp_da_size(actual)) {
      return;
    }
    EXPECT_TRUE(sp_str_equal_cstr(actual[it], expected[it]));
  }
  EXPECT_EQ(sp_da_size(actual), 4);
}

static void run_apply_test(s32* utest_result, apply_test_t test) {
  sp_mem_heap_t* heap = sp_mem_heap_new();
  sp_mem_t mem = sp_mem_heap_as_allocator(heap);

  spn_pkg_info_t info = sp_zero;
  sp_str_om_insert(info.libs, sp_str_lit("A"), sp_zero_s(spn_target_info_t));
  sp_str_om_insert(info.exes, sp_str_lit("main"), sp_zero_s(spn_target_info_t));
  sp_str_om_insert(info.scripts, sp_str_lit("B"), sp_zero_s(spn_target_info_t));
  sp_str_om_insert(info.tests, sp_str_lit("C"), sp_zero_s(spn_target_info_t));
  spn_target_info_t* lib = sp_str_om_at(info.libs, 0);
  spn_target_info_t* exe = sp_str_om_at(info.exes, 0);
  spn_target_info_t empty = sp_zero;
  spn_target_info_t* script = sp_str_om_at(info.scripts, 0);
  spn_target_info_t* unit_test = sp_str_om_at(info.tests, 0);

  struct {
    apply_list_t test;
    sp_da(sp_str_t)* plain;
    spn_gated_list_t* gated;
    const c8** expected;
  } lists [] = {
    { test.lib_source, &lib->source, &lib->gated.source, test.expect.lib_source },
    { test.source, &exe->source, &exe->gated.source, test.expect.source },
    { test.script_source, &script->source, &script->gated.source, test.expect.script_source },
    { test.test_source, &unit_test->source, &unit_test->gated.source, test.expect.test_source },
    { test.include, &exe->include, &exe->gated.include, test.expect.include },
    { test.define, &exe->define, &exe->gated.define, test.expect.define },
    { test.flags, &exe->flags, &exe->gated.flags, test.expect.flags },
    { test.sys_target, &exe->system_deps, &exe->gated.system_deps, test.expect.sys_target, },
    { test.deps, &exe->deps, &exe->gated.deps, test.expect.deps },
    { test.sys, &info.system_deps, &info.gated.system_deps, test.expect.sys, },
  };
  sp_carr_for(lists, it) {
    make_list(mem, lists[it].test, lists[it].plain, lists[it].gated);
  }

  spn_when_env_t env = sp_zero;
  spn_when_env_init(mem, &env);
  spn_when_env_set_facts(&env, test.facts);
  spn_pkg_apply_options(&info, &env);
  if (test.reapply) {
    spn_when_env_t reapply = sp_zero;
    spn_when_env_init(mem, &reapply);
    spn_when_env_set_facts(&reapply, test.reapply_facts);
    spn_pkg_apply_options(&info, &reapply);
  }

  EXPECT_TRUE(info.applied);
  sp_carr_for(lists, it) {
    expect_list(utest_result, *lists[it].plain, lists[it].expected);
  }
}

static void run_apply_option_test(s32* utest_result, apply_option_test_t test) {
  sp_mem_heap_t* heap = sp_mem_heap_new();
  sp_mem_t mem = sp_mem_heap_as_allocator(heap);
  spn_pkg_info_t info = {
    .define = sp_da_new(mem, sp_str_t),
    .public_define = sp_da_new(mem, sp_str_t),
  };

  sp_carr_for(test.define, it) {
    if (!test.define[it]) {
      break;
    }
    sp_da_push(info.define, sp_cstr_as_str(test.define[it]));
  }
  sp_carr_for(test.public_define, it) {
    if (!test.public_define[it]) {
      break;
    }
    sp_da_push(info.public_define, sp_cstr_as_str(test.public_define[it]));
  }
  sp_carr_for(test.options, it) {
    if (!test.options[it].name) {
      break;
    }
    spn_option_info_t option = {
      .name = sp_cstr_as_str(test.options[it].name),
      .public = test.options[it].public,
      .define = sp_cstr_as_str(test.options[it].define),
    };
    sp_str_om_insert(info.options, option.name, option);
  }

  spn_when_env_t env = sp_zero;
  spn_when_env_init(mem, &env);
  sp_carr_for(test.env, it) {
    if (!test.env[it].name) {
      break;
    }
    spn_option_value_t value = test.env[it].is_bool
      ? spn_option_value_bool(test.env[it].b)
      : spn_option_value_str(sp_cstr_as_str(test.env[it].str));
    spn_when_env_set(&env, sp_cstr_as_str(test.env[it].name), value);
  }

  spn_pkg_apply_options(&info, &env);

  EXPECT_TRUE(info.applied);
  expect_list(utest_result, info.define, test.expect.define);
  expect_list(utest_result, info.public_define, test.expect.public_define);
}

UTEST(options_apply, lib_source) {
  run_apply_test(utest_result, (apply_test_t) {
    .facts = { .os = SPN_OS_LINUX },
    .lib_source = {
      .values = {
        { .value = "main.c", .plain = true },
        { .value = "A" },
        { .value = "B", .when = { { "os", "windows" } } },
        { .value = "C", .when = { { "os", "linux" } } },
      },
    },
    .expect = {
      .lib_source = { "main.c", "A", "C" },
    },
  });
}

UTEST(options_apply, exe_source) {
  run_apply_test(utest_result, (apply_test_t) {
    .facts = { .os = SPN_OS_LINUX },
    .source = {
      .values = {
        { .value = "main.c", .plain = true },
        { .value = "A" },
        { .value = "B", .when = { { "os", "windows" } } },
        { .value = "C", .when = { { "os", "linux" } } },
      },
    },
    .expect = {
      .source = { "main.c", "A", "C" },
    },
  });
}

UTEST(options_apply, script_source) {
  run_apply_test(utest_result, (apply_test_t) {
    .facts = { .os = SPN_OS_LINUX },
    .script_source = {
      .values = {
        { .value = "main.c", .plain = true },
        { .value = "A" },
        { .value = "B", .when = { { "os", "windows" } } },
        { .value = "C", .when = { { "os", "linux" } } },
      },
    },
    .expect = {
      .script_source = { "main.c", "A", "C" },
    },
  });
}

UTEST(options_apply, test_source) {
  run_apply_test(utest_result, (apply_test_t) {
    .facts = { .os = SPN_OS_LINUX },
    .test_source = {
      .values = {
        { .value = "main.c", .plain = true },
        { .value = "A" },
        { .value = "B", .when = { { "os", "windows" } } },
        { .value = "C", .when = { { "os", "linux" } } },
      },
    },
    .expect = {
      .test_source = { "main.c", "A", "C" },
    },
  });
}

UTEST(options_apply, include) {
  run_apply_test(utest_result, (apply_test_t) {
    .facts = { .os = SPN_OS_LINUX },
    .include = {
      .values = {
        { .value = "A", .when = { { "os", "linux" } } },
      },
    },
    .expect = {
      .include = { "A" },
    },
  });
}

UTEST(options_apply, define) {
  run_apply_test(utest_result, (apply_test_t) {
    .facts = { .os = SPN_OS_LINUX },
    .define = {
      .values = {
        { .value = "SPUM", .when = { { "os", "linux" } } },
      },
    },
    .expect = {
      .define = { "SPUM" },
    },
  });
}

UTEST(options_apply, flags) {
  run_apply_test(utest_result, (apply_test_t) {
    .facts = { .os = SPN_OS_LINUX },
    .flags = {
      .values = {
        { .value = "A", .when = { { "os", "linux" } } },
      },
    },
    .expect = {
      .flags = { "A" },
    },
  });
}

UTEST(options_apply, target_system_deps) {
  run_apply_test(utest_result, (apply_test_t) {
    .facts = { .os = SPN_OS_LINUX },
    .sys_target = {
      .values = {
        { .value = "A", .when = { { "os", "linux" } } },
      },
    },
    .expect = {
      .sys_target = { "A" },
    },
  });
}

UTEST(options_apply, target_deps) {
  run_apply_test(utest_result, (apply_test_t) {
    .facts = { .os = SPN_OS_LINUX },
    .deps = {
      .values = {
        { .value = "A", .when = { { "os", "linux" } } },
      },
    },
    .expect = {
      .deps = { "A" },
    },
  });
}

UTEST(options_apply, package_system_deps) {
  run_apply_test(utest_result, (apply_test_t) {
    .facts = { .os = SPN_OS_LINUX },
    .sys = {
      .values = {
        { .value = "A", .when = { { "os", "linux" } } },
      },
    },
    .expect = {
      .sys = { "A" },
    },
  });
}

UTEST(options_apply, applies_once) {
  run_apply_test(utest_result, (apply_test_t) {
    .facts = { .os = SPN_OS_LINUX, .mode = SPN_BUILD_MODE_DEBUG },
    .reapply = true,
    .reapply_facts = { .os = SPN_OS_WINDOWS, .mode = SPN_BUILD_MODE_DEBUG },
    .source = {
      .values = {
        { .value = "A", .when = { { "mode", "debug" } } },
        { .value = "B", .when = { { "os", "windows" } } },
      },
    },
    .expect = {
      .source = { "A" },
    },
  });
}

UTEST(options_apply, private_define) {
  run_apply_option_test(utest_result, (apply_option_test_t) {
    .define = { "A" },
    .options = {
      { .name = "A", .define = "SPUM" },
    },
    .env = {
      { .name = "A", .is_bool = true, .b = true },
    },
    .expect = {
      .define = { "A", "SPUM" },
    },
  });
}

UTEST(options_apply, public_define) {
  run_apply_option_test(utest_result, (apply_option_test_t) {
    .options = {
      { .name = "A", .define = "SPUM", .public = true },
    },
    .env = {
      { .name = "A", .is_bool = true, .b = true },
    },
    .expect = {
      .define = { "SPUM" },
      .public_define = { "SPUM" },
    },
  });
}

UTEST(options_apply, multiple_options) {
  run_apply_option_test(utest_result, (apply_option_test_t) {
    .options = {
      { .name = "A", .define = "A" },
      { .name = "B", .define = "SPUM" },
    },
    .env = {
      { .name = "A", .is_bool = true },
      { .name = "B", .is_bool = true, .b = true },
    },
    .expect = {
      .define = { "SPUM" },
    },
  });
}

UTEST(options_apply, empty_define) {
  run_apply_option_test(utest_result, (apply_option_test_t) {
    .options = {
      { .name = "A", .define = "" },
    },
    .env = {
      { .name = "A", .is_bool = true, .b = true },
    },
  });
}

UTEST(options_apply, missing_option_value) {
  run_apply_option_test(utest_result, (apply_option_test_t) {
    .options = {
      { .name = "A", .define = "SPUM" },
    },
  });
}

UTEST(options_apply, non_bool_option_value) {
  run_apply_option_test(utest_result, (apply_option_test_t) {
    .options = {
      { .name = "A", .define = "SPUM" },
    },
    .env = {
      { .name = "A", .str = "A" },
    },
  });
}

UTEST(options_apply, false_option_value) {
  run_apply_option_test(utest_result, (apply_option_test_t) {
    .options = {
      { .name = "A", .define = "SPUM" },
    },
    .env = {
      { .name = "A", .is_bool = true },
    },
  });
}
