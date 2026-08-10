#include "options.h"

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
  const c8* name;
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
  const c8* name;
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

static void make_path_list(
  sp_mem_t mem,
  apply_list_t test,
  sp_da(spn_tree_path_t)* plain,
  spn_gated_path_list_t* gated
) {
  *plain = sp_da_new(mem, spn_tree_path_t);
  *gated = sp_da_new(mem, spn_gated_path_t);

  sp_carr_for(test.values, it) {
    apply_value_t* value = &test.values[it];
    if (!value->value) {
      break;
    }
    if (value->plain) {
      sp_da_push(*plain, ((spn_tree_path_t) { .path = sp_cstr_as_str(value->value), .tree = SPN_TREE_SOURCE }));
      continue;
    }
    sp_da_push(*gated, ((spn_gated_path_t) {
      .path = sp_cstr_as_str(value->value),
      .tree = SPN_TREE_SOURCE,
      .when = make_apply_when(mem, value->when, SP_CARR_LEN(value->when)),
    }));
  }
}

static sp_err_t expect_list(sp_test_t* t, sp_da(sp_str_t) actual, const c8** expected) {
  sp_for(et, 4) {
    if (!expected[et]) {
      sp_must_eq(t, sp_da_size(actual), et);
      return SP_OK;
    }
    sp_test_kv_c(t, "value", expected[et]);
    sp_must(t, et < sp_da_size(actual));
    sp_expect_str_eq_c(t, actual[et], expected[et]);
  }
  sp_must_eq(t, sp_da_size(actual), 4);
  return SP_OK;
}

static sp_err_t expect_path_list(sp_test_t* t, sp_da(spn_tree_path_t) actual, const c8** expected) {
  sp_for(et, 4) {
    if (!expected[et]) {
      sp_must_eq(t, sp_da_size(actual), et);
      return SP_OK;
    }
    sp_test_kv_c(t, "value", expected[et]);
    sp_must(t, et < sp_da_size(actual));
    sp_expect_str_eq_c(t, actual[et].path, expected[et]);
  }
  sp_must_eq(t, sp_da_size(actual), 4);
  return SP_OK;
}

static const apply_test_t list_tests [] = {
  {
    .name = "lib_source",
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
  },
  {
    .name = "exe_source",
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
  },
  {
    .name = "script_source",
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
  },
  {
    .name = "test_source",
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
  },
  {
    .name = "include",
    .facts = { .os = SPN_OS_LINUX },
    .include = {
      .values = {
        { .value = "A", .when = { { "os", "linux" } } },
      },
    },
    .expect = {
      .include = { "A" },
    },
  },
  {
    .name = "define",
    .facts = { .os = SPN_OS_LINUX },
    .define = {
      .values = {
        { .value = "SPUM", .when = { { "os", "linux" } } },
      },
    },
    .expect = {
      .define = { "SPUM" },
    },
  },
  {
    .name = "flags",
    .facts = { .os = SPN_OS_LINUX },
    .flags = {
      .values = {
        { .value = "A", .when = { { "os", "linux" } } },
      },
    },
    .expect = {
      .flags = { "A" },
    },
  },
  {
    .name = "target_system_deps",
    .facts = { .os = SPN_OS_LINUX },
    .sys_target = {
      .values = {
        { .value = "A", .when = { { "os", "linux" } } },
      },
    },
    .expect = {
      .sys_target = { "A" },
    },
  },
  {
    .name = "target_deps",
    .facts = { .os = SPN_OS_LINUX },
    .deps = {
      .values = {
        { .value = "A", .when = { { "os", "linux" } } },
      },
    },
    .expect = {
      .deps = { "A" },
    },
  },
  {
    .name = "package_system_deps",
    .facts = { .os = SPN_OS_LINUX },
    .sys = {
      .values = {
        { .value = "A", .when = { { "os", "linux" } } },
      },
    },
    .expect = {
      .sys = { "A" },
    },
  },
  {
    .name = "applies_once",
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
  },
};

