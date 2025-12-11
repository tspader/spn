#include "utest.h"

UTEST_MAIN()

#define uf utest_fixture
#define ur (*utest_result)

UTEST(stream, regular_failure) {
  fprintf(stdout, "hello, stdout!\n");
  fflush(stdout);
  fprintf(stderr, "hello, stderr!\n");
  fflush(stderr);

  EXPECT_EQ(69, 420);
}
