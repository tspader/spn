#ifndef SPN_TEST_CAPS_H
#define SPN_TEST_CAPS_H

#include "sp.h"
#include "compiler/driver.h"

typedef struct {
  spn_sanitizer_set_t sanitize;
  spn_os_t os;
  const c8* target;
} test_when_t;

sp_str_t test_when_blocked(const test_when_t* when);
bool test_when_runs(const test_when_t* when);

#endif
