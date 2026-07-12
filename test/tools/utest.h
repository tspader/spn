/*
   The latest version of this library is available on GitHub;
   https://github.com/sheredom/utest.h

   This version has been modified to depend only on sp.h, and to collect
   failure context (key/value pairs and freeform notes) instead of printing
   mid-test.
*/

/*
   This is free and unencumbered software released into the public domain.

   Anyone is free to copy, modify, publish, use, compile, sell, or
   distribute this software, either in source code form or as a compiled
   binary, for any purpose, commercial or non-commercial, and by any
   means.

   In jurisdictions that recognize copyright laws, the author or authors
   of this software dedicate any and all copyright interest in the
   software to the public domain. We make this dedication for the benefit
   of the public at large and to the detriment of our heirs and
   successors. We intend this dedication to be an overt act of
   relinquishment in perpetuity of all present and future rights to this
   software under copyright law.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
   IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
   OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
   ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
   OTHER DEALINGS IN THE SOFTWARE.

   For more information, please refer to <http://unlicense.org/>
*/

#ifndef SHEREDOM_UTEST_H_INCLUDED
#define SHEREDOM_UTEST_H_INCLUDED

#include "sp.h"

#define UTEST_TEST_PASSED (0)
#define UTEST_TEST_FAILURE (1)
#define UTEST_TEST_SKIPPED (2)

#if defined(__TINYC__)
#define UTEST_ATTRIBUTE(a) __attribute((a))
#elif defined(_MSC_VER)
#define UTEST_ATTRIBUTE(a)
#else
#define UTEST_ATTRIBUTE(a) __attribute__((a))
#endif

#if defined(_MSC_VER)
#define UTEST_WEAK __forceinline
#define UTEST_INLINE __forceinline
#elif defined(SP_WIN32)
#define UTEST_WEAK static UTEST_ATTRIBUTE(unused)
#define UTEST_INLINE inline
#elif defined(__clang__) || defined(__GNUC__) || defined(__TINYC__)
#define UTEST_WEAK UTEST_ATTRIBUTE(weak)
#define UTEST_INLINE inline
#else
#define UTEST_WEAK
#define UTEST_INLINE inline
#endif

#define UTEST_EXTERN extern

#if defined(_MSC_VER)

#if defined(_WIN64)
#define UTEST_SYMBOL_PREFIX
#else
#define UTEST_SYMBOL_PREFIX "_"
#endif

