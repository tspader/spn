#include "a.h"
#include "b.h"

int main() {
  return a_value() == 2 && b_value() == 3 ? 0 : 1;
}
