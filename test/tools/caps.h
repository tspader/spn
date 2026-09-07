#ifndef SPN_TEST_CAPS_H
#define SPN_TEST_CAPS_H

#include "sp.h"
#include "compiler/driver.h"
#include "toolchain/types.h"

#if defined(SP_ARM64)
  #define SPN_TEST_ARCH "aarch64"
#else
  #define SPN_TEST_ARCH "x86_64"
#endif

#if defined(SP_MACOS)
  #define SPN_TEST_TRIPLE SPN_TEST_ARCH "-macos-apple"
#elif defined(SP_WIN32)
  #define SPN_TEST_TRIPLE SPN_TEST_ARCH "-windows-gnu"
#else
  #define SPN_TEST_TRIPLE SPN_TEST_ARCH "-linux-gnu"
#endif

#define SPN_TEST_MAX_PROGRAMS 4
#define SPN_TEST_MAX_LANES 4

typedef struct {
  spn_sanitizer_set_t sanitize;
  spn_os_t os;
  spn_os_t host;
  spn_cc_driver_t driver;
  spn_ld_family_t linker;
  const c8* target;
  const c8* lanes [SPN_TEST_MAX_LANES];
  const c8* programs [SPN_TEST_MAX_PROGRAMS];
  bool exports;
  bool cxx;
  bool deterministic;
  bool msvc_todo;
  bool shell;
} test_when_t;

typedef struct {
  const c8* name;
  spn_toolchain_info_t* info;
} test_toolchain_t;

const test_toolchain_t* test_toolchain(void);
sp_str_t  test_lanes_toml(void);
spn_triple_t test_host(void);
const c8* test_target_alternate(void);
const c8* test_host_triple(void);
sp_str_t  test_when_blocked(test_when_t when);
bool      test_when_runs(const test_when_t* when);

#endif
