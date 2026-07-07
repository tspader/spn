#include "codec.h"

int codec_caps(void) {
  int caps = 0;
#ifdef CODEC_MP3
  caps |= 1;
#endif
#ifdef CODEC_OGG
  caps |= 2;
#endif
  return caps;
}
