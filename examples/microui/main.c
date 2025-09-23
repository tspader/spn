#include <stdio.h>
#include <string.h>
#include <microui.h>
#include <microui.c>

static int text_width(mu_Font font, const char *text, int len) {
  (void) font;
  if (len < 0) {
    len = (int) strlen(text);
  }
  return len * 8;
}

static int text_height(mu_Font font) {
  (void) font;
  return 16;
}

int main(void) {
  mu_Context ctx;
  mu_init(&ctx);
  ctx.text_width = text_width;
  ctx.text_height = text_height;
  mu_begin(&ctx);
  mu_end(&ctx);
  printf("frame: %d\n", ctx.frame);
  return 0;
}
