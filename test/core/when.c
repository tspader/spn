#define SP_IMPLEMENTATION
#include "sp.h"

#define UTEST_IMPLEMENTATION
#include "utest.h"

#include "test.h"
#include "spn.h"
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

static spn_option_value_t make_value(value_lit_t lit) {
  if (lit.str) {
    return spn_option_value_str(sp_str_view(lit.str));
  }
  if (lit.is_bool) {
    return spn_option_value_bool(lit.b);
  }
  return spn_option_value_none();
}

static spn_when_t make_when(sp_mem_t mem, const clause_lit_t* clauses) {
  spn_when_t when = { .clauses = sp_da_new(mem, spn_when_clause_t) };
  for (const clause_lit_t* it = clauses; it->key; it++) {
    spn_when_clause_t clause = {
      .key = sp_str_view(it->key),
      .negated = it->negated,
      .value = make_value((value_lit_t) { .str = it->str, .b = it->b, .is_bool = it->is_bool }),
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
    .mode = SPN_BUILD_MODE_DEBUG,
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
    .when = make_when(mem, (clause_lit_t []) { { "os", "windows" }, sp_zero }),
    .value = spn_option_value_str(sp_str_lit("schannel")),
  }));
  sp_da_push(option.defaults, ((spn_option_default_t) {
    .when = make_when(mem, (clause_lit_t []) { { .key = "os", .str = "wasi", .negated = true }, sp_zero }),
    .value = spn_option_value_str(sp_str_lit("openssl")),
  }));
  sp_da_push(option.defaults, ((spn_option_default_t) {
    .value = spn_option_value_str(sp_str_lit("off")),
  }));
  return option;
}


typedef struct {
  value_lit_t a;
  value_lit_t b;
  struct { bool equal; } expect;
} equal_t;

UTEST(option_value, equal) {
  equal_t tests [] = {
    { sp_zero,                            sp_zero,                            { true } },
    { { .is_bool = true, .b = true },     { .is_bool = true, .b = true },     { true } },
    { { .is_bool = true, .b = true },     { .is_bool = true },                { false } },
    { { .str = "openssl" },               { .str = "openssl" },               { true } },
    { { .str = "openssl" },               { .str = "schannel" },              { false } },
    { { .is_bool = true, .b = true },     { .str = "true" },                  { false } },
    { sp_zero,                            { .is_bool = true },                { false } },
  };

  sp_carr_for(tests, it) {
    EXPECT_EQ(spn_option_value_equal(make_value(tests[it].a), make_value(tests[it].b)), tests[it].expect.equal);
  }
}


typedef struct {
  clause_lit_t clauses [4];
  bool expect;
} eval_t;

UTEST(when, eval) {
  eval_t tests [] = {
    { sp_zero, true },
    {
      { { "os", "linux" } },
      true
    },
    {
      { { "os", "windows" } },
      false
    },
    {
      { { "tls", "openssl" } },
      true
    },
    {
      { { "tls", "schannel" } },
      false
    },
    {
      { { .key = "zstd", .is_bool = true, .b = true } },
      true
    },
    {
      { { .key = "zstd", .is_bool = true } },
      false
    },
    {
      { { .key = "abi", .str = "msvc", .negated = true } },
      true
    },
    {
      { { .key = "abi", .str = "gnu", .negated = true } },
      false
    },
    {
      {
        { "mode", "debug" },
        { .key = "abi", .str = "msvc", .negated = true } },
      true
    },
    {
      {
        { "mode", "release" },
        { .key = "abi", .str = "msvc", .negated = true }
      },
      false
    },
    {
      { { "opt", "2" } },
      true
    },
    {
      { { "opt", "0" } },
      false
    },
    {
      { { .key = "opt", .str = "z", .negated = true } },
      true
    },
    {
      { { .key = "sanitize_address", .is_bool = true, .b = true } },
      true
    },
    {
      { { .key = "sanitize_undefined", .is_bool = true, .b = true } },
      true
    },
    {
      { { .key = "sanitize_thread", .is_bool = true, .b = true } },
      false
    },
    {
      { { .key = "sanitize_thread", .is_bool = true } },
      true
    },
    {
      { { .key = "sanitize_address", .is_bool = true, .b = true, .negated = true } },
      false
    },
    {
      { { "simd", "avx2" } },
      false
    },
    {
      { { .key = "simd", .str = "avx2", .negated = true } },
      false
    },
    {
      { { "zstd", "true" } },
      false
    },
    {
      { { .key = "tls", .is_bool = true, .b = true } },
      false
    },
  };

  sp_mem_t mem = sp_mem_os_new();
  spn_when_env_t env = make_env(mem);
  sp_carr_for(tests, it) {
    spn_when_t when = make_when(mem, tests[it].clauses);
    EXPECT_EQ(spn_when_eval(&when, &env), tests[it].expect);
  }

  EXPECT_TRUE(spn_when_eval(SP_NULLPTR, &env));
}


