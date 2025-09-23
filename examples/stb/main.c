#include <stdio.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

int main(void) {
  stbi_set_flip_vertically_on_load(1);
  printf("stb image version: %d\n", STBI_VERSION);
  return 0;
}
