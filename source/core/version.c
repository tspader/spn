#include "version.h"

#ifndef SPN_BUILD_CHANNEL
  #define SPN_BUILD_CHANNEL "dev"
#endif

#ifndef SPN_BUILD_COMMIT
  #define SPN_BUILD_COMMIT ""
#endif

const c8* spn_build_channel = SPN_BUILD_CHANNEL;
const c8* spn_build_commit = SPN_BUILD_COMMIT;
