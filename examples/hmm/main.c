#include <stdio.h>
#include <HandmadeMath.h>

int main(void) {
  HMM_Vec3 a = HMM_V3(1.0f, 2.0f, 3.0f);
  HMM_Vec3 b = HMM_V3(4.0f, 5.0f, 6.0f);
  HMM_Vec3 sum = HMM_AddV3(a, b);
  printf("sum: %.1f %.1f %.1f\n", sum.X, sum.Y, sum.Z);
  return 0;
}
