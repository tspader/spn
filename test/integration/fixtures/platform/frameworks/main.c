#include <CoreFoundation/CoreFoundation.h>

int main() {
  CFAbsoluteTime time = CFAbsoluteTimeGetCurrent();
  return time > 0 ? 0 : 1;
}
