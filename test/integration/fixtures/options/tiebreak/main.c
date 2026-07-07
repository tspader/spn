#include "a.h"
#include "b.h"

int main() {
  return a_backend() == 2 && b_backend() == 2 ? 0 : 1;
}
