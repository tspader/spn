#include "spn.h"

void build(spn_pkg_ctx_t* b) {
  spn_cmake_t* cmake = spn_cmake_new(b);
  spn_cmake_add_define(cmake, "BUILD_TESTING", "OFF");
  spn_cmake_add_define(cmake, "CURL_DISABLE_TESTS", "ON");
  spn_cmake_add_define(cmake, "CURL_DISABLE_EXAMPLES", "ON");
  spn_cmake_add_define(cmake, "CURL_USE_OPENSSL", "ON");
  spn_cmake_add_define(cmake, "CURL_BROTLI", "OFF");
  spn_cmake_add_define(cmake, "CURL_ZSTD", "OFF");
  spn_cmake_add_define(cmake, "CURL_USE_LIBPSL", "OFF");
  spn_cmake_add_define(cmake, "CURL_USE_LIBSSH2", "OFF");
  spn_cmake_add_define(cmake, "CURL_USE_LIBRTMP", "OFF");
  spn_cmake_add_define(cmake, "USE_LIBIDN2", "OFF");
  spn_cmake_add_define(cmake, "USE_NGHTTP2", "OFF");
  spn_cmake_add_define(cmake, "CURL_DISABLE_LDAP", "ON");
  spn_cmake_add_define(cmake, "CURL_DISABLE_LDAPS", "ON");
  spn_cmake_add_define(cmake, "CURL_DISABLE_RTSP", "ON");
  spn_cmake_add_define(cmake, "CURL_DISABLE_FTP", "ON");
  spn_cmake_add_define(cmake, "CURL_DISABLE_FILE", "ON");
  spn_cmake_add_define(cmake, "CURL_DISABLE_TELNET", "ON");
  spn_cmake_add_define(cmake, "CURL_DISABLE_DICT", "ON");
  spn_cmake_add_define(cmake, "CURL_DISABLE_GOPHER", "ON");
  spn_cmake_add_define(cmake, "CURL_DISABLE_IMAP", "ON");
  spn_cmake_add_define(cmake, "CURL_DISABLE_POP3", "ON");
  spn_cmake_add_define(cmake, "CURL_DISABLE_SMTP", "ON");
  spn_cmake_add_define(cmake, "CURL_DISABLE_TFTP", "ON");
  spn_cmake_run(cmake);
}

void package(spn_pkg_ctx_t* b) {
  spn_cmake_t* cmake = spn_cmake_new(b);
  spn_cmake_install(cmake);
}
