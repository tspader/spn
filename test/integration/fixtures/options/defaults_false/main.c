#include "codec.h"

int main() {
  return codec_caps() == 2 ? 0 : 1;
}
