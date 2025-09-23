#define NK_IMPLEMENTATION
#include <nuklear.h>

int main(void) {
  struct nk_color color = nk_rgb(255, 0, 0);
  return color.r == 255 ? 0 : 1;
}
