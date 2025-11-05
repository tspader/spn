#define SP_IMPLEMENTATION
#include "sp.h"

#include <stdio.h>
#include <cJSON.h>
#include <cJSON.c>

int main(void) {
  cJSON* json = cJSON_CreateObject();
  cJSON_AddNumberToObject(json, "filmore", 69);
  cJSON_AddStringToObject(json, "guitar", "jerry");

  sp_str_builder_t builder = SP_ZERO_INITIALIZE();
  sp_str_builder_append_cstr(&builder, "Here's some JSON:");
  sp_str_builder_new_line(&builder);
  sp_str_builder_append_fmt(&builder, "{:fg brightcyan}", SP_FMT_CSTR(cJSON_Print(json)));
  sp_os_log(sp_str_builder_write(&builder));

  return 0;
}
