#include <stdio.h>
#include <cJSON.h>
#include <cJSON.c>

int main(void) {
  cJSON *root = cJSON_CreateObject();
  cJSON_AddNumberToObject(root, "value", 42);
  char *text = cJSON_PrintUnformatted(root);
  printf("%s\n", text);
  cJSON_free(text);
  cJSON_Delete(root);
  return 0;
}
