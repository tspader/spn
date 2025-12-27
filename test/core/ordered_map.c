#define SP_IMPLEMENTATION
#include "sp.h"

#define SP_TEST_IMPLEMENTATION
#include "test.h"
#include "utest.h"

#define SP_OM_IMPLEMENTATION
#include "ordered_map.h"

UTEST_MAIN()

#define uf utest_fixture
#define ur (*utest_result)

struct om {};

UTEST_F_SETUP(om) {}
UTEST_F_TEARDOWN(om) {}

UTEST_F(om, insert_and_get) {
  sp_om(s32) map = SP_NULLPTR;

  sp_om_insert(map, sp_str_lit("a"), 1);
  sp_om_insert(map, sp_str_lit("b"), 2);
  sp_om_insert(map, sp_str_lit("c"), 3);

  ASSERT_EQ(sp_om_size(map), 3);
  ASSERT_EQ(*sp_om_get(map, sp_str_lit("a")), 1);
  ASSERT_EQ(*sp_om_get(map, sp_str_lit("b")), 2);
  ASSERT_EQ(*sp_om_get(map, sp_str_lit("c")), 3);

  sp_om_free(map);
}

UTEST_F(om, insertion_order) {
  sp_om(s32) map = SP_NULLPTR;

  sp_om_insert(map, sp_str_lit("x"), 10);
  sp_om_insert(map, sp_str_lit("y"), 20);
  sp_om_insert(map, sp_str_lit("z"), 30);

  ASSERT_EQ(*sp_om_at(map, 0), 10);
  ASSERT_EQ(*sp_om_at(map, 1), 20);
  ASSERT_EQ(*sp_om_at(map, 2), 30);

  sp_om_free(map);
}

UTEST_F(om, iteration) {
  sp_om(s32) map = SP_NULLPTR;

  for (s32 i = 0; i < 5; i++) {
    sp_str_t key = sp_format("{}", SP_FMT_S32(i));
    sp_om_insert(map, key, i * 10);
  }

  s32 expected = 0;
  sp_om_for(map, it) {
    ASSERT_EQ(*sp_om_at(map, it), expected);
    expected += 10;
  }

  sp_om_free(map);
}

UTEST_F(om, has_key) {
  sp_om(s32) map = SP_NULLPTR;

  ASSERT_FALSE(sp_om_has(map, sp_str_lit("foo")));

  sp_om_insert(map, sp_str_lit("foo"), 42);

  ASSERT_TRUE(sp_om_has(map, sp_str_lit("foo")));
  ASSERT_FALSE(sp_om_has(map, sp_str_lit("bar")));

  sp_om_free(map);
}

UTEST_F(om, stable_pointers) {
  sp_om(s32) map = SP_NULLPTR;

  sp_om_insert(map, sp_str_lit("first"), 100);
  s32* first = sp_om_at(map, 0);

  for (s32 i = 0; i < 100; i++) {
    sp_str_t key = sp_format("key{}", SP_FMT_S32(i));
    sp_om_insert(map, key, i);
  }

  ASSERT_EQ(*first, 100);
  ASSERT_EQ(first, sp_om_at(map, 0));

  sp_om_free(map);
}

UTEST_F(om, duplicate_key_insert) {
  sp_om(s32) map = SP_NULLPTR;

  sp_om_insert(map, sp_str_lit("key"), 100);
  sp_om_insert(map, sp_str_lit("key"), 200);
  sp_om_insert(map, sp_str_lit("key"), 300);

  ASSERT_EQ(sp_om_size(map), 1);
  ASSERT_EQ(*sp_om_get(map, sp_str_lit("key")), 100);

  sp_om_free(map);
}

UTEST_F(om, getp_and_missing_key) {
  sp_om(s32) map = SP_NULLPTR;

  sp_om_insert(map, sp_str_lit("exists"), 42);

  s32** pp = sp_om_getp(map, sp_str_lit("exists"));
  ASSERT_NE(pp, SP_NULLPTR);
  ASSERT_EQ(**pp, 42);

  /* sp_om_get dereferences result, so use getp for missing key check */
  s32** missing_pp = sp_om_getp(map, sp_str_lit("missing"));
  ASSERT_EQ(missing_pp, SP_NULLPTR);

  sp_om_free(map);
}

UTEST_F(om, null_map_operations) {
  sp_om(s32) map = SP_NULLPTR;

  ASSERT_EQ(sp_om_size(map), 0);
  /* sp_om_get dereferences, so only test getp on NULL map */
  ASSERT_EQ(sp_om_getp(map, sp_str_lit("any")), SP_NULLPTR);
  ASSERT_FALSE(sp_om_has(map, sp_str_lit("any")));

  sp_om_free(map);
}

typedef struct {
  s32 x;
  s32 y;
  sp_str_t name;
} test_point_t;

UTEST_F(om, struct_values) {
  sp_om(test_point_t) map = SP_NULLPTR;

  sp_om_insert(map, sp_str_lit("origin"), ((test_point_t){.x = 0, .y = 0, .name = sp_str_lit("origin")}));
  sp_om_insert(map, sp_str_lit("point1"), ((test_point_t){.x = 10, .y = 20, .name = sp_str_lit("first")}));

  ASSERT_EQ(sp_om_size(map), 2);

  test_point_t* origin = sp_om_get(map, sp_str_lit("origin"));
  ASSERT_EQ(origin->x, 0);
  ASSERT_EQ(origin->y, 0);

  test_point_t* p1 = sp_om_at(map, 1);
  ASSERT_EQ(p1->x, 10);
  ASSERT_EQ(p1->y, 20);

  sp_om_free(map);
}

// INTERN
struct intern {};
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
  } markers = SP_ZERO_INITIALIZE();

  sp_intern_t* intern = sp_alloc_type(sp_intern_t);
  sp_intern_init(intern);
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
    sp_str_t str = sp_format("entry_{}", SP_FMT_U32(it));
    sp_intern_get_or_insert(intern, str);
    bytes_used = sp_intern_bytes_used(intern);
  }
}
