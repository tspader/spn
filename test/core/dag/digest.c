typedef struct {
  const c8* data;
  const c8* hex;
} digest_test_t;

UTEST_EMPTY_FIXTURE(digest)

static void run_digest_test(s32* utest_result, digest_test_t t) {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  sp_str_t data = sp_str_view(t.data);
  spn_dag_digest_t digest = spn_dag_digest(data.data, data.len);
  EXPECT_STR(spn_dag_digest_hex(scratch.mem, digest), t.hex);
  sp_mem_end_scratch(scratch);
}

UTEST_F(digest, empty) {
  run_digest_test(&ur, (digest_test_t) {
    .data = "",
    .hex = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"
  });
}

UTEST_F(digest, abc) {
  run_digest_test(&ur, (digest_test_t) {
    .data = "abc",
    .hex = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"
  });
}
