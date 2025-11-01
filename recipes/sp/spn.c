#include "spn/spn.h"

void build() {

}

void package(spn_dep_t* dep) {
  spn_copy(dep, SPN_DIR_SOURCE, "sp.h", SPN_DIR_WORK, "");
}
