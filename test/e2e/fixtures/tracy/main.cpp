#include <tracy/Tracy.hpp>

int main() {
  FrameMark;
#ifdef TRACY_ENABLE
  return 0;
#else
  return 1;
#endif
}
