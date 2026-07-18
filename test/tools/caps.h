#ifndef SPN_TEST_CAPS_H
#define SPN_TEST_CAPS_H

#include "sp.h"
#include "compiler/driver.h"

typedef struct {
  spn_sanitizer_set_t sanitize;
  spn_os_t os;
  const c8* target;
} test_when_t;

typedef struct {
  const c8* name;
  spn_cc_driver_t driver;
  spn_abi_t abi;
  const c8* targets [12];
} test_toolchain_t;

const test_toolchain_t* test_toolchain(void);
spn_triple_t test_host(void);
const c8* test_target_alternate(void);
sp_str_t  test_when_blocked(const test_when_t* when);
bool      test_when_runs(const test_when_t* when);

#endif