typedef struct {
  clause_lit_t clauses [4];
  struct { const c8* value; } expect;
} to_str_t;

UTEST(when, to_str) {
  to_str_t tests [] = {
    { sp_zero, { "always" } },
    {
      {
        { "tls", "openssl" },
        { .key = "os", .str = "wasi", .negated = true }
      },
      { "tls = \"openssl\", os != \"wasi\"" }
    },
    {
      {
        { .key = "zstd", .is_bool = true, .b = true }
      },
      { "zstd = true" }
    },
  };

  sp_mem_t mem = sp_mem_os_new();
  sp_carr_for(tests, it) {
    spn_when_t when = make_when(mem, tests[it].clauses);
    EXPECT_TRUE(sp_str_equal_cstr(spn_when_to_str(mem, &when), tests[it].expect.value));
  }
}


typedef struct {
  const c8* option;
  value_lit_t value;
  struct { bool ok; } expect;
} value_ok_t;

UTEST(option, value_ok) {
  value_ok_t tests [] = {
    {
      "zstd",
      { .is_bool = true },
      { true }
    },
    {
      "zstd",
      { .str = "false" },
      { false }
    },
    {
      "zstd",
      sp_zero,
      { false }
    },
    {
      "tls",
      { .str = "openssl" },
      { true }
    },
    {
      "tls",
      { .str = "gnutls" },
      { false }
    },
    {
      "tls",
      { .is_bool = true, .b = true },
      { false }
    },
  };

  sp_mem_t mem = sp_mem_os_new();
  spn_option_info_t zstd = make_option_zstd(mem);
  spn_option_info_t tls = make_option_tls(mem);
  sp_carr_for(tests, it) {
    spn_option_info_t* option = sp_cstr_equal(tests[it].option, "tls") ? &tls : &zstd;
    EXPECT_EQ(spn_option_value_ok(option, make_value(tests[it].value)), tests[it].expect.ok);
  }
}


typedef struct {
  spn_os_t os;
  struct { const c8* value; } expect;
} resolve_t;

UTEST(option, resolve_first_match) {
  resolve_t tests [] = {
    { SPN_OS_WINDOWS, { "schannel" } },
    { SPN_OS_LINUX,   { "openssl" } },
    { SPN_OS_MACOS,   { "openssl" } },
    { SPN_OS_WASI,    { "off" } },
  };

  sp_mem_t mem = sp_mem_os_new();
  spn_option_info_t tls = make_option_tls(mem);
  sp_carr_for(tests, it) {
    spn_when_env_t env;
    spn_when_env_init(mem, &env);
    spn_when_env_set_facts(&env, (spn_when_facts_t) {
      .os = tests[it].os,
      .arch = SPN_ARCH_X64,
      .mode = SPN_BUILD_MODE_DEBUG,
    });

    spn_option_value_t value = spn_option_resolve(&tls, &env);
    EXPECT_EQ(value.kind, SPN_OPTION_VALUE_STR);
    EXPECT_TRUE(sp_str_equal_cstr(value.str, tests[it].expect.value));
  }
}

UTEST(option, resolve_no_match) {
  sp_mem_t mem = sp_mem_os_new();
  spn_when_env_t env;
  spn_when_env_init(mem, &env);

  spn_option_info_t tls = make_option_tls(mem);
  sp_da_pop(tls.defaults);
  spn_option_value_t value = spn_option_resolve(&tls, &env);
  EXPECT_EQ(value.kind, SPN_OPTION_VALUE_NONE);
}
