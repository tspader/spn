#include "spn_test.h"

#include "spn/core.h"
#include "when/when.h"


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
  sp_for(it, count) {
    if (!clauses[it].key) break;
    spn_when_clause_t clause = {
      .key = sp_str_view(clauses[it].key),
      .negated = clauses[it].negated,
      .value = make_value((value_lit_t) { .str = clauses[it].str, .b = clauses[it].b, .is_bool = clauses[it].is_bool }),
    };
    sp_da_push(when.clauses, clause);
  }
  return when;
}

static spn_when_env_t make_env(sp_mem_t mem) {
  spn_when_env_t env;
  spn_when_env_init(mem, &env);
  spn_when_env_set_facts(&env, (spn_when_facts_t) {
    .os = SPN_OS_LINUX,
    .arch = SPN_ARCH_X64,
    .abi = SPN_ABI_GNU,
    .mode = SPN_MODE_DEBUG,
    .opt = SPN_OPT_LEVEL_2,
    .sanitizers = SPN_SANITIZER_ADDRESS | SPN_SANITIZER_UNDEFINED,
  });
  spn_when_env_set(&env, sp_str_lit("tls"), spn_option_value_str(sp_str_lit("openssl")));
  spn_when_env_set(&env, sp_str_lit("zstd"), spn_option_value_bool(true));
  return env;
}

static spn_option_info_t make_option_zstd(sp_mem_t mem) {
  return (spn_option_info_t) {
    .name = sp_str_lit("zstd"),
    .type = SPN_OPTION_TYPE_BOOL,
    .defaults = sp_da_new(mem, spn_option_default_t),
  };
}

static spn_option_info_t make_option_tls(sp_mem_t mem) {
  spn_option_info_t option = {
    .name = sp_str_lit("tls"),
    .type = SPN_OPTION_TYPE_ENUM,
    .values = sp_da_new(mem, sp_str_t),
    .defaults = sp_da_new(mem, spn_option_default_t),
  };
  sp_da_push(option.values, sp_str_lit("schannel"));
  sp_da_push(option.values, sp_str_lit("openssl"));
  sp_da_push(option.values, sp_str_lit("off"));

  sp_da_push(option.defaults, ((spn_option_default_t) {
    .when = make_when(mem, (clause_lit_t []) { { "os", "windows" } }, 1),
    .value = spn_option_value_str(sp_str_lit("schannel")),
  }));
  sp_da_push(option.defaults, ((spn_option_default_t) {
    .when = make_when(mem, (clause_lit_t []) { { .key = "os", .str = "wasi", .negated = true } }, 1),
    .value = spn_option_value_str(sp_str_lit("openssl")),
  }));
  sp_da_push(option.defaults, ((spn_option_default_t) {
    .value = spn_option_value_str(sp_str_lit("off")),
  }));
  return option;
}


typedef struct {
  const c8* name;
  value_lit_t a;
  value_lit_t b;
  struct { bool equal; } expect;
} equal_t;

static const equal_t equal_tests [] = {
  { .name = "none_eq_none",                                                                        .expect = { true } },
  { .name = "true_eq_true",  .a = { .is_bool = true, .b = true }, .b = { .is_bool = true, .b = true }, .expect = { true } },
  { .name = "true_ne_false", .a = { .is_bool = true, .b = true }, .b = { .is_bool = true } },
  { .name = "str_eq_same",   .a = { .str = "openssl" },           .b = { .str = "openssl" },           .expect = { true } },
  { .name = "str_ne_diff",   .a = { .str = "openssl" },           .b = { .str = "schannel" } },
  { .name = "bool_ne_str",   .a = { .is_bool = true, .b = true }, .b = { .str = "true" } },
  { .name = "none_ne_bool",                                       .b = { .is_bool = true } },
};

sp_test_each(option_value, equal, equal_t, equal_tests) {
  sp_expect_eq(t, spn_option_value_equal(make_value(it->a), make_value(it->b)), it->expect.equal);
  return SP_OK;
}


