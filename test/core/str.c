#include "spn_test.h"

#include "str/str.h"

#define MAX_ENTRIES 8

typedef struct {
  const c8* name;
  const c8* str;
  c8 sep;
  const c8* expect [MAX_ENTRIES];
  u32 num_expect;
} word_t;

static const word_t word_tests [] = {
  { "plain",        "a,b,c",   ',', { "a", "b", "c" }, 3 },
  { "single",       "a",       ',', { "a" },           1 },
  { "empty",        "",        ',', { 0 },             0 },
  { "leading_sep",  ",a",      ',', { "a" },           1 },
  { "trailing_sep", "a,",      ',', { "a" },           1 },
  { "inner_empty",  "a,,b",    ',', { "a", "b" },      2 },
  { "only_seps",    ",,",      ',', { 0 },             0 },
  { "path_unix",    "/a:/b",   ':', { "/a", "/b" },    2 },
  { "path_windows", "C:\\a;;", ';', { "C:\\a" },       1 },
};

sp_test_each(str, word, word_t, word_tests) {
  u32 n = 0;
  sp_str_for_word(sp_cstr_as_str(it->str), it->sep, word) {
    sp_must(t, n < it->num_expect);
    sp_expect_str_eq_c(t, word.entry, it->expect[n]);
    n++;
  }
  sp_expect_eq(t, n, it->num_expect);
  return SP_OK;
}
