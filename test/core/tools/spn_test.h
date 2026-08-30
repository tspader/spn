#ifndef SPN_TEST_H
#define SPN_TEST_H

#include "sp.h"
#include "macro/macro.h"
#include "sp/sp_test.h"
#include "fixture.h"

#include "ctx/types.h"
#include "event/types.h"

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

sp_err_t spn_test_ctx_setup(sp_test_t* t);
sp_da(spn_event_t) spn_test_drain_errs(sp_mem_t mem);

#endif
