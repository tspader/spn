#include "foo.h"
#include "tool.h"

#if FOO_VERSION != 10
  #error "tool must compile against its own foo 1.0.0, not the root's pick"
#endif

int tool_value(void) {
  return foo_version() + 100;
}
