#include <stdio.h>
#include <miniz.h>
#include <miniz.c>
#include <miniz_tdef.c>
#include <miniz_tinfl.c>

int main(void) {
  mz_ulong crc = mz_crc32(MZ_CRC32_INIT, (const unsigned char*)"spn", 3);
  printf("crc32: %u\n", (unsigned) crc);
  return 0;
}
