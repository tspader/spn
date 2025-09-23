#include <stdio.h>
#include <uv.h>

int main(void) {
  uv_loop_t loop;
  if (uv_loop_init(&loop) != 0) {
    fprintf(stderr, "failed to initialize loop\n");
    return 1;
  }
  printf("libuv loop alive: %d\n", uv_loop_alive(&loop));
  uv_loop_close(&loop);
  return 0;
}
