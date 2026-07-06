#include "a.h"
#include "c.h"

int main() {
  return c_value() == 190 && a_value() == 191 ? 0 : 1;
}
