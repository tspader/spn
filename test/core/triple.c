#define SP_IMPLEMENTATION
#include "sp.h"

#define UTEST_IMPLEMENTATION
#include "utest.h"

#include "test.h"
#include "spn.h"
#include "triple/triple.h"

UTEST_MAIN();


///////////////
// EXECUTOR //
//////////////
typedef struct {
  const c8* triple;
  spn_arch_t arch;
  spn_os_t os;
  spn_abi_t abi;
} test_t;



void run_case(s32* utest_result, test_t test) {
  spn_triple_t triple = spn_triple_from_str(sp_str_view(test.triple));
  EXPECT_EQ(triple.arch, test.arch);
  EXPECT_EQ(triple.os, test.os);
  EXPECT_EQ(triple.abi, test.abi);
}

UTEST(triple, parse) {
  test_t tests [] = {
    { "x86_64-windows-mingw32", SPN_ARCH_X64, SPN_OS_WINDOWS, SPN_ABI_MINGW },
  };

  sp_carr_for(tests, it) {
    run_case(utest_result, tests[it]);
  }
}
