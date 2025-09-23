#include <stdio.h>
#include <sodium.h>

int main(void) {
  if (sodium_init() < 0) {
    fprintf(stderr, "failed to initialize libsodium\n");
    return 1;
  }
  unsigned char hash[crypto_hash_sha256_BYTES];
  crypto_hash_sha256(hash, (const unsigned char*)"spn", 3);
  printf("hash length: %zu\n", sizeof hash);
  return 0;
}
