#include <stdio.h>
#include <stdint.h>
#include <quickjs.h>

int main(void) {
  JSRuntime *rt = JS_NewRuntime();
  JSContext *ctx = JS_NewContext(rt);
  JSValue value = JS_NewInt32(ctx, 42);
  int32_t out = 0;
  JS_ToInt32(ctx, &out, value);
  printf("quickjs value: %d\n", out);
  JS_FreeValue(ctx, value);
  JS_FreeContext(ctx);
  JS_FreeRuntime(rt);
  return 0;
}
