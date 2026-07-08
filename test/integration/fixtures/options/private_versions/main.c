#include "codec.h"
#include "snd.h"

int main() {
  if (snd_value() != 2) return 1;
  if (codec_value() != 3) return 2;
  return 0;
}
