#include "dag_test.h"

typedef struct {
  const c8* name;
  const c8* data;
  const c8* hex;
} digest_test_t;

static const digest_test_t digest_tests [] = {
  {
    .name = "empty",
    .data = "",
    .hex = "af1349b9f5f9a1a6a0404dea36dcc9499bcb25c9adc112b7cc9a93cae41f3262"
  },
  {
    .name = "abc",
    .data = "abc",
    .hex = "6437b3ac38465133ffb63b75273a8db548c558465d79db03fd359c6cd5bd9d85"
  },
};

sp_test_each(dag_digest, hash, digest_test_t, digest_tests) {
  sp_str_t data = sp_str_view(it->data);
  spn_dag_digest_t digest = spn_dag_digest(data.data, data.len);
  sp_expect(t, spn_dag_digest_valid(digest));
  sp_expect_str_eq_c(t, spn_dag_digest_hex(sp_test_arena(t), digest), it->hex);
  return SP_OK;
}

sp_test(dag_digest, zero_is_invalid) {
  sp_expect(t, !spn_dag_digest_valid((spn_dag_digest_t) sp_zero));
  return SP_OK;
}
