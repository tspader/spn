#include "mixer.h"

#ifdef MIXER_FLAC
#include "flaclib.h"
#endif

int mixer_caps(void) {
#ifdef MIXER_FLAC
  return flaclib_value();
#else
  return 0;
#endif
}
