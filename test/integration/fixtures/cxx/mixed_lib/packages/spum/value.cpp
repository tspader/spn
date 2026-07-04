#include "spum.h"

namespace spum {

struct value {
  int get() const { return 69; }
};

}

extern "C" int spum_cxx_value(void) {
  spum::value* v = new spum::value();
  int result = v->get();
  delete v;
  return result;
}
