#ifndef SPN_VERSION_H
#define SPN_VERSION_H

#if defined(__has_include)
  #if __has_include("build_info.gen.h")
    #include "build_info.gen.h"
  #endif
#endif

#ifndef SPN_VERSION
  #define SPN_VERSION "0.2.0"
#endif

#ifndef SPN_BUILD_CHANNEL
  #define SPN_BUILD_CHANNEL "dev"
#endif

#ifndef SPN_BUILD_COMMIT
  #define SPN_BUILD_COMMIT ""
#endif

#endif
