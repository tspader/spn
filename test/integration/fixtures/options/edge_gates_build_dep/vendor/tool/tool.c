#include "tool.h"

int tool_value(void) {
#ifdef TOOL_CODEGEN
  return 9;
#else
  return 3;
#endif
}