#pragma section(".CRT$XCU", read)
#define UTEST_INITIALIZER(f)                                                   \
  static void __cdecl f(void);                                                 \
  __pragma(comment(linker, "/include:" UTEST_SYMBOL_PREFIX #f "_"))            \
      __declspec(allocate(".CRT$XCU")) void(__cdecl * f##_)(void) = f;         \
  static void __cdecl f(void)

#else

#define UTEST_INITIALIZER(f)                                                   \
  static void f(void) UTEST_ATTRIBUTE(constructor);                            \
  static void f(void)

#endif

#if defined(__clang__)
/* clang-format off */
#define UTEST_AUTO(x)                                                          \
  _Pragma("clang diagnostic push")                                             \
      _Pragma("clang diagnostic ignored \"-Wgnu-auto-type\"") __auto_type      \
          _Pragma("clang diagnostic pop")
/* clang-format on */
#define UTEST_HAS_EVAL 1
#elif defined(__GNUC__) || defined(__TINYC__)
#define UTEST_AUTO(x) __typeof__(x + 0)
#define UTEST_HAS_EVAL 1
#else
#define UTEST_HAS_EVAL 0
#endif

typedef void (*utest_test_fn_t)(s32*);

typedef struct {
  utest_test_fn_t func;
  const c8* name;
  const c8* set;
  const c8* test;
} utest_test_t;

typedef struct {
  sp_str_t key;
  sp_str_t value;
} utest_attr_t;

typedef struct {
  sp_str_t file;
  u32 line;
  sp_str_t expected;
  sp_str_t actual;
  sp_da(utest_attr_t) attrs;
} utest_failure_t;

struct utest_state_s {
  sp_mem_t mem;
  sp_da(utest_test_t) tests;
  sp_da(utest_failure_t) failures;
  sp_da(utest_attr_t) staged;
};

UTEST_EXTERN struct utest_state_s utest_state;

UTEST_WEAK sp_mem_t utest_mem(void);
UTEST_WEAK sp_mem_t utest_mem(void) {
  if (!utest_state.mem.on_alloc) {
    utest_state.mem = sp_mem_os_new();
    utest_state.tests = sp_da_new(utest_state.mem, utest_test_t);
    utest_state.failures = sp_da_new(utest_state.mem, utest_failure_t);
    utest_state.staged = sp_da_new(utest_state.mem, utest_attr_t);
  }
  return utest_state.mem;
}

UTEST_WEAK void utest_register(utest_test_fn_t func, const c8* name, const c8* set, const c8* test);
UTEST_WEAK void utest_register(utest_test_fn_t func, const c8* name, const c8* set, const c8* test) {
  utest_mem();
  sp_da_push(utest_state.tests, ((utest_test_t) {
    .func = func,
    .name = name,
    .set = set,
    .test = test,
  }));
}

/*
  Failure context. Stage key/value pairs (or keyless notes) before an
  assertion; the next assertion attaches them to its failure record, or
  discards them if it passes. The runner prints everything once the test
  finishes.
*/
UTEST_WEAK void utest_kv(const c8* key, sp_str_t value);
UTEST_WEAK void utest_kv(const c8* key, sp_str_t value) {
  sp_mem_t mem = utest_mem();
  sp_da_push(utest_state.staged, ((utest_attr_t) {
    .key = sp_str_copy(mem, sp_cstr_as_str(key)),
    .value = sp_str_copy(mem, value),
  }));
}

UTEST_WEAK void utest_note(sp_str_t value);
UTEST_WEAK void utest_note(sp_str_t value) {
  sp_mem_t mem = utest_mem();
  sp_da_push(utest_state.staged, ((utest_attr_t) {
    .value = sp_str_copy(mem, value),
  }));
}

UTEST_WEAK void utest_context_reset(void);
UTEST_WEAK void utest_context_reset(void) {
  if (utest_state.staged) {
    sp_da_clear(utest_state.staged);
  }
}

UTEST_WEAK void utest_fail(s32* utest_result, const c8* file, u32 line, sp_str_t expected, sp_str_t actual);
UTEST_WEAK void utest_fail(s32* utest_result, const c8* file, u32 line, sp_str_t expected, sp_str_t actual) {
  sp_mem_t mem = utest_mem();
  sp_da_push(utest_state.failures, ((utest_failure_t) {
    .file = sp_cstr_as_str(file),
    .line = line,
    .expected = sp_str_copy(mem, expected),
    .actual = sp_str_copy(mem, actual),
    .attrs = utest_state.staged,
  }));
  utest_state.staged = sp_da_new(mem, utest_attr_t);
  *utest_result = UTEST_TEST_FAILURE;
}

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wvariadic-macros"
#endif
#define utest_fmt_s(...) sp_fmt(utest_mem(), __VA_ARGS__).value
#ifdef __clang__
#pragma clang diagnostic pop
#endif

#if defined(__clang__)
#define UTEST_FORMATTER UTEST_WEAK UTEST_ATTRIBUTE(overloadable)

UTEST_FORMATTER sp_str_t utest_fmt(signed char c);
UTEST_FORMATTER sp_str_t utest_fmt(signed char c) {
  return utest_fmt_s("{}", sp_fmt_int(c));
}

UTEST_FORMATTER sp_str_t utest_fmt(unsigned char c);
UTEST_FORMATTER sp_str_t utest_fmt(unsigned char c) {
  return utest_fmt_s("{}", sp_fmt_uint(c));
}

UTEST_FORMATTER sp_str_t utest_fmt(f32 v);
UTEST_FORMATTER sp_str_t utest_fmt(f32 v) {
  return utest_fmt_s("{:.3}", sp_fmt_float(v));
}

UTEST_FORMATTER sp_str_t utest_fmt(f64 v);
UTEST_FORMATTER sp_str_t utest_fmt(f64 v) {
  return utest_fmt_s("{:.3}", sp_fmt_float(v));
}

UTEST_FORMATTER sp_str_t utest_fmt(s32 v);
UTEST_FORMATTER sp_str_t utest_fmt(s32 v) {
  return utest_fmt_s("{}", sp_fmt_int(v));
}

UTEST_FORMATTER sp_str_t utest_fmt(u32 v);
UTEST_FORMATTER sp_str_t utest_fmt(u32 v) {
  return utest_fmt_s("{}", sp_fmt_uint(v));
}

UTEST_FORMATTER sp_str_t utest_fmt(s64 v);
UTEST_FORMATTER sp_str_t utest_fmt(s64 v) {
  return utest_fmt_s("{}", sp_fmt_int(v));
}

UTEST_FORMATTER sp_str_t utest_fmt(u64 v);
UTEST_FORMATTER sp_str_t utest_fmt(u64 v) {
  return utest_fmt_s("{}", sp_fmt_uint(v));
}

UTEST_FORMATTER sp_str_t utest_fmt(void* v);
UTEST_FORMATTER sp_str_t utest_fmt(void* v) {
  return utest_fmt_s("{}", sp_fmt_ptr(v));
}

UTEST_FORMATTER sp_str_t utest_fmt(const c8* v);
UTEST_FORMATTER sp_str_t utest_fmt(const c8* v) {
  return utest_fmt_s("{}", sp_fmt_cstr(v));
}

UTEST_FORMATTER sp_str_t utest_fmt(sp_str_t v);
UTEST_FORMATTER sp_str_t utest_fmt(sp_str_t v) {
  return utest_fmt_s("{}", sp_fmt_str(v));
}
#else
#define utest_fmt(v) sp_str_lit("?")
#endif

#define UTEST_SKIP(msg)                                                        \
  do {                                                                         \
    if (UTEST_TEST_PASSED == *utest_result) {                                  \
      *utest_result = UTEST_TEST_SKIPPED;                                      \
    }                                                                          \
    return;                                                                    \
  } while (0)

#if UTEST_HAS_EVAL

#if defined(__clang__)
#define UTEST_DIAG_PUSH                                                        \
  _Pragma("clang diagnostic push")                                             \
  _Pragma("clang diagnostic ignored \"-Wfloat-equal\"")
#define UTEST_DIAG_POP _Pragma("clang diagnostic pop")
#else
#define UTEST_DIAG_PUSH
#define UTEST_DIAG_POP
#endif

#define UTEST_COND(x, y, sx, sy, cond, is_assert)                              \
  do {                                                                         \
    UTEST_AUTO(x) utest_x = (x);                                               \
    UTEST_AUTO(y) utest_y = (y);                                               \
    UTEST_DIAG_PUSH                                                            \
    bool utest_ok = !!((utest_x)cond(utest_y));                                \
    UTEST_DIAG_POP                                                             \
    if (utest_ok) {                                                            \
      utest_context_reset();                                                   \
    } else {                                                                   \
      utest_fail(utest_result, __FILE__, __LINE__,                             \
        utest_fmt_s("{} {} {}",                                                \
          sp_fmt_cstr(sx), sp_fmt_cstr(#cond), sp_fmt_cstr(sy)),               \
        utest_fmt_s("{} vs {}",                                                \
          sp_fmt_str(utest_fmt(utest_x)), sp_fmt_str(utest_fmt(utest_y))));    \
      if (is_assert) {                                                         \
        return;                                                                \
      }                                                                        \
    }                                                                          \
  } while (0)

#else

#define UTEST_COND(x, y, sx, sy, cond, is_assert)                              \
  do {                                                                         \
    if ((x)cond(y)) {                                                          \
      utest_context_reset();                                                   \
    } else {                                                                   \
      utest_fail(utest_result, __FILE__, __LINE__,                             \
        utest_fmt_s("{} {} {}",                                                \
          sp_fmt_cstr(sx), sp_fmt_cstr(#cond), sp_fmt_cstr(sy)),               \
        sp_str_lit(""));                                                       \
      if (is_assert) {                                                         \
        return;                                                                \
      }                                                                        \
    }                                                                          \
  } while (0)

#endif

#define UTEST_BOOL(x, sx, expected, is_assert)                                 \
  do {                                                                         \
    bool utest_x = !!(x);                                                      \
    if (utest_x == (expected)) {                                               \
      utest_context_reset();                                                   \
    } else {                                                                   \
      utest_fail(utest_result, __FILE__, __LINE__,                             \
        utest_fmt_s("{} is {}",                                                \
          sp_fmt_cstr(sx), sp_fmt_cstr((expected) ? "true" : "false")),        \
        sp_str_view((expected) ? "false" : "true"));                           \
      if (is_assert) {                                                         \
        return;                                                                \
      }                                                                        \
    }                                                                          \
  } while (0)

#define EXPECT_EQ(x, y) UTEST_COND(x, y, #x, #y, ==, 0)
#define ASSERT_EQ(x, y) UTEST_COND(x, y, #x, #y, ==, 1)
#define EXPECT_NE(x, y) UTEST_COND(x, y, #x, #y, !=, 0)
#define ASSERT_NE(x, y) UTEST_COND(x, y, #x, #y, !=, 1)
#define EXPECT_LT(x, y) UTEST_COND(x, y, #x, #y, <, 0)
#define ASSERT_LT(x, y) UTEST_COND(x, y, #x, #y, <, 1)
#define EXPECT_LE(x, y) UTEST_COND(x, y, #x, #y, <=, 0)
#define ASSERT_LE(x, y) UTEST_COND(x, y, #x, #y, <=, 1)
#define EXPECT_GT(x, y) UTEST_COND(x, y, #x, #y, >, 0)
#define ASSERT_GT(x, y) UTEST_COND(x, y, #x, #y, >, 1)
#define EXPECT_GE(x, y) UTEST_COND(x, y, #x, #y, >=, 0)
#define ASSERT_GE(x, y) UTEST_COND(x, y, #x, #y, >=, 1)

#define EXPECT_TRUE(x) UTEST_BOOL(x, #x, true, 0)
#define ASSERT_TRUE(x) UTEST_BOOL(x, #x, true, 1)
#define EXPECT_FALSE(x) UTEST_BOOL(x, #x, false, 0)
#define ASSERT_FALSE(x) UTEST_BOOL(x, #x, false, 1)

#define UTEST(SET, NAME)                                                       \
  UTEST_EXTERN struct utest_state_s utest_state;                               \
  static void utest_run_##SET##_##NAME(s32* utest_result);                     \
  UTEST_INITIALIZER(utest_register_##SET##_##NAME) {                           \
    utest_register(&utest_run_##SET##_##NAME, #SET "." #NAME, #SET, #NAME);    \
  }                                                                            \
  static void utest_run_##SET##_##NAME(s32* utest_result)

#define UTEST_F_SETUP(FIXTURE)                                                 \
  static void utest_f_setup_##FIXTURE(s32* utest_result,                       \
                                      struct FIXTURE* utest_fixture)

#define UTEST_F_TEARDOWN(FIXTURE)                                              \
  static void utest_f_teardown_##FIXTURE(s32* utest_result,                    \
                                         struct FIXTURE* utest_fixture)

#define UTEST_EMPTY_FIXTURE(FIXTURE) \
  struct FIXTURE { u32 placeholder; }; \
  UTEST_F_SETUP(FIXTURE) {} \
  UTEST_F_TEARDOWN(FIXTURE) {}

#define UTEST_F(FIXTURE, NAME)                                                 \
  UTEST_EXTERN struct utest_state_s utest_state;                               \
  static void utest_f_setup_##FIXTURE(s32*, struct FIXTURE*);                  \
  static void utest_f_teardown_##FIXTURE(s32*, struct FIXTURE*);               \
  static void utest_run_##FIXTURE##_##NAME(s32*, struct FIXTURE*);             \
  static void utest_f_##FIXTURE##_##NAME(s32* utest_result) {                  \
    struct FIXTURE fixture;                                                    \
    sp_mem_zero(&fixture, sizeof(fixture));                                    \
    utest_f_setup_##FIXTURE(utest_result, &fixture);                           \
    if (UTEST_TEST_PASSED != *utest_result) {                                  \
      return;                                                                  \
    }                                                                          \
    utest_run_##FIXTURE##_##NAME(utest_result, &fixture);                      \
    utest_f_teardown_##FIXTURE(utest_result, &fixture);                        \
  }                                                                            \
  UTEST_INITIALIZER(utest_register_##FIXTURE##_##NAME) {                       \
    utest_register(&utest_f_##FIXTURE##_##NAME, #FIXTURE "." #NAME, #FIXTURE, #NAME); \
  }                                                                            \
  static void utest_run_##FIXTURE##_##NAME(s32* utest_result,                  \
                                           struct FIXTURE* utest_fixture)

#if defined(__clang__)
#if __has_warning("-Wunsafe-buffer-usage")
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#endif
#endif

UTEST_WEAK bool utest_filtered(const c8* filter, const c8* testcase);
UTEST_WEAK bool utest_filtered(const c8* filter, const c8* testcase) {
  if (!filter) {
    return false;
  }

  const c8* filter_cur = filter;
  const c8* testcase_cur = testcase;
  const c8* filter_wildcard = SP_NULLPTR;

  while (('\0' != *filter_cur) && ('\0' != *testcase_cur)) {
    if ('*' == *filter_cur) {
      filter_wildcard = filter_cur;
      filter_cur++;

      while (('\0' != *filter_cur) && ('\0' != *testcase_cur)) {
        if ('*' == *filter_cur) {
          break;
        } else if (*filter_cur != *testcase_cur) {
          filter_cur = filter_wildcard;
        }

        testcase_cur++;
        filter_cur++;
      }

      if (('\0' == *filter_cur) && ('\0' == *testcase_cur)) {
        return false;
      }

      if ('\0' == *testcase_cur) {
        return true;
      }
    } else {
      if (*testcase_cur != *filter_cur) {
        return true;
      } else {
        testcase_cur++;
        filter_cur++;
      }
    }
  }

  if (('\0' != *filter_cur) ||
      (('\0' != *testcase_cur) &&
       ((filter == filter_cur) || ('*' != filter_cur[-1])))) {
    return true;
  }

  return false;
}

#if defined(__clang__)
#if __has_warning("-Wunsafe-buffer-usage")
#pragma clang diagnostic pop
#endif
#endif

UTEST_WEAK void utest_report_attr(sp_str_t bar, sp_str_t key, sp_str_t value, u32 width);
UTEST_WEAK void utest_report_attr(sp_str_t bar, sp_str_t key, sp_str_t value, u32 width) {
  sp_mem_t mem = utest_mem();
  value = sp_str_trim_right(value);
  sp_da(sp_str_t) lines = sp_str_split_c8(mem, value, '\n');

  if (sp_str_empty(key)) {
    sp_da_for(lines, it) {
      sp_log("  {}{}", sp_fmt_str(bar), sp_fmt_str(lines[it]));
    }
    return;
  }

  if (sp_da_size(lines) <= 1) {
    sp_log("  {}{.gray} {}",
      sp_fmt_str(bar),
      sp_fmt_str(sp_str_pad(mem, key, width)),
      sp_fmt_str(value));
    return;
  }

  sp_log("  {}{.gray}", sp_fmt_str(bar), sp_fmt_str(key));
  sp_da_for(lines, it) {
    sp_log("  {}  {}", sp_fmt_str(bar), sp_fmt_str(lines[it]));
  }
}

UTEST_WEAK void utest_report_failure(utest_failure_t* failure);
UTEST_WEAK void utest_report_failure(utest_failure_t* failure) {
  sp_mem_t mem = utest_mem();
  sp_str_t bar = sp_fmt(mem, "{.red}", sp_fmt_cstr("▐ ")).value;

  u32 width = sizeof("expected") - 1;
  sp_da_for(failure->attrs, it) {
    width = sp_max(width, failure->attrs[it].key.len);
  }

  sp_log("  {}{.gray}:{.gray}",
    sp_fmt_str(bar),
    sp_fmt_str(failure->file),
    sp_fmt_uint(failure->line));
  if (!sp_str_empty(failure->expected)) {
    utest_report_attr(bar, sp_str_lit("expected"), failure->expected, width);
  }
  if (!sp_str_empty(failure->actual)) {
    utest_report_attr(bar, sp_str_lit("actual"), failure->actual, width);
  }
  sp_da_for(failure->attrs, it) {
    utest_attr_t* attr = &failure->attrs[it];
    utest_report_attr(bar, attr->key, attr->value, width);
  }
}

static UTEST_INLINE s32 utest_main(s32 argc, const c8** argv);
s32 utest_main(s32 argc, const c8** argv) {
#if defined(SP_FREESTANDING)
  {
    typedef void (*_utest_init_fn)(void);
    extern _utest_init_fn __init_array_start[];
    extern _utest_init_fn __init_array_end[];
    for (_utest_init_fn* fn = __init_array_start; fn < __init_array_end; fn++) {
      (*fn)();
    }
  }
#endif

  sp_mem_t mem = utest_mem();
  const c8* filter = SP_NULLPTR;

  for (s32 index = 1; index < argc; index++) {
    sp_str_t arg = sp_cstr_as_str(argv[index]);
    sp_str_t filter_flag = sp_str_lit("--filter=");

    if (sp_str_equal_cstr(arg, "--help")) {
      sp_os_print(sp_str_lit(
        "utest.h - the single file unit testing solution for C/C++!\n"
        "Command line Options:\n"
        "  --help              Show this message and exit.\n"
        "  --filter=<filter>   Filter the test cases to run (EG. "
        "MyTest*.a would run MyTestCase.a but not MyTestCase.b).\n"
        "  --list-tests        List testnames, one per line. Output "
        "names can be passed to --filter.\n"));
      return 0;
    } else if (sp_str_starts_with(arg, filter_flag)) {
      filter = argv[index] + filter_flag.len;
    } else if (sp_str_equal_cstr(arg, "--list-tests")) {
      sp_da_for(utest_state.tests, it) {
        sp_log("{}", sp_fmt_cstr(utest_state.tests[it].name));
      }
      return 0;
    }
  }

  u64 ran = 0;
  sp_da_for(utest_state.tests, it) {
    if (!utest_filtered(filter, utest_state.tests[it].name)) {
      ran++;
    }
  }

  sp_str_t arch = sp_str_lit("unknown");
#if defined(SP_AMD64)
  arch = sp_str_lit("x86_64");
#elif defined(SP_ARM64)
  arch = sp_str_lit("aarch64");
#endif

  sp_str_t abi = sp_str_lit("unknown");
#if defined(SP_LIBC_MSVC)
  abi = sp_str_lit("msvc");
#elif defined(SP_LIBC_GNU)
  abi = sp_str_lit("gnu");
#elif defined(SP_LIBC_NONE)
  abi = sp_str_lit("none");
#elif defined(SP_MACOS)
  abi = sp_str_lit("apple");
#elif defined(SP_LINUX)
  abi = sp_str_lit("musl");
#endif

  sp_log(
    "> running {.black} test cases on {}-{}-{}",
    sp_fmt_uint(ran),
    sp_fmt_str(arch), sp_fmt_str(sp_os_get_name()), sp_fmt_str(abi)
  );

  sp_da(const c8*) failed = sp_da_new(mem, const c8*);
  sp_da(const c8*) skipped = sp_da_new(mem, const c8*);

  sp_da_for(utest_state.tests, it) {
    utest_test_t* test = &utest_state.tests[it];
    if (utest_filtered(filter, test->name)) {
      continue;
    }

    sp_print("{}.{}...", sp_fmt_cstr(test->set), sp_fmt_cstr(test->test));

    sp_da_clear(utest_state.failures);
    sp_da_clear(utest_state.staged);

    s32 result = UTEST_TEST_PASSED;
    s64 ns = (s64)sp_tm_now_point();
    test->func(&result);
    ns = (s64)sp_tm_now_point() - ns;

    const c8* const units[] = { "ns", "us", "ms", "s" };
    u32 unit = 0;
    s64 time = ns;
    while (unit < sp_carr_len(units) - 1 && time >= 10000) {
      time /= 1000;
      unit++;
    }

    switch (result) {
      case UTEST_TEST_FAILURE: {
        sp_da_push(failed, test->name);
        sp_print("{.red} ", sp_fmt_cstr("failed"));
        break;
      }
      case UTEST_TEST_SKIPPED: {
        sp_da_push(skipped, test->name);
        sp_print("{.yellow} ", sp_fmt_cstr("skipped"));
        break;
      }
      default: {
        sp_print("{.green} ", sp_fmt_cstr("ok"));
        break;
      }
    }
    sp_log("{.gray}{.gray}", sp_fmt_int(time), sp_fmt_cstr(units[unit]));

    sp_da_for(utest_state.failures, ft) {
      utest_report_failure(&utest_state.failures[ft]);
    }
  }

  sp_log("> {.green} passed, {.red} failed, {.yellow} skipped",
    sp_fmt_uint(ran - sp_da_size(failed) - sp_da_size(skipped)),
    sp_fmt_uint(sp_da_size(failed)),
    sp_fmt_uint(sp_da_size(skipped)));

  sp_da_for(failed, it) {
    sp_log("  {.red} {}", sp_fmt_cstr("failed"), sp_fmt_cstr(failed[it]));
  }

  return (s32)sp_da_size(failed);
}

/*
   we need, in exactly one source file, define the global struct that will hold
   the data we need to run utest. This macro allows the user to declare the
   data without having to use the UTEST_MAIN macro, thus allowing them to write
   their own main() function.
*/
#define UTEST_STATE() struct utest_state_s utest_state = {0}

/*
   define a main() function to call into utest.h and start executing tests! A
   user can optionally not use this macro, and instead define their own main()
   function and manually call utest_main. The user must, in exactly one source
   file, use the UTEST_STATE macro to declare a global struct variable that
   utest requires.
*/
#define UTEST_MAIN()                                                           \
  UTEST_STATE();                                                               \
  s32 _utest_entry(s32 argc, const c8** argv) {                                \
    return utest_main(argc, argv);                                             \
  }                                                                            \
  SP_MAIN(_utest_entry)

#endif /* SHEREDOM_UTEST_H_INCLUDED */
