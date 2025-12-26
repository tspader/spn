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
  sp_str_builder_append_cstr(&builder, "rendered json:");
  sp_str_builder_new_line(&builder);
  sp_str_builder_append_fmt(&builder, "{:fg brightcyan}", SP_FMT_CSTR(cJSON_Print(json)));
  sp_os_print(sp_str_builder_write(&builder));
  sp_os_print(sp_str_lit("\n"));

  return 0;
}
