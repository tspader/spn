#include "a.h"
#include "b.h"

int main() {
  return a_caps() == 3 && b_caps() == 3 ? 0 : 1;
}
