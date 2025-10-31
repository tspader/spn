#include "spn/spn.h"

spn_version_info_t spn_version() {
  return (spn_version_info_t) {
    .version = SPN_VERSION,
    .commit = SPN_COMMIT,
  };
}
