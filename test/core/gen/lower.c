#include "spn_test.h"

#include "gen.h"
#include "jtd.h"

typedef struct {
  const c8* name;
  const c8* skip;
  bool ok;
} lower_test_t;

static const lower_test_t tests [] = {
  { .name = "keyed_agree", .ok = true },
  { .name = "keyed_conflict", .skip = "conflicting key metadata silently takes the first" },
};

sp_test_each(gen_lower, lower, lower_test_t, tests) {
  if (it->skip) {
    return sp_test_skip(t, "{}", sp_fmt_cstr(it->skip));
  }

  sp_mem_t mem = sp_test_arena(t);

  sp_str_t path = test_repo_path(mem, sp_fmt(mem, "test/core/gen/schemas/{}.jtd.json", sp_fmt_cstr(it->name)).value);
  jtd_result_t jtd = sp_zero;
  sp_must_eq(t, JTD_OK, jtd_parse_file(mem, path, &jtd));

  gen_t* g = gen_new(mem);
  sp_expect_eq(t, gen_lower(g, sp_str_lit("R"), jtd.root), it->ok);
  sp_expect_eq(t, sp_str_empty(g->err), it->ok);

  return SP_OK;
}
