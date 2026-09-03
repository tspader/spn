#include <stdio.h>

#include "qrcodegen.h"

int main(int argc, char** argv) {
  const char* text = argc > 1 ? argv[1] : "https://www.youtube.com/watch?v=dQw4w9WgXcQ";

  uint8_t qr [qrcodegen_BUFFER_LEN_MAX];
  uint8_t scratch [qrcodegen_BUFFER_LEN_MAX];
  if (!qrcodegen_encodeText(text, scratch, qr, qrcodegen_Ecc_MEDIUM,
        qrcodegen_VERSION_MIN, qrcodegen_VERSION_MAX, qrcodegen_Mask_AUTO, true)) {
    return 1;
  }

  int size = qrcodegen_getSize(qr);
  for (int y = -2; y < size + 2; y++) {
    for (int x = -2; x < size + 2; x++) {
      fputs(qrcodegen_getModule(qr, x, y) ? "  " : "##", stdout);
    }
    fputc('\n', stdout);
  }
  return 0;
}
