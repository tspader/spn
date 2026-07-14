#define SP_IMPLEMENTATION
#include "sp.h"
#include "sp/macro.h"

#include "utest.h"

#include "intern/intern.h"
#include "ctx/types.h"

spn_ctx_t spn;

UTEST_MAIN()

#define uf utest_fixture
#define ur (*utest_result)

struct intern { u8 unused; };
UTEST_F_SETUP(intern) {}
UTEST_F_TEARDOWN(intern) {}

sp_str_t intern_and_mark(sp_intern_t* intern, const c8* str, u64* marker) {
  sp_str_t interned = sp_intern_get_or_insert_str(intern, sp_str_view(str));
  *marker = sp_intern_bytes_used(intern);
  return interned;
}

UTEST_F(intern, hello) {
  struct {
    u64 before;
    u64 a;
    u64 b;
    u64 c;
    u64 d;
    u64 e;
    u64 f;
  } markers = sp_zero;

  sp_mem_t mem = sp_mem_os_new();
  sp_intern_t* intern = sp_alloc_type(mem, sp_intern_t);
  sp_intern_init(intern, mem);
  EXPECT_EQ(sp_intern_size(intern), 1);

  sp_str_t a = intern_and_mark(intern, "a", &markers.a);
  sp_str_t b = intern_and_mark(intern, "a", &markers.b);
  EXPECT_EQ(sp_intern_size(intern), 2);
  EXPECT_EQ(a.data, b.data);
  EXPECT_EQ(a.len, b.len);
  EXPECT_EQ(markers.a, markers.b);

  sp_str_t c = intern_and_mark(intern, "c", &markers.c);
  sp_str_t d = intern_and_mark(intern, "d", &markers.d);
  sp_str_t e = intern_and_mark(intern, "e", &markers.e);
  sp_str_t f = intern_and_mark(intern, "f", &markers.f);
  EXPECT_EQ(sp_intern_size(intern), 6);
  EXPECT_NE(c.data, d.data);
  EXPECT_NE(c.data, e.data);
  EXPECT_NE(c.data, f.data);
  EXPECT_NE(d.data, e.data);
  EXPECT_NE(d.data, f.data);
  EXPECT_NE(e.data, f.data);

  u64 bytes_used = sp_intern_bytes_used(intern);
  sp_for(it, 4096) {
    sp_str_t str = sp_fmt(mem, "entry_{}", sp_fmt_uint(it)).value;
    sp_intern_get_or_insert(intern, str);
    bytes_used = sp_intern_bytes_used(intern);
  }
}