sp_test_each(options_apply, lists, apply_test_t, list_tests) {
  sp_mem_t mem = sp_test_arena(t);

  spn_pkg_info_t info = sp_zero;
  sp_str_om_insert(info.libs, sp_str_lit("A"), sp_zero_s(spn_target_info_t));
  sp_str_om_insert(info.exes, sp_str_lit("main"), sp_zero_s(spn_target_info_t));
  sp_str_om_insert(info.scripts, sp_str_lit("B"), sp_zero_s(spn_target_info_t));
  sp_str_om_insert(info.tests, sp_str_lit("C"), sp_zero_s(spn_target_info_t));
  spn_target_info_t* lib = sp_str_om_at(info.libs, 0);
  spn_target_info_t* exe = sp_str_om_at(info.exes, 0);
  spn_target_info_t* script = sp_str_om_at(info.scripts, 0);
  spn_target_info_t* unit_test = sp_str_om_at(info.tests, 0);

  struct {
    apply_list_t test;
    sp_da(spn_tree_path_t)* plain;
    spn_gated_path_list_t* gated;
    const c8** expected;
  } path_lists [] = {
    { it->lib_source, &lib->source, &lib->gated.source, it->expect.lib_source },
    { it->source, &exe->source, &exe->gated.source, it->expect.source },
    { it->script_source, &script->source, &script->gated.source, it->expect.script_source },
    { it->test_source, &unit_test->source, &unit_test->gated.source, it->expect.test_source },
    { it->include, &exe->include, &exe->gated.include, it->expect.include },
  };
  struct {
    apply_list_t test;
    sp_da(sp_str_t)* plain;
    spn_gated_list_t* gated;
    const c8** expected;
  } lists [] = {
    { it->define, &exe->define, &exe->gated.define, it->expect.define },
    { it->flags, &exe->flags, &exe->gated.flags, it->expect.flags },
    { it->sys_target, &exe->system_deps, &exe->gated.system_deps, it->expect.sys_target, },
    { it->deps, &exe->deps, &exe->gated.deps, it->expect.deps },
    { it->sys, &info.system_deps, &info.gated.system_deps, it->expect.sys, },
  };
  sp_carr_for(path_lists, lt) {
    make_path_list(mem, path_lists[lt].test, path_lists[lt].plain, path_lists[lt].gated);
  }
  sp_carr_for(lists, lt) {
    make_list(mem, lists[lt].test, lists[lt].plain, lists[lt].gated);
  }

  spn_when_env_t env = sp_zero;
  spn_when_env_init(mem, &env);
  spn_when_env_set_facts(&env, it->facts);
  spn_pkg_apply_options(&info, &env);
  if (it->reapply) {
    spn_when_env_t reapply = sp_zero;
    spn_when_env_init(mem, &reapply);
    spn_when_env_set_facts(&reapply, it->reapply_facts);
    spn_pkg_apply_options(&info, &reapply);
  }

  sp_expect(t, info.applied);
  sp_carr_for(path_lists, lt) {
    sp_err_t err = expect_path_list(t, *path_lists[lt].plain, path_lists[lt].expected);
    if (err) return err;
  }
  sp_carr_for(lists, lt) {
    sp_err_t err = expect_list(t, *lists[lt].plain, lists[lt].expected);
    if (err) return err;
  }

  return SP_OK;
}

static const apply_option_test_t option_tests [] = {
  {
    .name = "private_define",
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
  },
  {
    .name = "public_define",
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
  },
  {
    .name = "multiple_options",
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
  },
  {
    .name = "empty_define",
    .options = {
      { .name = "A", .define = "" },
    },
    .env = {
      { .name = "A", .is_bool = true, .b = true },
    },
  },
  {
    .name = "missing_option_value",
    .options = {
      { .name = "A", .define = "SPUM" },
    },
  },
  {
    .name = "non_bool_option_value",
    .options = {
      { .name = "A", .define = "SPUM" },
    },
    .env = {
      { .name = "A", .str = "A" },
    },
  },
  {
    .name = "false_option_value",
    .options = {
      { .name = "A", .define = "SPUM" },
    },
    .env = {
      { .name = "A", .is_bool = true },
    },
  },
};

sp_test_each(options_apply, option_defines, apply_option_test_t, option_tests) {
  sp_mem_t mem = sp_test_arena(t);
  spn_pkg_info_t info = {
    .define = sp_da_new(mem, sp_str_t),
    .public_define = sp_da_new(mem, sp_str_t),
  };

  sp_carr_for(it->define, dt) {
    if (!it->define[dt]) {
      break;
    }
    sp_da_push(info.define, sp_cstr_as_str(it->define[dt]));
  }
  sp_carr_for(it->public_define, pt) {
    if (!it->public_define[pt]) {
      break;
    }
    sp_da_push(info.public_define, sp_cstr_as_str(it->public_define[pt]));
  }
  sp_carr_for(it->options, ot) {
    if (!it->options[ot].name) {
      break;
    }
    spn_option_info_t option = {
      .name = sp_cstr_as_str(it->options[ot].name),
      .public = it->options[ot].public,
      .define = sp_cstr_as_str(it->options[ot].define),
    };
    sp_str_om_insert(info.options, option.name, option);
  }

  spn_when_env_t env = sp_zero;
  spn_when_env_init(mem, &env);
  sp_carr_for(it->env, et) {
    if (!it->env[et].name) {
      break;
    }
    spn_option_value_t value = it->env[et].is_bool
      ? spn_option_value_bool(it->env[et].b)
      : spn_option_value_str(sp_cstr_as_str(it->env[et].str));
    spn_when_env_set(&env, sp_cstr_as_str(it->env[et].name), value);
  }

  spn_pkg_apply_options(&info, &env);

  sp_expect(t, info.applied);
  sp_err_t err = expect_list(t, info.define, it->expect.define);
  if (err) return err;
  return expect_list(t, info.public_define, it->expect.public_define);
}
