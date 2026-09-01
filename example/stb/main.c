#include <stdio.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define WIDTH 256
#define HEIGHT 256

static unsigned char pixels [WIDTH * HEIGHT * 3];

int main(void) {
  for (int y = 0; y < HEIGHT; y++) {
    for (int x = 0; x < WIDTH; x++) {
      int dx = x - WIDTH / 2;
      int dy = y - HEIGHT / 2;
      int inside = dx * dx + dy * dy < 80 * 80;

      unsigned char* pixel = &pixels[(y * WIDTH + x) * 3];
      pixel[0] = inside ? 240 : x;
      pixel[1] = inside ? 90 : y;
      pixel[2] = inside ? 60 : 128;
    }
  }

  stbi_write_png("out.png", WIDTH, HEIGHT, 3, pixels, WIDTH * 3);
  printf("wrote out.png\n");
  return 0;
}
