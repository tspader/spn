#include "codec.h"

int codec_mp3(void) {
#ifdef CODEC_MP3
  return 1;
#else
  return 0;
#endif
}
