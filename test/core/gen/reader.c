#include "spn_test.h"

#include "release.gen.h"

typedef struct {
  const c8* name;
  const c8* skip;
  bool err;
} read_test_t;

static const read_test_t tests [] = {
  { .name = "complete" },
  { .name = "malformed", .err = true },
  { .name = "missing_required_str", .skip = "json readers do not enforce required fields", .err = true },
  { .name = "missing_required_bool", .skip = "json readers do not enforce required fields", .err = true },
};

sp_test_each(gen_reader, release, read_test_t, tests) {
  if (it->skip) {
    return sp_test_skip(t, "{}", sp_fmt_cstr(it->skip));
  }

  sp_mem_t mem = sp_test_arena(t);

  sp_str_t path = test_repo_path(mem, sp_fmt(mem, "test/core/gen/releases/{}.json", sp_fmt_cstr(it->name)).value);
  sp_str_t json = sp_zero;
  sp_must(t, !sp_io_read_file(mem, path, &json));

  spn_cg_release_t release = sp_zero;
  bool ok = spn_release_read(json, &release, mem);
  sp_expect_eq(t, ok, !it->err);
  if (ok) {
    sp_expect_golden(t, sp_test_format(t, "golden/{}.json", sp_fmt_cstr(it->name)), spn_release_write(mem, &release));
  }

  return SP_OK;
}
