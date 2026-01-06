#include "spn.h"

#define SP_IMPLEMENTATION
#include "sp.h"

#include "cJSON.h"
#include "cJSON.c"

void configure(spn_build_ctx_t* b) {
  spn_log(b, "hello!");

  sp_str_t foo = sp_str_lit("hello from sp.h!");
  spn_log(b, sp_str_to_cstr(foo));

  cJSON* json = cJSON_CreateObject();
  cJSON_AddNumberToObject(json, "filmore", 69);
  cJSON_AddStringToObject(json, "guitar", "jerry");

  spn_log(b, cJSON_Print(json));

}
