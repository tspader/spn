#include "foo.h"
#include "tool.h"

int tool_value(void) {
  return foo_version() + 100;
}
