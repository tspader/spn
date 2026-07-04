#include "main.embed.h"

static int str_equal(const char* a, const char* b) {
  while (*a && *b) {
    if (*a != *b) {
      return 0;
    }
    a++;
    b++;
  }
  return *a == *b;
}

int main() {
  const unsigned char expected[] = { 'h', 'e', 'l', 'l', 'o', ' ', 'w', 'o', 'r', 'l', 'd' };
  unsigned long long expected_len = sizeof(expected);

  if (hello_txt_size != expected_len) {
    return 1;
  }

  for (unsigned long long it = 0; it < expected_len; it++) {
    if (hello_txt[it] != expected[it]) {
      return 2;
    }
  }

  if (spn_embed_count != 1) {
    return 3;
  }

  if (!str_equal(spn_embed_manifest[0].path, "hello.txt")) {
    return 4;
  }

  if (spn_embed_manifest[0].size != expected_len) {
    return 5;
  }

  const unsigned char* data = (const unsigned char*)spn_embed_manifest[0].data;
  for (unsigned long long it = 0; it < expected_len; it++) {
    if (data[it] != expected[it]) {
      return 6;
    }
  }

  return 0;
}
