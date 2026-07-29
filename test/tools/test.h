#ifndef SPN_TEST_H
#define SPN_TEST_H
#include "sp.h"
#include "sp/macro.h"
#include "utest.h"
#include "fixture.h"

#define ut (*utest_fixture)
#define uf utest_fixture
#define ur (*utest_result)
#define UTEST_RESULT(r) s32* utest_result = (r)


// CONTEXT
typedef struct {
  tmpfs_t fs;
  sp_mem_arena_t* arena;
} ctx_t;

typedef struct {
  sp_str_t repo;
  struct {
    sp_str_t dir;
    sp_str_t fixtures;
  } test;
} ctx_paths_t;

ctx_t* ctx_get();
void   ctx_init(ctx_t* ctx);
void   ctx_deinit(ctx_t* ctx);
ctx_paths_t ctx_get_paths(ctx_t* ctx);


// UTEST
#define SP_TEST_STREQ(a, b, sa, sb, is_assert) \
  do { \
    sp_str_t utest_a = (a); \
    sp_str_t utest_b = (b); \
    if (str_equal(utest_a, utest_b)) { \
      utest_context_reset(); \
    } else { \
      utest_fail(utest_result, __FILE__, __LINE__, \
        utest_fmt_s("{} == {}", sp_fmt_cstr(sa), sp_fmt_cstr(sb)), \
        utest_fmt_s("{.quote} vs {.quote}", sp_fmt_str(utest_a), sp_fmt_str(utest_b))); \
      if (is_assert) { \
        return; \
      } \
    } \
  } while (0)

#define SP_EXPECT_STR_EQ_CSTR(a, b) SP_TEST_STREQ((a), SP_CSTR(b), #a, #b, false)
#define SP_EXPECT_STR_EQ(a, b) SP_TEST_STREQ((a), (b), #a, #b, false)
#define SP_EXPECT_ERR(err) EXPECT_EQ(sp_err_get(), err)


#endif
