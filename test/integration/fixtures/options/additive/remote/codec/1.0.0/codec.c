#include "codec.h"

int codec_caps(void) {
#if defined(CODEC_AUDIO) && defined(CODEC_VIDEO)
  return 3;
#elif !defined(CODEC_AUDIO) && defined(CODEC_VIDEO)
  return 2;
#else
#error "unexpected codec options"
#endif
}

#if defined(CODEC_AUDIO) && defined(CODEC_VIDEO)
int codec_audio_video() {
  return 3;
}
#elif !defined(CODEC_AUDIO) && defined(CODEC_VIDEO)
int codec_video() {
  return 2;
}
#endif
