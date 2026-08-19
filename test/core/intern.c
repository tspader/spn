#include "spn_test.h"

#include "intern/intern.h"

#define INTERN_TEST_ENTRIES 4096

static sp_intern_t* intern_new(sp_mem_t mem) {
  sp_intern_t* intern = sp_alloc_type(mem, sp_intern_t);
  sp_intern_init(intern, mem);
  return intern;
}

sp_test(intern, dedupe) {
  sp_mem_t mem = sp_test_arena(t);
  sp_intern_t* intern = intern_new(mem);
  sp_must_eq(t, 1, sp_intern_size(intern));

  sp_str_t a = sp_intern_get_or_insert_str(intern, sp_str_lit("A"));
  u64 bytes = sp_intern_bytes_used(intern);
  sp_str_t b = sp_intern_get_or_insert_str(intern, sp_str_lit("A"));

  sp_expect_eq(t, 2, sp_intern_size(intern));
  sp_expect(t, (void*)a.data == (void*)b.data);
  sp_expect_eq(t, a.len, b.len);
  sp_expect_eq(t, bytes, sp_intern_bytes_used(intern));

  return SP_OK;
}

sp_test(intern, distinct) {
  sp_mem_t mem = sp_test_arena(t);
  sp_intern_t* intern = intern_new(mem);

  const c8* strs [] = { "A", "B", "C", "D" };
  sp_str_t interned [sp_carr_len(strs)];
  sp_carr_for(strs, it) {
    interned[it] = sp_intern_get_or_insert_str(intern, sp_str_view(strs[it]));
  }

  sp_must_eq(t, 1 + sp_carr_len(strs), sp_intern_size(intern));
  sp_carr_for(strs, a) {
    sp_for(b, a) {
      sp_expect(t, interned[a].data != interned[b].data);
    }
  }

  return SP_OK;
}

sp_test(intern, stable_under_growth) {
  sp_mem_t mem = sp_test_arena(t);
  sp_intern_t* intern = intern_new(mem);

  sp_str_t* first = sp_alloc_n(mem, sp_str_t, INTERN_TEST_ENTRIES);
  sp_for(it, INTERN_TEST_ENTRIES) {
    first[it] = sp_intern_get_or_insert_str(intern, sp_fmt(mem, "E{}", sp_fmt_uint(it)).value);
  }
  sp_must_eq(t, 1 + INTERN_TEST_ENTRIES, sp_intern_size(intern));

  u64 bytes = sp_intern_bytes_used(intern);
  sp_for(it, INTERN_TEST_ENTRIES) {
    sp_str_t again = sp_intern_get_or_insert_str(intern, sp_fmt(mem, "E{}", sp_fmt_uint(it)).value);
    sp_must(t, (void*)first[it].data == (void*)again.data);
  }
  sp_expect_eq(t, 1 + INTERN_TEST_ENTRIES, sp_intern_size(intern));
  sp_expect_eq(t, bytes, sp_intern_bytes_used(intern));

  return SP_OK;
}
