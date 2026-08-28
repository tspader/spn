#define SP_IMPLEMENTATION
#include "sp.h"
#include "sp_template.h"

#define gate_filter ".github/ci-gate.jq"
#define stub_dir "tools/ci"
#define out_dir ".tmp/ci"
#define cfmt(mem, ...) sp_str_to_cstr(mem, sp_fmt(mem, __VA_ARGS__).value)

typedef struct {
  const c8* build;
  const c8* cold;
  const c8* package;
  const c8* canary;
  bool pass;
} gate_test_t;

typedef struct {
  const c8* run;
  const c8* conclusion;
  bool pass;
} greenlit_test_t;

typedef struct {
  sp_mem_t mem;
  sp_str_t template;
  const c8* needs;
  const c8* path;
} ci_t;

static const gate_test_t gate_tests [] = {
  { .build = "success",   .cold = "skipped", .package = "success", .canary = "skipped", .pass = true },
  { .build = "success",   .cold = "skipped", .package = "success", .canary = "success", .pass = true },
  { .build = "failure",   .cold = "success", .package = "success", .canary = "success", .pass = true },
  { .build = "failure",   .cold = "success", .package = "success", .canary = "skipped", .pass = true },
  { .build = "failure",   .cold = "failure", .package = "skipped", .canary = "skipped" },
  { .build = "failure",   .cold = "skipped", .package = "skipped", .canary = "skipped" },
  { .build = "success",   .cold = "skipped", .package = "failure", .canary = "skipped" },
  { .build = "failure",   .cold = "success", .package = "success", .canary = "failure" },
  { .build = "cancelled", .cold = "skipped", .package = "skipped", .canary = "skipped" },
};

static const greenlit_test_t greenlit_tests [] = {
  { .run = "123", .conclusion = "success", .pass = true },
  { .run = "",    .conclusion = "" },
  { .run = "123", .conclusion = "failure" },
  { .run = "123", .conclusion = "" },
  { .run = "123", .conclusion = "cancelled" },
};

static s32 gate_status(ci_t* c, const gate_test_t* test) {
  sp_template_scope_t* scope = sp_template_scope_create(c->mem);
  sp_template_set(scope, sp_str_lit("build"), sp_cstr_as_str(test->build));
  sp_template_set(scope, sp_str_lit("cold"), sp_cstr_as_str(test->cold));
  sp_template_set(scope, sp_str_lit("package"), sp_cstr_as_str(test->package));
  sp_template_set(scope, sp_str_lit("canary"), sp_cstr_as_str(test->canary));

  sp_io_dyn_mem_writer_t sink = sp_zero;
  sp_io_dyn_mem_writer_init(c->mem, &sink);
  sp_assert(!sp_template_render(&sink.base, c->template, scope, SP_NULLPTR));
  sp_assert(!sp_fs_create_file_cstr(sp_cstr_as_str(c->needs), sp_str_to_cstr(c->mem, sp_io_dyn_mem_writer_as_str(&sink))));

  sp_ps_output_t result = sp_ps_run_c(c->mem, (sp_ps_config_cstr_t) {
    .command = "jq",
    .args = { "--exit-status", "--from-file", gate_filter, c->needs },
  });
  return result.status.exit_code;
}

static s32 greenlit_status(ci_t* c, const greenlit_test_t* test) {
  sp_ps_output_t result = sp_ps_run_c(c->mem, (sp_ps_config_cstr_t) {
    .command = "sh",
    .args = { "tools/greenlit.sh", "ci", "0000000" },
    .env = {
      .extra = {
        { "PATH", c->path },
        { "GREENLIT_RUN", test->run },
        { "GREENLIT_CONCLUSION", test->conclusion },
      },
    },
    .io = { .err = { .mode = SP_PS_IO_MODE_NULL } },
  });
  return result.status.exit_code;
}

static bool is_digits(sp_str_t str) {
  sp_for(it, str.len) {
    if (str.data[it] < '0' || str.data[it] > '9') {
      return false;
    }
  }
  return str.len > 0;
}

static bool is_version(sp_mem_t mem, sp_str_t str) {
  sp_da(sp_str_t) parts = sp_str_split_c8(mem, str, '.');
  if (sp_da_size(parts) != 3) {
    return false;
  }
  sp_da_for(parts, it) {
    if (!is_digits(parts[it])) {
      return false;
    }
  }
  return true;
}

static u32 check_gates(ci_t* c) {
  u32 failures = 0;
  sp_carr_for(gate_tests, it) {
    const gate_test_t* test = &gate_tests[it];
    bool passed = (gate_status(c, test) == 0) == test->pass;
    failures += !passed;
    sp_log("{} gate {} {} {} {}",
      sp_fmt_cstr(passed ? "ok  " : "FAIL"),
      sp_fmt_cstr(test->build),
      sp_fmt_cstr(test->cold),
      sp_fmt_cstr(test->package),
      sp_fmt_cstr(test->canary)
    );
  }
  return failures;
}

static u32 check_greenlits(ci_t* c) {
  u32 failures = 0;
  sp_carr_for(greenlit_tests, it) {
    const greenlit_test_t* test = &greenlit_tests[it];
    bool passed = (greenlit_status(c, test) == 0) == test->pass;
    failures += !passed;
    sp_log("{} greenlit run='{}' conclusion='{}'",
      sp_fmt_cstr(passed ? "ok  " : "FAIL"),
      sp_fmt_cstr(test->run),
      sp_fmt_cstr(test->conclusion)
    );
  }
  return failures;
}

static u32 check_version(ci_t* c) {
  sp_ps_output_t result = sp_ps_run_c(c->mem, (sp_ps_config_cstr_t) {
    .command = "sh",
    .args = { "tools/stage0.sh", "--version" },
  });
  bool passed = !result.status.exit_code && is_version(c->mem, sp_str_trim_right(result.out));
  sp_log("{} stage0 --version", sp_fmt_cstr(passed ? "ok  " : "FAIL"));
  return !passed;
}

static void setup(ci_t* c) {
  sp_fs_create_dir(sp_cstr_as_str(out_dir));
  sp_assert(!sp_io_read_file(c->mem, sp_cstr_as_str(stub_dir "/needs.json"), &c->template));

  c->needs = out_dir "/needs.json";
  c->path = cfmt(c->mem, "{}/{}:{}",
    sp_fmt_str(sp_fs_get_cwd(c->mem)),
    sp_fmt_cstr(stub_dir),
    sp_fmt_str(sp_os_env_get(sp_str_lit("PATH")))
  );
}

s32 main() {
  ci_t c = sp_zero;
  c.mem = sp_mem_os_new();

  if (!sp_fs_is_file(sp_cstr_as_str(gate_filter))) {
    sp_log("ci: run from the spn repo root");
    return 1;
  }
  setup(&c);

  u32 failures = check_gates(&c) + check_greenlits(&c) + check_version(&c);
  return failures ? 1 : 0;
}
