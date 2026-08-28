#include "toolchain.h"

#define SHA256_MAX_CASES 4

typedef struct {
  const c8* data;
  u64 fill;
  bool file;
  bool missing;
  const c8* expect;
} sha256_case_t;

typedef struct {
  const c8* name;
  sha256_case_t cases [SHA256_MAX_CASES];
} sha256_test_t;

static const sha256_test_t tests [] = {
  {
    .name = "known_vectors",
    .cases = {
      { .data = "abc", .expect = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad" },
      { .data = "", .expect = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855" },
      { .data = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", .expect = "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1" },
    },
  },
  {
    .name = "padding_boundaries",
    .cases = {
      { .fill = 55, .expect = "9f4390f8d30c2dd92ec9f095b65e2b9ae9b0a925a5258e241c9f1e910f734318" },
      { .fill = 56, .expect = "b35439a4ac6f0948b6d6f9e3c6af0f5f590ce20f1bde7090ef7970686ec6738a" },
      { .fill = 64, .expect = "ffe054fe7ae0cb6dc65c3af9b61d5209f439851db43d0ba5997337df154668eb" },
      { .fill = 65, .expect = "635361c48bb9eab14198e76ea8ab7f1a41685d6ad62aa9146d301d4f17eb0ae0" },
    },
  },
  {
    .name = "file_larger_than_chunk",
    .cases = {
      { .fill = 1000000, .file = true, .expect = "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0" },
    },
  },
  {
    .name = "file_matches_bytes",
    .cases = {
      { .data = "abc", .file = true, .expect = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad" },
      { .missing = true },
    },
  },
};

sp_test_each(sha256, digest, sha256_test_t, tests) {
  sp_mem_t mem = sp_test_arena(t);

  sp_carr_for(it->cases, at) {
    sha256_case_t c = it->cases[at];
    if (!c.data && !c.fill && !c.missing) {
      break;
    }

    if (c.missing) {
      sp_str_t hex = sp_zero;
      sp_expect_eq(t, (u32)SPN_ERROR, (u32)spn_digest_file_hex(SPN_DIGEST_SHA256, mem, sp_fs_join_path(mem, sp_test_dir(t), sp_str_lit("missing.bin")), &hex));
      continue;
    }

    sp_str_t data = sp_zero;
    if (c.fill) {
      c8* bytes = (c8*)sp_alloc(mem, c.fill);
      sp_mem_fill_u8(bytes, c.fill, (u8)0x61);
      data = sp_str(bytes, (u32)c.fill);
    } else {
      data = sp_str_view(c.data);
    }

    u8 digest [32] = sp_zero;
    spn_digest(SPN_DIGEST_SHA256, data.data, data.len, digest);
    sp_expect_str_eq_c(t, spn_digest_hex(mem, digest), c.expect);

    if (c.file) {
      sp_str_t path = sp_fs_join_path(mem, sp_test_dir(t), sp_fmt(mem, "{}.bin", sp_fmt_uint(at)).value);
      sp_fs_create_file_str(path, data);
      sp_str_t hex = sp_zero;
      sp_must_eq(t, (u32)SPN_OK, (u32)spn_digest_file_hex(SPN_DIGEST_SHA256, mem, path, &hex));
      sp_expect_str_eq_c(t, hex, c.expect);
    }
  }

  return SP_OK;
}
