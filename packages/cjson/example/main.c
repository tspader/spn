#include <stdio.h>
#include <cJSON.h>
#include <cJSON.c>

int main(void) {
  cJSON* json = cJSON_CreateObject();
  cJSON_AddNumberToObject(json, "filmore", 69);
  cJSON_AddStringToObject(json, "guitar", "jerry");

  printf("%s\n", cJSON_Print(json));

  return 0;
}
