#include "a.h"
#include "codec.h"

int a_caps(void) {
  return codec_caps();
}

#if defined(CODEC_AUDIO) && defined(CODEC_VIDEO)
int a_audio_video() {
  return codec_audio_video();
}
#elif !defined(CODEC_AUDIO) && defined(CODEC_VIDEO)
int a_video() {
  return codec_video();
}
#else
#error "unexpected codec options"
#endif
