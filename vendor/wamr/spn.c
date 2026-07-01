#include "spn.h"

spn_err_t package(spn_t* spn) {
  if (spn_copy(spn, SPN_DIR_SOURCE, "core/iwasm/include/*.h", SPN_DIR_INCLUDE, ".")) {
    return SPN_ERROR;
  }
  return SPN_OK;
}
