#include "spn.h"

SPN_EXPORT
s32 package(spn_t* spn) {
  spn_fs_copy("/source/data/D", "/store/misc");
  return 0;
}
