#include "a.h"
#include "b.h"

int main() {
  return a_caps() == b_caps() ? a_caps() : 100;
}