typedef struct {
  const c8* name;
  clause_lit_t clauses [4];
  struct { bool pass; } expect;
} eval_t;

static const eval_t eval_tests [] = {
  {
    .name = "always",
    .expect = { true },
  },
  {
    .name = "os_match",
    .clauses = { { "os", "linux" } },
    .expect = { true },
  },
  {
    .name = "os_mismatch",
    .clauses = { { "os", "windows" } },
  },
  {
    .name = "enum_option_match",
    .clauses = { { "tls", "openssl" } },
    .expect = { true },
  },
  {
    .name = "enum_option_mismatch",
    .clauses = { { "tls", "schannel" } },
  },
  {
    .name = "bool_option_match",
    .clauses = { { .key = "zstd", .is_bool = true, .b = true } },
    .expect = { true },
  },
  {
    .name = "bool_option_mismatch",
    .clauses = { { .key = "zstd", .is_bool = true } },
  },
  {
    .name = "negated_miss_passes",
    .clauses = { { .key = "abi", .str = "msvc", .negated = true } },
    .expect = { true },
  },
  {
    .name = "negated_match_fails",
    .clauses = { { .key = "abi", .str = "gnu", .negated = true } },
  },
  {
    .name = "conjunction_all_match",
    .clauses = {
      { "mode", "debug" },
      { .key = "abi", .str = "msvc", .negated = true },
    },
    .expect = { true },
  },
  {
    .name = "conjunction_one_fails",
    .clauses = {
      { "mode", "release" },
      { .key = "abi", .str = "msvc", .negated = true },
    },
  },
  {
    .name = "opt_match",
    .clauses = { { "opt", "2" } },
    .expect = { true },
  },
  {
    .name = "opt_mismatch",
    .clauses = { { "opt", "0" } },
  },
  {
    .name = "negated_opt_miss_passes",
    .clauses = { { .key = "opt", .str = "z", .negated = true } },
    .expect = { true },
  },
  {
    .name = "sanitize_address_on",
    .clauses = { { .key = "sanitize_address", .is_bool = true, .b = true } },
    .expect = { true },
  },
  {
    .name = "sanitize_undefined_on",
    .clauses = { { .key = "sanitize_undefined", .is_bool = true, .b = true } },
    .expect = { true },
  },
  {
    .name = "sanitize_thread_off",
    .clauses = { { .key = "sanitize_thread", .is_bool = true, .b = true } },
  },
  {
    .name = "sanitize_thread_false_matches",
    .clauses = { { .key = "sanitize_thread", .is_bool = true } },
    .expect = { true },
  },
  {
    .name = "negated_sanitizer_fails",
    .clauses = { { .key = "sanitize_address", .is_bool = true, .b = true, .negated = true } },
  },
  {
    .name = "unknown_key_fails",
    .clauses = { { "simd", "avx2" } },
  },
  {
    .name = "negated_unknown_key_fails",
    .clauses = { { .key = "simd", .str = "avx2", .negated = true } },
  },
  {
    .name = "str_against_bool_option_fails",
    .clauses = { { "zstd", "true" } },
  },
  {
    .name = "bool_against_enum_option_fails",
    .clauses = { { .key = "tls", .is_bool = true, .b = true } },
  },
};

sp_test_each(when, eval, eval_t, eval_tests) {
  sp_mem_t mem = sp_test_arena(t);
  spn_when_env_t env = make_env(mem);
  spn_when_t when = make_when(mem, it->clauses, sp_carr_len(it->clauses));
  sp_expect_eq(t, spn_when_eval(&when, &env), it->expect.pass);
  return SP_OK;
}

sp_test(when, eval_null) {
  spn_when_env_t env = make_env(sp_test_arena(t));
  sp_expect(t, spn_when_eval(SP_NULLPTR, &env));
  return SP_OK;
}


typedef struct {
  const c8* name;
  clause_lit_t clauses [4];
  struct { const c8* value; } expect;
} to_str_t;

