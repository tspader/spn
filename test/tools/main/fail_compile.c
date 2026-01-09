#include "utest.h"

UTEST_MAIN()

#define uf utest_fixture
#define ur (*utest_result)

UTEST(stream, regular_failure) {
  printf("%d", this_will_fail_to_compile_to);
}
