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
    .hex = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"
  },
  {
    .name = "abc",
    .data = "abc",
    .hex = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"
  },
};

sp_test_each(digest, hash, digest_test_t, digest_tests) {
  sp_str_t data = sp_str_view(it->data);
  spn_dag_digest_t digest = spn_dag_digest(data.data, data.len);
  sp_expect(t, spn_dag_digest_valid(digest));
  sp_expect_str_eq_c(t, spn_dag_digest_hex(sp_test_arena(t), digest), it->hex);
  return SP_OK;
}

sp_test(digest, zero_is_invalid) {
  sp_expect(t, !spn_dag_digest_valid((spn_dag_digest_t) sp_zero));
  return SP_OK;
}
