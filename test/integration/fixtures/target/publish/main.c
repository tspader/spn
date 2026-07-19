#include "kit.h"
#include "kit/a.h"
#include "kit/b.h"

#if KIT_VALUE != 1 || KIT_A != 2 || KIT_B != 3
#error "expected published headers"
#endif

int main() {
  return 0;
}
