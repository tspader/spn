#define SP_IMPLEMENTATION
#include "sp.h"

#include <stdio.h>
#include <curl/curl.h>

int main(void) {
  sp_init_default();
  SP_LOG("{:fg brightyellow}", SP_FMT_CSTR("curl_global_init()"));
  if (curl_global_init(CURL_GLOBAL_DEFAULT)) {
    SP_FATAL("{:fg brightyellow} failed", SP_FMT_CSTR("curl_global_init()"));
    SP_EXIT_FAILURE();
  }

  SP_LOG("{:fg brightyellow}", SP_FMT_CSTR("curl_easy_init()"));
  CURL* curl = curl_easy_init();
  if (!curl) {
    SP_FATAL("{:fg brightyellow} failed", SP_FMT_CSTR("curl_easy_init()"));
    SP_EXIT_FAILURE();
  }

  const c8* url = "https://example.org/get";
  SP_LOG("Making a request to {:fg brightcyan}", SP_FMT_CSTR(url));
  curl_easy_setopt(curl, CURLOPT_URL, url);
  if (curl_easy_perform(curl)) {
    SP_FATAL("{:fg brightcyan} failed", SP_FMT_CSTR("curl_easy_perform()"));
    SP_EXIT_FAILURE();
  }

  SP_LOG("{:fg green}", SP_FMT_CSTR("Success!"));

  return 0;
}
