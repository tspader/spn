#include "kram.h"
#include "spum.h"

int main() {
  return kram_uv() == -1 && spum_value() == 2 ? 0 : 1;
}
