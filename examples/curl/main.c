#include <stdio.h>
#include <curl/curl.h>

int main(void) {
  if (curl_global_init(CURL_GLOBAL_DEFAULT) != 0) {
    fprintf(stderr, "curl init failed\n");
    return 1;
  }
  CURL *handle = curl_easy_init();
  if (handle) {
    curl_easy_cleanup(handle);
  }
  curl_global_cleanup();
  puts("curl ready");
  return 0;
}
