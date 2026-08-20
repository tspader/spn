#ifndef SPN_TEST_CAPS_H
#define SPN_TEST_CAPS_H

#include "sp.h"
#include "compiler/driver.h"

#if defined(SP_ARM64)
  #define SPN_TEST_ARCH "aarch64"
#else
  #define SPN_TEST_ARCH "x86_64"
#endif

#if defined(SP_MACOS)
  #define SPN_TEST_TRIPLE SPN_TEST_ARCH "-macos"
#elif defined(SP_WIN32)
  #define SPN_TEST_TRIPLE SPN_TEST_ARCH "-windows-gnu"
#else
  #define SPN_TEST_TRIPLE SPN_TEST_ARCH "-linux-gnu"
#endif

typedef struct {
  spn_sanitizer_set_t sanitize;
  spn_os_t os;
  const c8* target;
  bool exports;
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
sp_str_t  test_when_blocked(test_when_t when);
bool      test_when_runs(const test_when_t* when);

#endif