static const to_str_t to_str_tests [] = {
  {
    .name = "always",
    .expect = { "always" },
  },
  {
    .name = "str_and_negated",
    .clauses = {
      { "tls", "openssl" },
      { .key = "os", .str = "wasi", .negated = true },
    },
    .expect = { "tls = \"openssl\", os != \"wasi\"" },
  },
  {
    .name = "bool_clause",
    .clauses = {
      { .key = "zstd", .is_bool = true, .b = true },
    },
    .expect = { "zstd = true" },
  },
};

sp_test_each(when, to_str, to_str_t, to_str_tests) {
  sp_mem_t mem = sp_test_arena(t);
  spn_when_t when = make_when(mem, it->clauses, sp_carr_len(it->clauses));
  sp_expect_str_eq_c(t, spn_when_to_str(mem, &when), it->expect.value);
  return SP_OK;
}


SP_TYPEDEF_FN(spn_option_info_t, make_option_fn_t, sp_mem_t mem);

typedef struct {
  const c8* name;
  make_option_fn_t option;
  value_lit_t value;
  struct { bool ok; } expect;
} value_ok_t;

static const value_ok_t value_ok_tests [] = {
  {
    .name = "bool_accepts_bool",
    .option = make_option_zstd,
    .value = { .is_bool = true },
    .expect = { true },
  },
  {
    .name = "bool_rejects_str",
    .option = make_option_zstd,
    .value = { .str = "false" },
  },
  {
    .name = "bool_rejects_none",
    .option = make_option_zstd,
  },
  {
    .name = "enum_accepts_member",
    .option = make_option_tls,
    .value = { .str = "openssl" },
    .expect = { true },
  },
  {
    .name = "enum_rejects_nonmember",
    .option = make_option_tls,
    .value = { .str = "gnutls" },
  },
  {
    .name = "enum_rejects_bool",
    .option = make_option_tls,
    .value = { .is_bool = true, .b = true },
  },
};

sp_test_each(option, value_ok, value_ok_t, value_ok_tests) {
  spn_option_info_t option = it->option(sp_test_arena(t));
  sp_expect_eq(t, spn_option_value_ok(&option, make_value(it->value)), it->expect.ok);
  return SP_OK;
}


typedef struct {
  const c8* name;
  spn_os_t os;
  struct { const c8* value; } expect;
} resolve_t;

static const resolve_t resolve_tests [] = {
  { "windows_takes_schannel", SPN_OS_WINDOWS, { "schannel" } },
  { "linux_takes_openssl",    SPN_OS_LINUX,   { "openssl" } },
  { "macos_takes_openssl",    SPN_OS_MACOS,   { "openssl" } },
  { "wasi_takes_fallback",    SPN_OS_WASI,    { "off" } },
};

sp_test_each(option, resolve_first_match, resolve_t, resolve_tests) {
  sp_mem_t mem = sp_test_arena(t);
  spn_option_info_t tls = make_option_tls(mem);

  spn_when_env_t env;
  spn_when_env_init(mem, &env);
  spn_when_env_set_facts(&env, (spn_when_facts_t) {
    .os = it->os,
    .arch = SPN_ARCH_X64,
    .mode = SPN_MODE_DEBUG,
  });

  spn_option_value_t value = spn_option_resolve(&tls, &env);
  sp_expect_eq(t, value.kind, SPN_OPTION_VALUE_STR);
  sp_expect_str_eq_c(t, value.str, it->expect.value);
  return SP_OK;
}

sp_test(option, resolve_no_match) {
  sp_mem_t mem = sp_test_arena(t);
  spn_when_env_t env;
  spn_when_env_init(mem, &env);

  spn_option_info_t tls = make_option_tls(mem);
  sp_da_pop(tls.defaults);
  spn_option_value_t value = spn_option_resolve(&tls, &env);
  sp_expect_eq(t, value.kind, SPN_OPTION_VALUE_NONE);
  return SP_OK;
}
