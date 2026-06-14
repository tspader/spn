#include "yyjson.h"

#include <stdio.h>
#include <string.h>

int main(void) {
  const char* json = "{\"tool\":\"spn\",\"answer\":42}";

  yyjson_doc* doc = yyjson_read(json, strlen(json), 0);
  if (!doc) {
    fprintf(stderr, "yyjson_stub: parse failed\n");
    return 1;
  }

  yyjson_val* root   = yyjson_doc_get_root(doc);
  yyjson_val* tool   = yyjson_obj_get(root, "tool");
  yyjson_val* answer = yyjson_obj_get(root, "answer");

  printf("yyjson %s linked: tool=%s answer=%d\n",
         YYJSON_VERSION_STRING,
         yyjson_get_str(tool),
         yyjson_get_int(answer));

  yyjson_doc_free(doc);
  return 0;
}
