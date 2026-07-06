#include "num.h"
#include "foo.h"
#include "tool.h"

#if NUM_VERSION != 10
  #error "tool must resolve num 1.0.0 in its own unit, not the root's 2.0.0"
#endif

int tool_value(void) {
  return foo_value();
}
