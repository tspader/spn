#ifndef SPN_TEST_SUITE_H
#define SPN_TEST_SUITE_H

#include "sp.h"
#include "sp/sp_test.h"
#include "macro/macro.h"

#define r(str) str "\n"
#define q(str) #str
#define kv(key, val) q(key) ": " #val

#define ts(section) "[" q(section) "]\n"
#define tk(key) q(key) " = "
#define tkv(key, val) tk(key) #val

static inline bool test_str_is_hex(sp_str_t str) {
  sp_for(it, str.len) {
    c8 c = str.data[it];
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) {
      return false;
    }
  }
  return str.len > 0;
}

#endif
