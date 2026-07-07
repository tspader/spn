#include "codec.h"

int codec_caps(void) {
  int caps = 0;
#ifdef CODEC_AUDIO
  caps |= 1;
#endif
#ifdef CODEC_VIDEO
  caps |= 2;
#endif
  return caps;
}
